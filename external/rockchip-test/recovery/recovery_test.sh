#!/bin/bash
# recovery/recovery_test.sh - Recovery test dispatcher
# Supports both interactive and CLI modes.

DIR_RECOVERY="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "recovery function test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        recovery) exec bash "$DIR_RECOVERY/test_function_recovery.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***            RECOVERY TEST                      ***"
echo "***                                               ***"
echo "*****************************************************"
echo "***********************************************************"
echo "recovery function test:					1"
echo "***********************************************************"

read -t 30 RECOVERY_CHOICE

case ${RECOVERY_CHOICE} in
    1) bash "$DIR_RECOVERY/test_function_recovery.sh" ;;
    *) echo "not found your input." ;;
esac
