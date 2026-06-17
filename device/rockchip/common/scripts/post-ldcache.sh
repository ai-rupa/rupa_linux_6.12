#!/bin/bash -e

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/post-helper}"

[ "$RK_ROOTFS_LD_CACHE" ] || exit 0

if ! grep -q glibc-ld.so.cache "$TARGET_DIR"/lib/ld-linux* &>/dev/null; then
	notice "glibc's ld.so.cache is unsupported"
	exit 0
fi

message "Creating ld.so.cache for $TARGET ..."

if [ -d "$TARGET_DIR/etc/ld.so.conf.d" ] && \
	[ ! -f "$TARGET_DIR/etc/ld.so.conf" ]; then
        echo "include /etc/ld.so.conf.d/*.conf" > "$TARGET_DIR/etc/ld.so.conf"
        chmod 644 "$TARGET_DIR/etc/ld.so.conf"
fi

"$RK_TOOLS_DIR/x86_64/ldconfig" -r "$TARGET_DIR/"
