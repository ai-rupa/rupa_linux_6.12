#!/bin/bash
# 使用预处理器从RkAiqVersion.h获取正确的版本值

RKAIQ_ROOT=$1
VERSION_HEADER="${RKAIQ_ROOT}/RkAiqVersion.h"
MODULE_HEADER="${RKAIQ_ROOT}/RkIspModuleHwVersion.h"

# 从环境变量获取ISP_HW_VERSION
ISP_VER="${ISP_HW_VERSION:- -DISP_HW_V35}"

# 提取-D后的宏名
ISP_MACRO=$(echo "$ISP_VER" | sed 's/-D//')

# 默认值
IQ_VERSION="1.0.0"
CHIP_NAME_VAL="RV1126B"

# 模块版本默认值
ISP_HW_VERSION_VAL="0.0"
AWB_HW_VERSION_VAL="0.0"
AE_HW_VERSION_VAL="0.0"
AF_HW_VERSION_VAL="0.0"
BLC_HW_VERSION_VAL="0.0"
BTNR_HW_VERSION_VAL="0.0"
BNR_HW_VERSION_VAL="0.0"
CAC_HW_VERSION_VAL="0.0"
CCM_HW_VERSION_VAL="0.0"
CSM_HW_VERSION_VAL="0.0"
CNR_HW_VERSION_VAL="0.0"
DEGM_HW_VERSION_VAL="0.0"
DM_HW_VERSION_VAL="0.0"
DPC_HW_VERSION_VAL="0.0"
DRC_HW_VERSION_VAL="0.0"
ENHANCE_HW_VERSION_VAL="0.0"
AIBNR_HW_VERSION_VAL="0.0"
AIREMOSAIC_HW_VERSION_VAL="0.0"
LSC_HW_VERSION_VAL="0.0"
GAMMA_HW_VERSION_VAL="0.0"
SHARPNESS_HW_VERSION_VAL="0.0"
DEHAZE_HW_VERSION_VAL="0.0"
YNR_HW_VERSION_VAL="0.0"
YTNR_HW_VERSION_VAL="0.0"
HISTEQ_HW_VERSION_VAL="0.0"
HDRMGE_HW_VERSION_VAL="0.0"
HSV_HW_VERSION_VAL="0.0"
LDCV_HW_VERSION_VAL="0.0"
LDCH_HW_VERSION_VAL="0.0"
FPN_HW_VERSION_VAL="0.0"
GIC_HW_VERSION_VAL="0.0"
LUT3D_HW_VERSION_VAL="0.0"
DEHAZE_HW_VERSION_VAL="0.0"
RGBIR_HW_VERSION_VAL="0.0"
YUVME_HW_VERSION_VAL="0.0"

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

if [ -f "$MODULE_HEADER" ]; then
    # 使用gcc -E -dM 获取所有宏定义，然后过滤
    MODULE_DEFINES=$(gcc -E -dM "$MODULE_HEADER" $ISP_VER -D${ISP_MACRO} 2>/dev/null)
    
    # 提取模块版本信息
    AWB_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define AWBSTATS_HW_VERSION" | head -1)
    if [ -n "$AWB_HW_LINE" ]; then
        AWB_HW_VERSION_VAL=$(echo "$AWB_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    AE_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define AESTATS_HW_VERSION" | head -1)
    if [ -n "$AE_HW_LINE" ]; then
        AE_HW_VERSION_VAL=$(echo "$AE_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    AF_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define AFSTATS_HW_VERSION" | head -1)
    if [ -n "$AF_HW_LINE" ]; then
        AF_HW_VERSION_VAL=$(echo "$AF_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    BLC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define BLC_HW_VERSION" | head -1)
    if [ -n "$BLC_HW_LINE" ]; then
        BLC_HW_VERSION_VAL=$(echo "$BLC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    BTNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define BTNR_HW_VERSION" | head -1)
    if [ -n "$BTNR_HW_LINE" ]; then
        BTNR_HW_VERSION_VAL=$(echo "$BTNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    CAC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define CAC_HW_VERSION" | head -1)
    if [ -n "$CAC_HW_LINE" ]; then
        CAC_HW_VERSION_VAL=$(echo "$CAC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    CCM_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define CCM_HW_VERSION" | head -1)
    if [ -n "$CCM_HW_LINE" ]; then
        CCM_HW_VERSION_VAL=$(echo "$CCM_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    CSM_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define CSM_HW_VERSION" | head -1)
    if [ -n "$CSM_HW_LINE" ]; then
        CSM_HW_VERSION_VAL=$(echo "$CSM_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    CNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define CNR_HW_VERSION" | head -1)
    if [ -n "$CNR_HW_LINE" ]; then
        CNR_HW_VERSION_VAL=$(echo "$CNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DEGM_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DEGM_HW_VERSION" | head -1)
    if [ -n "$DEGM_HW_LINE" ]; then
        DEGM_HW_VERSION_VAL=$(echo "$DEGM_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DM_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DM_HW_VERSION" | head -1)
    if [ -n "$DM_HW_LINE" ]; then
        DM_HW_VERSION_VAL=$(echo "$DM_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DPC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DPC_HW_VERSION" | head -1)
    if [ -n "$DPC_HW_LINE" ]; then
        DPC_HW_VERSION_VAL=$(echo "$DPC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DRC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DRC_HW_VERSION" | head -1)
    if [ -n "$DRC_HW_LINE" ]; then
        DRC_HW_VERSION_VAL=$(echo "$DRC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    ENHANCE_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define ENHANCE_HW_VERSION" | head -1)
    if [ -n "$ENHANCE_HW_LINE" ]; then
        ENHANCE_HW_VERSION_VAL=$(echo "$ENHANCE_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    AIBNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define AIBNR_HW_VERSION" | head -1)
    if [ -n "$AIBNR_HW_LINE" ]; then
        AIBNR_HW_VERSION_VAL=$(echo "$AIBNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    AIREMOSAIC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define AIREMOSAIC_HW_VERSION" | head -1)
    if [ -n "$AIREMOSAIC_HW_LINE" ]; then
        AIREMOSAIC_HW_VERSION_VAL=$(echo "$AIREMOSAIC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    LSC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define LSC_HW_VERSION" | head -1)
    if [ -n "$LSC_HW_LINE" ]; then
        LSC_HW_VERSION_VAL=$(echo "$LSC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    GAMMA_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define GAMMA_HW_VERSION" | head -1)
    if [ -n "$GAMMA_HW_LINE" ]; then
        GAMMA_HW_VERSION_VAL=$(echo "$GAMMA_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    CCM_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define CCM_HW_VERSION" | head -1)
    if [ -n "$CCM_HW_LINE" ]; then
        CCM_HW_VERSION_VAL=$(echo "$CCM_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    SATURATION_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define SATURATION_HW_VERSION" | head -1)
    if [ -n "$SATURATION_HW_LINE" ]; then
        SATURATION_HW_VERSION_VAL=$(echo "$SATURATION_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    SHARPNESS_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define SHARP_HW_VERSION" | head -1)
    if [ -n "$SHARPNESS_HW_LINE" ]; then
        SHARPNESS_HW_VERSION_VAL=$(echo "$SHARPNESS_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DEHAZE_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DEHAZE_HW_VERSION" | head -1)
    if [ -n "$DEHAZE_HW_LINE" ]; then
        DEHAZE_HW_VERSION_VAL=$(echo "$DEHAZE_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    YNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define YNR_HW_VERSION" | head -1)
    if [ -n "$YNR_HW_LINE" ]; then
        YNR_HW_VERSION_VAL=$(echo "$YNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    HISTEQ_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define HISTEQ_HW_VERSION" | head -1)
    if [ -n "$HISTEQ_HW_LINE" ]; then
        HISTEQ_HW_VERSION_VAL=$(echo "$HISTEQ_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    HDRMGE_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define HDRMGE_HW_VERSION" | head -1)
    if [ -n "$HDRMGE_HW_LINE" ]; then
        HDRMGE_HW_VERSION_VAL=$(echo "$HDRMGE_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    HSV_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define HSV_HW_VERSION" | head -1)
    if [ -n "$HSV_HW_LINE" ]; then
        HSV_HW_VERSION_VAL=$(echo "$HSV_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    LDCV_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define LDCV_HW_VERSION" | head -1)
    if [ -n "$LDCV_HW_LINE" ]; then
        LDCV_HW_VERSION_VAL=$(echo "$LDCV_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    BNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define BNR_HW_VERSION" | head -1)
    if [ -n "$BNR_HW_LINE" ]; then
        BNR_HW_VERSION_VAL=$(echo "$BNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    YTNR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define YTNR_HW_VERSION" | head -1)
    if [ -n "$YTNR_HW_LINE" ]; then
        YTNR_HW_VERSION_VAL=$(echo "$YTNR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    LDCH_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define LDCH_HW_VERSION" | head -1)
    if [ -n "$LDCH_HW_LINE" ]; then
        LDCH_HW_VERSION_VAL=$(echo "$LDCH_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    FPN_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define FPN_HW_VERSION" | head -1)
    if [ -n "$FPN_HW_LINE" ]; then
        FPN_HW_VERSION_VAL=$(echo "$FPN_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    GIC_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define GIC_HW_VERSION" | head -1)
    if [ -n "$GIC_HW_LINE" ]; then
        GIC_HW_VERSION_VAL=$(echo "$GIC_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    LUT3D_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define LUT3D_HW_VERSION" | head -1)
    if [ -n "$LUT3D_HW_LINE" ]; then
        LUT3D_HW_VERSION_VAL=$(echo "$LUT3D_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    DEHAZE_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define DEHAZE_HW_VERSION" | head -1)
    if [ -n "$DEHAZE_HW_LINE" ]; then
        DEHAZE_HW_VERSION_VAL=$(echo "$DEHAZE_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    RGBIR_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define RGBIR_HW_VERSION" | head -1)
    if [ -n "$RGBIR_HW_LINE" ]; then
        RGBIR_HW_VERSION_VAL=$(echo "$RGBIR_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
    
    YUVME_HW_LINE=$(echo "$MODULE_DEFINES" | grep "define YUVME_HW_VERSION" | head -1)
    if [ -n "$YUVME_HW_LINE" ]; then
        YUVME_HW_VERSION_VAL=$(echo "$YUVME_HW_LINE" | sed 's/.*"\([^"]*\)".*/\1/')
    fi
fi

# 直接替换output.h中的版本宏
if [ -f output.h ]; then
    # 替换IQ_STRUCT_MARK
    sed -i "s/IQ_STRUCT_MARK/\"${IQ_VERSION}\"/g" output.h
    # 替换CHIP_NAME
    sed -i "s/CHIP_NAME/\"${CHIP_NAME_VAL}\"/g" output.h
    # 替换模块版本信息
    sed -i "s/ISP_HW_VERSION/\"${ISP_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/AWBSTATS_HW_VERSION/\"${AWB_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/AESTATS_HW_VERSION/\"${AE_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/AFSTATS_HW_VERSION/\"${AF_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/BLC_HW_VERSION/\"${BLC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/CNR_HW_VERSION/\"${CNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DEGM_HW_VERSION/\"${DEGM_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DM_HW_VERSION/\"${DM_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DPC_HW_VERSION/\"${DPC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DRC_HW_VERSION/\"${DRC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/ENHANCE_HW_VERSION/\"${ENHANCE_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/AIBNR_HW_VERSION/\"${AIBNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/AIREMOSAIC_HW_VERSION/\"${AIREMOSAIC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/BTNR_HW_VERSION/\"${BTNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/CAC_HW_VERSION/\"${CAC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/CCM_HW_VERSION/\"${CCM_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/CSM_HW_VERSION/\"${CSM_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/LSC_HW_VERSION/\"${LSC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/GAMMA_HW_VERSION/\"${GAMMA_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/SHARP_HW_VERSION/\"${SHARPNESS_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DEHAZE_HW_VERSION/\"${DEHAZE_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/YNR_HW_VERSION/\"${YNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/HISTEQ_HW_VERSION/\"${HISTEQ_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/HDRMGE_HW_VERSION/\"${HDRMGE_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/HSV_HW_VERSION/\"${HSV_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/LDCV_HW_VERSION/\"${LDCV_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/BNR_HW_VERSION/\"${BNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/YTNR_HW_VERSION/\"${YTNR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/LDCH_HW_VERSION/\"${LDCH_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/FPN_HW_VERSION/\"${FPN_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/GIC_HW_VERSION/\"${GIC_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/LUT3D_HW_VERSION/\"${LUT3D_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/DEHAZE_HW_VERSION/\"${DEHAZE_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/RGBIR_HW_VERSION/\"${RGBIR_HW_VERSION_VAL}\"/g" output.h
    sed -i "s/YUVME_HW_VERSION/\"${YUVME_HW_VERSION_VAL}\"/g" output.h
fi

echo "Generated iq_version and updated output.h done"
#echo "  IQ_STRUCT_MARK: ${IQ_VERSION}"
#echo "  CHIP_NAME: ${CHIP_NAME_VAL}"
#echo "  Module versions:"
#echo "    ISP: ${ISP_HW_VERSION_VAL}"
#echo "    AWB: ${AWB_HW_VERSION_VAL}"
#echo "    AE: ${AE_HW_VERSION_VAL}"
#echo "    AF: ${AF_HW_VERSION_VAL}"
#echo "    BLC: ${BLC_HW_VERSION_VAL}"
#echo "    BTNR: ${BTNR_HW_VERSION_VAL}"
#echo "    BNR: ${BNR_HW_VERSION_VAL}"
#echo "    CNR: ${CNR_HW_VERSION_VAL}"
#echo "    DM: ${DM_HW_VERSION_VAL}"
#echo "    DPC: ${DPC_HW_VERSION_VAL}"
#echo "    DRC: ${DRC_HW_VERSION_VAL}"
#echo "    ENHANCE: ${ENHANCE_HW_VERSION_VAL}"
#echo "    AIBNR: ${AIBNR_HW_VERSION_VAL}"
#echo "    LSC: ${LSC_HW_VERSION_VAL}"
#echo "    GAMMA: ${GAMMA_HW_VERSION_VAL}"
#echo "    SHARP: ${SHARPNESS_HW_VERSION_VAL}"
#echo "    DEHAZE: ${DEHAZE_HW_VERSION_VAL}"
#echo "    YNR: ${YNR_HW_VERSION_VAL}"
#echo "    YTNR: ${YTNR_HW_VERSION_VAL}"
#echo "    HISTEQ: ${HISTEQ_HW_VERSION_VAL}"
#echo "    HDRMGE: ${HDRMGE_HW_VERSION_VAL}"
#echo "    HSV: ${HSV_HW_VERSION_VAL}"
#echo "    LDCV: ${LDCV_HW_VERSION_VAL}"
#echo "    LDCH: ${LDCH_HW_VERSION_VAL}"
#echo "    FPN: ${FPN_HW_VERSION_VAL}"
#echo "    GIC: ${GIC_HW_VERSION_VAL}"
#echo "    LUT3D: ${LUT3D_HW_VERSION_VAL}"
#echo "    RGBIR: ${RGBIR_HW_VERSION_VAL}"
#echo "    YUVME: ${YUVME_HW_VERSION_VAL}"
