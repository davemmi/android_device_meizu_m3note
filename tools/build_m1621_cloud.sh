#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# LineageOS 20.0 for the Meizu M5 Note (M1621) on a fresh Ubuntu 22.04 or
# 24.04 server, as root. Needs >= 300 GB free in the build directory and
# >= 16 GB RAM. Installs the build packages, syncs LineageOS 20.0 with the
# M1621 local manifest and builds the flashable zip.
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
NEED_GB=300

if [ "${1:-}" != "--inside" ]; then
    mkdir -p "$TOP"
    free_gb=$(df -BG --output=avail "$TOP" | tail -1 | tr -dc 0-9)
    mem_gb=$(free -g | awk '/^Mem:/ {print $2}')
    echo "Build directory $TOP: $free_gb GB free; RAM $mem_gb GB; $(nproc) CPUs"
    if [ "$free_gb" -lt "$NEED_GB" ]; then
        echo "ABBRUCH: zu wenig Platz in $TOP ($free_gb GB, gebraucht ca. $NEED_GB GB)."
        echo "Ein Volume (z. B. 350 GB) anhängen, mounten und mit TOP=<mountpunkt>/los erneut starten."
        exit 1
    fi
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y bc bison build-essential ccache curl flex g++-multilib \
        gcc-multilib git git-lfs gnupg gperf imagemagick lib32readline-dev \
        lib32z1-dev libelf-dev liblz4-tool libsdl1.2-dev libssl-dev libxml2 \
        libxml2-utils lzop pngcrush python3 python-is-python3 rsync \
        schedtool squashfs-tools tmux xsltproc zip zlib1g-dev
    # Android 13 host prebuilts still load libncurses.so.5 / libtinfo.so.5
    apt-get install -y libncurses5 || apt-get install -y libncurses6 libtinfo6
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
    tmux new-session -d -s los "TOP=$TOP bash $TOP/build_m1621_cloud.sh --inside 2>&1 | tee -a $TOP/build.log; echo; echo 'Ende (Erfolg oder Fehler, siehe oben). Enter schliesst die Sitzung.'; read"
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

export USE_CCACHE=1 CCACHE_EXEC=/usr/bin/ccache
ccache -M 50G >/dev/null
set +u    # envsetup.sh and lunch read unset variables
source build/envsetup.sh
breakfast m3note
m -j"$(nproc)" bacon
date
ls -la out/target/product/m3note/*.zip out/target/product/m3note/boot.img out/target/product/m3note/recovery.img
sha256sum out/target/product/m3note/*.zip
