#!/bin/bash
# common/common.sh - Main library for rockchip-test framework
# Source this file from any test script to access all common utilities.
#
# Usage:
#   source "$(dirname "$0")/../common/common.sh"
#   source "${ROCKCHIP_TEST_ROOT}/common/common.sh"

# Guard against double-sourcing
[[ -n "${_ROCKCHIP_COMMON_LOADED:-}" ]] && return 0
_ROCKCHIP_COMMON_LOADED=1

# Determine ROCKCHIP_TEST_ROOT (parent of common/)
ROCKCHIP_TEST_ROOT="${ROCKCHIP_TEST_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
export ROCKCHIP_TEST_ROOT

# Source sub-modules in dependency order
source "${ROCKCHIP_TEST_ROOT}/common/platform.sh"
source "${ROCKCHIP_TEST_ROOT}/common/logging.sh"
source "${ROCKCHIP_TEST_ROOT}/common/config.sh"
