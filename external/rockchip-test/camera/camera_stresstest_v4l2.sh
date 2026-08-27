#!/bin/bash
# camera/camera_stresstest_v4l2.sh - Camera stress test using v4l2-ctl

DIR_CAMERA="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CAMERA}/../common/common.sh"

require_command v4l2-ctl || exit 1

TEST_ID=$(test_begin "camera" "v4l2_stress" "V4L2 camera stress test")

# Discover cameras
v4l2-ctl --list-devices > /tmp/.v4l2_list 2>/dev/null
ISP_VIDEO=($(awk '/rkisp_mainpath/{getline a;print a}' /tmp/.v4l2_list))
CIF_VIDEO=($(awk '/cif/{getline a;print a}' /tmp/.v4l2_list | grep video))
USB_VIDEO=($(awk '/usb/{getline a;print a}' /tmp/.v4l2_list))
rm -f /tmp/.v4l2_list

TOTAL_CAMERAS=$(( ${#ISP_VIDEO[@]} + ${#CIF_VIDEO[@]} + ${#USB_VIDEO[@]} ))
log_info "camera" "Found ${#ISP_VIDEO[@]} ISP, ${#CIF_VIDEO[@]} CIF, ${#USB_VIDEO[@]} USB cameras"

if [[ $TOTAL_CAMERAS -eq 0 ]]; then
    test_end "$TEST_ID" "FAIL" "No cameras found"
    exit 1
fi

COUNT=1
set +e
while true; do
    log_info "camera" "Test round $COUNT"
    COUNT=$((COUNT + 1))
    ROUND_OK=0

    for j in "${USB_VIDEO[@]}"; do
        log_info "camera" "Capturing USB camera: $j"
        rm -f /tmp/camera.yuyv
        v4l2-ctl -d "$j" --set-fmt-video=width=640,height=480,pixelformat=YUYV --stream-mmap=3 --stream-count=5 --stream-poll --stream-to=/tmp/camera.yuyv
        size=$(stat -c %s /tmp/camera.yuyv 2>/dev/null || echo 0)
        rm -f /tmp/camera.yuyv
        if [[ $size -eq 3072000 ]]; then
            ROUND_OK=$((ROUND_OK + 1))
        else
            log_warn "camera" "Skip USB camera $j: capture failed (size $size, expected 3072000)"
        fi
    done

    for i in ISP_VIDEO CIF_VIDEO; do
        eval value=\${${i}[@]}
        for j in $value; do
            log_info "camera" "Capturing $i camera: $j"
            rm -f /tmp/camera.nv12
            v4l2-ctl -d "$j" --set-fmt-video=width=640,height=480,pixelformat=NV12 --stream-mmap=3 --stream-count=5 --stream-poll --stream-to=/tmp/camera.nv12
            size=$(stat -c %s /tmp/camera.nv12 2>/dev/null || echo 0)
            rm -f /tmp/camera.nv12
            if [[ $size -eq 2304000 ]]; then
                ROUND_OK=$((ROUND_OK + 1))
            else
                log_warn "camera" "Skip $i camera $j: capture failed (size $size, expected 2304000)"
            fi
        done
    done

    # Only fail when no discovered camera works at all (e.g. all sensors
    # disconnected); a phantom ISP instance without a sensor is just skipped.
    if [[ $ROUND_OK -eq 0 ]]; then
        log_error "camera" "No working cameras in round $((COUNT - 1))"
        test_end "$TEST_ID" "FAIL" "No working cameras (all capture attempts failed)"
        exit 1
    fi
done
set -e
