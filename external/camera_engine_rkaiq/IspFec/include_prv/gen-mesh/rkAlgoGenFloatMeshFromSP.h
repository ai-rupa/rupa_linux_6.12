#ifndef __RKALGO_GEN_FLOAT_MESH_FROM_SP_H__
#define __RKALGO_GEN_FLOAT_MESH_FROM_SP_H__

#include "rkAlgoGenMeshComm.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* 用于生成定制化的下采样浮点表的数据结构 */
typedef struct RKALGO_FLOAT_MESH_INFO_SP
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
}RKALGO_FLOAT_MESH_INFO_SP_S;

/* 计算定制化的下采样浮点表的相关参数 */
int32_t calcFloatMeshSizeSP(RKALGO_FLOAT_MESH_INFO_SP_S *pStFloatMeshInfoSp);

/* 生成定制化的下采样浮点表：改变输出宽高 ---> 实现图像缩放(长和宽非等比例缩放，幅型比会发生变化) */
int32_t genFloatMeshForScale(
    const RKALGO_FLOAT_MESH_INFO_SP_S *pStFloatMeshInfoSp,
    float *pf32Mapx, float *pf32Mapy
);

/* 生成定制化的下采样浮点表：图像缩放系数scale ---> 实现图像fov缩放进行补偿(以图像中心为缩放中心，进行等比例缩放) */
int32_t genFloatMeshForFovCompensation(
    RKALGO_FLOAT_MESH_INFO_SP_S *pStFloatMeshInfoSp,
    float scale,
    float *pf32Mapx, float *pf32Mapy
);

/* 生成定制化的下采样浮点表：图像旋转角度angle ---> 图像旋转(以图像中心为旋转中心) */
int32_t genFloatMeshForRotate(
    RKALGO_FLOAT_MESH_INFO_SP_S *pStFloatMeshInfoSp,
    float angle,
    float *mapx, float *mapy
);

/* 保存下采样浮点表 */
int32_t saveFloatMeshSP(
    const char *savePath,
    const RKALGO_FLOAT_MESH_INFO_SP_S *pStFloatMeshInfoSp,
    const float *pf32MeshX,
    const float *pf32MeshY,
    int32_t level
);


#endif // !__RKALGO_GEN_FLOAT_MESH_FROM_SP_H__
