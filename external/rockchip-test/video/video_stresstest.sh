#!/bin/bash
# video/video_stresstest.sh - Video playback stress test (loop all videos in directory)

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_VIDEO}/../common/common.sh"

if [[ -d "/mnt/udisk/videos" ]]; then
    TEST_DIR=/mnt/udisk/videos
elif [[ -d "/userdata/videos" ]]; then
    TEST_DIR=/userdata/videos
else
    log_error "video" "No test videos found. Put videos in /mnt/udisk/videos or /userdata/videos"
    exit 1
fi

TEST_ID=$(test_begin "video" "stress" "Video stress test from $TEST_DIR")
log_info "video" "Starting video stress test from: $TEST_DIR"

COUNT=1
set +e
while true; do
    log_info "video" "Playback round $COUNT"
    gst-play-1.0 --flags=3 "$TEST_DIR"
    sleep 1
    COUNT=$((COUNT + 1))
done
set -e
