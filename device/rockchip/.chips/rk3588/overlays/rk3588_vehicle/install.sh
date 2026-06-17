#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"
RK_SDK_DIR="${RK_SDK_DIR:-$OVERLAY_DIR/../../../../../..}"

function install_touch_ko() {
    local ko_name="$1"

	if [ -n "$RK_KERNEL_VERSION" ]; then
		local kernel_src="$RK_SDK_DIR/kernel-$RK_KERNEL_VERSION"
	else
		local kernel_src="$RK_SDK_DIR/kernel"
	fi

    local src_ko="$(find "$kernel_src/drivers/input/touchscreen" -name "$ko_name" -type f | head -n1)"
    local dest_ko="$TARGET_DIR/usr/lib/modules/$ko_name"

    if [ -f "$src_ko" ]; then
        notice "cp $src_ko -> $dest_ko"
        mkdir -p "$TARGET_DIR/usr/lib/modules"
        cp -f "$src_ko" "$dest_ko"
    else
        notice "$kernel_src/drivers/input/touchscreen/ not found $ko_name "
    fi
}

case "$POST_OS" in
	buildroot)
		notice "Installing vehicle overlay for $POST_OS..."
		$RK_RSYNC "$OVERLAY_DIR/$POST_OS/" "$TARGET_DIR/"

		install_touch_ko himax_mmi.ko
		install_touch_ko ili210x.ko
		;;
esac
