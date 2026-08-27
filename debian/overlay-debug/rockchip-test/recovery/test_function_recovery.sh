#!/bin/bash

delay=10
total=${1:-10000}
CNT=/data/rockchip-test/reboot_cnt

if [ ! -e "/usr/bin/updateEngine" ]; then
	echo "Please check if have rktoolkit built\n"
fi

# check log on /userdata/recovery/last_log
updateEngine --misc=wipe_userdata --reboot
sync

