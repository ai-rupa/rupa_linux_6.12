#!/bin/bash
# npu/npu_freq_scaling.sh - NPU frequency scaling test
# Usage: npu_freq_scaling.sh [test_seconds] [stay_seconds]

DIR_NPU="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_NPU}/../common/common.sh"

# Parse framework CLI options first (e.g. --duration from `rockchip_test.sh
# --module npu --test scaling --duration N`), then allow standalone positional
# overrides: `npu_freq_scaling.sh <test_seconds> <stay_seconds>`.
# Only numeric positionals override, so leftover flag tokens are ignored.
parse_args "$@"
TEST_SECONDS="$(get_duration)"
STAY_SECONDS=2
# Standalone positional form only — entered when $1 is a bare number, never
# when $1 is a framework flag (so a flag's value can't leak into STAY_SECONDS).
if [[ "${1:-}" =~ ^[0-9]+$ ]]; then
    TEST_SECONDS="$1"
    [[ "${2:-}" =~ ^[0-9]+$ ]] && STAY_SECONDS="$2"
fi

NPU_PATH=/sys/class/devfreq/*.npu/

# Resolve the glob and verify the NPU devfreq node exists.
NPU_PATH_RESOLVED=$(ls -d $NPU_PATH 2>/dev/null | head -1)
if [[ -z "$NPU_PATH_RESOLVED" ]]; then
    log_error "npu" "NPU devfreq path not found"
    exit 1
fi
NPU_PATH="$NPU_PATH_RESOLVED"

log_info "npu" "NPU freq scaling test: ${TEST_SECONDS}s, stay ${STAY_SECONDS}s per freq"

TEST_ID=$(test_begin "npu" "freq_scaling" "NPU frequency cycling for ${TEST_SECONDS}s")

unset FREQS
read -ra FREQS < "$NPU_PATH/available_frequencies"
RANDOM=$$$(date +%s)
time_cnt=0

set +e
while true; do
    if [[ $time_cnt -ge $TEST_SECONDS ]]; then
        test_end "$TEST_ID" "PASS" "Completed $time_cnt seconds of NPU freq cycling"
        exit 0
    fi

    echo userspace > "$NPU_PATH/governor"
    FREQ=${FREQS[RANDOM % ${#FREQS[@]}]}
    log_info "npu" "Setting NPU freq to $FREQ"
    echo "${FREQ}" > "$NPU_PATH/userspace/set_freq"
    sleep "$STAY_SECONDS"
    time_cnt=$((time_cnt + STAY_SECONDS))
done
set -e
