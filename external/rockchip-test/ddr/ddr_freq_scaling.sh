#!/bin/bash
# ddr/ddr_freq_scaling.sh - DDR frequency scaling test

DIR_DDR="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_DDR}/../common/common.sh"

DMC_PATH=/sys/class/devfreq/dmc

if [[ ! -e "$DMC_PATH" ]]; then
    log_error "ddr" "DMC path not found: $DMC_PATH (is dmc enabled?)"
    exit 1
fi

set_ddr_freq() {
    echo userspace > "$DMC_PATH/governor"
    echo "$1" > "$DMC_PATH/userspace/set_freq"
    local cur
    cur=$(cat "$DMC_PATH/cur_freq")

    if [[ "$cur" -eq "$1" ]]; then
        log_info "ddr" "DDR freq changed to $cur Hz"
    else
        log_error "ddr" "Failed to change freq to $1 Hz, current: $cur Hz"
        return 1
    fi
}

# Single frequency set mode
if [[ "$#" -ge 1 && "$1" != --* ]]; then
    read -ra array < "$DMC_PATH/available_frequencies"
    found=0
    for freq in "${array[@]}"; do
        if [[ "$1" -eq "$freq" ]]; then
            set_ddr_freq "$1"
            exit $?
        fi
    done
    log_error "ddr" "$1 is not in available frequencies: ${array[*]}"
    log_info "ddr" "Current freq: $(cat "$DMC_PATH/cur_freq") Hz"
    exit 1
fi

# Frequency cycling mode
read -ra FREQS < "$DMC_PATH/available_frequencies"
RANDOM=$$$(date +%s)
cnt=0

TEST_ID=$(test_begin "ddr" "freq_scaling" "DDR frequency cycling")

while true; do
    echo userspace > "$DMC_PATH/governor"
    FREQ=${FREQS[RANDOM % ${#FREQS[@]}]}
    log_debug "ddr" "Cycle: $cnt, setting freq $FREQ"
    set_ddr_freq "${FREQ}" || { test_end "$TEST_ID" "FAIL" "freq set error"; exit 1; }
    ((cnt++))
    sleep 2
done
