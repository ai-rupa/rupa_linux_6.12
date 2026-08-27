#!/bin/bash -e
# gpu/glmark2_test.sh - Unified glmark2 test for all modes
# Usage: bash glmark2_test.sh --mode fullscreen|normal|offscreen|stress

DIR_GPU="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_GPU}/../common/common.sh"

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# Parse CLI arguments
parse_args "$@"

# Parse mode from args or default to fullscreen
MODE="${OPT_MODE:-fullscreen}"

# Validate mode
case "$MODE" in
    fullscreen|normal|offscreen|stress) ;;
    *)
        log_error "gpu" "Invalid mode: $MODE (use fullscreen|normal|offscreen|stress)"
        exit 1
        ;;
esac

# Check GPU support
case "$SOC_FAMILY" in
    rk1808|rk3308)
        test_skip "gpu" "glmark2_${MODE}" "SoC ${SOC_FAMILY} has no GPU support"
        exit 0
        ;;
    unknown)
        log_warn "gpu" "Unknown SoC, attempting glmark2 with default settings"
        ;;
esac

# Build glmark2 command based on mode and SoC
build_glmark2_cmd() {
    local cmd=""

    # Apply taskset for non-stress modes on big.LITTLE SoCs
    if [[ "$MODE" != "stress" ]]; then
        local cores
        cores=$(get_gpu_taskset_cores)
        [[ -n "$cores" ]] && cmd="taskset -c $cores "
    fi

    cmd="${cmd}glmark2-es2-wayland"

    # Mode-specific flags
    case "$MODE" in
        fullscreen) cmd="$cmd --fullscreen --annotate" ;;
        normal)     cmd="$cmd --annotate" ;;
        offscreen)  cmd="$cmd --off-screen" ;;
        stress)     cmd="$cmd --run-forever --fullscreen --annotate" ;;
    esac

    # SoC-specific visual config (stencil=8 for older/limited GPUs)
    case "$SOC_FAMILY" in
        px3se|rk3036|rk312x|rk3328|rk3528)
            cmd="$cmd --visual-config='stencil=8'"
            ;;
    esac

    echo "$cmd"
}

set_performance_governor

CMD=$(build_glmark2_cmd)
TEST_ID=$(test_begin "gpu" "glmark2_${MODE}" "glmark2 ${MODE} on ${SOC_FAMILY}")
log_info "gpu" "Running: $CMD"

set +e
eval "$CMD" 2>&1 | tee -a "$(get_log_path gpu)"
EXIT_CODE=${PIPESTATUS[0]}
set -e

if [[ $EXIT_CODE -eq 0 ]]; then
    test_end "$TEST_ID" "PASS"
else
    test_end "$TEST_ID" "FAIL" "glmark2 exited with code $EXIT_CODE"
    exit 1
fi
