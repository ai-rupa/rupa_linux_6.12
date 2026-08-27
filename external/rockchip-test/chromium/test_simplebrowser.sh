#!/bin/bash -e
# chromium/test_simplebrowser.sh - SimpleBrowser test

DIR_CHROMIUM="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CHROMIUM}/../common/common.sh"

export mpp_syslog_perror=1

# Set debug parameters
KERNEL_VERSION=$(uname -r)
if echo "$KERNEL_VERSION" | grep -q '^4\.4'; then
    echo 0x100 > /sys/module/rk_vcodec/parameters/debug
else
    echo 0x100 > /sys/module/rk_vcodec/parameters/mpp_dev_debug
fi

set_performance_governor

TEST_ID=$(test_begin "chromium" "simplebrowser" "SimpleBrowser test on $SOC_FAMILY")

if [[ -e "/usr/lib/qt/examples/webenginewidgets/simplebrowser" ]]; then
    cd /usr/lib/qt/examples/webenginewidgets/simplebrowser
    log_info "chromium" "Starting SimpleBrowser"
    ./simplebrowser
    test_end "$TEST_ID" "PASS"
else
    log_error "chromium" "SimpleBrowser not found. Include qtwebengine.config in defconfig."
    test_end "$TEST_ID" "FAIL" "SimpleBrowser not installed"
    exit 1
fi
