#!/bin/bash -e

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/post-helper}"

cd "$TARGET_DIR"

OS_SCRIPT="$RK_SCRIPTS_DIR/post-$POST_OS.sh"
if [ -x "$OS_SCRIPT" ]; then
	notice "Running $(basename "$OS_SCRIPT") for $TARGET_DIR..."
	"$OS_SCRIPT"
fi

if [ "$RK_ROOTFS_CONSOLE_LOGIN" ]; then
	message "Enabling login on serial console..."
	if [ -r lib/systemd/system/serial-getty@.service ]; then
		sed -i "s~/bin/[^ ]*sh -l~/bin/login~" \
			lib/systemd/system/serial-getty@.service
	fi

	if [ -r etc/inittab ]; then
		if grep -q "Put a getty on the serial port" etc/inittab; then
			sed -i "s~\(.*respawn.*\)/bin/[^ ]*sh\>~\1/bin/login~" \
				etc/inittab
		else
			sed -i 's/\<login -p root\>/login/' etc/inittab
		fi
	fi
fi
