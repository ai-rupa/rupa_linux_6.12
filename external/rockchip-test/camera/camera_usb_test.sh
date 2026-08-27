#!/bin/bash
# camera/camera_usb_test.sh - USB camera test

DIR_CAMERA="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CAMERA}/../common/common.sh"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/gstreamer-1.0
SINK=$(get_video_sink)

TEST_ID=$(test_begin "camera" "usb" "USB camera test with $SINK")

# Get USB camera device list
v4l2-ctl --list-devices > /tmp/.v4l2_list 2>/dev/null
USB_VIDEO=($(awk '/usb/{getline a;print a}' /tmp/.v4l2_list))
rm -f /tmp/.v4l2_list

if [[ ${#USB_VIDEO[@]} -eq 0 ]]; then
    test_end "$TEST_ID" "FAIL" "No USB cameras found"
    exit 1
fi

log_info "camera" "Found ${#USB_VIDEO[@]} USB camera(s), sink: $SINK"

set +e
for device in "${USB_VIDEO[@]}"; do
    log_info "camera" "Testing USB camera: $device"
    gst-launch-1.0 -e v4l2src device="$device" ! \
        image/jpeg ! jpegparse ! mppjpegdec ! \
        $SINK sync=false
done
EXIT_CODE=$?
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "USB camera test failed"
    exit 1
fi
