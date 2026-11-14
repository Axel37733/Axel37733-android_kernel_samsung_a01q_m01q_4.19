# Linux 4.19 kernel for the Samsung Galaxy A01_m01 (a01q, m01q SDM439)

Port of the LineageOS/CAF msm8937 4.19 kernel (Redmi 7A, 4.19.325-cip135) to the Samsung Galaxy A01. The original Samsung 4.9 kernel was used as a reference for the device tree, drivers and TrustZone/ADSP interfaces.

## Building

    a01q-tools/build_a01.sh   # builds Image.gz (config: arch/arm64/configs/vendor/a01q.config)
    a01q-tools/build_dtb.sh   # builds the device tree
    a01q-tools/pack_los.sh    # packs boot.img using the ramdisk of a LineageOS boot.img

Toolchain: Proton clang 13. Keep the stock dtbo partition: the bootloader applies it on top of sdm439.dtb.

By AF220708
