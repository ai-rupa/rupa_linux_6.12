#!/bin/bash
# common/platform.sh - SoC detection and platform utilities

# Guard against double-sourcing
[[ -n "${_ROCKCHIP_PLATFORM_LOADED:-}" ]] && return 0
_ROCKCHIP_PLATFORM_LOADED=1

# Global variables set by detect_soc():
#   SOC_FAMILY    - e.g., "rk3588", "rk3568", "px30"
#   SOC_VARIANT   - e.g., "rk3588s" (when distinguishable)
#   SOC_COMPATIBLE - raw compatible string from device-tree

# detect_soc()
#   Reads /proc/device-tree/compatible, normalizes with tr '\0' ' ',
#   then uses case pattern matching to identify the SoC.
#   Sets SOC_FAMILY, SOC_VARIANT, SOC_COMPATIBLE as global vars.
#   Returns 0 on success, 1 if SoC is unrecognized.
detect_soc() {
    local compatible
    compatible=$(tr '\0' ' ' < /proc/device-tree/compatible 2>/dev/null || echo "")
    SOC_COMPATIBLE="$compatible"

    case "$compatible" in
        *rk3588s*)   SOC_FAMILY="rk3588"; SOC_VARIANT="rk3588s" ;;
        *rk3588*)    SOC_FAMILY="rk3588"; SOC_VARIANT="rk3588" ;;
        *rk3576*)    SOC_FAMILY="rk3576"; SOC_VARIANT="rk3576" ;;
        *rk3572*)    SOC_FAMILY="rk3572"; SOC_VARIANT="rk3572" ;;
        *rk3568*)    SOC_FAMILY="rk3568"; SOC_VARIANT="rk3568" ;;
        *rk3566*)    SOC_FAMILY="rk3566"; SOC_VARIANT="rk3566" ;;
        *rk3562*)    SOC_FAMILY="rk3562"; SOC_VARIANT="rk3562" ;;
        *rk3528*)    SOC_FAMILY="rk3528"; SOC_VARIANT="rk3528" ;;
        *rk3399pro*) SOC_FAMILY="rk3399pro"; SOC_VARIANT="rk3399pro" ;;
        *rk3399*)    SOC_FAMILY="rk3399"; SOC_VARIANT="rk3399" ;;
        *rk3358*)    SOC_FAMILY="rk3358"; SOC_VARIANT="rk3358" ;;
        *rk3328*)    SOC_FAMILY="rk3328"; SOC_VARIANT="rk3328" ;;
        *rk3326*)    SOC_FAMILY="rk3326"; SOC_VARIANT="rk3326" ;;
        *rk3308*)    SOC_FAMILY="rk3308"; SOC_VARIANT="rk3308" ;;
        *rk3288*)    SOC_FAMILY="rk3288"; SOC_VARIANT="rk3288" ;;
        *rk3128*|*rk3126*) SOC_FAMILY="rk312x"; SOC_VARIANT="rk312x" ;;
        *rk3036*)    SOC_FAMILY="rk3036"; SOC_VARIANT="rk3036" ;;
        *rk1808*)    SOC_FAMILY="rk1808"; SOC_VARIANT="rk1808" ;;
        *px3se*)     SOC_FAMILY="px3se"; SOC_VARIANT="px3se" ;;
        *px30*)      SOC_FAMILY="px30"; SOC_VARIANT="px30" ;;
        *rv1126bp*)  SOC_FAMILY="rv1126bp"; SOC_VARIANT="rv1126bp" ;;
        *rv1126b*)   SOC_FAMILY="rv1126b"; SOC_VARIANT="rv1126b" ;;
        *)           SOC_FAMILY="unknown"; SOC_VARIANT="unknown"; return 1 ;;
    esac
    return 0
}

# get_gpu_taskset_cores()
#   Returns the taskset -c argument appropriate for the current SoC.
#   Prints cores string (e.g., "4-7") or empty string if no affinity needed.
get_gpu_taskset_cores() {
    case "${SOC_FAMILY:-unknown}" in
        rk3588|rk3576)     echo "4-7" ;;
        rk3572)            echo "6-7" ;;
        rk3399|rk3399pro)  echo "4-5" ;;
        *)                 echo "" ;;
    esac
}

# has_gpu()
#   Returns 0 if the platform has a Mali GPU (/dev/mali* exists), 1 otherwise.
has_gpu() {
    ls /dev/mali* >/dev/null 2>&1
}

# get_video_sink()
#   Prints "waylandsink" if GPU present, "kmssink" otherwise.
get_video_sink() {
    if has_gpu; then
        echo "waylandsink"
    else
        echo "kmssink"
    fi
}

# Governor management
_GOVERNOR_SAVED=""
_GOVERNOR_RESTORE_TRAP_SET=0

# set_performance_governor()
#   Saves current governor settings and sets all to "performance".
#   Registers EXIT trap to restore automatically.
set_performance_governor() {
    if [[ $_GOVERNOR_RESTORE_TRAP_SET -eq 0 ]]; then
        # Save current governors
        _GOVERNOR_SAVED=""
        local gov_file
        for gov_file in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
            [[ -f "$gov_file" ]] || continue
            _GOVERNOR_SAVED="${_GOVERNOR_SAVED}${gov_file}:$(cat "$gov_file")\n"
        done
        trap _governor_cleanup EXIT
        _GOVERNOR_RESTORE_TRAP_SET=1
    fi
    echo performance | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_governor >/dev/null 2>&1 || true
}

# restore_governor()
#   Restores governors saved by set_performance_governor().
restore_governor() {
    if [[ -n "$_GOVERNOR_SAVED" ]]; then
        local entry gov_file gov_value
        while IFS= read -r entry; do
            [[ -z "$entry" ]] && continue
            gov_file="${entry%%:*}"
            gov_value="${entry#*:}"
            [[ -f "$gov_file" ]] && echo "$gov_value" > "$gov_file" 2>/dev/null || true
        done < <(echo -e "$_GOVERNOR_SAVED")
        _GOVERNOR_SAVED=""
    fi
    _GOVERNOR_RESTORE_TRAP_SET=0
}

# _governor_cleanup()
#   Trap handler for EXIT signal.
_governor_cleanup() {
    restore_governor
}

# resolve_uri(input)
#   Converts a file path to a file:// URI if it's a local path.
#   Sets global RESOLVED_URI.
resolve_uri() {
    local input="$1"
    case "$input" in
        file://*|http://*|https://*|rtsp://*|rtmp://*)
            RESOLVED_URI="$input"
            ;;
        /*)
            RESOLVED_URI="file://${input}"
            ;;
        *)
            RESOLVED_URI="file://$(pwd)/${input}"
            ;;
    esac
}

# Auto-detect SoC on source
detect_soc || true
