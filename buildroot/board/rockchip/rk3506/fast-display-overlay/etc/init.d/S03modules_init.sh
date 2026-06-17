#!/bin/sh
#
# Modules init
#

start() {
	find /lib/modules/$(uname -r)/kernel/ -name "*.ko" \
		-exec sh -c 'modprobe "$(basename -s .ko "$1")"' _ {} \;
}

case "$1" in
	start)
		start
		;;
	*)
		echo "Usage: $0 start"
		exit 1
esac

exit $?
