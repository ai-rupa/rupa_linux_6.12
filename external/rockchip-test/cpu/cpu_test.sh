#!/bin/bash
# cpu/cpu_test.sh - CPU test dispatcher
# Supports both interactive and CLI modes.

DIR_CPU="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "stress-ng test"
    echo "cpu auto scaling"
    echo "stress-ng + cpu auto scaling"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        stress) exec bash "$DIR_CPU/cpu_stress_test.sh" "${@:3}" ;;
        scaling) exec bash "$DIR_CPU/cpu_freq_scaling.sh" "${@:3}" ;;
        both)
            bash "$DIR_CPU/cpu_stress_test.sh" "${@:3}" &
            exec bash "$DIR_CPU/cpu_freq_scaling.sh" "${@:3}"
            ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***                 CPU TEST                      ***"
echo "***                                               ***"
echo "*****************************************************"
echo "*****************************************************"
echo "stress-ng test:                               1"
echo "cpu auto scaling:                             2"
echo "stress-ng + cpu auto scaling:                 3"
echo "*****************************************************"

read -t 30 CPUFREQ_CHOICE

case ${CPUFREQ_CHOICE} in
    1) bash "$DIR_CPU/cpu_stress_test.sh" & ;;
    2) bash "$DIR_CPU/cpu_freq_scaling.sh" & ;;
    3)
        bash "$DIR_CPU/cpu_stress_test.sh" &
        bash "$DIR_CPU/cpu_freq_scaling.sh" &
        ;;
    *) echo "not found your input." ;;
esac
