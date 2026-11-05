#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# LineageOS 20.0 for the Meizu M5 Note (M1621) on a fresh Ubuntu 22.04 or
# 24.04 server, as root. Needs >= 260 GB free in the build directory and
# >= 16 GB RAM. Installs the build packages, syncs LineageOS 20.0 with the
# M1621 local manifest and builds the flashable zip. Without ccache (one
# build) source (--depth=1) and out/ take about 230-260 GB; CCACHE=1 adds
# a 50 GB cache for repeated builds.
#
#   curl -LO https://raw.githubusercontent.com/davemmi/android_device_meizu_m3note/lineage-20.0-m1621/tools/build_m1621_cloud.sh
#   bash build_m1621_cloud.sh                 # builds in /root/los
#   TOP=/mnt/vol/los bash build_m1621_cloud.sh   # builds on a mounted volume
#
# The build runs in the tmux session "los" and keeps going when the SSH
# connection drops: reattach with `tmux attach -t los`. Log: $TOP/build.log.
# Result: $TOP/out/target/product/m3note/lineage-20.0-*-m3note.zip
# (boot.img and recovery.img next to it).

set -euo pipefail

TOP=${TOP:-/root/los}
MANIFEST=https://raw.githubusercontent.com/davemmi/android_device_meizu_m3note/lineage-20.0-m1621/m1621.xml
NEED_GB=${NEED_GB:-$([ "${CCACHE:-0}" = 1 ] && echo 300 || echo 260)}

if [ "${1:-}" != "--inside" ]; then
    mkdir -p "$TOP"
    free_gb=$(df -BG --output=avail "$TOP" | tail -1 | tr -dc 0-9)
    mem_gb=$(free -g | awk '/^Mem:/ {print $2}')
    echo "Build directory $TOP: $free_gb GB free; RAM $mem_gb GB; $(nproc) CPUs"
    # a rerun on a synced tree needs only the room for out/
    if [ ! -d "$TOP/.repo" ] && [ "$free_gb" -lt "$NEED_GB" ]; then
        echo "ABBRUCH: zu wenig Platz in $TOP ($free_gb GB, gebraucht ca. $NEED_GB GB)."
        echo "Ein Volume (z. B. 350 GB) anhängen, mounten und mit TOP=<mountpunkt>/los erneut starten."
        exit 1
    fi
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y bc bison build-essential ccache curl flex git git-lfs \
        gnupg gperf python3 rsync tmux zip
    # Names differ between Ubuntu releases (22.04 ... 26.04): take the first
    # candidate of each group that exists, skip a group with none.
    for group in g++-multilib gcc-multilib imagemagick lib32readline-dev \
            lib32z1-dev libelf-dev "lz4 liblz4-tool" libsdl1.2-dev \
            "libsdl1.2-compat-dev libsdl1.2-dev" libssl-dev \
            "libxml2-16 libxml2" libxml2-utils lzop pngcrush \
            python-is-python3 schedtool squashfs-tools xsltproc zlib1g-dev \
            "libncurses5 libncurses6" "libtinfo5 libtinfo6"; do
        for pkg in $group; do
            if apt-get install -y "$pkg" >/dev/null 2>&1; then
                echo "installed $pkg"; break
            fi
        done
    done
    curl -fsSL -o /usr/local/bin/repo https://storage.googleapis.com/git-repo-downloads/repo
    chmod a+x /usr/local/bin/repo
    git lfs install
    git config --global user.name >/dev/null || git config --global user.name "m1621 builder"
    git config --global user.email >/dev/null || git config --global user.email "builder@localhost"
    git config --global color.ui false
    # some swap against out-of-memory kills in the link steps
    if [ "$mem_gb" -lt 30 ] && ! swapon --show | grep -q /swapfile; then
        fallocate -l 16G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
    fi
    cp "$0" "$TOP/build_m1621_cloud.sh"
    tmux kill-session -t los 2>/dev/null || true
    tmux new-session -d -s los "TOP=$TOP CCACHE=${CCACHE:-0} bash $TOP/build_m1621_cloud.sh --inside 2>&1 | tee -a $TOP/build.log; echo; echo 'Ende (Erfolg oder Fehler, siehe oben). Enter schliesst die Sitzung.'; read"
    echo "Build laeuft in der tmux-Sitzung 'los' (tmux attach -t los), Log: $TOP/build.log"
    exit 0
fi

cd "$TOP"
date
if [ ! -d .repo ]; then
    repo init -u https://github.com/LineageOS/android.git -b lineage-20.0 --git-lfs --depth=1
fi
mkdir -p .repo/local_manifests
curl -fsSL -o .repo/local_manifests/m1621.xml "$MANIFEST"
repo sync -c -j"$(nproc)" --force-sync --no-tags --no-clone-bundle --optimized-fetch --prune
date

if [ "${CCACHE:-0}" = 1 ]; then
    export USE_CCACHE=1 CCACHE_EXEC=/usr/bin/ccache
    ccache -M 50G >/dev/null
fi
set +u    # envsetup.sh and lunch read unset variables
source build/envsetup.sh
breakfast m3note
m -j"$(nproc)" bacon
date
ls -la out/target/product/m3note/*.zip out/target/product/m3note/boot.img out/target/product/m3note/recovery.img
sha256sum out/target/product/m3note/*.zip
