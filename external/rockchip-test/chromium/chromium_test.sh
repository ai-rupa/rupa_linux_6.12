#!/bin/bash
# chromium/chromium_test.sh - Chromium test dispatcher
# Supports both interactive and CLI modes.

DIR_CHROMIUM="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "Chromium test"
    echo "SimpleBrowser test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        chromium)     exec bash "$DIR_CHROMIUM/test_chromium_with_video.sh" "${@:3}" ;;
        simplebrowser) exec bash "$DIR_CHROMIUM/test_simplebrowser.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***            CHROMIUM TEST                      ***"
echo "***                                               ***"
echo "*****************************************************"
echo "***********************************************************"
echo "Chromium test:						1"
echo "SimpleBrowser test:					2"
echo "***********************************************************"

read -t 30 CHROMIUM_CHOICE

case ${CHROMIUM_CHOICE} in
    1) bash "$DIR_CHROMIUM/test_chromium_with_video.sh" ;;
    2) bash "$DIR_CHROMIUM/test_simplebrowser.sh" ;;
    *) echo "not found your input." ;;
esac
