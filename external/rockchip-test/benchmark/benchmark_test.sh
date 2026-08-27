#!/bin/bash
# benchmark/benchmark_test.sh - Benchmark test dispatcher
# Supports both interactive and CLI modes.

DIR_BENCHMARK="$(cd "$(dirname "$0")" && pwd)"

# CLI mode: --list
if [[ "${1:-}" == "--list" ]]; then
    echo "unixbench test"
    echo "glmark2 test"
    exit 0
fi

# CLI mode: --test <name>
if [[ "${1:-}" == "--test" ]]; then
    case "${2:-}" in
        unixbench) exec bash "$DIR_BENCHMARK/unixbench_test.sh" "${@:3}" ;;
        glmark2)   exec bash "$DIR_BENCHMARK/glmark2_test.sh" "${@:3}" ;;
        *) echo "Unknown test: $2" >&2; exit 1 ;;
    esac
fi

# Interactive mode (original behavior)
echo "*****************************************************"
echo "***                                               ***"
echo "***                BENCHMARK TEST                 ***"
echo "***                                               ***"
echo "*****************************************************"
echo "*****************************************************"
echo "unixbench test:                                  1"
echo "glmark2 test:                                    2"
echo "*****************************************************"

read -t 30 BENCHMARK_CHOICE

case ${BENCHMARK_CHOICE} in
    1) bash "$DIR_BENCHMARK/unixbench_test.sh" ;;
    2) bash "$DIR_BENCHMARK/glmark2_test.sh" ;;
    *) echo "not found your input." ;;
esac
