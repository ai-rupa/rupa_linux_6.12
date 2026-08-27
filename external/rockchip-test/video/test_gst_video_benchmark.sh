#!/bin/bash
# video/test_gst_video_benchmark.sh - Video encode/decode benchmark
# Generates HTML report and JSON results for CI.

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_VIDEO}/../common/common.sh"

resolutions=("640x360" "640x480" "800x600" "1024x768" "1280x720" "1920x1080" "2560x1440" "3840x2160")

if [[ "$#" -gt 0 && "$1" != --* ]]; then
    resolutions=("$@")
fi

outhtml=/userdata/gstreamer_stress_test.html
TEST_ID=$(test_begin "video" "benchmark" "Video encode/decode benchmark")

log_info "video" "Starting video benchmark for resolutions: ${resolutions[*]}"

echo "<table border=\"1\">
  <tr>
    <th rowspan=\"2\">resolutions</th>
    <th colspan=\"3\">encode</th>
    <th colspan=\"3\">decode</th>
  </tr>
  <tr>
    <th>H264</th>
    <th>H265</th>
    <th>JPEG</th>
    <th>H264</th>
    <th>H265</th>
    <th>JPEG</th>
  </tr>
" > "$outhtml"

OVERALL_PASS=1

for res in "${resolutions[@]}"; do
    IFS='x' read -r width height <<< "$res"
    log_info "video" "Testing resolution: $res"

    if [[ $width -lt 800 ]]; then
        buffers=1800
    elif [[ $width -lt 1280 ]]; then
        buffers=900
    else
        buffers=300
    fi
    blocksize=$((width * height * 3 / 2))
    testfiledir=/userdata/gstvideotest
    rm -rf "$testfiledir"
    mkdir -p "$testfiledir"
    logfiledir=$(get_log_path video)
    logfiledir="${logfiledir%.log}_benchmark"
    mkdir -p "$logfiledir"

    rawfile=${testfiledir}/videotestsrc_${width}x${height}_nv12_${buffers}frames.yuv
    h264file=${testfiledir}/videotestsrc_${width}x${height}_nv12.h264
    h265file=${testfiledir}/videotestsrc_${width}x${height}_nv12.h265
    jpegfile=${testfiledir}/videotestsrc_${width}x${height}_nv12.jpeg
    h264enclog=${logfiledir}/h264enc_${width}x${height}.log
    h264declog=${logfiledir}/h264dec_${width}x${height}.log
    h265enclog=${logfiledir}/h265enc_${width}x${height}.log
    h265declog=${logfiledir}/h265dec_${width}x${height}.log
    jpegenclog=${logfiledir}/jpegenc_${width}x${height}.log
    jpegdeclog=${logfiledir}/jpegdec_${width}x${height}.log

    # Generate raw file
    gst-launch-1.0 videotestsrc num-buffers=$buffers ! video/x-raw,framerate=30/1,width=${width},height=${height},format=NV12 ! filesink location=$rawfile > /dev/null 2>&1
    if [[ ! -f $rawfile ]]; then
        log_error "video" "Failed to generate raw file: $rawfile"
        OVERALL_PASS=0
        continue
    fi

    # H264 encode
    gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mpph264enc ! h264parse ! filesink location=$h264file > "$h264enclog" 2>&1
    if [[ ! -f $h264file ]]; then
        log_error "video" "Failed to generate h264 file"
        OVERALL_PASS=0
        continue
    fi
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mpph264enc ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$h264enclog" 2>&1
    h264enc_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$h264enclog" | awk '{ print $NF }')
    h264enc_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$h264enclog" | awk '{ print $NF }')

    # H264 decode
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$h264file ! h264parse ! mppvideodec ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$h264declog" 2>&1
    h264dec_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$h264declog" | awk '{ print $NF }')
    h264dec_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$h264declog" | awk '{ print $NF }')

    # H265 encode
    gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mpph265enc ! h265parse ! filesink location=$h265file > "$h265enclog" 2>&1
    if [[ ! -f $h265file ]]; then
        log_error "video" "Failed to generate h265 file"
        OVERALL_PASS=0
        continue
    fi
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mpph265enc ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$h265enclog" 2>&1
    h265enc_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$h265enclog" | awk '{ print $NF }')
    h265enc_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$h265enclog" | awk '{ print $NF }')

    # H265 decode
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$h265file ! h265parse ! mppvideodec ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$h265declog" 2>&1
    h265dec_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$h265declog" | awk '{ print $NF }')
    h265dec_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$h265declog" | awk '{ print $NF }')

    # JPEG encode
    gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mppjpegenc ! jpegparse ! jifmux ! filesink location=$jpegfile > "$jpegenclog" 2>&1
    if [[ ! -f $jpegfile ]]; then
        log_error "video" "Failed to generate jpeg file"
        OVERALL_PASS=0
        continue
    fi
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$rawfile blocksize=$blocksize ! video/x-raw,width=$width,height=$height,format=NV12,framerate=30/1 ! mppjpegenc ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$jpegenclog" 2>&1
    jpegenc_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$jpegenclog" | awk '{ print $NF }')
    jpegenc_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$jpegenclog" | awk '{ print $NF }')

    # JPEG decode
    GST_DEBUG=*fps*:7 gst-launch-1.0 filesrc location=$jpegfile ! jpegparse ! mppjpegdec ! fpsdisplaysink video-sink=fakesink text-overlay=false signal-fps-measurements=true sync=false > "$jpegdeclog" 2>&1
    jpegdec_max_fps=$(awk '/max-fps/ { line=$0 } END { print line }' "$jpegdeclog" | awk '{ print $NF }')
    jpegdec_min_fps=$(awk '/min-fps/ { line=$0 } END { print line }' "$jpegdeclog" | awk '{ print $NF }')

    log_info "video" "${res} H264 enc: max=${h264enc_max_fps} min=${h264enc_min_fps}"
    log_info "video" "${res} H265 enc: max=${h265enc_max_fps} min=${h265enc_min_fps}"
    log_info "video" "${res} JPEG enc: max=${jpegenc_max_fps} min=${jpegenc_min_fps}"

    echo "<tr>
    <th>${width}x${height}</th>
    <th>max ${h264enc_max_fps}<br>min ${h264enc_min_fps}</th>
    <th>max ${h265enc_max_fps}<br>min ${h265enc_min_fps}</th>
    <th>max ${jpegenc_max_fps}<br>min ${jpegenc_min_fps}</th>
    <th>max ${h264dec_max_fps}<br>min ${h264dec_min_fps}</th>
    <th>max ${h265dec_max_fps}<br>min ${h265dec_min_fps}</th>
    <th>max ${jpegdec_max_fps}<br>min ${jpegdec_min_fps}</th>
  </tr>" >> "$outhtml"
done

echo "</table>" >> "$outhtml"

if [[ $OVERALL_PASS -eq 1 ]]; then
    test_end "$TEST_ID" "PASS" "Benchmark completed, report at $outhtml"
else
    test_end "$TEST_ID" "FAIL" "Some resolutions failed, report at $outhtml"
    exit 1
fi
