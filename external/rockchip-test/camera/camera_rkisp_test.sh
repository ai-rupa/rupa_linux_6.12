#!/bin/bash
# camera/camera_rkisp_test.sh - RKISP camera test

DIR_CAMERA="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CAMERA}/../common/common.sh"

SINK=$(get_video_sink)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/gstreamer-1.0

# Find camera device
CAM_DEVICE=""
for cam in /dev/video-camera0 /dev/video0 /dev/video1; do
    if [[ -e "$cam" ]]; then
        CAM_DEVICE="$cam"
        break
    fi
done

if [[ -z "$CAM_DEVICE" ]]; then
    log_error "camera" "No camera device found"
    exit 1
fi

TEST_ID=$(test_begin "camera" "rkisp" "RKISP camera test with $SINK")
log_info "camera" "Using camera device: $CAM_DEVICE, sink: $SINK"

set +e
gst-launch-1.0 -v v4l2src device="$CAM_DEVICE" ! \
    video/x-raw,format=NV12,width=640,height=480,framerate=30/1 ! \
    "$SINK"
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "gst-launch exited with code $EXIT_CODE"
    exit 1
fi
