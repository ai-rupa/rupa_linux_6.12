#ifndef __RKALGO_GEN_FLOAT_MESH_FROM_RK_H__
#define __RKALGO_GEN_FLOAT_MESH_FROM_RK_H__

#include "rkAlgoGenMeshComm.h"
#include "rkAlgoGenFloatMeshComm.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* 联合校正映射表的切块模式 */
typedef enum RKALGO_UNITE_MESH_MODE {
    RKALGO_UNITE_MESH_1X2 = 0,                                /* 按1x2切块, 得到2个子映射表 */
    RKALGO_UNITE_MESH_2X2 = 1,                                /* 按2x2切块, 得到4个子映射表 */
    RKALGO_UNITE_MESH_3X3 = 2,                                /* 按3x3切块, 得到9个子映射表 */
    RKALGO_UNITE_MESH_BUTT,
} RKALGO_UNITE_MESH_MODE_E;

/* 相机参数 */
typedef struct RKALGO_CAM_COEFF_INFO
{
    double cx, cy;                                          /* 镜头的光心 */
    double a0, a2, a3, a4;                                  /* 镜头的畸变系数 */
    double c, d, e;                                         /* 内参[c d;e 1] */
    double sf;                                              /* sf控制视角, sf越大视角越大 */

    int invPolyTanNum0;                                     /* level = 0时的rho-tanTheta多项式的系数个数 */
    double invPolyTanCoeff0[INV_POLY_COEFF_NUM];            /* level = 0时的rho-tanTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */

    int invPolyCotNum0;                                     /* level = 0时的rho-cotTheta多项式的系数个数 */
    double invPolyCotCoeff0[INV_POLY_COEFF_NUM];            /* level = 0时的rho-cotTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */

    int invPolyTanNum255;                                   /* level = 255时的rho-tanTheta多项式的系数个数 */
    double invPolyTanCoeff255[INV_POLY_COEFF_NUM];          /* level = 255时的rho-tanTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */

    int invPolyCotNum255;                                   /* level = 255时的rho-cotTheta多项式的系数个数 */
    double invPolyCotCoeff255[INV_POLY_COEFF_NUM];          /* level = 255时的rho-cotTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */
}RKALGO_CAM_COEFF_INFO_S;

typedef struct RKALGO_CAM_COEFF_ONE_LEVEL_INFO
{
    double cx, cy;                                          /* 镜头的光心 */
    double a0, a2, a3, a4;                                  /* 镜头的畸变系数 */
    double c, d, e;                                         /* 内参[c d;e 1] */
    double sf;                                              /* sf控制视角, sf越大视角越大 */
    uint32_t level;                                         /* 当前畸变校正等级 */
    int invPolyTanNum;                                      /* rho-tanTheta多项式的系数个数 */
    double invPolyTanCoeff[INV_POLY_COEFF_NUM];             /* level = 0时的rho-tanTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */
    int invPolyCotNum;                                      /* rho-cotTheta多项式的系数个数 */
    double invPolyCotCoeff[INV_POLY_COEFF_NUM];             /* level = 0时的rho-cotTheta多项式系数, 最高次数(INV_POLY_COEFF_NUM-1)次 */
}RKALGO_CAM_COEFF_ONE_LEVEL_INFO_S;

typedef struct RKALGO_FLOAT_MESH_INFO_RK
{
    uint32_t srcW;                                          /* 输入图像宽 */
    uint32_t srcH;                                          /* 输入图像高 */
    uint32_t dstW;                                          /* 输出图像宽 */
    uint32_t dstH;                                          /* 输出图像高 */
    RKALGO_MESH_TYPE_E enMeshType;                          /* 硬件类型：FEC/LDCH/LDCV/LDCH+LDCV/debug */
    RKALGO_FEC_VERSION_E enFecVersion;                      /* 硬件FEC版本，例如rv1109, rv1126, rk3588, rv1126b */
    RKALGO_LDCH_VERSION_E enLdchVersion;                    /* 硬件LDCH版本(后级没有LDCV)，例如rv1109, rv1126, rk356x, rk3588, rv1106, rk3562 */
    RKALGO_LDCH_LDCV_VERSION_E enLdchLdcvVersion;           /* 硬件LDCH + LDCV版本，例如rk3576 */
    RKALGO_MESH_STEP_OPT_E enMeshStepOpt;                   /* 浮点映射表的采样步长选项 */

    uint32_t dstWidAlign;                                   /* 输出图像宽对齐 */
    uint32_t dstHgtAlign;                                   /* 输出图像高对齐 */
    uint32_t meshStepW;                                     /* 浮点映射表的宽步长 */
    uint32_t meshStepH;                                     /* 浮点映射表的高步长 */
    uint32_t meshW;                                         /* 浮点映射表的宽 */
    uint32_t meshH;                                         /* 浮点映射表的高 */
    uint64_t u64FloatMeshBufSize;                           /* 单个浮点映射表的buffer大小 */
    uint32_t maxLevel;                                      /* 硬件最大可以校正的畸变等级 */
    uint32_t correctX;                                      /* 水平x方向校正: 1代表校正, 0代表不校正 */
    uint32_t correctY;                                      /* 垂直y方向校正: 1代表校正, 0代表不校正 */
    uint32_t saveMaxFovX;                                   /* 保留水平x方向最大FOV: 1代表保留, 0代表不保留 */

    double cropStepW[2000], cropStepH[2000];
    double cropStartW[2000], cropStartH[2000];
}RKALGO_FLOAT_MESH_INFO_RK_S;

/* 生成联合校正映射表的相关参数 */
typedef struct RKALGO_UNITE_MESH_INFO {
    /* 输入 */
    uint32_t bigWid;                                        /* 原始输出分辨率表的宽高 */
    uint32_t bigHgt;
    uint32_t marginW;                                       /* 横向外扩预留大小, 单位: 像素 */
    uint32_t marginH;                                       /* 纵向外扩预留大小, 单位: 像素 */
    RKALGO_UNITE_MESH_MODE_E enUniteMeshMode;               /* 联合校正映射表的切块模式: 1x2, 2x2, 3x3等 */
    RKALGO_MESH_TYPE_E enMeshType;                          /* 硬件类型：FEC/LDCH/LDCV/LDCH+LDCV/debug */
    RKALGO_FEC_VERSION_E enFecVersion;                      /* 硬件FEC版本，例如rv1109, rv1126, rk3588, rv1126b */
    RKALGO_LDCH_VERSION_E enLdchVersion;                    /* 硬件LDCH版本(后级没有LDCV)，例如rv1109, rv1126, rk356x, rk3588, rv1106, rk3562 */
    RKALGO_LDCH_LDCV_VERSION_E enLdchLdcvVersion;           /* 硬件LDCH + LDCV版本，例如rk3576 */
    RKALGO_MESH_STEP_OPT_E enMeshStepOpt;                   /* 浮点映射表的采样步长选项 */
    uint32_t correctX;                                      /* 水平x方向校正: 1代表校正, 0代表不校正 */
    uint32_t correctY;                                      /* 垂直y方向校正: 1代表校正, 0代表不校正 */
    uint32_t saveMaxFovX;                                   /* 保留水平x方向最大FOV: 1代表保留, 0代表不保留 */

    /* 输出 */
    uint32_t blockNum;                                      /* 切块数量 */
    uint32_t blockWid[9];                                   /* 每个切块的宽高 */
    uint32_t blockHgt[9];
    uint32_t srcStartX[9];                                  /* 输入图在全图坐标系下的范围 */
    uint32_t srcStartY[9];
    uint32_t srcEndX[9];
    uint32_t srcEndY[9];
    uint32_t dstStartX[9];                                  /* 输出图在自身坐标系下的有效范围, 用于后续拼成全图 */
    uint32_t dstStartY[9];
    uint32_t dstEndX[9];
    uint32_t dstEndY[9];
    uint32_t canvasStartX[9];                               /* 输出图在全图坐标系下的范围 */
    uint32_t canvasStartY[9];
    uint32_t canvasEndX[9];
    uint32_t canvasEndY[9];
    double cx[9];                                           /* 镜头的光心 */
    double cy[9];
    double a0[9];                                           /* 镜头的畸变系数 */
    double a2[9];
    double a3[9];
    double a4[9];

    RKALGO_CAM_COEFF_INFO_S blockCamCoeff[9];
    RKALGO_FLOAT_MESH_INFO_RK_S blockFloatMeshInfoRk[9];
}RKALGO_UNITE_MESH_INFO_S;


/* 外部接口：计算RK参数模型下的浮点映射表的宽高 */
int32_t calcFloatMeshSizeRK(RKALGO_FLOAT_MESH_INFO_RK_S *pStFloatMeshInfoRk);

/* 外部接口：生成下采样浮点表之前的相关参数初始化 */
int32_t genFloatMeshNLevelInit(
    RKALGO_CAM_COEFF_INFO_S *pCamCoeff,
    RKALGO_FLOAT_MESH_INFO_RK_S *genFloatMeshInfo
);

/* 外部接口：生成不同校正程度的下采样浮点表，采样步长可配 */
int32_t genFloatMeshNLevel(
    const RKALGO_CAM_COEFF_INFO_S *pCamCoeff,
    const RKALGO_FLOAT_MESH_INFO_RK_S *genFloatMeshInfo,
    uint32_t level,
    float *pf32MapxOri, float *pf32MapyOri
);

/* 外部接口：保存下采样浮点表 */
int32_t saveFloatMeshRK(
    const char *savePath,
    const RKALGO_FLOAT_MESH_INFO_RK_S *pStFloatMeshInfoRk,
    const float *pf32MeshX,
    const float *pf32MeshY,
    int32_t level
);

/* 外部接口-联合校正：生成联合校正下采样浮点表之前的相关参数初始化 */
int32_t genUniteFloatMeshNLevelInit(
    const RKALGO_CAM_COEFF_INFO_S *pStCamCoeff,             /* 输入: 原始RK模型参数 */
    RKALGO_UNITE_MESH_INFO_S *pStUniteMeshInfo              /* 输入/输出: 联合校正配置参数和联合校正RK模型参数 */
);




#endif // !__RKALGO_GEN_FLOAT_MESH_FROM_RK_H__
