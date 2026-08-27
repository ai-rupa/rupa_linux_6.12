#!/bin/bash
# cpu/cpu_stress_test.sh - CPU stress test using stress-ng or stress
# Supports --duration <seconds> to override default test duration.

DIR_CPU="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CPU}/../common/common.sh"

parse_args "$@"
DURATION=$(get_duration)
CPU_CORES=$(nproc)

stress_test_stress_ng() {
    log_info "cpu" "Starting CPU stress test with stress-ng on $CPU_CORES cores for ${DURATION}s"
    stress-ng --cpu "$CPU_CORES" --timeout "$DURATION"
}

stress_test_stress() {
    log_info "cpu" "Starting CPU stress test with stress on $CPU_CORES cores for ${DURATION}s"
    stress --cpu "$CPU_CORES" --timeout "$DURATION"
}

TEST_ID=$(test_begin "cpu" "stress" "CPU stress on $CPU_CORES cores for ${DURATION}s")

set +e
if command -v stress-ng >/dev/null 2>&1; then
    stress_test_stress_ng
    EXIT_CODE=$?
elif command -v stress >/dev/null 2>&1; then
    stress_test_stress
    EXIT_CODE=$?
else
    log_error "cpu" "Neither stress nor stress-ng tool is installed"
    test_end "$TEST_ID" "FAIL" "stress tools not found"
    exit 1
fi
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "stress tool exited with code $EXIT_CODE"
    exit 1
fi
