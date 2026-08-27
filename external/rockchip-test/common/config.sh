#!/bin/bash
# common/config.sh - Configuration management and CLI argument parsing

# Guard against double-sourcing
[[ -n "${_ROCKCHIP_CONFIG_LOADED:-}" ]] && return 0
_ROCKCHIP_CONFIG_LOADED=1

# Default test parameters (overridable via environment or CLI args)
DEFAULT_DURATION="${DEFAULT_DURATION:-86400}"   # 24 hours
DEFAULT_LOOPS="${DEFAULT_LOOPS:-10000}"
DEFAULT_DELAY="${DEFAULT_DELAY:-3}"

# CLI option globals (set by parse_args)
OPT_MODULE=""
OPT_TEST=""
OPT_DURATION=""
OPT_LOOPS=""
OPT_MODE=""
OPT_REPORT=""
OPT_LIST=""
OPT_RUN_ALL=""
OPT_QUIET=""
OPT_ARGS=()

# parse_args("$@")
#   Generic argument parser for CLI mode.
#   Sets OPT_* global variables.
#   Prints usage and exits on --help or invalid args.
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --module)
                OPT_MODULE="$2"; shift 2 ;;
            --test)
                OPT_TEST="$2"; shift 2 ;;
            --duration)
                OPT_DURATION="$2"; shift 2 ;;
            --loops)
                OPT_LOOPS="$2"; shift 2 ;;
            --mode)
                OPT_MODE="$2"; shift 2 ;;
            --report)
                OPT_REPORT="$2"; shift 2 ;;
            --list)
                OPT_LIST=1; shift ;;
            --run-all)
                OPT_RUN_ALL=1; shift ;;
            --quiet|-q)
                OPT_QUIET=1; shift ;;
            --help|-h)
                _print_usage "$1"; exit 0 ;;
            --)
                shift; OPT_ARGS=("$@"); break ;;
            -*)
                echo "Error: Unknown option: $1" >&2
                echo "Use --help for usage information." >&2
                exit 1 ;;
            *)
                OPT_ARGS+=("$1"); shift ;;
        esac
    done
}

# _print_usage()
#   Prints CLI usage information.
_print_usage() {
    cat <<EOF
Usage: rockchip_test.sh [OPTIONS]

Options:
  --list                        List all available modules
  --module <name>               Specify module to test
  --test <name>                 Specify test within module
  --mode <mode>                 Test mode (e.g., fullscreen, normal, offscreen, stress)
  --duration <seconds>          Test duration (default: $DEFAULT_DURATION)
  --loops <count>               Loop count (default: $DEFAULT_LOOPS)
  --report <format>             Report format: json or text
  --run-all                     Run all non-interactive tests
  --quiet, -q                   Suppress console output
  --help, -h                    Show this help message

Examples:
  rockchip_test.sh                                      # Interactive mode
  rockchip_test.sh --list                               # List all modules
  rockchip_test.sh --module cpu --list                  # List tests in cpu module
  rockchip_test.sh --module cpu --test stress           # Run CPU stress test
  rockchip_test.sh --module gpu --test glmark2 --mode fullscreen
  rockchip_test.sh --run-all --duration 3600 --report json
EOF
}

# require_root()
#   Exits with error if not running as root.
require_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "config" "This test requires root privileges"
        exit 1
    fi
}

# require_command(cmd_name)
#   Checks if a command exists. Returns 1 if not found.
require_command() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        log_error "config" "Required command not found: $cmd"
        return 1
    fi
    return 0
}

# require_file(path)
#   Checks if a file exists. Returns 1 if not found.
require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        log_error "config" "Required file not found: $path"
        return 1
    fi
    return 0
}

# require_device(path)
#   Checks if a device node exists. Returns 1 if not found.
require_device() {
    local path="$1"
    if [[ ! -e "$path" ]]; then
        log_error "config" "Required device not found: $path"
        return 1
    fi
    return 0
}

# get_duration()
#   Returns the effective test duration: OPT_DURATION if set, else DEFAULT_DURATION.
get_duration() {
    echo "${OPT_DURATION:-$DEFAULT_DURATION}"
}

# get_loops()
#   Returns the effective loop count: OPT_LOOPS if set, else DEFAULT_LOOPS.
get_loops() {
    echo "${OPT_LOOPS:-$DEFAULT_LOOPS}"
}
