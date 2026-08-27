#!/bin/bash
# ddr/stressapptest_test.sh - DDR stressapptest stress test

DIR_DDR="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_DDR}/../common/common.sh"

parse_args "$@"
DURATION=$(get_duration)

# Get available memory for testing (half of available, minus 10MB safety margin)
mem_avail_size=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
mem_test_size="$(( (mem_avail_size / 1024 / 2) - 10 ))"

RESULT_LOG=$(get_log_path ddr)

TEST_ID=$(test_begin "ddr" "stressapptest" "stressapptest -s $DURATION -M $mem_test_size")

log_info "ddr" "Starting stressapptest for ${DURATION}s with ${mem_test_size}MB"
log_info "ddr" "Log at $RESULT_LOG"

stressapptest -s "$DURATION" -i 4 -C 4 -W --stop_on_errors -M "$mem_test_size" -l "$RESULT_LOG" &
