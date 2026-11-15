# Linux 4.19 kernel for the Samsung Galaxy A01_m01 (a01q, m01q SDM439)

Port of the LineageOS/CAF msm8937 4.19 kernel (Redmi 7A, 4.19.325-cip135) to the Samsung Galaxy A01. The original Samsung 4.9 kernel was used as a reference for the device tree, drivers and TrustZone/ADSP interfaces.

## Building

    RAMDISK=/path/to/lineageos-boot-ramdisk a01q-tools/build.sh pack

Without `pack` it only builds Image.gz and the device tree. Toolchain: Proton clang 13 (set CLANG_DIR if it is not in ~/toolchains/proton-clang). Keep the stock dtbo partition: the bootloader applies it on top of sdm439.dtb.

By AF220708
