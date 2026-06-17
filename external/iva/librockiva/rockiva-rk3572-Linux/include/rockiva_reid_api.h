/****************************************************************************
*
*    Copyright (c) 2022 by Rockchip Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Rockchip Corporation. This is proprietary information owned by
*    Rockchip Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Rockchip Corporation.
*
*****************************************************************************/

#ifndef __ROCKIVA_REID_API_H__
#define __ROCKIVA_REID_API_H__

#include "rockiva_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */

#define ROCKIVA_OBJECT_FEATURE_SIZE_MAX        (4096)                     /* 特征值空间大小 */
#define ROCKIVA_OBJECT_INFO_SIZE_MAX           (32)                       /* 特征入库信息字符串长度(用户填入) */
#define ROCKIVA_OBJECT_ID_MAX_NUM              (100)                      /* 特征比对结果的TOP数目（最大）*/

/* ---------------------------规则配置----------------------------------- */

/* 行人重识别规则设置 */
typedef struct
{
    uint8_t enable;                 /* 使能 1：开启；0：关闭 */
    uint8_t mode;                   /* 重识别模式 0：基于图像的检索；1：基于文本的检索；2：两者都支持（默认） */
    uint32_t intervalTime;  /* 运行时间间隔（单位毫秒） */
} RockIvaPersonReIDRule;

/* 视频结构化初始化参数配置 */
typedef struct
{
    RockIvaPersonReIDRule personReIDRule;      /* 行人重识别规则 */
} RockIvaReIDTaskParams;

/* ------------------------------------------------------------------ */

/* -------------------------- 算法处理结果 --------------------------- */

/* 目标重识别特征信息 */
typedef struct
{
    uint16_t feature[ROCKIVA_OBJECT_FEATURE_SIZE_MAX]; /* 重识别特征 */
    uint32_t size;                                       /* 特征长度 */
    uint16_t version;                              /* 特征版本 */
} RockIvaObjectReidentityFeature;

/* 单个行人目标重识别基本信息 */
typedef struct 
{
    uint32_t objId;                  /* 目标ID[0,2^32) */
    uint64_t frameId;                /* 目标所在帧序号 */
    RockIvaRectangle objectRect;     /* 目标区域原始位置 */
    RockIvaObjectReidentityFeature feature1; /* 重识别特征: 用于基于图像的检索 */
    RockIvaObjectReidentityFeature feature2; /* 重识别特征: 用于基于文本的检索 */
} RockIvaPersonReidentityInfo;

/* 行人重识别结果全部信息 */
typedef struct
{
    uint32_t frameId;                                  /* 输入图像帧ID */
    uint32_t channelId;                                /* 通道号 */
    RockIvaImage frame;                                /* 对应的输入图像帧 */
    uint32_t objNum;                                   /* 目标个数 */
    RockIvaPersonReidentityInfo objectInfo[ROCKIVA_MAX_OBJ_NUM]; /* 重识别信息 */
} RockIvaPersonReidentityResult;

/* 入库特征对应的详细信息，用户输入 */
typedef struct {
    char objectIdInfo[ROCKIVA_OBJECT_INFO_SIZE_MAX];
} RockIvaObjectIdInfo;

/* ---------------------------------------------------------------- */

/**
 * @brief 行人重识别结果回调函数
 *
 * result 结果
 * status 状态码
 * userdata 用户自定义数据
 */
typedef void (*ROCKIVA_REID_PersonReidentityCallback)(const RockIvaPersonReidentityResult* result,
                                                        const RockIvaExecuteStatus status, void* userdata);

typedef struct
{
    ROCKIVA_REID_PersonReidentityCallback personReidentityCallback;
} RockIvaReIDResultCallback;

/**
 * @brief 初始化
 *
 * @param handle [INOUT] 初始化完成的handle
 * @param initParams [IN] 初始化参数
 * @param resultCallback [IN] 回调函数
 * @return RockIvaRetCode
 */
RockIvaRetCode ROCKIVA_REID_Init(RockIvaHandle handle, const RockIvaReIDTaskParams* initParams,
                                   const RockIvaReIDResultCallback callback);

/**
 * @brief 运行时重新配置(重新配置会导致内部的一些记录清空复位，但是模型不会重新初始化)
 *
 * @param handle [IN] handle
 * @param initParams [IN] 配置参数
 * @return RockIvaRetCode
 */
RockIvaRetCode ROCKIVA_REID_Reset(RockIvaHandle handle, const RockIvaReIDTaskParams* initParams);

/**
 * @brief 销毁
 *
 * @param handle [IN] handle
 * @return RockIvaRetCode
 */
RockIvaRetCode ROCKIVA_REID_Release(RockIvaHandle handle);

/**
 * @brief 图像特征提取接口
 * @param handle [IN] handle
 * @param image [IN] 输入图像
 * @param feature [OUT] 输出特征
 */
RockIvaRetCode ROCKIVA_REID_ExtractImageFeature(RockIvaHandle handle, const RockIvaImage* image,
                                   RockIvaObjectReidentityFeature* feature);

/**
 * @brief 文本特征提取接口
 * @param handle [IN] handle
 * @param text [IN] 输入文本
 * @param feature [OUT] 输出特征
 */
RockIvaRetCode ROCKIVA_REID_ExtractTextFeature(RockIvaHandle handle, char* text,
                                   RockIvaObjectReidentityFeature* feature);

/**
 * @brief 1:1目标特征比对接口
 * 
 * @param feature1 [IN] 目标特征1
 * @param feature2 [IN] 目标特征2
 * @param score [OUT] 目标1:1比对相似度(范围0-1.0)
 * @return RockIvaRetCode
 */
RockIvaRetCode ROCKIVA_REID_FeatureCompare(const void* feature1, const void* feature2, float* score);

#ifdef __cplusplus
}
#endif /* end of __cplusplus */

#endif /* end of #ifndef __ROCKIVA_REID_API_H__ */