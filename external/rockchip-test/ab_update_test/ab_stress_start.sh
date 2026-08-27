#!/bin/sh
OTA_IMG=/userdata/update_ota.img
CNT_FILE=/userdata/rockchip-test/ab_stress_cnt

if [ -z "$1" ] || ! [ "$1" -gt 0 ] 2>/dev/null; then
    echo "Usage: $0 <loop_count>"
    echo "Example: $0 20"
    exit 1
fi

if [ ! -f "$OTA_IMG" ]; then
    echo "ERROR: $OTA_IMG not found, please push OTA image first"
    exit 1
fi

mkdir -p /userdata/rockchip-test
echo "$1" > $CNT_FILE
sync
echo "AB stress test started: $1 loops"
reboot
