#include "profile.h"

#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace meizu {
namespace {
struct File {
    int fd = -1;
    File() = default;
    explicit File(int value) : fd(value) {}
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept : fd(other.fd) { other.fd = -1; }
    ~File() { if (fd >= 0) close(fd); }
};

bool RelativePath(const std::string& path) {
    if (path.empty() || path.size() > 1024) return false;
    for (char c : path) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'))
            return false;
    }
    std::istringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == "..") return false;
    }
    if (path.back() == '/') return false;
    const auto slash = path.find('/');
    const auto top = path.substr(0, slash);
    if (slash == std::string::npos ||
        (top != "lib" && top != "lib64" && top != "bin" && top != "etc" &&
         top != "firmware")) return false;
    // Profiles cannot replace init, SELinux, identity or linker policy.
    if (path == "etc" || path.compare(0, 9, "etc/init/") == 0 ||
        path.compare(0, 12, "etc/selinux/") == 0 ||
        path.compare(0, 7, "etc/ld.") == 0 ||
        path.compare(0, 10, "etc/vintf/") == 0) return false;
    return true;
}

bool TrustedDirectory(int fd) {
    struct stat st {};
    return fd >= 0 && fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) &&
           st.st_uid == 0 && !(st.st_mode & 0022);
}

File OpenRegular(int root, const std::string& relative) {
    File dir(dup(root));
    if (!TrustedDirectory(dir.fd)) return File();
    std::istringstream parts(relative);
    std::string part;
    while (std::getline(parts, part, '/')) {
        bool last = parts.peek() == std::char_traits<char>::eof();
        int next = openat(dir.fd, part.c_str(), O_NOFOLLOW | O_CLOEXEC |
                          (last ? O_RDONLY : O_RDONLY | O_DIRECTORY));
        if (next < 0) return File();
        if (!last && !TrustedDirectory(next)) { close(next); return File(); }
        close(dir.fd);
        dir.fd = next;
    }
    struct stat st {};
    if (fstat(dir.fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != 0 ||
        (st.st_mode & 0022)) return File();
    return dir;
}

std::string Hash(int fd) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
    char buffer[65536];
    ssize_t count = 0;
    while (ok && (count = read(fd, buffer, sizeof(buffer))) > 0)
        ok = EVP_DigestUpdate(ctx, buffer, count) == 1;
    unsigned char digest[32];
    unsigned int size = 0;
    ok = ok && count == 0 && EVP_DigestFinal_ex(ctx, digest, &size) == 1 && size == 32;
    EVP_MD_CTX_free(ctx);
    if (!ok) return "";
    std::string result;
    for (auto c : digest) {
        result += "0123456789abcdef"[c >> 4];
        result += "0123456789abcdef"[c & 15];
    }
    return result;
}
}

std::string BoardFromHandoff(const std::string& handoff) {
    for (const auto* board : {"m681", "l681"}) {
        const std::string expected = std::string("schema=1 selected_dt=") + board +
            " physical=" + board + " verified=1 origin=bootloader";
        if (handoff == expected || handoff == expected + "\n") return board;
    }
    return "";
}

std::string BoardFromCompatible(const std::string& compatible) {
    std::istringstream input(compatible);
    std::string token, board;
    while (std::getline(input, token, '\0')) {
        std::string candidate;
        if (token == "meizu,m681") candidate = "m681";
        if (token == "meizu,l681") candidate = "l681";
        if (token == "meizu,m1621") candidate = "m1621";
        if (!candidate.empty()) {
            if (!board.empty() && board != candidate) return "";
            board = candidate;
        }
    }
    return board;
}

std::string ProfileForBoard(const std::string& board) {
    return board == "m1621" ? "m681" : board;
}

bool ActivateProfile(const std::string& vendor, const std::string& board,
                     std::string* error) {
    auto fail = [&](const std::string& message) { *error = message; return false; };
    if (board != "m681" && board != "l681") return fail("unverified board");
    File root(open(vendor.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    File profiles(openat(root.fd, "meizu", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    File profile(openat(profiles.fd, board.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    File manifest = OpenRegular(profile.fd, "mounts.tsv");
    if (!TrustedDirectory(root.fd) || !TrustedDirectory(profiles.fd) ||
        !TrustedDirectory(profile.fd) || manifest.fd < 0) return fail("missing profile");
    std::string packet;
    char buffer[4096];
    ssize_t count;
    while ((count = read(manifest.fd, buffer, sizeof(buffer))) > 0) {
        packet.append(buffer, count);
        if (packet.size() > 1024 * 1024) return fail("oversized manifest");
    }
    if (count < 0) return fail("unreadable manifest");
    std::istringstream input(packet);
    std::string line;
    if (!std::getline(input, line) || line != "m3note-profile-v1 " + board)
        return fail("profile identity mismatch");
    if (!std::getline(input, line) || line != "origin_verified=1")
        return fail("profile source origin not admitted");
    struct Entry { std::string path; std::string sha; };
    std::vector<Entry> entries;
    while (std::getline(input, line)) {
        if (entries.size() >= 4096) return fail("too many files");
        const auto tab = line.find('\t');
        if (tab != 64) return fail("malformed manifest");
        std::string sha = line.substr(0, tab), path = line.substr(tab + 1);
        if (!RelativePath(path) || !std::all_of(sha.begin(), sha.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            return fail("invalid profile path or hash");
        for (const auto& entry : entries)
            if (entry.path == path) return fail("duplicate mount");
        File source = OpenRegular(profile.fd, path), target = OpenRegular(root.fd, path);
        if (source.fd < 0 || target.fd < 0 || Hash(source.fd) != sha)
            return fail("missing file, unsafe inode or hash mismatch: " + path);
        entries.push_back({path, sha});
    }
    if (entries.empty()) return fail("empty profile");
    std::vector<std::string> mounted;
    for (const auto& entry : entries) {
        // Keep descriptor use bounded even for a profile with thousands of
        // files; revalidate each immutable source before mounting its inode.
        File source = OpenRegular(profile.fd, entry.path), target = OpenRegular(root.fd, entry.path);
        bool valid = source.fd >= 0 && target.fd >= 0 && Hash(source.fd) == entry.sha;
        const auto src = "/proc/self/fd/" + std::to_string(source.fd);
        const auto dst = "/proc/self/fd/" + std::to_string(target.fd);
        const auto name = vendor + "/" + entry.path;
        bool ok = valid && mount(src.c_str(), dst.c_str(), nullptr, MS_BIND, nullptr) == 0;
        if (ok) {
            mounted.push_back(name);
            ok = mount(nullptr, name.c_str(), nullptr, MS_BIND | MS_REMOUNT | MS_RDONLY, nullptr) == 0;
        }
        if (!ok) {
            bool rollback = true;
            for (auto it = mounted.rbegin(); it != mounted.rend(); ++it)
                if (umount2(it->c_str(), 0)) rollback = false;
            return fail(rollback ? "profile mount failed; rolled back" : "profile rollback failed");
        }
    }
    return true;
}
}
