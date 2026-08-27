#!/bin/bash
# video/video_test.sh - Video test dispatcher
# Supports both interactive and CLI modes.

DIR_VIDEO="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "video test"
    echo "video test with FPS display"
    echo "video max FPS test without display"
    echo "multivideo test"
    echo "video benchmark test"
    echo "video stresstest"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        video)      exec bash "$DIR_VIDEO/test_gst_video.sh" "${@:3}" ;;
        fps)        exec bash "$DIR_VIDEO/test_gst_video_fps.sh" "${@:3}" ;;
        maxfps)     exec bash "$DIR_VIDEO/test_gst_video_maxfps.sh" "${@:3}" ;;
        multi)      exec bash "$DIR_VIDEO/test_gst_multivideo.sh" "${@:3}" ;;
        benchmark)  exec bash "$DIR_VIDEO/test_gst_video_benchmark.sh" "${@:3}" ;;
        stress)     exec bash "$DIR_VIDEO/video_stresstest.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***            VIDEO TEST                         ***"
echo "***                                               ***"
echo "*****************************************************"
echo "***********************************************************"
echo "video test:                                              1"
echo "video test with FPS display:                             2"
echo "video max FPS test without display:                      3"
echo "multivideo test:                                         4"
echo "video benchmark test:                                    5"
echo "video stresstest:                                        6"
echo "***********************************************************"

read -t 30 VIDEO_CHOICE

case ${VIDEO_CHOICE} in
    1) bash "$DIR_VIDEO/test_gst_video.sh" ;;
    2) bash "$DIR_VIDEO/test_gst_video_fps.sh" ;;
    3) bash "$DIR_VIDEO/test_gst_video_maxfps.sh" ;;
    4) bash "$DIR_VIDEO/test_gst_multivideo.sh" ;;
    5) bash "$DIR_VIDEO/test_gst_video_benchmark.sh" ;;
    6) bash "$DIR_VIDEO/video_stresstest.sh" ;;
    *) echo "not found your input." ;;
esac
