#!/bin/bash
# pcie/pcie_function_test.sh - PCIe device enumeration and performance test

DIR_PCIE="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_PCIE}/../common/common.sh"

DEVICE_ID="${1:-21:00.0}"
NVME_DEV="${2:-/dev/nvme0n1}"

TEST_ID=$(test_begin "pcie" "function" "PCIe function test")

log_info "pcie" "Listing all PCI devices:"
lspci -vvv 2>&1 | tee -a "$(get_log_path pcie)"

log_info "pcie" "Reading Vendor/Device ID of $DEVICE_ID"
VENDOR_DEVICE=$(setpci -s "$DEVICE_ID" 0x00.L 2>&1)
log_info "pcie" "Vendor/Device: $VENDOR_DEVICE"

if [[ -e "$NVME_DEV" ]]; then
    log_info "pcie" "Testing read/write performance of $NVME_DEV"
    WRITE_OUTPUT=$(dd if=/dev/zero of="$NVME_DEV" bs=1M count=1024 conv=fdatasync oflag=direct 2>&1)
    READ_OUTPUT=$(dd if="$NVME_DEV" of=/dev/null bs=1M count=1024 iflag=direct 2>&1)
    log_info "pcie" "Write: $(echo "$WRITE_OUTPUT" | grep -e "copied" -e "bytes")"
    log_info "pcie" "Read: $(echo "$READ_OUTPUT" | grep -e "copied" -e "bytes")"
else
    log_warn "pcie" "NVMe device $NVME_DEV not found, skipping performance test"
fi

test_end "$TEST_ID" "PASS"
