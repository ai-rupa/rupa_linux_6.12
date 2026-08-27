#!/bin/bash
# recovery/test_function_recovery.sh - Recovery wipe-all test

DIR_RECOVERY="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_RECOVERY}/../common/common.sh"

require_command updateEngine || {
    log_error "recovery" "updateEngine not found. Please check if updateEngine is built."
    exit 1
}

TEST_ID=$(test_begin "recovery" "wipe_all" "Recovery wipe-all test")
log_info "recovery" "Running updateEngine --misc=wipe_userdata --reboot"

set +e
updateEngine --misc=wipe_userdata --reboot
EXIT_CODE=$?
set -e

sync

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "updateEngine --misc=wipe_userdata --reboot exited with code $EXIT_CODE"
    exit 1
fi
