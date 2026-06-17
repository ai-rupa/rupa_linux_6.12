#!/bin/bash
# 使用预处理器从RkAiqVersion.h获取正确的版本值

RKAIQ_ROOT=$1
VERSION_HEADER="${RKAIQ_ROOT}/RkAiqVersion.h"

# 从环境变量获取ISP_HW_VERSION
ISP_VER="${ISP_HW_VERSION:- -DISP_HW_V35}"

# 提取-D后的宏名
ISP_MACRO=$(echo "$ISP_VER" | sed 's/-D//')

# 默认值
IQ_VERSION="1.0.0"
CHIP_NAME_VAL="RV1126B"

if [ -f "$VERSION_HEADER" ]; then
    # 方法1：使用gcc -E -dM 获取所有宏定义，然后过滤
    ALL_DEFINES=$(gcc -E -dM "$VERSION_HEADER" $ISP_VER -D${ISP_MACRO} 2>/dev/null)
    
    # 提取IQ_STRUCT_VERSION（排除unknown的情况）
    IQ_VER_LINE=$(echo "$ALL_DEFINES" | grep "define IQ_STRUCT_MARK" | grep -v "unknown" | head -1)
    if [ -n "$IQ_VER_LINE" ]; then
        IQ_VERSION=$(echo "$IQ_VER_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    # 提取CHIP_NAME（排除unknown的情况）
    CHIP_LINE=$(echo "$ALL_DEFINES" | grep "define CHIP_NAME" | grep -v "unknown" | head -1)
    if [ -n "$CHIP_LINE" ]; then
        CHIP_NAME_VAL=$(echo "$CHIP_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
fi

# 直接替换output.h中的版本宏
if [ -f output.h ]; then
    # 替换IQ_STRUCT_MARK
    sed -i "s/IQ_STRUCT_MARK/\"${IQ_VERSION}\"/g" output.h
    # 替换CHIP_NAME
    sed -i "s/CHIP_NAME/\"${CHIP_NAME_VAL}\"/g" output.h
fi

echo "Generated iq_version and updated output.h:"
echo "  IQ_STRUCT_MARK: ${IQ_VERSION}"
echo "  CHIP_NAME: ${CHIP_NAME_VAL}"
