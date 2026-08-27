#!/bin/bash
# camera/camera_test.sh - Camera test dispatcher
# Supports both interactive and CLI modes.

DIR_CAMERA="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "camera rkisp test"
    echo "camera usb test"
    echo "camera stresstest by v4l2"
    echo "camera stresstest by rkisp_demo"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        rkisp)    exec bash "$DIR_CAMERA/camera_rkisp_test.sh" "${@:3}" ;;
        usb)      exec bash "$DIR_CAMERA/camera_usb_test.sh" "${@:3}" ;;
        v4l2)     exec bash "$DIR_CAMERA/camera_stresstest_v4l2.sh" "${@:3}" ;;
        rkisp_demo) exec bash "$DIR_CAMERA/camera_stresstest_rkisp_demo.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***                CAMERA TEST                    ***"
echo "***                                               ***"
echo "*****************************************************"
echo "*****************************************************"
echo "camera rkisp test:                                  1"
echo "camera usb test:                                    2"
echo "camera stresstest by v4l2:                          3"
echo "camera stresstest by rkisp_demo:                    4"
echo "*****************************************************"

read -t 30 CAMERA_CHOICE

case ${CAMERA_CHOICE} in
    1) bash "$DIR_CAMERA/camera_rkisp_test.sh" ;;
    2) bash "$DIR_CAMERA/camera_usb_test.sh" ;;
    3) bash "$DIR_CAMERA/camera_stresstest_v4l2.sh" ;;
    4) bash "$DIR_CAMERA/camera_stresstest_rkisp_demo.sh" ;;
    *) echo "not found your input." ;;
esac
