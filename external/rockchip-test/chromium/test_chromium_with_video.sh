#!/bin/bash -e
# chromium/test_chromium_with_video.sh - Chromium with video playback test

DIR_CHROMIUM="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CHROMIUM}/../common/common.sh"

export mpp_syslog_perror=1

# Set debug parameters
KERNEL_VERSION=$(cat /proc/version)
if [[ $KERNEL_VERSION =~ "4.4" ]]; then
    echo 0x100 > /sys/module/rk_vcodec/parameters/debug
else
    echo 0x100 > /sys/module/rk_vcodec/parameters/mpp_dev_debug
fi

set_performance_governor

# Create device node and symbolic link
touch /dev/video-dec0
if [[ ! -e /usr/lib/libMali.so.1 ]]; then
    ln -sf /usr/lib/libmali.so /usr/lib/libMali.so.1
fi

# Chromium launch arguments
CHROMIUM_ARGS=(
    "--no-sandbox"
    "--gpu-sandbox-start-early"
    "--ozone-platform=wayland"
    "--ignore-gpu-blacklist"
    "--enable-wayland-ime"
)

# RK3328 specific optimization
if [[ "$SOC_FAMILY" == "rk3328" ]]; then
    CHROMIUM_ARGS+=("--in-process-gpu")
    log_info "chromium" "Applying RK3328 specific optimizations"
fi

TEST_ID=$(test_begin "chromium" "chromium" "Chromium video test on $SOC_FAMILY")
log_info "chromium" "Detected SoC: $SOC_FAMILY"

if [[ -e "/usr/bin/chromium" ]]; then
    chromium "${CHROMIUM_ARGS[@]}" "file:///oem/SampleVideo_1280x720_5mb.mp4"
    test_end "$TEST_ID" "PASS"
else
    log_error "chromium" "Chromium not found at /usr/bin/chromium"
    test_end "$TEST_ID" "FAIL" "Chromium not installed"
    exit 1
fi
