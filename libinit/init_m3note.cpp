#include "profile.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

// Exported by libinit; the socket property service is not running yet when
// vendor_load_properties is called from PropertyInit.
namespace android::init {
uint32_t InitPropertySet(const std::string&, const std::string&);
}

namespace {

// Display rotation SurfaceFlinger must add for this kernel.  A kernel that
// handles the panel flip itself reports it in
// /sys/module/mtkfb/parameters/forge_sf_hwrotation.  Without the file the
// kernel flips OVL by 180 on both boards: right for the M681 panel, upside
// down for L681, which then needs 180 on top.
std::string SfRotation(const std::string& board) {
    std::string text;
    if (android::base::ReadFileToString("/sys/module/mtkfb/parameters/forge_sf_hwrotation", &text)) {
        const std::string value = text.substr(0, text.find_first_of(" \t\r\n"));
        if (value == "0" || value == "90" || value == "180" || value == "270") {
            LOG(INFO) << "m3note-profile: display rotation " << value << " from the kernel";
            return value;
        }
        LOG(WARNING) << "m3note-profile: ignoring forge_sf_hwrotation '" << value << "'";
    }
    return board == "l681" ? "180" : "0";
}

}  // namespace

void vendor_load_properties() {
    std::string compatible;
    if (!android::base::ReadFileToString("/proc/device-tree/compatible", &compatible))
        android::base::ReadFileToString("/sys/firmware/devicetree/base/compatible", &compatible);
    // SurfaceFlinger reads only ro.surface_flinger.primary_display_orientation;
    // ro.sf.hwrotation is published with the same value for older consumers.
    // Set before the profile: the rotation depends on the board, not on the blobs.
    const std::string rotation = SfRotation(meizu::BoardFromCompatible(compatible));
    android::init::InitPropertySet("ro.surface_flinger.primary_display_orientation",
                                   "ORIENTATION_" + rotation);
    android::init::InitPropertySet("ro.sf.hwrotation", rotation);
    // The common kernel publishes the factory-verified board here.  A
    // single-board kernel has no m3note_board module; its boot image carries
    // only its own DTB, so the DT compatible is the board.  A present but
    // unverified handoff is still refused below.
    constexpr char kIdentity[] = "/sys/module/m3note_board/parameters/identity";
    std::string board, source;
    if (access(kIdentity, F_OK) == 0) {
        std::string handoff;
        android::base::ReadFileToString(kIdentity, &handoff);
        board = meizu::BoardFromHandoff(handoff);
        source = "factory";
    } else if (errno == ENOENT) {
        board = meizu::BoardFromCompatible(compatible);
        source = "dt";
    }
    std::string error;
    // The M5 Note (m1621) has no factory handoff and no profile of its own:
    // it is identified by its DT and runs the M681 vendor profile.
    const std::string profile = meizu::ProfileForBoard(board);
    if (!board.empty())
        android::init::InitPropertySet("ro.vendor.meizu.board", board);
    if (board.empty() || board != meizu::BoardFromCompatible(compatible) ||
        !meizu::ActivateProfile("/system/vendor", profile, &error)) {
        if (error == "profile rollback failed")
            LOG(FATAL) << "M3 Note vendor profile rollback failed";
        LOG(ERROR) << "M3 Note vendor profile unavailable: " << error;
        android::init::InitPropertySet("ro.vendor.meizu.profile", "unknown");
        android::init::InitPropertySet("ro.vendor.meizu.profile_ready", "0");
        return;
    }
    if (android::init::InitPropertySet("ro.vendor.meizu.profile", profile) == 0)
        android::init::InitPropertySet("ro.vendor.meizu.profile_ready", "1");
    android::init::InitPropertySet("ro.vendor.meizu.profile_source", source);
    // Board-specific audio configuration: audio_get_configuration_paths()
    // (system/media audio_config.h) searches /vendor/etc/audio/sku_<sku> first.
    // The M5 Note runs the M681 profile but has a speaker (AW8738), so it
    // gets its own SKU (audio policy; its VINTF manifest is the M681 one).
    android::init::InitPropertySet("ro.boot.product.vendor.sku",
                                   board == "m1621" ? board : profile);
}
