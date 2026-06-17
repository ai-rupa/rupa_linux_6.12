#!/bin/bash -e

[ "$RK_ROOTFS_PREBUILT_TOOLS" ] || exit 0

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

DEST_DIR="$TARGET_DIR/usr/bin/"
mkdir -p "$DEST_DIR"

unset TOOL_ARCH
case "$RK_KERNEL_ARCH" in
	arm) TOOL_ARCH=armhf ;;
	arm64) TOOL_ARCH=aarch64 ;;
	riscv)
		if [ "$RK_CHIP_RISCV64" ]; then
			TOOL_ARCH=riscv64
		fi
		;;
esac

if [ -z "$TOOL_ARCH" ]; then
	notice "No available prebuilt tools..."
	exit 0
fi

message "Installing prebuilt tools..."

$RK_RSYNC --exclude=adbd --exclude=README "$OVERLAY_DIR/$TOOL_ARCH/" "$DEST_DIR/"
