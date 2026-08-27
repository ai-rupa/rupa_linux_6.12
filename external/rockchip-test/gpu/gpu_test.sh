#!/bin/bash
# gpu/gpu_test.sh - GPU test dispatcher
# Supports both interactive and CLI modes.

DIR_GPU="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "glmark2 fullscreen test"
    echo "glmark2 normal test (800x600)"
    echo "glmark2 offscreen test"
    echo "glmark2 stress test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        fullscreen) exec bash "$DIR_GPU/glmark2_test.sh" --mode fullscreen "$@" ;;
        normal)     exec bash "$DIR_GPU/glmark2_test.sh" --mode normal "$@" ;;
        offscreen)  exec bash "$DIR_GPU/glmark2_test.sh" --mode offscreen "$@" ;;
        stress)     exec bash "$DIR_GPU/glmark2_test.sh" --mode stress "$@" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***                 GPU TEST                      ***"
echo "***                                               ***"
echo "*****************************************************"
echo "***********************************************************"
echo "glmark2 fullscreen test:                                 1"
echo "glmark2 normal test (800x600):                           2"
echo "glmark2 offscreen test:                                  3"
echo "glmark2 stress test:                                     4"
echo "***********************************************************"

read -t 30 GPU_CHOICE

case ${GPU_CHOICE} in
    1) bash "$DIR_GPU/glmark2_test.sh" --mode fullscreen ;;
    2) bash "$DIR_GPU/glmark2_test.sh" --mode normal ;;
    3) bash "$DIR_GPU/glmark2_test.sh" --mode offscreen ;;
    4) bash "$DIR_GPU/glmark2_test.sh" --mode stress ;;
    *) echo "not found your input." ;;
esac
