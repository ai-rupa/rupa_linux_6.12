#!/bin/bash
# camera/camera_stresstest_rkisp_demo.sh - Camera stress test using rkisp_demo

DIR_CAMERA="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_CAMERA}/../common/common.sh"

require_command rkisp_demo || exit 1
require_command media-ctl || exit 1

TEST_ID=$(test_begin "camera" "rkisp_demo_stress" "rkisp_demo camera stress test")

# Discover camera devices
CAM_NUM=0
MEDIA_MAX=20
CIF_PATH="stream_cif"
ISP_PATH="rkisp_mainpath"

for i in $(seq 0 $MEDIA_MAX); do
    MEDIA_DEV=/dev/media$i
    ISP_NODE=$(media-ctl -d "$MEDIA_DEV" -e "$ISP_PATH" 2>/dev/null)
    CIF_NODE=$(media-ctl -d "$MEDIA_DEV" -e "$CIF_PATH" 2>/dev/null)

    if echo "$ISP_NODE" | grep -q "^/dev/video"; then
        CAM_NUM=$((CAM_NUM + 1))
        eval VIDEO_NODE$i="$ISP_NODE"
        log_info "camera" "Found ISP camera at $ISP_NODE (media$i)"
    elif echo "$CIF_NODE" | grep -q "^/dev/video"; then
        Link=$(media-ctl -d "$MEDIA_DEV" -p 2>/dev/null | grep "0 link")
        if echo "$Link" | grep -q "0 link"; then
            log_debug "camera" "media$i: CIF not linked, skipping"
        else
            CAM_NUM=$((CAM_NUM + 1))
            eval VIDEO_NODE$i="$CIF_NODE"
            log_info "camera" "Found CIF camera at $CIF_NODE (media$i)"
        fi
    fi
done

if [[ $CAM_NUM -eq 0 ]]; then
    test_end "$TEST_ID" "FAIL" "No cameras found"
    exit 1
fi

log_info "camera" "Found $CAM_NUM camera(s)"

COUNT=1
set +e
while true; do
    log_info "camera" "Test round $COUNT"
    COUNT=$((COUNT + 1))
    for i in $(seq 0 $CAM_NUM); do
        VIDEO_DEV=$(eval echo \$VIDEO_NODE$i)
        if echo "$VIDEO_DEV" | grep -q "^/dev/video"; then
            log_info "camera" "Testing camera: $VIDEO_DEV"
            rkisp_demo --device="$VIDEO_DEV" --stream-to=/tmp/video$i.yuv --count=100
            EXIT_CODE=$?
            if [[ $EXIT_CODE -ne 0 ]]; then
                test_end "$TEST_ID" "FAIL" "rkisp_demo failed on $VIDEO_DEV"
                exit 1
            fi
        fi
    done
done
set -e
