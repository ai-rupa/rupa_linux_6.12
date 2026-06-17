#!/bin/bash -e

cd "$TARGET_DIR"

message "Installing full-busybox..."
RK_ROOTFS_PREFER_PREBUILT_TOOLS=y ensure_tools bin/busybox

# Login root on serial console
if [ -r etc/inittab ]; then
	sed -i 's~\(respawn:.*\)/bin/start_getty.*~\1/bin/login -p root~' \
		etc/inittab
fi

# Drop Poky warnings in motd
if [ -r etc/motd ]; then
	sed -i '/^WARNING: Poky/,+2d' etc/motd
fi

# Use uid to detect root user
if [ -r etc/profile ]; then
	sed -i 's~"$HOME" != "/home/root"~$(id -u) -ne 0~' etc/profile
fi

if [ -r etc/ntp.conf ] && \
	! grep -q "^server .*ntp" etc/ntp.conf; then
	message "Applying global NTP server..."
	echo >> etc/ntp.conf
	echo "server 0.pool.ntp.org iburst" >> etc/ntp.conf
	echo "server 1.pool.ntp.org iburst" >> etc/ntp.conf
	echo "server 2.pool.ntp.org iburst" >> etc/ntp.conf
	echo "server 3.pool.ntp.org iburst" >> etc/ntp.conf
fi

# Switch from the compat to the files module
# See: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=880846
if [ -r etc/nsswitch.conf ]; then
	sed -i 's/\<compat$/files/' etc/nsswitch.conf
fi
