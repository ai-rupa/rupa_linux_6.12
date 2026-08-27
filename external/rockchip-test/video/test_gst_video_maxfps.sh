#!/bin/bash
# video/test_gst_video_maxfps.sh - Video max FPS test without display

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_VIDEO}/../common/common.sh"

URI="${1:-/oem/SampleVideo_1280x720_5mb.mp4}"
export mpp_syslog_perror=1

resolve_uri "$URI"
URI="$RESOLVED_URI"

TEST_ID=$(test_begin "video" "maxfps" "Video max FPS test (fakesink)")
log_info "video" "Testing max FPS: $URI"

set +e
GST_DEBUG=fps*:7 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink \
    video-sink="fakesink" \
    text-overlay=false \
    signal-fps-measurements=true \
    sync=false
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "gst-launch-1.0 exited with code $EXIT_CODE"
    exit 1
fi
