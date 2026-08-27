#!/bin/bash
# ddr/ddr_test.sh - DDR test dispatcher
# Supports both interactive and CLI modes.

DIR_DDR="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "memtester"
    echo "stressapptest"
    echo "ddr auto scaling"
    echo "stressapptest + memtester"
    echo "stressapptest + memtester + ddr auto scaling"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        memtester)      exec bash "$DIR_DDR/memtester_test.sh" "${@:3}" ;;
        stressapptest)  exec bash "$DIR_DDR/stressapptest_test.sh" "${@:3}" ;;
        scaling)        exec bash "$DIR_DDR/ddr_freq_scaling.sh" "${@:3}" ;;
        combo)
            bash "$DIR_DDR/stressapptest_test.sh" "${@:3}" &
            exec bash "$DIR_DDR/memtester_test.sh" "${@:3}"
            ;;
        full)
            bash "$DIR_DDR/stressapptest_test.sh" "${@:3}" &
            bash "$DIR_DDR/memtester_test.sh" "${@:3}" &
            exec bash "$DIR_DDR/ddr_freq_scaling.sh" "${@:3}"
            ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***            DDR TEST                           ***"
echo "***                                               ***"
echo "*****************************************************"
echo "*****************************************************"
echo "memtester:                                      1"
echo "stressapptest:                                  2"
echo "ddr auto scaling:                               3"
echo "stressapptest + memtester:                      4"
echo "stressapptest + memtester + ddr auto scaling:   5"
echo "*****************************************************"
read -t 30 -p "please input test module: " DDR_CHOICE

case ${DDR_CHOICE} in
    1) bash "$DIR_DDR/memtester_test.sh" & ;;
    2) bash "$DIR_DDR/stressapptest_test.sh" & ;;
    3) bash "$DIR_DDR/ddr_freq_scaling.sh" & ;;
    4)
        bash "$DIR_DDR/stressapptest_test.sh" &
        bash "$DIR_DDR/memtester_test.sh" &
        ;;
    5)
        bash "$DIR_DDR/stressapptest_test.sh" &
        bash "$DIR_DDR/memtester_test.sh" &
        bash "$DIR_DDR/ddr_freq_scaling.sh" &
        ;;
    *) echo "not found your input." ;;
esac
