#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

case "$POST_OS" in
	buildroot | debian)
		notice "Installing vehicle dual evb overlay for $POST_OS..."
		$RK_RSYNC "$OVERLAY_DIR/$POST_OS/" "$TARGET_DIR/"
		;;
esac
