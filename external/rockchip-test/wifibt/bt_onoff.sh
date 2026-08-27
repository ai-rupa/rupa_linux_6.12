#!/bin/bash
# wifibt/bt_onoff.sh - Bluetooth on/off cycling stress test

DIR_WIFIBT="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_WIFIBT}/../common/common.sh"

require_command wifibt-init.sh || exit 1

TEST_ID=$(test_begin "bluetooth" "onoff" "Bluetooth on/off cycling test")

cnt=0
set +e
while true; do
    wifibt-init.sh
    sleep 5
    hciconfig hci0 up && hciconfig -a | grep UP
    if [[ $? -ne 0 ]]; then
        log_error "bluetooth" "BT test failed at cycle $cnt"
        dmesg > "$(get_log_path bluetooth)_dmesg.txt"
        test_end "$TEST_ID" "FAIL" "Bluetooth failed at cycle $cnt"
        exit 1
    fi
    sleep 1
    cnt=$((cnt + 1))
    log_debug "bluetooth" "BT on/off cycle $cnt"
done
set -e
