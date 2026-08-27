#!/bin/bash
# npu/npu_stress_test.sh - NPU inference stress test
# Supports both RKNPU2 (rknn_common_test) and RKNPU3 (rknn3_cnn_demo) platforms.

DIR_NPU="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_NPU}/../common/common.sh"

# get_npu_backend()
#   Selects the NPU backend for the current SoC family.
#   Sets: NPU_BIN, NPU_MODEL (file for RKNPU2, directory for RKNPU3), NPU_IMAGE.
#   Returns 1 if the SoC has no known NPU backend.
get_npu_backend() {
    case "$SOC_FAMILY" in
        rk3572)
            # RKNPU3: rknn3_cnn_demo takes a model directory; assets ship with rockchip-test.
            NPU_BIN="rknn3_cnn_demo"
            NPU_MODEL="${ROCKCHIP_TEST_ROOT}/npu/model/RK3572/mobilenetv2-224x224-w8a8-1/"
            NPU_IMAGE="${ROCKCHIP_TEST_ROOT}/npu/model/dog_224x224.jpg"
            ;;
        rk3588)
            NPU_BIN="rknn_common_test"
            NPU_MODEL="/usr/share/model/RK3588/mobilenet_v1.rknn"
            NPU_IMAGE="/usr/share/model/dog_224x224.jpg"
            ;;
        rk3566|rk3568)
            NPU_BIN="rknn_common_test"
            NPU_MODEL="/usr/share/model/RK3566_RK3568/mobilenet_v1.rknn"
            NPU_IMAGE="/usr/share/model/dog_224x224.jpg"
            ;;
        rk3562)
            NPU_BIN="rknn_common_test"
            NPU_MODEL="/usr/share/model/RK3562/mobilenet_v1.rknn"
            NPU_IMAGE="/usr/share/model/dog_224x224.jpg"
            ;;
        rk3576)
            NPU_BIN="rknn_common_test"
            NPU_MODEL="/usr/share/model/RK3576/mobilenet_v1.rknn"
            NPU_IMAGE="/usr/share/model/dog_224x224.jpg"
            ;;
        rv1126b)
            NPU_BIN="rknn_common_test"
            NPU_MODEL="/usr/share/model/RV1126B/mobilenet_v1.rknn"
            NPU_IMAGE="/usr/share/model/dog_224x224.jpg"
            ;;
        *)
            log_error "npu" "No NPU model for SoC: $SOC_FAMILY"
            return 1
            ;;
    esac
}

# run_npu_inference()
#   Runs a single inference with the selected backend.
run_npu_inference() {
    if [[ "$NPU_BIN" == "rknn3_cnn_demo" ]]; then
        rknn3_cnn_demo -f "$NPU_MODEL" -i "$NPU_IMAGE" -cl 10
    else
        rknn_common_test "$NPU_MODEL" "$NPU_IMAGE" 10
    fi
}

get_npu_backend || exit 1

require_command "$NPU_BIN" || exit 1
require_file "$NPU_IMAGE" || exit 1
# NPU_MODEL may be a file (RKNPU2) or a directory (RKNPU3); check existence either way.
if [[ ! -e "$NPU_MODEL" ]]; then
    log_error "npu" "NPU model not found: $NPU_MODEL"
    exit 1
fi

TEST_ID=$(test_begin "npu" "stress" "NPU inference stress on $SOC_FAMILY")
log_info "npu" "Running $NPU_BIN with model: $NPU_MODEL"

set +e
while true; do
    run_npu_inference
    EXIT_CODE=$?
    if [[ $EXIT_CODE -ne 0 ]]; then
        test_end "$TEST_ID" "FAIL" "$NPU_BIN exited with code $EXIT_CODE"
        exit 1
    fi
    # Pause between inference rounds (matches the validated stress cadence).
    sleep 2
done
set -e
