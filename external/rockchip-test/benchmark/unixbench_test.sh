#!/bin/bash
# benchmark/unixbench_test.sh - UnixBench benchmark test

DIR_BENCHMARK="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_BENCHMARK}/../common/common.sh"

UNIXBENCH_PATH="/opt/unixbench"
RESULT_FILE="${LOG_DIR}/unixbench_result_$(date +%Y%m%d_%H%M%S).log"
ERROR_FILE="${LOG_DIR}/unixbench_error_$(date +%Y%m%d_%H%M%S).log"

set_performance_governor

if [[ ! -f "${UNIXBENCH_PATH}/Run" ]]; then
    log_error "benchmark" "UnixBench not found at ${UNIXBENCH_PATH}/Run"
    exit 1
fi

mkdir -p "${LOG_DIR}"

TEST_ID=$(test_begin "benchmark" "unixbench" "UnixBench benchmark")
log_info "benchmark" "Starting UnixBench tests, results at $RESULT_FILE"

set +e
(cd "${UNIXBENCH_PATH}" && ./Run) >"${RESULT_FILE}" 2>"${ERROR_FILE}"
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS" "Results at $RESULT_FILE"
else
    test_end "$TEST_ID" "FAIL" "UnixBench exited with code $EXIT_CODE"
    exit 1
fi
