/****************************************************************************
 *
 *    Copyright (c) 2023 by Rockchip Corp.  All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Rockchip Corporation. This is proprietary information owned by
 *    Rockchip Corporation. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Rockchip Corporation.
 *
 *****************************************************************************/
#pragma once

#include <stdint.h>

#define RK_GPU_REMAP_VERSION "V1.1.6"

#ifdef __ANDROID__
#define REMAP_DEBUG_LOG_GATE "vendor.gpu.remap.log"
#else
#define REMAP_DEBUG_LOG_ENV "RK_GPU_REMAP_DEBUG_LOG"
#endif

typedef enum RK_GPU_REMAP_FORMAT_E {
    FORMAT_GRAY      = 0,
    FORMAT_RGB888    = 1,
    FORMAT_NV12      = 2, // 仅支持NV12和NV12_AFBC格式，其他为预留，未支持
    FORMAT_NV12_AFBC = 3,
} GPU_REMAP_FORMAT;

typedef struct RK_GPU_REMAP_IMG_RECT_S {
    int iLeft_;   /* 矩形区域 left点坐标 */
    int iTop_;    /* 矩形区域 top点坐标 */
    int iRight_;  /* 矩形区域 right点坐标 */
    int iBottom_; /* 矩形区域 bottom点坐标 */
} GPU_REMAP_IMG_RECT;

typedef struct RK_GPU_REMAP_IMAGE_S {
    int iFd_;           /* 图像缓存fd，指向具体的图像内存，通常为 dma-buffer fd */
    void *mVirAddr_;    /* 虚拟地址 */
    int iWidth_;        /* 描述图像宽度，单位为 pixel */
    int iHeight_;       /* 描述图像高度，单位为 pixel */
    int iWidthStride_;  /* 描述图像宽 stride，单位为 pixel */
    int iHeightStride_; /* 描述图像高 stride，单位为 pixel */
    int iByteStride_;   /* 描述图像行长度 stride，单位为 byte */
    int iSize_;         /* 描述图像完整尺寸，单位为 byte */
    GPU_REMAP_IMG_RECT mRect_; /* 描述图像有效的矩形区域 */
    GPU_REMAP_FORMAT mFormat_; /* 描述图像格式*/
} GPU_REMAP_IMAGE;

typedef struct RK_GPU_REMAP_MESH_S {
    int meshIndex;       // mesh表的索引值
    int meshxStep_;      // 生成mesh表时x方向的步长
    int meshyStep_;      // 生成mesh表时y方向的步长
    int meshWidth_;      // mesh表的宽度
    int meshHeight_;     // mesh表的高度
    void* meshxBufAddr_;  // mesh表x坐标的保存地址
    int meshxFd_;         // mesh表x坐标的fd
    void* meshyBufAddr_;  // mesh表y坐标的保存地址
    int meshyFd_;         // mesh表y坐标的fd
} GPU_REMAP_MESH;

typedef struct RK_GPU_REMAP_TASK_S {
    int useMeshIndex;          // 使用第几个mesh表
    GPU_REMAP_IMAGE* srcImage; // 输入图像
    GPU_REMAP_IMAGE* dstImage; // 输出图像
    bool ifRunAsync;           // 是否需要异步执行操作
} REMAP_TASK;

typedef struct RK_GPU_REMAP_CONTEXT_S {
    int contextIndex;                 // context的索引值
    GPU_REMAP_IMAGE* srcImage;        // 输入图像的信息
    GPU_REMAP_IMAGE* dstImage;        // 输出图像的信息
    int initMeshAmount;               // Remap所需mesh表的总数
    GPU_REMAP_MESH** initMeshes;      // 保存Remap所需的所有mesh表的指针
} REMAP_CONTEXT;

typedef enum RK_GPU_REMAP_STATUS_E{
    GPU_REMAP_SUCCESS,
    GPU_REMAP_INVALID_PARAM,
    GPU_REMAP_FAILED,
} REMAP_STATUS;

#ifdef __cplusplus
extern "C"
{
#endif

REMAP_STATUS rkGpuRemapInit(REMAP_CONTEXT* context);
REMAP_STATUS rkGpuRemapRun(REMAP_CONTEXT& context, REMAP_TASK* remapTask);
REMAP_STATUS rkGpuRemapUpdateMesh(REMAP_CONTEXT& context, GPU_REMAP_MESH* newMesh);
void* rkGpuRemapCreateFence(REMAP_CONTEXT& context);
REMAP_STATUS rkGpuRemapWaitFence(REMAP_CONTEXT& context, void* remapFence);
REMAP_STATUS rkGpuRemapDeInit(REMAP_CONTEXT& context);

#ifdef __cplusplus
}
#endif
