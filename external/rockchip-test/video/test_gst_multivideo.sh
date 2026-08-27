#!/bin/bash
# video/test_gst_multivideo.sh - Multi-video simultaneous playback test

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_VIDEO}/../common/common.sh"

URI="${1:-/oem/SampleVideo_1280x720_5mb.mp4}"
export mpp_syslog_perror=1

resolve_uri "$URI"
URI="$RESOLVED_URI"

TEST_ID=$(test_begin "video" "multivideo" "Multi-video playback (9 streams)")
log_info "video" "Playing 9 video streams: $URI"

set +e
while true; do
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps0 video-sink="waylandsink render-rectangle=\"<0,180,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps1 video-sink="waylandsink render-rectangle=\"<360,180,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps2 video-sink="waylandsink render-rectangle=\"<720,180,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps3 video-sink="waylandsink render-rectangle=\"<0,420,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps4 video-sink="waylandsink render-rectangle=\"<360,420,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps5 video-sink="waylandsink render-rectangle=\"<720,420,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps6 video-sink="waylandsink render-rectangle=\"<0,660,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps7 video-sink="waylandsink render-rectangle=\"<360,660,360,240>\"" text-overlay=false &
    GST_DEBUG=fps*:5 gst-launch-1.0 uridecodebin uri="$URI" ! fpsdisplaysink name=fps8 video-sink="waylandsink render-rectangle=\"<720,660,360,240>\"" text-overlay=false &
    wait
done
set -e
