#!/bin/bash
# Run this from within a bash shell

export PATH=/data/project_codes/rkaiq-xos-toolchain/llvm/bin:$PATH
export AIQ_BUILD_HOST_DIR=/data/project_codes/rkaiq-xos-toolchain/llvm
export AIQ_BUILD_ARCH=arm

# Auto detect toolchain triple by checking llvm/bin executables
function detect_toolchain_triple() {
    local toolchain_bin_dir="$1"
    local target_arch="$2"

    if [[ ! -d "$toolchain_bin_dir" ]]; then
        echo "Error: Toolchain directory $toolchain_bin_dir does not exist"
        return 1
    fi

    exec_fie=$(ls $toolchain_bin_dir | grep "${target_arch}" | grep -oP '[a-zA-Z][a-zA-Z0-9_-]*-[a-zA-Z0-9_-]+clang(?=\s|$|[^+])')
    exec_fie_path=${toolchain_bin_dir}/${exec_fie}

    if [ ! -f "${exec_fie_path}" ]; then
        return 1
    fi

    if file "$exec_fie_path" | grep -q "shell script\|script"; then
       cat "$exec_fie_path" | grep -oP 'target[ =]\K[a-zA-Z][a-zA-Z0-9_-]*-[a-zA-Z0-9_-]+'
    else
        echo "Error: Cannot find suitable toolchain for $target_arch architecture"
        return 1
    fi
}

# Detect toolchain triple automatically
DETECTED_TRIPLE=$(detect_toolchain_triple "$AIQ_BUILD_HOST_DIR/bin" "$AIQ_BUILD_ARCH")

if [[ -n "$DETECTED_TRIPLE" && "$DETECTED_TRIPLE" != Error* ]]; then
    export AIQ_BUILD_TOOLCHAIN_TRIPLE="$DETECTED_TRIPLE"
    echo "Auto detected toolchain triple: $AIQ_BUILD_TOOLCHAIN_TRIPLE"
else
    echo "Error: Failed to detect toolchain triple ${DETECTED_TRIPLE}"
    exit 1
fi


export AIQ_BUILD_SYSROOT=../sysroot/
TOOLCHAIN_FILE=$(pwd)/../../cmake/toolchains/clang-xos.cmake
OUTPUT=$(pwd)/output/${AIQ_BUILD_ARCH}
SOURCE_PATH=$OUTPUT/../../../../


mkdir -p $OUTPUT
pushd $OUTPUT

cmake -G "Ninja" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DRKAIQ_TARGET_SOC=${RKAIQ_TARGET_SOC} \
    -DARCH=${AIQ_BUILD_ARCH} \
    -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
    -DCMAKE_SKIP_RPATH=TRUE \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
    -DISP_HW_VERSION=${ISP_HW_VERSION} \
    -DCMAKE_INSTALL_PREFIX="installed" \
    -DRKAIQ_BUILD_FOR_XOS=TRUE \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
    $SOURCE_PATH \
&& ninja -j$(nproc)

status_code=$?

popd

exit $status_code

