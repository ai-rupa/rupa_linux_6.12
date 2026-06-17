#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

[ -x "$TARGET_DIR/usr/bin/weston" ] || exit 0

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

sed -i 's/\(WESTON_USER=\)weston/\1root/' "$TARGET_DIR/etc/init.d/weston"

message "Installing weston overlay..."
$RK_RSYNC "$OVERLAY_DIR/" "$TARGET_DIR/"

message "Installing Rockchip camera and video test scripts..."
$RK_RSYNC "$RK_SDK_DIR/external/rockchip-test/" \
	"$TARGET_DIR/rockchip-test/" \
	--include="camera/" --include="video/" --exclude="/*"
