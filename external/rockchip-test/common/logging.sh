#!/bin/bash
# common/logging.sh - Structured logging and test result framework

# Guard against double-sourcing
[[ -n "${_ROCKCHIP_LOGGING_LOADED:-}" ]] && return 0
_ROCKCHIP_LOGGING_LOADED=1

# Configuration (overridable via environment variables)
LOG_DIR="${LOG_DIR:-/userdata/rockchip-test}"
LOG_LEVEL="${LOG_LEVEL:-INFO}"       # DEBUG, INFO, WARN, ERROR
LOG_CONSOLE="${LOG_CONSOLE:-1}"      # 1 = also print to stderr
LOG_JSON="${LOG_JSON:-0}"            # 1 = emit JSON lines for CI

# Internal: numeric level for comparison
_log_level_num() {
    case "$1" in
        DEBUG) echo 0 ;;
        INFO)  echo 1 ;;
        WARN)  echo 2 ;;
        ERROR) echo 3 ;;
        *)     echo 1 ;;
    esac
}

# _log_internal(level, module, message)
#   Core logging function. Writes to LOG_DIR/<module>.log and optionally stderr.
_log_internal() {
    local level="$1" module="$2" message="$3"
    local current_level_num log_level_num timestamp log_line

    current_level_num=$(_log_level_num "$level")
    log_level_num=$(_log_level_num "$LOG_LEVEL")

    [[ $current_level_num -lt $log_level_num ]] && return 0

    timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    mkdir -p "$LOG_DIR" 2>/dev/null || true

    if [[ "$LOG_JSON" == "1" ]]; then
        log_line="{\"ts\":\"${timestamp}\",\"level\":\"${level}\",\"module\":\"${module}\",\"msg\":\"${message}\"}"
    else
        log_line="[${timestamp}] [${level}] [${module}] ${message}"
    fi

    echo "$log_line" >> "${LOG_DIR}/${module}.log" 2>/dev/null || true

    if [[ "$LOG_CONSOLE" == "1" ]]; then
        echo "$log_line" >&2
    fi
}

# Public logging functions
log_debug() { _log_internal "DEBUG" "$1" "$2"; }
log_info()  { _log_internal "INFO"  "$1" "$2"; }
log_warn()  { _log_internal "WARN"  "$1" "$2"; }
log_error() { _log_internal "ERROR" "$1" "$2"; }

# get_log_path(module)
#   Returns the full path to the log file for a given module.
get_log_path() {
    echo "${LOG_DIR}/${1}.log"
}

# Test lifecycle tracking
declare -A _TEST_START_TIMES=()

# test_begin(module, test_name, [description])
#   Records test start time and logs the beginning of a test.
#   Prints a test_id for use with test_end.
test_begin() {
    local module="$1" test_name="$2" description="${3:-}"
    local test_id="${module}_${test_name}"
    local start_time
    start_time=$(date +%s)

    _TEST_START_TIMES["$test_id"]="$start_time"

    if [[ -n "$description" ]]; then
        log_info "$module" "TEST BEGIN: ${test_name} - ${description}"
    else
        log_info "$module" "TEST BEGIN: ${test_name}"
    fi

    echo "$test_id"
}

# test_end(test_id, status, [message])
#   status: "PASS" or "FAIL"
#   Calculates duration, writes structured result, logs completion.
test_end() {
    local test_id="$1" status="$2" message="${3:-}"
    local end_time duration module test_name

    end_time=$(date +%s)

    # Parse module and test_name from test_id
    module="${test_id%%_*}"
    test_name="${test_id#*_}"

    if [[ -n "${_TEST_START_TIMES[$test_id]:-}" ]]; then
        duration=$(( end_time - _TEST_START_TIMES[$test_id] ))
        unset "_TEST_START_TIMES[$test_id]"
    else
        duration=0
    fi

    local log_msg="TEST END: ${test_name} [${status}] duration=${duration}s"
    [[ -n "$message" ]] && log_msg="${log_msg} - ${message}"

    if [[ "$status" == "PASS" ]]; then
        log_info "$module" "$log_msg"
    else
        log_error "$module" "$log_msg"
    fi

    write_result_json "$module" "$test_name" "$status" "$duration" "$(get_log_path "$module")" "$message"
}

# test_skip(module, test_name, reason)
#   Logs a test as skipped with reason.
test_skip() {
    local module="$1" test_name="$2" reason="$3"
    log_warn "$module" "TEST SKIP: ${test_name} - ${reason}"
    write_result_json "$module" "$test_name" "SKIP" "0" "$(get_log_path "$module")" "$reason"
}

# write_result_json(module, test, status, duration, log_file, [details])
#   Appends a structured result entry to LOG_DIR/results.json
write_result_json() {
    local module="$1" test="$2" status="$3" duration="$4" log_file="$5" details="${6:-}"
    local timestamp
    timestamp=$(date '+%Y-%m-%dT%H:%M:%S%z')

    mkdir -p "$LOG_DIR" 2>/dev/null || true

    local result_file="${LOG_DIR}/results.json"

    # Initialize file if it doesn't exist
    if [[ ! -f "$result_file" ]]; then
        echo '{"test_suite":"rockchip-test","platform":"'"${SOC_FAMILY:-unknown}"'","results":[]}' > "$result_file"
    fi

    # Append result as a new line (JSONL-style for easy appending)
    local entry
    entry=$(printf '{"module":"%s","test":"%s","status":"%s","duration_seconds":%s,"log_file":"%s","timestamp":"%s","details":"%s"}' \
        "$module" "$test" "$status" "$duration" "$log_file" "$timestamp" "$details")

    echo "$entry" >> "${LOG_DIR}/results.jsonl" 2>/dev/null || true

    # Also emit to stdout in JSON mode for CI capture
    if [[ "$LOG_JSON" == "1" ]]; then
        echo "$entry"
    fi
}

# generate_report(format)
#   format: "json" or "text"
#   Reads LOG_DIR/results.jsonl and outputs a summary report.
generate_report() {
    local format="${1:-text}"
    local result_file="${LOG_DIR}/results.jsonl"

    if [[ ! -f "$result_file" ]]; then
        echo "No test results found."
        return 1
    fi

    local total pass fail skip
    total=$(wc -l < "$result_file")
    pass=$(grep -c '"status":"PASS"' "$result_file" 2>/dev/null || echo 0)
    fail=$(grep -c '"status":"FAIL"' "$result_file" 2>/dev/null || echo 0)
    skip=$(grep -c '"status":"SKIP"' "$result_file" 2>/dev/null || echo 0)

    case "$format" in
        json)
            cat <<EOF
{
  "test_suite": "rockchip-test",
  "platform": "${SOC_FAMILY:-unknown}",
  "timestamp": "$(date '+%Y-%m-%dT%H:%M:%S%z')",
  "summary": {
    "total": $total,
    "pass": $pass,
    "fail": $fail,
    "skip": $skip
  },
  "results": [
EOF
            local first=1
            while IFS= read -r line; do
                if [[ $first -eq 1 ]]; then
                    echo "    $line"
                    first=0
                else
                    echo "    ,$line"
                fi
            done < "$result_file"
            echo "  ]"
            echo "}"
            ;;
        text)
            echo "========================================"
            echo "  rockchip-test Results Summary"
            echo "  Platform: ${SOC_FAMILY:-unknown}"
            echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "========================================"
            echo "  Total:  $total"
            echo "  Pass:   $pass"
            echo "  Fail:   $fail"
            echo "  Skip:   $skip"
            echo "========================================"
            if [[ $fail -gt 0 ]]; then
                echo ""
                echo "Failed tests:"
                grep '"status":"FAIL"' "$result_file" | while IFS= read -r line; do
                    echo "  - $line"
                done
            fi
            ;;
    esac
}
