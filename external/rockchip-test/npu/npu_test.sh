#!/bin/bash
# npu/npu_test.sh - NPU test dispatcher
# Supports both interactive and CLI modes.

DIR_NPU="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "npu stress test"
    echo "npu scale frequency test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        stress)    exec bash "$DIR_NPU/npu_stress_test.sh" "${@:3}" ;;
        scaling)   exec bash "$DIR_NPU/npu_freq_scaling.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***            NPU TEST                           ***"
echo "***                                               ***"
echo "*****************************************************"
echo "***********************************************************"
echo "npu stress test:                  1"
echo "npu scale frequency test:         2"
echo "***********************************************************"

read -t 30 NPU_CHOICE

case ${NPU_CHOICE} in
    1) bash "$DIR_NPU/npu_stress_test.sh" ;;
    2) bash "$DIR_NPU/npu_freq_scaling.sh" 86400 2 ;;
    *) echo "not found your input." ;;
esac
