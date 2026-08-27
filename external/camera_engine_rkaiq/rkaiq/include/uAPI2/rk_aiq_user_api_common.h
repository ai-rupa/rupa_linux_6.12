

#ifndef __RK_AIQ_USER_API2_COMMON_H__
#define __RK_AIQ_USER_API2_COMMON_H__
#include "common/rk_aiq_comm.h"
//#include "iq_parser_v2/RkAiqCalibDbTypesV2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum opMode_e {
    OP_AUTO   = 0,
    OP_MANUAL = 1,
    OP_SEMI_AUTO = 2,
    OP_REG_MANUAL = 3,
    OP_INVAL
} opMode_t;

/*
*****************************
* Common
*****************************
*/

typedef enum dayNightScene_e {
    DAYNIGHT_SCENE_DAY   = 0,
    DAYNIGHT_SCENE_NIGHT = 1,
    DAYNIGHT_SCENE_INVAL,
} dayNightScene_t;

typedef struct paRange_s {
    float max;
    float min;
} paRange_t;

typedef enum awbRange_e {
    AWB_RANGE_0 = 0,
    AWB_RANGE_1 = 1,
    AWB_RANGE_INVAL,
} awbRange_t;

typedef enum aeMode_e {
    AE_AUTO          = 0,
    AE_IRIS_PRIOR    = 1,
    AE_SHUTTER_PRIOR = 2,
} aeMode_t;


/*
*****************
*   (-1000, -1000)    (1000, -1000)
*   -------------------
*   |                 |
*   |                 |
*   |       (0,0)     |
*   |                 |
*   |                 |
*   -------------------
*                     (1000, 1000)
*****************
*/
typedef struct paRect_s {
    int x;
    int y;
    unsigned int w;
    unsigned int h;
} paRect_t;

typedef enum aeMeasAreaType_e {
    AE_MEAS_AREA_AUTO = 0,
    AE_MEAS_AREA_UP,
    AE_MEAS_AREA_BOTTOM,
    AE_MEAS_AREA_LEFT,
    AE_MEAS_AREA_RIGHT,
    AE_MEAS_AREA_CENTER,
} aeMeasAreaType_t;

typedef enum expPwrLineFreq_e {
    EXP_PWR_LINE_FREQ_DIS   = 0,
    EXP_PWR_LINE_FREQ_50HZ  = 1,
    EXP_PWR_LINE_FREQ_60HZ  = 2,
} expPwrLineFreq_t;

typedef enum antiFlickerMode_e {
    ANTIFLICKER_NORMAL_MODE = 0,
    ANTIFLICKER_AUTO_MODE   = 1,
} antiFlickerMode_t;

typedef struct frameRateInfo_s {
    opMode_t         mode;
    unsigned int     fps; /* valid when manual mode*/
} frameRateInfo_t;

typedef struct AiqIspModuleHwVer_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(AWBStatsVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(AWBSTATS_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(AWBStats hardware version .
        Freq of use: low))  */
    char awbStats[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(AEStatsVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(AESTATS_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(AEStats hardware version .
        Freq of use: low))  */
    char aeStats[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(AFStatsVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(AFSTATS_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(AFStats hardware version .
        Freq of use: low))  */
    char afStats[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(BLCVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(BLC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(BLC hardware version .
        Freq of use: low))  */
    char blc[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(BTNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(BTNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(BTNR hardware version .
        Freq of use: low))  */
    char btnr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(CACVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(CAC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(CAC hardware version .
        Freq of use: low))  */
    char cac[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(CCMVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(CCM_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(CCM hardware version .
        Freq of use: low))  */
    char ccm[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(CSMVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(CSM_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(CSM hardware version .
        Freq of use: low))  */
    char csm[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(CNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(CNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(CNR hardware version .
        Freq of use: low))  */
    char cnr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(DEGMVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(DEGM_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(DEGM hardware version .
        Freq of use: low))  */
    char degm[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(DMVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(DM_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(DM hardware version .
        Freq of use: low))  */
    char dm[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(DPCVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(DPC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(DPC hardware version .
        Freq of use: low))  */
    char dpc[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(DRCVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(DRC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(DRC hardware version .
        Freq of use: low))  */
    char drc[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(ENHANCEVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(ENHANCE_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(ENHANCE hardware version .
        Freq of use: low))  */
    char enhance[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(AIBNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(AIBNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(AIBNR hardware version .
        Freq of use: low))  */
    char aibnr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(AIREMOSAICVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(AIREMOSAIC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(AIREMOSAIC hardware version .
        Freq of use: low))  */
    char airemosaic[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(GAMMAVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(GAMMA_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(GAMMA hardware version .
        Freq of use: low))  */
    char gamma[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(YNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(YNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(YNR hardware version .
        Freq of use: low))  */
    char ynr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(LSCVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(LSC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(LSC hardware version .
        Freq of use: low))  */
    char lsc[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(SHARPVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(SHARP_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(SHARP hardware version .
        Freq of use: low))  */
    char sharp[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(HISTEQVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(HISTEQ_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(HISTEQ hardware version .
        Freq of use: low))  */
    char histeq[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(HDRMGEVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(HDRMGE_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(HDRMGE hardware version .
        Freq of use: low))  */
    char hdrmge[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(HSVVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(HSV_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(HSV hardware version .
        Freq of use: low))  */
    char hsv[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(LDCVVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(LDCV_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(LDCV hardware version .
        Freq of use: low))  */
    char ldcv[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(BNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(BNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(BNR hardware version .
        Freq of use: low))  */
    char bnr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(YTNRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(YTNR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(YTNR hardware version .
        Freq of use: low))  */
    char ytnr[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(LDCHVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(LDCH_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(LDCH hardware version .
        Freq of use: low))  */
    char ldch[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(FPNVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(FPN_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(FPN hardware version .
        Freq of use: low))  */
    char fpn[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(GICVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(GIC_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(GIC hardware version .
        Freq of use: low))  */
    char gic[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(LUT3DVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(LUT3D_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(LUT3D hardware version .
        Freq of use: low))  */
    char lut3d[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(DEHAZEVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(DEHAZE_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(DEHAZE hardware version .
        Freq of use: low))  */
    char dehaze[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(RGBIRVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(RGBIR_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(RGBIR hardware version .
        Freq of use: low))  */
    char rgbir[4];
    /* M4_GENERIC_DESC(
        M4_ALIAS(YUVMEVersion),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,7),
        M4_DEFAULT(YUVME_HW_VERSION),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(YUVME hardware version .
        Freq of use: low))  */
    char yuvme[4];
} AiqIspModuleHwVer_t;

typedef struct CalibDbProductInfo_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(ChipName),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,255),
        M4_DEFAULT(CHIP_NAME),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(.
        Freq of use: low))  */
    char chip_name[8];
    /* M4_GENERIC_DESC(
        M4_ALIAS(Author),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,255),
        M4_DEFAULT(RK),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(.
        Freq of use: low))  */
    char author[16];
    /* M4_GENERIC_DESC(
        M4_ALIAS(Date),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,255),
        M4_DEFAULT(0),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(.
        Freq of use: low))  */
    char date[16];
    /* M4_GENERIC_DESC(
        M4_ALIAS(IQStructMark),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,255),
        M4_DEFAULT(IQ_STRUCT_MARK),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(denote the changes of iq struct, value string pattern is: modify_ver.add_ver.delete_ver .
        Freq of use: low))  */
    char iq_struct_mark[16];
    /* M4_GENERIC_DESC(
        M4_ALIAS(IQVer),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0,255),
        M4_DEFAULT(0),
        M4_DYNAMIC(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(.
        Freq of use: low))  */
    char iq_ver[16];
    // M4_STRUCT_DESC("moduleVer", "normal_ui_style")
    AiqIspModuleHwVer_t moduleVer;
} CalibDbProductInfo_t;

typedef struct rk_aiq_version_info_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(aiq_ver),
        M4_TYPE(string),
        M4_SIZE_EX(1,1),
        M4_DEFAULT(""),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO.
		Freq of use: high))  */
    char aiq_ver[32];
    /* M4_GENERIC_DESC(
        M4_ALIAS(productInfo),
        M4_TYPE(struct),
        M4_SIZE_EX(1,47),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO))   */
    CalibDbProductInfo_t productInfo;
} rk_aiq_version_info_t;

typedef struct rk_aiq_module_ctl_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(type),
        M4_TYPE(enum),
        M4_ENUM_DEF(camAlgoResultType),
        M4_DEFAULT(RESULT_TYPE_INVALID),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(Reference enum types.\n
		Freq of use: high))  */
    camAlgoResultType type;
    /* M4_GENERIC_DESC(
        M4_ALIAS(en),
        M4_TYPE(bool),
        M4_DEFAULT(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO.
		Freq of use: high))  */
    bool en;
    /* M4_GENERIC_DESC(
        M4_ALIAS(bypass),
        M4_TYPE(bool),
        M4_DEFAULT(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO.
		Freq of use: high))  */
    bool bypass;
    /* M4_GENERIC_DESC(
        M4_ALIAS(opMode),
        M4_TYPE(enum),
        M4_ENUM_DEF(rk_aiq_op_mode_t),
        M4_DEFAULT(RK_AIQ_OP_MODE_AUTO),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(The current operation mode))  */
    rk_aiq_op_mode_t opMode;
} rk_aiq_module_ctl_t;

typedef struct rk_aiq_module_list_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(module_ctl),
        M4_TYPE(struct),
        M4_SIZE_EX(1,47),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO))   */
    rk_aiq_module_ctl_t module_ctl[RESULT_TYPE_MAX_PARAM];
} rk_aiq_module_list_t;

typedef struct algo_interp_iso_info_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(type),
        M4_TYPE(enum),
        M4_ENUM_DEF(camAlgoResultType),
        M4_DEFAULT(RESULT_TYPE_INVALID),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(Reference enum types.\n
		Freq of use: high))  */
    camAlgoResultType type;
    /* M4_GENERIC_DESC(
        M4_ALIAS(interpIso),
        M4_TYPE(u32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0, 640000),
        M4_DEFAULT(50),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(original iso.\n
        Freq of use: high))  */
    uint32_t interpIso;
    /* M4_GENERIC_DESC(
        M4_ALIAS(interpIso),
        M4_TYPE(u32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0, 640000),
        M4_DEFAULT(50),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(interpolation iso high.\n
        Freq of use: high))  */
    uint32_t isoH;
    /* M4_GENERIC_DESC(
        M4_ALIAS(interpIso),
        M4_TYPE(u32),
        M4_SIZE_EX(1,1),
        M4_RANGE_EX(0, 640000),
        M4_DEFAULT(50),
        M4_DIGIT_EX(0),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(interpolation iso low.\n
        Freq of use: high))  */
    uint32_t isoL;
} algo_interp_iso_info_t;

typedef struct algo_interp_iso_list_s {
    /* M4_GENERIC_DESC(
        M4_ALIAS(interp_iso),
        M4_TYPE(struct),
        M4_SIZE_EX(1,55),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO))   */
    algo_interp_iso_info_t isoInfo[RESULT_TYPE_MAX_PARAM];
    /* M4_GENERIC_DESC(
        M4_ALIAS(interp_isoCIS),
        M4_TYPE(struct),
        M4_SIZE_EX(1,55),
        M4_UI_MODULE(normal_ui_style),
        M4_HIDE_EX(0),
        M4_RO(0),
        M4_ORDER(0),
        M4_NOTES(TODO))   */
    algo_interp_iso_info_t isoCISInfo[RESULT_TYPE_MAX_PARAM];
} algo_interp_iso_list_t;

typedef enum cSpaceMode_e {
    CSPACE_MODE_BT601_FULL  = 0,
    CSPACE_MODE_BT601_LIMIT = 1,
    CSPACE_MODE_BT709_FULL  = 2,
    CSPACE_MODE_BT709_LIMIT = 3,
    CSPACE_MODE_OTHER_FULL  = 253,
    CSPACE_MODE_OTHER_LIMIT = 254,
    CSPACE_MODE_OTHERS      = 255,
} cSpaceMode_t;

#ifdef __cplusplus
}
#endif

#endif  /*__RK_AIQ_USER_API2_COMMON_H__*/
