#!/bin/bash
# suspend_resume/suspend_resume.sh - Suspend-to-RAM test

DIR_SR="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_SR}/../common/common.sh"

SUSPEND_TIME=60
WAKE_DELAY=5
MAX_CYCLES=10000

# Verify RTC existence
if [[ ! -e "/sys/class/rtc/rtc0/wakealarm" ]]; then
    log_error "suspend_resume" "RTC not detected, please check RTC configuration"
    exit 1
fi

# Platform-specific configuration
if [[ "$SOC_FAMILY" == "rv1126b" ]]; then
    log_info "suspend_resume" "RV1126B platform detected, skipping DMC governor and wake interval setup"
    # Configure hardware wake interval (0x8000 = 1 second wake interval)
    io -4 0x20834310 0x8000
    SUSPEND_TIME=1
fi

TEST_ID=$(test_begin "suspend_resume" "rtc" "Suspend/resume test: $MAX_CYCLES cycles, ${SUSPEND_TIME}s suspend")

auto_suspend_resume_rtc() {
    local cnt=0

    # Sync system time to RTC
    hwclock --systohc
    log_info "suspend_resume" "Automated suspend/resume test initiated"

    while [[ $cnt -lt $MAX_CYCLES ]]; do
        log_info "suspend_resume" "Cycle $cnt/$MAX_CYCLES"

        # Set RTC wakeup timer
        echo 0 > /sys/class/rtc/rtc0/wakealarm
        echo "+${SUSPEND_TIME}" > /sys/class/rtc/rtc0/wakealarm

        # Initiate system suspend
        pm-suspend

        # Post-wakeup handling
        log_debug "suspend_resume" "System resumed, next cycle in ${WAKE_DELAY}s"
        sleep "${WAKE_DELAY}"

        cnt=$((cnt + 1))
    done

    test_end "$TEST_ID" "PASS" "Completed $MAX_CYCLES suspend/resume cycles"
}

set +e
auto_suspend_resume_rtc
set -e
