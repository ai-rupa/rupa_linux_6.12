#!/bin/bash
# video/test_gst_video.sh - Basic video playback test

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_VIDEO}/../common/common.sh"

URI="${1:-/oem/SampleVideo_1280x720_5mb.mp4}"
export mpp_syslog_perror=1

resolve_uri "$URI"
URI="$RESOLVED_URI"

SINK=$(get_video_sink)

TEST_ID=$(test_begin "video" "playback" "Video playback with $SINK")
log_info "video" "Playing: $URI with sink=$SINK"

set +e
gst-launch-1.0 uridecodebin uri="$URI" ! "$SINK" fullscreen=true
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "gst-launch-1.0 exited with code $EXIT_CODE"
    exit 1
fi
