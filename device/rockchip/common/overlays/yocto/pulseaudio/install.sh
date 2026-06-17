#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

[ -x "$TARGET_DIR/usr/bin/pulseaudio" ] || exit 0

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

$RK_RSYNC "$OVERLAY_DIR/" "$TARGET_DIR/"
