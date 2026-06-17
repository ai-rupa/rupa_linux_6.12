/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __RKADK_COMMON_H__
#define __RKADK_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rk_mpi_vi.h"
#include "rkadk_log.h"
#include <pthread.h>
#include <stdbool.h>
#include "rkadk_type.h"

// dump isp process result
//#define RKADK_DUMP_ISP_RESULT

// camera sensor count
#define RKADK_MAX_SENSOR_CNT 8

// simultaneous record files num
#define RECORD_FILE_NUM_MAX 2

#define RKADK_MAX_FILE_PATH_LEN 128
#define RKADK_PIX_FMT_LEN 32
#define RKADK_INTF_FMT_LEN 32
#define RKADK_SPLICE_MODE_LEN 10
#define RKADK_THREAD_NAME_LEN 32

#define JPEG_SLICE_WIDTH_MAX 8192
#define JPEG_SLICE_HEIGHT_MAX 8192

#define RKADK_BUFFER_LEN 64
#define RKADK_PATH_LEN 128
#define RKADK_RC_MODE_LEN 5

typedef char (*ARRAY_FILE_NAME)[RKADK_MAX_FILE_PATH_LEN];

typedef enum {
  RKADK_THUMB_TYPE_NV12 = 0,
  RKADK_THUMB_TYPE_JPEG,
  RKADK_THUMB_TYPE_RGB565,
  RKADK_THUMB_TYPE_RGBA8888,
  RKADK_THUMB_TYPE_BGRA8888,
  RKADK_THUMB_TYPE_BUTT
} RKADK_THUMB_TYPE_E;

typedef struct {
  RKADK_U32 u32X;
  RKADK_U32 u32Y;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
} RKADK_RECT_S;

typedef struct {
  const char *pThumbDir;    /* save thumb file path */
  bool bBuildInThm;         /* whether is built-in thumbnail to the file */
} RKADK_THUMB_PARAM_S;

typedef struct {
  RKADK_THUMB_TYPE_E enType;
  // 4 alignment
  RKADK_U32 u32Width;
  // 2 alignment
  RKADK_U32 u32Height;
  // 4 alignment
  RKADK_U32 u32VirWidth;
  // 2 alignment
  RKADK_U32 u32VirHeight;
  RKADK_U8 *pu8Buf;
  RKADK_U32 u32BufSize;

  // Whether the channel is created and destroyed separately using RKADK_GetThmChnCreate and RKADK_GetThmChnDestory
  RKADK_BOOL bChnExCreate;
  RKADK_S32 s32VdecChn;
  RKADK_S32 s32VpssGrp;
  RKADK_S32 s32VpssChn;

  RKADK_THUMB_PARAM_S *pstThumbParam;       /* only for ts/flv */
} RKADK_THUMB_ATTR_S;

typedef RKADK_THUMB_ATTR_S RKADK_FRAME_ATTR_S;

typedef struct {
  RKADK_U32 u32SrcWidth;  // 4 alignment
  RKADK_U32 u32SrcHeight; // 2 alignment

  RKADK_THUMB_TYPE_E enDstType;
  RKADK_U32 u32DstWidth;  // 4 alignment
  RKADK_U32 u32DstHeight; // 2 alignment

  RKADK_S32 s32VdecChn;
  RKADK_S32 s32VpssGrp;
  RKADK_S32 s32VpssChn;
} RKADK_THUMB_CHN_ATTR_S;

typedef enum {
  RKADK_MIC_TYPE_LEFT = 0,
  RKADK_MIC_TYPE_RIGHT,
  RKADK_MIC_TYPE_BOTH,
  RKADK_MIC_TYPE_BUTT
} RKADK_MIC_TYPE_E;

typedef enum {
  RKADK_CODEC_TYPE_H264 = 0,
  RKADK_CODEC_TYPE_H265,
  RKADK_CODEC_TYPE_MJPEG,
  RKADK_CODEC_TYPE_JPEG,
  RKADK_CODEC_TYPE_G711A,
  RKADK_CODEC_TYPE_G711U,
  RKADK_CODEC_TYPE_G726,
  RKADK_CODEC_TYPE_MP2,
  RKADK_CODEC_TYPE_MP3,
  RKADK_CODEC_TYPE_ACC,
  RKADK_CODEC_TYPE_PCM,
  RKADK_CODEC_TYPE_BUTT
} RKADK_CODEC_TYPE_E;

typedef enum {
  RKADK_VQE_MODE_AI_TALK = 0,
  RKADK_VQE_MODE_AI_RECORD,
  RKADK_VQE_MODE_BUTT
} RKADK_VQE_MODE_E;

typedef enum {
  RKADK_STREAM_TYPE_SENSOR,
  RKADK_STREAM_TYPE_VIDEO_MAIN,
  RKADK_STREAM_TYPE_VIDEO_SUB,
  RKADK_STREAM_TYPE_SNAP,
  RKADK_STREAM_TYPE_PREVIEW,
  RKADK_STREAM_TYPE_LIVE,
  RKADK_STREAM_TYPE_DISP,
  RKADK_STREAM_TYPE_THUMB,
  RKADK_STREAM_TYPE_THIRD_STREAM,
  RKADK_STREAM_TYPE_BUTT
} RKADK_STREAM_TYPE_E;

typedef enum {
  VO_FORMAT_ARGB8888 = 0,
  VO_FORMAT_ABGR8888,
  VO_FORMAT_RGB888,
  VO_FORMAT_BGR888,
  VO_FORMAT_ARGB1555,
  VO_FORMAT_ABGR1555,
  VO_FORMAT_RGB565,
  VO_FORMAT_BGR565,
  VO_FORMAT_RGB444,
  VO_FORMAT_NV12,
  VO_FORMAT_NV21
} RKADK_VO_FORMAT_E;

typedef enum {
  SPLICE_MODE_RGA = 0,
  SPLICE_MODE_GPU,
  SPLICE_MODE_BYPASS
} RKADK_VO_SPLICE_MODE_E;

typedef enum {
  DISPLAY_TYPE_HDMI = 0,
  DISPLAY_TYPE_EDP,
  DISPLAY_TYPE_VGA,
  DISPLAY_TYPE_DP,
  DISPLAY_TYPE_HDMI_EDP,
  DISPLAY_TYPE_MIPI,
  DISPLAY_TYPE_LCD,
  DISPLAY_TYPE_DEFAULT,
} RKADK_VO_INTF_TYPE_E;

typedef enum {
  RKADK_DECODE_MODE_FRAME = 0,
  RKADK_DECODE_MODE_STREAM,
  RKADK_DECODE_MODE_FRAME_SLICE,
  RKADK_DECODE_MODE_SLICE,
  RKADK_DECODE_MODE_COMPAT,
  RKADK_DECODE_MODE_BUTT,
} RKADK_VDEC_DECODE_MODE_E;

typedef struct tagRKADK_PARAM_VENC_ATTR_S {
  RKADK_U32 max_width;
  RKADK_U32 max_height;
  RKADK_U32 width;
  RKADK_U32 height;
  RKADK_U32 bufsize;
  RKADK_U32 bitrate;
  RKADK_U32 framerate;
  RKADK_U32 gop;
  RKADK_U32 profile;
  RKADK_CODEC_TYPE_E codec_type;
  RKADK_U32 venc_chn;
  bool enable_vpss;
  RKADK_U32 vpss_grp;
  RKADK_U32 vpss_chn;
  bool post_aiisp;
  char rc_mode[RKADK_RC_MODE_LEN]; /* options: CBR/VBR/AVBR */
  bool enable_pskip;
} RKADK_PARAM_VENC_ATTR_S;

typedef struct {
  RKADK_U32 index;
  RKADK_U32 u32ViChn;
  VI_CHN_ATTR_S stChnAttr;
} RKADK_PRAAM_VI_ATTR_S;

#ifndef UPALIGNTO
#define UPALIGNTO(value, align) (((value) + (align) - 1) / (align) * (align))
#endif

/* Pointer Check */
#define RKADK_CHECK_POINTER(p, errcode)                                        \
  do {                                                                         \
    if (!(p)) {                                                                \
      RKADK_LOGE("pointer[%s] is NULL", #p);                                   \
      return errcode;                                                          \
    }                                                                          \
  } while (0)

/* Pointer Check */
#define RKADK_CHECK_POINTER_N(p)                                               \
  do {                                                                         \
    if (!(p)) {                                                                \
      RKADK_LOGE("pointer[%s] is NULL", #p);                                   \
      return;                                                                  \
    }                                                                          \
  } while (0)

/* CameraID Check */
#define RKADK_CHECK_CAMERAID(id, errcode)                                      \
  do {                                                                         \
    if ((id) < 0 || (id) >= RKADK_MAX_SENSOR_CNT) {                            \
      RKADK_LOGE("invalid camera id: %d", id);                                 \
      return errcode;                                                          \
    }                                                                          \
  } while (0)

/* Init Check */
#define RKADK_CHECK_INIT(init, errcode)                                        \
  do {                                                                         \
    if (!(init)) {                                                             \
      RKADK_LOGE("[%s] not init", #init);                                      \
      return errcode;                                                          \
    }                                                                          \
  } while (0)

/* Mutex Lock */
#define RKADK_MUTEX_INIT_LOCK(mutex)                                           \
  do {                                                                         \
    (RKADK_VOID) pthread_mutex_init(&mutex, NULL);                             \
  } while (0)
#define RKADK_MUTEX_LOCK(mutex)                                                \
  do {                                                                         \
    (RKADK_VOID) pthread_mutex_lock(&mutex);                                   \
  } while (0)
#define RKADK_MUTEX_UNLOCK(mutex)                                              \
  do {                                                                         \
    (RKADK_VOID) pthread_mutex_unlock(&mutex);                                 \
  } while (0)
#define RKADK_MUTEX_DESTROY(mutex)                                             \
  do {                                                                         \
    (RKADK_VOID) pthread_mutex_destroy(&mutex);                                \
  } while (0)

#define RKADK_CHECK_EQUAL(a, b, mutex, ret)                                    \
  do {                                                                         \
    if (a == b) {                                                              \
      RKADK_MUTEX_UNLOCK(mutex);                                               \
      return ret;                                                              \
    }                                                                          \
  } while (0)

#define RKADK_SWAP16(x)                                                        \
  (((RKADK_U16)(x)&0x00ff) << 8) | (((RKADK_U16)(x)&0xff00) >> 8)

#define RKADK_SWAP32(x)                                                        \
  ((((RKADK_U32)(x) & (0xff000000)) >> 24) |                                   \
   (((RKADK_U32)(x) & (0x00ff0000)) >> 8) |                                    \
   (((RKADK_U32)(x) & (0x0000ff00)) << 8) |                                    \
   (((RKADK_U32)(x) & (0x000000ff)) << 24))

#ifdef __cplusplus
}
#endif
#endif
