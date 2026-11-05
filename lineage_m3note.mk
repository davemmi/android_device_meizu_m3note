# SPDX-License-Identifier: Apache-2.0
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, device/meizu/m3note/device.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)
PRODUCT_NAME := lineage_m3note
PRODUCT_DEVICE := m3note
PRODUCT_BRAND := Meizu
PRODUCT_MODEL := M5 Note
PRODUCT_MANUFACTURER := Meizu
PRODUCT_SHIPPING_API_LEVEL := 22
# Full Treble: without it linkerconfig uses the legacy single namespace, and
# android.hardware.audio.service aborts with "Binder threadpool cannot be
# shrunk after starting"; the VNDK apex is not visible either.
# ro.treble.enabled follows PRODUCT_FULL_TREBLE, which needs this override
# below shipping API level 26.
PRODUCT_FULL_TREBLE_OVERRIDE := true
PRODUCT_CHARACTERISTICS := nosdcard
TARGET_BOOT_ANIMATION_RES := 1080
PRODUCT_BUILD_USERDATA_IMAGE := false
