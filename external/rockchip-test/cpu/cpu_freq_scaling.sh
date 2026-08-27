#!/bin/bash
# cpu/cpu_freq_scaling.sh - CPU frequency scaling test
# Supports --duration <seconds> to override default test duration.

DIR_CPU="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CPU}/../common/common.sh"

parse_args "$@"

set_cpu_freq() {
    local policy_path=$1
    local target_freq=$2
    echo userspace > "${policy_path}/scaling_governor"
    echo "${target_freq}" > "${policy_path}/scaling_setspeed"
    local cur
    cur=$(cat "${policy_path}/scaling_cur_freq")
    local min
    min=$(cat "${policy_path}/scaling_min_freq")
    if [[ "$cur" -eq "$target_freq" ]] || [[ "$cur" -le "$min" ]]; then
        log_info "cpu" "Policy ${policy_path##*/} changed to ${cur} KHz"
    else
        log_error "cpu" "Failed to change freq to ${target_freq} KHz, current: ${cur} KHz"
        return 1
    fi
}

cycle_frequencies() {
    local duration
    duration=$(get_duration)
    local end_time=$(( $(date +%s) + duration ))
    local cnt=0

    TEST_ID=$(test_begin "cpu" "freq_scaling" "CPU frequency cycling for ${duration}s")

    while [[ $(date +%s) -lt $end_time ]]; do
        for policy_path in /sys/devices/system/cpu/cpufreq/policy*; do
            read -ra freqs <<< "$(cat "${policy_path}/scaling_available_frequencies")"
            local freq=${freqs[RANDOM % ${#freqs[@]}]}
            log_debug "cpu" "Cycle: $cnt, setting freq $freq"
            set_cpu_freq "$policy_path" "$freq" || { test_end "$TEST_ID" "FAIL" "freq set error"; return 1; }
            ((cnt++))
        done
        sleep 10
    done

    test_end "$TEST_ID" "PASS" "Completed $cnt frequency cycles"
}

# Check for user input to set a specific frequency or cycle frequencies
if [[ "$#" -ge 1 && "$1" != --* ]]; then
    set_cpu_freq "/sys/devices/system/cpu/cpufreq/policy0" "$1"
else
    cycle_frequencies
fi
