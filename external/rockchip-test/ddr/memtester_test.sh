#!/bin/bash
# ddr/memtester_test.sh - DDR memtester stress test

DIR_DDR="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_DDR}/../common/common.sh"

# Get available memory for testing (half of available, minus 10MB safety margin)
mem_avail_size=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
mem_test_size="$(( (mem_avail_size / 1024 / 2) - 10 ))M"

TEST_ID=$(test_begin "ddr" "memtester" "memtester $mem_test_size")

log_info "ddr" "Starting memtester with $mem_test_size"
log_info "ddr" "Log at $(get_log_path ddr)"

memtester "$mem_test_size" 2>&1 | tee -a "$(get_log_path ddr)" &
