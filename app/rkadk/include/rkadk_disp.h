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

#ifndef __RKADK_DISP_H__
#define __RKADK_DISP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rkadk_common.h"
#include "rk_mpi_vdec.h"

#define RKADK_DISP_SUB_STREAM_NUM 2

typedef RKADK_S32 (*RKADK_DISP_MB_FREE_CB)(void *);

typedef struct {
  bool bEofFlag;
  char *data;
  RKADK_U32 u32Len;
  RKADK_U64 u64Pts;
  RKADK_U32 u32Seq;
  bool bBypass;             /* if bypass, must set pFreeCB; */
  RKADK_DISP_MB_FREE_CB pFreeCB;
} RKADK_DISP_DATA_S;

typedef struct {
  RKADK_U32 u32SrcWidth;
  RKADK_U32 u32SrcHeight;
  RKADK_CODEC_TYPE_E enCodecType; // RKADK_CODEC_TYPE_BUTT is not change
} RKADK_DISP_VDEC_ATTR_S;

typedef struct {
  RKADK_RECT_S stVpssCropRect;
  RKADK_RECT_S stVoRect;
  RKADK_S32 s32Priority;  /* Overlay priority, higher value, higher priority, -1 is not change */
  RKADK_DISP_VDEC_ATTR_S stVdecAttr;
} RKADK_DISP_STREAM_ATTR_S;

typedef struct {
  RKADK_RECT_S stVpssCropRect;
  RKADK_RECT_S stVoRect;
  RKADK_S32 s32Priority;  /* Overlay priority, higher value, higher priority, -1 is not change */
  RKADK_DISP_STREAM_ATTR_S stSubStreamAttr[RKADK_DISP_SUB_STREAM_NUM];
} RKADK_DISP_ATTR_S;

typedef struct {
  RKADK_U32 u32VoChn;
  RKADK_RECT_S stVoRect;
  RKADK_U32 rotation;     /* Range:[0,3], 0:0, 1:90, 2:180, 3:270 */
} RKADK_DISP_VO_CFG_S;

typedef struct {
  RKADK_U32 u32VdecChn;
  RKADK_U32 u32SrcWidth;
  RKADK_U32 u32SrcHeight;
  RKADK_CODEC_TYPE_E enCodecType;
  RKADK_U32 u32StreamBufCnt;                /* stream(input) buffer cnt, default: 8 */
  RKADK_U32 u32FrameBufCnt;                 /* frame(output) buffer cnt, default: 3 */
  RKADK_U32 u32FrameBufSize;                /* if not set, calculated internally */
  VIDEO_MODE_E u32DecodeMode;               /* decode mode */
  VIDEO_OUTPUT_ORDER_E enOutputOrder;       /* frames output order */
} RKADK_DISP_VDEC_CFG_S;

typedef struct {
  RKADK_BOOL bEnable;                       /* enable subwindow */
  RKADK_S32 s32CamId;                       /* subwindow camera id, set -1 when RKADK_STREAM_TYPE_THIRD_STREAM */
  RKADK_STREAM_TYPE_E enStreamType;         /* subwindow stream type */
  RKADK_DISP_VO_CFG_S stVoCfg;              /* VO configuration reads from ini when RKADK_STREAM_TYPE_DISP */
  RKADK_DISP_VDEC_CFG_S stVdecCfg;          /* stream must set stVdecCfg when RKADK_STREAM_TYPE_THIRD_STREAM */
} RKADK_DISP_STREAM_CFG_S;

typedef struct {
  RKADK_U32 u32CamId;
  RKADK_DISP_STREAM_CFG_S stSubStream[RKADK_DISP_SUB_STREAM_NUM];
} RKADK_DISP_CFG_S;

RKADK_S32 RKADK_DISP_Init(RKADK_U32 u32CamId);
RKADK_S32 RKADK_DISP_DeInit(RKADK_U32 u32CamId);
RKADK_S32 RKADK_DISP_SetAttr(RKADK_U32 u32CamId, RKADK_DISP_ATTR_S *pstAttr);

RKADK_S32 RKADK_DISP_InitEx(RKADK_DISP_CFG_S *pstAttr, RKADK_MW_PTR *ppHandle);
RKADK_S32 RKADK_DISP_DeInitEx(RKADK_MW_PTR pHandle);
RKADK_S32 RKADK_DISP_SetAttrEx(RKADK_DISP_ATTR_S *pstAttr, RKADK_MW_PTR pHandle);

RKADK_S32 RKADK_DISP_SendVideoStream(RKADK_MW_PTR pHandle, RKADK_U32 u32VdecChn, RKADK_DISP_DATA_S *pstData);

#ifdef __cplusplus
}
#endif
#endif
