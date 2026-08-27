#!/bin/bash
# wifibt/wifi_onoff.sh - WiFi on/off cycling stress test

DIR_WIFIBT="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_WIFIBT}/../common/common.sh"

TEST_ID=$(test_begin "wifi" "onoff" "WiFi on/off cycling test")

cnt=0
set +e
while true; do
    ifconfig wlan0 down && ifconfig wlan0 up && ifconfig wlan0 | grep UP
    if [[ $? -ne 0 ]]; then
        log_error "wifi" "WiFi test failed at cycle $cnt"
        dmesg > "$(get_log_path wifi)_dmesg.txt"
        test_end "$TEST_ID" "FAIL" "WiFi failed at cycle $cnt"
        exit 1
    fi
    sleep 1
    cnt=$((cnt + 1))
    log_debug "wifi" "WiFi on/off cycle $cnt"
done
set -e
