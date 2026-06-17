#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

[ "$POST_INIT_SYSTEMD" ] || exit 0

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

message "Installing systemd-fsck-prepare service..."

install_systemd_service "$OVERLAY_DIR/systemd-fsck-prepare.service"
