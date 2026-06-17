#!/bin/sh
### BEGIN INIT INFO
# Provides:       usb-gadget
# Required-Start: $local_fs $syslog
# Required-Stop:  $local_fs
# Default-Start:  S
# Default-Stop:   K
# Description:    Manage USB gadget functions
### END INIT INFO

case "$1" in
	start|restart|stop) /usr/bin/usb-gadget $1 ;;
	*)
		echo "Usage: [start|stop|restart]" >&2
		exit 3
		;;
esac

:
