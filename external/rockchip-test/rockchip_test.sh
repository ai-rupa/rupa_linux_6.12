#!/bin/bash
# rockchip_test.sh - Main entry point for rockchip-test suite
# Supports both interactive and CLI modes.
#
# Interactive mode (no arguments):
#   rockchip_test.sh
#
# CLI mode:
#   rockchip_test.sh --list
#   rockchip_test.sh --module cpu --list
#   rockchip_test.sh --module cpu --test stress --duration 3600
#   rockchip_test.sh --run-all --report json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common/common.sh"

# Module registry
declare -A MODULE_SCRIPTS=(
    [ddr]="ddr/ddr_test.sh"
    [cpu]="cpu/cpu_test.sh"
    [gpu]="gpu/gpu_test.sh"
    [npu]="npu/npu_test.sh"
    [suspend_resume]="suspend_resume/suspend_resume.sh"
    [reboot]="auto_reboot/auto_reboot.sh"
    [power_lost]="flash_test/power_lost_test.sh"
    [flash]="flash_test/flash_stress_test.sh"
    [recovery]="recovery/recovery_test.sh"
    [audio]="audio/audio_functions_test.sh"
    [camera]="camera/camera_test.sh"
    [video]="video/video_test.sh"
    [bluetooth]="wifibt/bt_onoff.sh"
    [wifi]="wifibt/wifi_onoff.sh"
    [pcie]="pcie/pcie_test.sh"
    [chromium]="chromium/chromium_test.sh"
    [benchmark]="benchmark/benchmark_test.sh"
    [ab_update]="ab_update_test/ab_stress_start.sh"
)

declare -A MODULE_DESCRIPTIONS=(
    [ddr]="DDR stress test (memtester, stressapptest)"
    [cpu]="CPU stress test (stress-ng, freq scaling)"
    [gpu]="GPU test (glmark2 variants)"
    [npu]="NPU stress test (rknn inference)"
    [suspend_resume]="Suspend-to-RAM test (RTC wakeup)"
    [reboot]="Auto reboot stress test"
    [power_lost]="Power lost stress test"
    [flash]="Flash write/read stress test"
    [recovery]="Recovery wipe-all test"
    [audio]="Audio playback/capture test"
    [camera]="Camera test (rkisp, USB, v4l2)"
    [video]="Video test (GStreamer encode/decode)"
    [bluetooth]="Bluetooth on/off cycling test"
    [wifi]="WiFi on/off cycling test"
    [pcie]="PCIe device test"
    [chromium]="Chromium browser test"
    [benchmark]="Benchmark (UnixBench, glmark2)"
    [ab_update]="AB update stress test"
)

# Ordered module list for display
MODULE_ORDER=(ddr cpu gpu npu suspend_resume reboot power_lost flash recovery audio camera video bluetooth wifi pcie chromium benchmark ab_update)

list_modules() {
    local i=1
    for mod in "${MODULE_ORDER[@]}"; do
        printf "%-3s %-15s %s\n" "$i" "$mod" "${MODULE_DESCRIPTIONS[$mod]}"
        i=$((i + 1))
    done
}

list_tests() {
    local module="$1"
    local script="${MODULE_SCRIPTS[$module]:-}"
    if [[ -z "$script" ]]; then
        echo "Error: Unknown module '$module'" >&2
        echo "Available modules: ${!MODULE_SCRIPTS[*]}" >&2
        exit 1
    fi
    bash "${SCRIPT_DIR}/${script}" --list
}

run_module() {
    local module="$1"
    local test_name="$2"
    shift 2
    local script="${MODULE_SCRIPTS[$module]:-}"
    if [[ -z "$script" ]]; then
        echo "Error: Unknown module '$module'" >&2
        exit 1
    fi
    bash "${SCRIPT_DIR}/${script}" --test "$test_name" "$@"
}

run_all() {
    local failed=0
    for mod in "${MODULE_ORDER[@]}"; do
        log_info "main" "Running module: $mod"
        local script="${MODULE_SCRIPTS[$mod]}"
        # Skip modules that require interactive input or external resources
        case "$mod" in
            reboot|power_lost|ab_update|recovery|bluetooth|wifi)
                log_warn "main" "Skipping $mod (requires manual intervention or external resources)"
                test_skip "main" "$mod" "Skipped: requires manual intervention"
                continue
                ;;
        esac
        bash "${SCRIPT_DIR}/${script}" --test "$(bash "${SCRIPT_DIR}/${script}" --list | head -1 | tr ' ' '_')" 2>&1 || failed=1
    done
    return $failed
}

# Interactive mode (original behavior, preserved exactly)
CURRENT_DIR="$SCRIPT_DIR"

moudle_env() {
    export MODULE_CHOICE
}

module_choice() {
    echo "******************************************************"
    echo "***                                                ***"
    echo "***          *****************************         ***"
    echo "***          *    ROCKCHIPS TEST TOOLS   *         ***"
    echo "***          *  V3.0 updated on 20260603 *         ***"
    echo "***          *****************************         ***"
    echo "***                                                ***"
    echo "*****************************************************"
    echo ""
    echo "*****************************************************"
    echo "ddr test:             1 (ddr stress test)"
    echo "cpu test:             2 (cpu stress test)"
    echo "gpu test:             3 (gpu stress test)"
    echo "npu test:             4 (npu stress test)"
    echo "suspend_resume test:  5 (suspend resume)"
    echo "reboot test:          6 (auto reboot test)"
    echo "power lost test:      7 (power lost test)"
    echo "flash stress test:    8 (flash stress test)"
    echo "recovery test:        9 (recovery wipe all test)"
    echo "audio test:           10 (audio test)"
    echo "camera test:          11 (camera test)"
    echo "video test:           12 (video test)"
    echo "bluetooth test:       13 (bluetooth test)"
    echo "wifi test:            14 (wifi test)"
    echo "wifibt config test:   15 (wifibt config test)"
    echo "pcie test:            16 (pcie test)"
    echo "chromium test:        17 (chromium with video test)"
    echo "benchmark test:       18 (unixbench, glmark2...)"
    echo "ab update test:       19 (ab update stress test)"
    echo "*****************************************************"

    read -t 30 -p "please input test module: " MODULE_CHOICE
}

module_test() {
    case ${MODULE_CHOICE} in
        1)  bash "$CURRENT_DIR/ddr/ddr_test.sh" ;;
        2)  bash "$CURRENT_DIR/cpu/cpu_test.sh" ;;
        3)  bash "$CURRENT_DIR/gpu/gpu_test.sh" ;;
        4)  bash "$CURRENT_DIR/npu/npu_test.sh" ;;
        5)  bash "$CURRENT_DIR/suspend_resume/suspend_resume.sh" ;;
        6)
            fcnt=/userdata/rockchip/reboot_cnt
            [[ -e "$fcnt" ]] && rm -f "$fcnt"
            bash "$CURRENT_DIR/auto_reboot/auto_reboot.sh"
            ;;
        7)
            fcnt=/userdata${CURRENT_DIR}/power_lost/reboot_cnt
            [[ -e "$fcnt" ]] && rm -f "$fcnt"
            bash "$CURRENT_DIR/flash_test/power_lost_test.sh" &
            ;;
        8)  bash "$CURRENT_DIR/flash_test/flash_stress_test.sh" 5 20000 & ;;
        9)  bash "$CURRENT_DIR/recovery/recovery_test.sh" ;;
        10) bash "$CURRENT_DIR/audio/audio_functions_test.sh" ;;
        11) bash "$CURRENT_DIR/camera/camera_test.sh" ;;
        12) bash "$CURRENT_DIR/video/video_test.sh" ;;
        13)
            bin=/usr/bin/rkwifibt_app_test
            if [[ -e "$bin" ]]; then
                "$bin" BT
            else
                echo "WiFiBT testfile don't exist! fallback to bt_onoff.sh"
                bash "$CURRENT_DIR/wifibt/bt_onoff.sh" &
            fi
            ;;
        14)
            bin=/usr/bin/rkwifibt_app_test
            if [[ -e "$bin" ]]; then
                "$bin" WiFi
            else
                echo "WiFiBT testfile don't exist! fallback to wifi_onoff.sh"
                bash "$CURRENT_DIR/wifibt/wifi_onoff.sh" &
            fi
            ;;
        15)
            bin=/usr/bin/rkwifibt_app_test
            if [[ -e "$bin" ]]; then
                "$bin" Network
            else
                echo "WiFiBT testfile don't exist!"
            fi
            ;;
        16) bash "$CURRENT_DIR/pcie/pcie_test.sh" ;;
        17) bash "$CURRENT_DIR/chromium/chromium_test.sh" ;;
        18) bash "$CURRENT_DIR/benchmark/benchmark_test.sh" ;;
        19) bash "$CURRENT_DIR/ab_update_test/ab_stress_start.sh" 10000 & ;;
    esac
}

# Main entry point
main() {
    if [[ $# -eq 0 ]]; then
        # No arguments: interactive mode (backward compatible)
        moudle_env
        module_choice
        module_test
    else
        # CLI mode
        parse_args "Rockchip Test Suite" "$@"

        # Handle --list at top level
        if [[ -n "${OPT_LIST:-}" ]]; then
            if [[ -n "${OPT_MODULE:-}" ]]; then
                list_tests "$OPT_MODULE"
            else
                list_modules
            fi
            exit 0
        fi

        # Handle --run-all
        if [[ -n "${OPT_RUN_ALL:-}" ]]; then
            run_all
            EXIT_CODE=$?
            if [[ -n "${OPT_REPORT:-}" ]]; then
                generate_report "$OPT_REPORT"
            fi
            exit $EXIT_CODE
        fi

        # Handle --module with --test
        if [[ -n "${OPT_MODULE:-}" ]]; then
            if [[ -z "${OPT_TEST:-}" ]]; then
                echo "Error: --test required with --module" >&2
                echo "Use --list to see available tests" >&2
                exit 1
            fi
            run_module "$OPT_MODULE" "$OPT_TEST" "${OPT_ARGS[@]}"
            EXIT_CODE=$?
            if [[ -n "${OPT_REPORT:-}" ]]; then
                generate_report "$OPT_REPORT"
            fi
            exit $EXIT_CODE
        fi

        # No module specified
        echo "Error: --module required in CLI mode (or use --run-all)" >&2
        echo "Use --list to see available modules" >&2
        exit 1
    fi
}

main "$@"
