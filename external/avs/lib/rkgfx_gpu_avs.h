/****************************************************************************
*
*    Copyright (c) 2020 - 2021 by Rockchip Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Rockchip Corporation. This is proprietary information owned by
*    Rockchip Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Rockchip Corporation.
*
*****************************************************************************/

#ifndef RKGFX_GPU_AVS_H
#define RKGFX_GPU_AVS_H

#include <vector>

#define AVS_VER "v6.9b5"

#ifdef __ANDROID__
#define AVS_DEBUG_LOG_GATE "vendor.gpu.avs.log"
#else
#define AVS_DEBUG_LOG_ENV "RK_GPU_AVS_DEBUG_LOG"
#endif

typedef unsigned int uint32_t;

typedef enum rkPSR_FORMAT_E {
    PSR_TYPE_GRAY     = 0,
    PSR_TYPE_RGB      = 1,
    PSR_TYPE_YUV420SP = 2,         // 仅支持NV12格式，其他为预留，未支持
} PSR_FORMAT_E;

typedef enum rkPSR_FUSE_TYPE_E {
    FUSE_MULTI_BAND = 0,                // 多频段融合, 需上传alpha, 当频段数为0时为简单alpha融合
    FUSE_MULTI_BAND_DYNAMIC_ALPHA = 1,  // 动态拼缝多频段融合, 仅支持 FUSE_GPU_ALPHA_U8 格式
} PSR_FUSE_TYPE_E;

typedef enum rkPSR_FUSE_ALPHA_TYPE_E { // 多频段融合使用的alpha数据类型
    FUSE_GPU_ALPHA_FLOAT = 0,          // float
    FUSE_GPU_ALPHA_U8 = 1,             // uchar
} PSR_FUSE_ALPHA_TYPE_E;

typedef enum rkPSR_DA_MODE_E {        //动态拼缝多频段融合可选择的模式
    DA_MODE_ACCURATE = 0,
    DA_MODE_FAST = 1,
    DA_MODE_SLOW_ACCURATE = 2,
    DA_MODE_BUTT
} PSR_DA_MODE_E;

typedef struct rkPSR_BLOCK_GAINS_PARAMS { //亮度矫正使用，预留，未支持
    int input_image_width;
    int input_image_height;
    int input_image_stride;
    int input_image_channel;
    int block_image_width;
    int block_image_height;
    int gain_step_x;
    int gain_step_y;
} PSR_BLOCK_GAINS_PARAMS;

typedef struct rkPSR_OVERLAP_BLOCK {
    int overlap_num;                 // 重叠区域的个数
    std::vector<int> band_num;       // 图像进行多频段融合时分成的频段数
    std::vector<int> dst_startx;     // 通过表校正后输出图的起始位置
    std::vector<int> dst_width;      // 通过表校正后输出图的宽度
    std::vector<int> mesh_width;     // mesh表的宽度
    std::vector<int> mesh_height;    // mesh表的高度
    std::vector<int> alpha_width;    // alpha表的宽度(第0层)
    std::vector<int> alpha_height;   // alpha表的高度(第0层)
} PSR_OVERLAP_BLOCK;

typedef struct rkPSR_NONOVERLAP_BLOCK {
    int nonoverlap_num;              // 非重叠区域的个数
    std::vector<int> dst_startx;     // 通过表校正后输出图的起始位置
    std::vector<int> dst_width;      // 通过表校正后输出图的宽度
    std::vector<int> mesh_width;     // mesh表的宽度
    std::vector<int> mesh_height;    // mesh表的高度
} PSR_NONOVERLAP_BLOCK;

typedef struct PSR_INITPARAMS_S {
    int camera_num;                                     //相机的个数
    int image_stride;                                   //图像的stride
    unsigned int output_image_stride;                   //输出图像的stride
    PSR_FORMAT_E image_formate;                         //图像的格式
    int src_width;                                      //输入图的宽度
    int src_height;                                     //输入图的高度
    int bld_width;                                      //融合后图的宽度
    int bld_height;                                     //融合后图的高度
    int step_x;                                         //生成mesh表时宽度方向的步长
    int step_y;                                         //生成mesh表时高度方向的步长
    std::vector<PSR_NONOVERLAP_BLOCK> nonoverlap_block; //非重叠区域信息
    std::vector<PSR_OVERLAP_BLOCK> overlap_block;       //重叠区域信息
    std::vector<int> image_block_num;                   //每帧图有几个mesh表，即该帧图需要分成几张图进行计算

    //亮度校正所需的配置参数
    uint32_t use_gain;                                  //是否使用gain变换进行亮度校正
    int gain_step_x;                                    //分块亮度校正时x方向的步长
    int gain_step_y;                                    //分块亮度校正时y方向的步长

    uint32_t useSync;
} PSR_INITPARAMS_T;

typedef struct PSR_DRM_BUF_MSG_S{
    void *vir_addr;  //虚拟地址
    int mem_fd;
    int mem_offset;  //虚拟地址的偏移量，需要赋初始值0
    long mem_size;   //GPU拼接内部暂不需要该值
}PSR_DRM_BUF_MSG_T;

typedef struct PSR_REMAP_MESH_S {
    int meshIndex;       // mesh表的索引值
    int meshxStep_;      // 生成mesh表时x方向的步长
    int meshyStep_;      // 生成mesh表时y方向的步长
    int meshWidth_;      // mesh表的宽度
    int meshHeight_;     // mesh表的高度
    void* meshxBufAddr_;  // mesh表x坐标的保存地址
    int meshxFd_;         // mesh表x坐标的fd
    void* meshyBufAddr_;  // mesh表y坐标的保存地址
    int meshyFd_;         // mesh表y坐标的fd
} PSR_REMAP_MESH;

typedef struct RKGFX_AVS_CONTEXT_S {
    uint32_t  apiVersion;
    uint32_t  numView;
    char     *meshFilePath;
    uint32_t  inputBufferWidth;
    uint32_t  inputBufferHeight;
    uint32_t  inputBufferFormat;
    uint32_t  inputIsAFBC;
    uint32_t  outputBufferWidth;
    uint32_t  outputBufferHeight;
    uint32_t  outputBufferFormat;
    uint32_t  outputIsAFBC;
    uint32_t  useFd;
    int      *inputFdArray;
    uint32_t  useSync;
    int       avsFuseType;    //多频段融合类型，定义见: PSR_FUSE_TYPE_E
    int       avsAlphaType;   //融合使用的ALPHA表数据类型，定义见：PSR_FUSE_ALPHA_TYPE_E
    uint32_t  useMeshBufAddress;
    void *AVS_initParams;
    void **avsRemapMeshArray; //存放 PSR_REMAP_MESH 指针的数组指针
    void **meshAlphaDrmBufMsgArray;  //存放 PSR_DRM_BUF_MSG_T 指针的数组指针
} RKGFX_AVS_CONTEXT_T;

typedef enum {
    RKGFX_AVS_SUCCESS,
    RKGFX_AVS_INVALID_PARAM,
    RKGFX_AVS_FAILED,
} RKGFX_AVS_STATUS;

#ifdef __cplusplus
extern "C"
{
#endif

RKGFX_AVS_STATUS RKGFX_AVS_init(void* context);
RKGFX_AVS_STATUS RKGFX_AVS_AllProcess(void* context);
RKGFX_AVS_STATUS RKGFX_AVS_resize(void* context);
RKGFX_AVS_STATUS RKGFX_AVS_update_mesh(void* context, PSR_REMAP_MESH* newMesh);
RKGFX_AVS_STATUS RKGFX_AVS_deinit(void* context);
RKGFX_AVS_STATUS RKGFX_AVS_wait_sync(void* gles_proceess_sync);
void* RKGFX_AVS_create_sync(void);

#ifdef __cplusplus
}
#endif
#endif  // RKGFX_GPU_AVS_H





