#!/bin/bash
# pcie/pcie_test.sh - PCIe test dispatcher
# Supports both interactive and CLI modes.

DIR_PCIE="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "pcie function test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        function) exec bash "$DIR_PCIE/pcie_function_test.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***                 PCIE TEST                     ***"
echo "***                                               ***"
echo "*****************************************************"
echo "*****************************************************"
echo "pcie test:					  1"
echo "*****************************************************"

read -t 30 PCIE_CHOICE

case ${PCIE_CHOICE} in
    1) bash "$DIR_PCIE/pcie_function_test.sh" & ;;
    *) echo "not found your input." ;;
esac
