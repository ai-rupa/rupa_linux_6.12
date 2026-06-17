#!/bin/bash -e

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/post-helper}"

FSTAB="etc/fstab"

del_part()
{
	SRC="$1"
	MOUNTPOINT="$2"

	# Remove old entries with same mountpoint
	sed -i "/[[:space:]]${MOUNTPOINT//\//\\\/}[[:space:]]/d" "$FSTAB"

	if [ "$SRC" != tmpfs ]; then
		# Remove old entries with same source
		sed -i "/^${SRC//\//\\\/}[[:space:]]/d" "$FSTAB"
	fi
}

fixup_part()
{
	del_part $@

	SRC="$1"
	MOUNTPOINT="$2"
	FS_TYPE="$3"
	MOUNT_OPTS="$4"
	PASS="$5"

	# Append new entry
	echo -e "$SRC\t$MOUNTPOINT\t$FS_TYPE\t$MOUNT_OPTS\t0 $PASS" >> "$FSTAB"

	mkdir -p "$TARGET_DIR/$MOUNTPOINT"
}

fixup_root()
{
	message "Fixing up rootfs: $1 ${RK_ROOTFS_RO:+(ro)}"

	SRC="/dev/root"
	MOUNTPOINT="/"
	FS_TYPE=$1
	MOUNT_OPTS="defaults"
	PASS="1"

	ROOTFS_ARRAY=($(grep "[[:space:]]/[[:space:]]" "$FSTAB" || true))
	if [ ${#ROOTFS_ARRAY[@]} -ne 0 ]; then
		SRC="${ROOTFS_ARRAY[0]}"
		MOUNT_OPTS="${ROOTFS_ARRAY[3]}"
		PASS="${ROOTFS_ARRAY[5]}"
	fi

	if [ "$RK_ROOTFS_RO" ] && \
		! echo "$MOUNT_OPTS" | grep -E ",ro\>|^ro\>" ; then
		MOUNT_OPTS="${MOUNT_OPTS},ro"
	fi

	fixup_part "$SRC" "$MOUNTPOINT" "$FS_TYPE" "$MOUNT_OPTS" "$PASS"
}

fixup_basic_part()
{
	message "Fixing up basic partition: $@"

	FS_TYPE="$1"
	MOUNTPOINT="$2"
	MOUNT_OPTS="${3:-defaults}"

	fixup_part "$FS_TYPE" "$MOUNTPOINT" "$FS_TYPE" "$MOUNT_OPTS" 0
}

fixup_device_part()
{
	message "Fixing up device partition: $@"

	DEV="$1"

	[ "$DEV" ] || return 0

	MOUNTPOINT="${2:-/${DEV##*[/=]}}"
	FS_TYPE="${3:-ext4}"
	MOUNT_OPTS="${4:-defaults}"

	fixup_part "$DEV" "$MOUNTPOINT" "$FS_TYPE" "$MOUNT_OPTS" 2
}

message "Fixing up /etc/fstab..."

cd "$TARGET_DIR"

mkdir -p etc
touch "$FSTAB"

case "$RK_ROOTFS_TYPE" in
	ext[234])
		fixup_root "$RK_ROOTFS_TYPE"
		;;
	*)
		fixup_root auto
		;;
esac

if [ "$(readlink sbin/init)" != /lib/systemd/systemd ]; then
	message "Fixup basic partitions for non-systemd init..."
	fixup_basic_part proc /proc
	fixup_basic_part devtmpfs /dev
	fixup_basic_part devpts /dev/pts mode=0620,ptmxmode=0000,gid=5 # tty group
	fixup_basic_part tmpfs /dev/shm nosuid,nodev,noexec
	fixup_basic_part sysfs /sys nosuid,nodev,noexec
	fixup_basic_part configfs /sys/kernel/config
	fixup_basic_part debugfs /sys/kernel/debug
	fixup_basic_part pstore /sys/fs/pstore nosuid,nodev,noexec
fi

if [ "$RK_ROOTFS_FORCE_RAMTMP" ]; then
	fixup_basic_part tmpfs /tmp mode=1777
fi

if [ "$POST_OS" = recovery ]; then
	rm -rfv mnt/udisk mnt/sdcard mnt/usb_storage mnt/external_sd udisk sdcard

	fixup_device_part /dev/sda1 /mnt/udisk auto "defaults,nofail"
	ln -sfv mnt/udisk udisk
	ln -sfv udisk mnt/usb_storage

	fixup_device_part /dev/mmcblk1p1 /mnt/sdcard auto "defaults,nofail"
	ln -sfv mnt/sdcard sdcard
	ln -sfv sdcard mnt/external_sd
fi

for idx in $(seq 1 "$(rk_extra_part_num)"); do
	DEV="$(rk_extra_part_dev $idx)"
	MOUNTPOINT="$(rk_extra_part_mountpoint $idx)"
	FS_TYPE="$(rk_extra_part_fstype $idx)"

	# No fstab entry for built-in partitions
	if rk_extra_part_builtin $idx; then
		del_part "$DEV" "$MOUNTPOINT" "$FS_TYPE"
		continue
	fi

	fixup_device_part "$DEV" "$MOUNTPOINT" "$FS_TYPE" \
		"$(rk_extra_part_mnt_opts $idx)"
done
