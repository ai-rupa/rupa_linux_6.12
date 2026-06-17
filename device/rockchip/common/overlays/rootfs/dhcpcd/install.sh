#!/bin/bash -e

TARGET_DIR="$1"
[ "$TARGET_DIR" ] || exit 1

OVERLAY_DIR="$(dirname "$(realpath "$0")")"

[ -x "$TARGET_DIR/sbin/dhcpcd" ] || exit 0

if [ "$POST_INIT_SYSTEMD" ]; then
	if [ -r lib/systemd/system/dhcpcd.service ]; then
		message "Enabling dhcpcd service..."

		WANTS_DIR=etc/systemd/system/multi-user.target.wants
		mkdir -p "$WANTS_DIR"
		ln -rsf lib/systemd/system/dhcpcd.service "$WANTS_DIR/"
	else
		message "Installing dhcpcd service..."
		install_systemd_service "$OVERLAY_DIR/dhcpcd.service"
	fi
fi

if [ "$POST_INIT_SYSV$POST_INIT_BUSYBOX" ]; then
	if ! grep -wq "/sbin/dhcpcd" "$TARGET_DIR/etc/init.d" 2>/dev/null; then
		message "Installing dhcpcd service..."
		install_sysv_service "$OVERLAY_DIR/S41dhcpcd" S
		install_busybox_service "$OVERLAY_DIR/S41dhcpcd"
	fi
fi
