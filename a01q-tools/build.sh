#!/bin/bash
# Build the 4.19 kernel for the Samsung Galaxy A01 (a01q) and optionally pack a boot.img.
#
# Usage: a01q-tools/build.sh [pack]
#   (no args)  builds Image.gz and sdm439.dtb into $OUT
#   pack       also creates $OUT/boot.img (needs RAMDISK = ramdisk of a LineageOS boot.img)
#
# Environment: CLANG_DIR (default ~/toolchains/proton-clang), OUT (default ../out_a01q),
#              RAMDISK, MKBOOTIMG (default ~/mkbootimg/mkbootimg.py)
set -eo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-$REPO/../out_a01q}
CLANG_DIR=${CLANG_DIR:-$HOME/toolchains/proton-clang}
MKBOOTIMG=${MKBOOTIMG:-$HOME/mkbootimg/mkbootimg.py}
export PATH=$PATH:$CLANG_DIR/bin ARCH=arm64
M="O=$OUT CC=$CLANG_DIR/bin/clang HOSTCC=gcc HOSTCXX=g++ LD=ld.lld AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-"
cd "$REPO"
if [ ! -f "$OUT/.config" ]; then
  make $M vendor/msm8937_defconfig vendor/xiaomi/msm8937/common.config vendor/xiaomi/sdm439/mi439.config </dev/null >/dev/null
  scripts/kconfig/merge_config.sh -m -O "$OUT" "$OUT/.config" arch/arm64/configs/vendor/a01q.config >/dev/null
  make $M olddefconfig </dev/null >/dev/null
fi
make $M -j"$(nproc)" Image.gz 2>&1 | tee "$OUT/build.log" | tail -3
make $M DTC_FLAGS=-@ vendor/qcom/sdm439.dtb 2>&1 | tail -2
if [ "$1" = "pack" ]; then
  : "${RAMDISK:?set RAMDISK to the ramdisk of a LineageOS boot.img}"
  python3 "$MKBOOTIMG" --header_version 2 --os_version 15.0.0 --os_patch_level 2024-11 \
    --kernel "$OUT/arch/arm64/boot/Image.gz" --ramdisk "$RAMDISK" \
    --dtb "$OUT/arch/arm64/boot/dts/vendor/qcom/sdm439.dtb" \
    --pagesize 0x800 --base 0x0 --kernel_offset 0x80008000 --ramdisk_offset 0x82000000 \
    --second_offset 0x0 --tags_offset 0x81e00000 --dtb_offset 0x181f00000 --board msm8937 \
    --cmdline 'console=null androidboot.console=ttyMSM0 androidboot.hardware=qcom user_debug=30 msm_rtb.filter=0x237 ehci-hcd.park=3 androidboot.bootdevice=7824900.sdhci lpm_levels.sleep_disabled=1 androidboot.usbconfigfs=true loop.max_part=7 printk.devkmsg=on androidboot.selinux=permissive ignore_loglevel' \
    -o "$OUT/boot.img"
  echo -n SEANDROIDENFORCE >> "$OUT/boot.img"   # trailer expected by the Samsung bootloader
  echo "boot.img: $OUT/boot.img"
fi
