#!/bin/bash
# flash_test/power_lost_test.sh - Power lost stress test

DIR_FLASH="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_FLASH}/../common/common.sh"

# Use /userdata/rockchip-test/ path consistently with S99-auto-reboot and other tests
test_dir=/userdata/rockchip-test/power_lost
CNT=/userdata/rockchip-test/power_lost_cnt
total=5000
delay=3

mkdir -p /userdata/rockchip-test

if [[ ! -e "/userdata/rockchip-test/power_lost_test.sh" ]]; then
    cp "${DIR_FLASH}/power_lost_test.sh" /userdata/rockchip-test/
    # Also copy common dir so S99 can find dependencies when running via bash
    cp -r "${DIR_FLASH}/../common" /userdata/rockchip-test/common
    sync
fi

if [[ -e "$CNT" ]]; then
    cnt=$(cat "$CNT")
    log_info "power_lost" "Power up count: $cnt"
    echo "$(date)_power_up_$cnt" >> /userdata/rockchip-test/power_lost_test.log
else
    log_info "power_lost" "Reset power lost count"
    cnt=0
    echo 0 > "$CNT"
    rm -f /userdata/rockchip-test/power_lost_test.log
    echo "$(date) power lost test begin" >> /userdata/rockchip-test/power_lost_test.log
    mkdir -p "$test_dir"
fi

if [[ $cnt -ge $total ]]; then
    log_info "power_lost" "Power lost test finished ($total iterations)"
    echo "off" > "$CNT"
    rm -f /userdata/rockchip-test/power_lost_test.sh
    rm -rf /userdata/rockchip-test/common
    rm -rf "$test_dir"
    rm -f "$CNT"
    sync
    exit 0
fi

log_info "power_lost" "Current cnt=$cnt, total=$total"
echo "power lost test loop $cnt" >> /userdata/rockchip-test/power_lost_test.log
sleep "$delay"

if [[ "$cnt" == "off" ]]; then
    log_info "power_lost" "Power lost test stopped by user"
    rm -f /userdata/rockchip-test/power_lost_test.sh
    rm -rf /userdata/rockchip-test/common
    rm -rf "$test_dir"
    rm -f "$CNT"
    sync
    exit 0
fi

cnt=$((cnt + 1))
echo "$cnt" > "$CNT"
sync

rm -rf "$test_dir"/*
sync
dd if=/dev/random of="$test_dir/test_src" bs=1M count=2 2>/dev/null

set +e
while true; do
    log_debug "power_lost" "Loop $cnt"
    dd if="$test_dir/test_src" of="$test_dir/test_dst" 2>/dev/null
    sync

    if busybox diff "$test_dir/test_src" "$test_dir/test_dst" >/dev/null 2>&1; then
        log_debug "power_lost" "Compare OK"
        rm -f "$test_dir/test_dst"
        sync
    else
        log_error "power_lost" "Compare FAILED at loop $cnt"
        echo "$(date) compare failed at loop $cnt" >> /userdata/rockchip-test/power_lost_test.log
        exit 1
    fi
    sleep 0.1
done
set -e
