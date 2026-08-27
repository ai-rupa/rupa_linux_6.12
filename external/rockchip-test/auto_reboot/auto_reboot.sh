#!/bin/bash
# auto_reboot/auto_reboot.sh - Automated reboot stress test

# Use BASH_SOURCE to get real script path ($0 is unreliable when sourced)
DIR_REBOOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
source "${DIR_REBOOT}/../common/common.sh"

# Abort if common.sh failed to load to avoid running with undefined functions
if ! type parse_args &>/dev/null; then
    echo "[ERROR] Failed to source common.sh from DIR_REBOOT=$DIR_REBOOT" >&2
    exit 1
fi

parse_args "$@"

delay=8
total=$(get_loops)
CNT=/userdata/rockchip-test/reboot_cnt

mkdir -p /userdata/rockchip-test

if [[ ! -e "/userdata/rockchip-test/auto_reboot.sh" ]]; then
    cp "$0" /userdata/rockchip-test/auto_reboot.sh
    # Copy common to /userdata/common to preserve DIR_REBOOT/../common relative path
    cp -r "${DIR_REBOOT}/../common" /userdata/common
    echo "$total" > /userdata/rockchip-test/reboot_total_cnt
    sync
fi

TEST_ID=$(test_begin "reboot" "auto" "Auto reboot test: $total iterations")

while true; do
    if [[ -e "$CNT" ]]; then
        cnt=$(cat "$CNT")
    else
        log_info "reboot" "Reset reboot count"
        echo 0 > "$CNT"
        cnt=0
    fi

    log_info "reboot" "Reboot after ${delay}s (cnt=$cnt, total=$total)"
    cnt=$((cnt + 1))

    if [[ -z "$total" ]] || [[ $cnt -ge $total ]]; then
        test_end "$TEST_ID" "PASS" "Completed $total reboots"
        echo "off" > "$CNT"
        rm -f /userdata/rockchip-test/auto_reboot.sh
        rm -f /userdata/rockchip-test/reboot_total_cnt
        rm -rf /userdata/common
        rm -f "$CNT"
        sync
        exit 0
    fi

    echo "$cnt" > "$CNT"
    sync
    sleep "$delay"
    cnt=$(cat "$CNT")

    if [[ "$cnt" != "off" ]]; then
        sync
        sleep 1
        if [[ -e /sys/fs/pstore/console-ramoops-0 ]]; then
            log_info "reboot" "Checking console-ramoops-0"
            if ! grep -q "Restarting system" /sys/fs/pstore/console-ramoops-0 && [[ $cnt -ge 2 ]]; then
                if grep -q "panic" /sys/fs/pstore/console-ramoops-0; then
                    log_error "reboot" "Kernel panic detected in last reboot"
                    test_end "$TEST_ID" "FAIL" "Kernel panic at reboot $cnt"
                    rm -f /userdata/rockchip-test/auto_reboot.sh
                    rm -f /userdata/rockchip-test/reboot_total_cnt
                    rm -rf /userdata/common
                    sync
                    exit 1
                else
                    log_warn "reboot" "Potential data loss in last reboot"
                fi
            fi
        fi
        reboot
    else
        log_info "reboot" "Auto reboot stopped by user"
        rm -f /userdata/rockchip-test/auto_reboot.sh
        rm -f /userdata/rockchip-test/reboot_total_cnt
        rm -rf /userdata/common
        rm -f "$CNT"
        sync
        exit 0
    fi
done
