# SPDX-License-Identifier: Apache-2.0

LOCAL_PATH := device/meizu/m3note

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += $(LOCAL_PATH)

# AAPT
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

# Dalvik heap (M3 Note 2/3 GB, M5 Note 3/4 GB): without it the runtime
# falls back to a 16 MB app heap and larger apps (F-Droid, Aurora Store)
# die with OutOfMemoryError.
$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)

# Overlays
DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

# VINTF
DEVICE_MANIFEST_FILE := $(LOCAL_PATH)/manifest.xml

# Packages
PRODUCT_PACKAGES += libwpa_client wpa_supplicant hostapd android.hardware.health@2.1-impl android.hardware.health@2.1-service
# OTA board check (releasetools/releasetools.py)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/releasetools/check_board.sh:install/bin/check_board.sh
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/fstab.mt6755:$(TARGET_COPY_OUT_RAMDISK)/fstab.mt6755 \
    $(LOCAL_PATH)/rootdir/etc/fstab.mt6755:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.mt6755 \
    $(LOCAL_PATH)/rootdir/etc/init.m3note.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m3note.rc \
    $(LOCAL_PATH)/keylayout/mtk-kpd.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/mtk-kpd.kl \
    $(LOCAL_PATH)/wifi/wpa_supplicant.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant.conf \
    $(LOCAL_PATH)/wifi/p2p_supplicant.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant.conf \
    frameworks/native/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml

# Vendor blobs
$(call inherit-product, vendor/meizu/m3note/m3note-vendor.mk)

# Init
# Board init, USB gadget, device nodes and the MTK service rc files.
# CONSYS (wmt_*) is started by property (vendor.m681.wmt.start=1).
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init/hw/init.mt6755.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.mt6755.rc \
    $(LOCAL_PATH)/rootdir/etc/init/hw/init.mt6755.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.mt6755.usb.rc \
    $(LOCAL_PATH)/rootdir/etc/ueventd.mt6755.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.spm.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.spm.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.wifi.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.wifi.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.connectivity.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.connectivity.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.sensors.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.sensors.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.dvfs.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.dvfs.rc \
    $(LOCAL_PATH)/rootdir/etc/init.l681.dvfs.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.l681.dvfs.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.gps.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.gps.rc \
    $(LOCAL_PATH)/rootdir/etc/init.m681.thermal.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.thermal.rc \
    $(LOCAL_PATH)/rootdir/etc/init.trustonic.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.trustonic.rc

# Modem and RIL chain (nvram_daemon, ccci_*, gsm0710muxd, rild), M681 only.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.m681.modem.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.m681.modem.rc

# Shims
PRODUCT_PACKAGES += libm3note_n_gui libm3note_n_base libm3note_n_ui libm3note_n_perfservice libm3note_n_region libm3note_n_audio libm3note_n_mnld libm3note_gnss_navmsg
# Empty vendor libmedia.so: audio.primary.mt6755.so needs it by name.
PRODUCT_PACKAGES += libm3note_n_media
# RIL shims: the LD shim of mtk-ril.so/librilmtk.so and the vendor
# libnetd_client.so needed by mtk-ril.so.
PRODUCT_PACKAGES += libm3note_n_ril libm3note_n_netd_client
# Camera shims for the provider roots and the libraries the blobs need by
# name that Android 13 does not provide to vendor processes.
PRODUCT_PACKAGES += \
    libm3note_n_camera \
    libm3note_n_camera_jpeg \
    libm3note_n_dpfrag \
    libm3note_n_jpgenc \
    libm3note_n_android_runtime \
    libm3note_n_nativehelper \
    libm3note_n_filter_effects \
    libm3note_n_icuuc \
    libm3note_n_dng_sdk \
    libm3note_n_android \
    libm3note_n_jnigraphics \
    libm3note_n_vmp_render \
    libm3note_n_icui18n
# RIL
# librilutils: vendor AOSP library needed by name by mtk-ril.so, librilmtk.so
# and libril.  BOARD_PROVIDES_LIBRIL and ENABLE_VENDOR_RIL_SERVICE make the
# libril and rild names resolve to ril/.
PRODUCT_PACKAGES += librilutils

# Audio
PRODUCT_PACKAGES += audio.primary.default audio.r_submix.default audio.usb.default audio_policy.stub libdrmclearkeyplugin libmockdrmcryptoplugin libaudiopreprocessing

# HIDL HALs
# Vendor HAL services wrapping the MT6755 blobs.
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl-2.1 \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service \
    android.hardware.renderscript@1.0-impl \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service \
    android.hardware.audio@2.0-impl \
    android.hardware.audio@2.0-service \
    android.hardware.audio.effect@2.0-impl \
    android.hardware.audio@6.0-impl \
    android.hardware.audio.effect@6.0-impl \
    android.hardware.bluetooth.audio-impl \
    audio.bluetooth.default \
    android.hardware.keymaster@3.0-impl \
    android.hardware.keymaster@3.0-service \
    android.hardware.gatekeeper@1.0-service.software \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.0-service \
    android.hardware.drm@1.4-service.clearkey \
    android.hardware.gnss@1.0-impl \
    android.hardware.gnss@1.0-service \
    android.hardware.camera.provider@2.4-impl \
    android.hardware.camera.provider@2.4-service \
    android.hardware.sensors@1.0-impl \
    android.hardware.sensors@1.0-service \
    android.hardware.wifi@1.0-service \
    libwifi-hal-mt66xx \
    wificond

# Bluetooth: AOSP passthrough HCI impl, a lazy service declared only in
# vintf/manifest_m681.xml, and the 64-bit libbt-vendor.so it dlopen()s.
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-service.m3note \
    libm3note_btvendor

# Display
# HWC1 module for composer@2.1 (hwcomposer/), selected by
# ro.hardware.hwcomposer=forge in vendor.prop.
PRODUCT_PACKAGES += hwcomposer.forge

# Libraries needed by the blobs that are not otherwise in the image.  With
# BOARD_VNDK_VERSION the vendor namespace cannot see /system/lib*/libstdc++.so.
PRODUCT_PACKAGES += \
    libcamera_client_vendor \
    libstdc++.vendor \
    libtinyxml \
    libalsautils \
    libtinycompress

PRODUCT_PACKAGES += gralloc.default

# Vibrator
# AIDL service over the LED-class /sys/class/leds/vibrator.
PRODUCT_PACKAGES += android.hardware.vibrator-service.m3note

# Fingerprint
# Goodix GF516M on Trustonic TEE: HIDL 2.0 service, init rc and key layout.
PRODUCT_PACKAGES += android.hardware.biometrics.fingerprint@2.0-service.m3note
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/etc/init.goodixfpd.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.goodixfpd.rc \
    $(LOCAL_PATH)/keylayout/fp-keys.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/fp-keys.kl \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml

# Symlinks
PRODUCT_PACKAGES += m3note_vendor_symlinks

# Audio policy
# Android 13 reads XML only.  One file per board under
# /vendor/etc/audio/sku_<board>, picked by ro.boot.product.vendor.sku (set by
# libinit); the M681 variant (no speaker) is also the /vendor/etc fallback.
AUDIO_POLICY_CFG_DIR := frameworks/av/services/audiopolicy/config
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/audio_policy_configuration_m681.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_m681/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/audio_policy_configuration_l681.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_l681/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/audio_policy_configuration_m1621.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/sku_m1621/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/audio_policy_configuration_m681.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(AUDIO_POLICY_CFG_DIR)/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    $(AUDIO_POLICY_CFG_DIR)/bluetooth_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_audio_policy_configuration.xml \
    $(AUDIO_POLICY_CFG_DIR)/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    $(AUDIO_POLICY_CFG_DIR)/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    $(AUDIO_POLICY_CFG_DIR)/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    $(AUDIO_POLICY_CFG_DIR)/surround_sound_configuration_5_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/surround_sound_configuration_5_0.xml \
    frameworks/av/media/libeffects/data/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml

# Keymaster
# Both boards declare the AOSP software keymaster 4.0: the MobiCore TEE
# keymaster does not work with this kernel, and a declared HAL that never
# registers would block keystore and boot.  The per-SKU VINTF manifests are
# set by DEVICE_MANIFEST_SKUS in BoardConfig.mk.
PRODUCT_PACKAGES += android.hardware.keymaster@4.0-service

# Power
# AIDL Power HAL with cpufreq floors on policy0/policy4: app launch -> max
# (1.5 s cap), fling -> 70 % of max, ADPF -> floor while frames miss target.
# Touch boost stays in the kernel.  Config: configs/power/fleet_power.json.
PRODUCT_PACKAGES += \
    android.hardware.power-service.meizu
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/power/fleet_power.json:$(TARGET_COPY_OUT_VENDOR)/etc/power/fleet_power.json
