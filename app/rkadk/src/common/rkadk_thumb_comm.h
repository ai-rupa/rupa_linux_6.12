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

#ifndef __RKADK_THUMB_COMM_H__
#define __RKADK_THUMB_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rkadk_media_comm.h"
#include "rkadk_param.h"

#define THM_VDEC_CHN (VDEC_MAX_CHN_NUM - 1)
#define THM_VPSS_GRP (VPSS_MAX_GRP_NUM - 1)
#define THM_VPSS_CHN 0

typedef enum {
  RKADK_THUMB_MODULE_PHOTO = 0,
  RKADK_THUMB_MODULE_MAIN_RECORD,
  RKADK_THUMB_MODULE_SUB_RECORD,
  RKADK_THUMB_MODULE_BUTT
} RKADK_THUMB_MODULE_E;

RKADK_S32 ThumbnailInit(RKADK_U32 u32CamId, RKADK_THUMB_MODULE_E enThumbModule,
                        RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg);

RKADK_S32 ThumbnailDeInit(RKADK_U32 u32CamId, RKADK_THUMB_MODULE_E enThumbModule,
                          RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg);

RKADK_S32 ThumbnailPhotoData(RKADK_U8 *pJpegdata, RKADK_U32 JpegLen,
                             VENC_STREAM_S stThuFrame,
                             RKADK_U8 *pNewPhoto);

RKADK_S32 ThumbnailChnBind(RKADK_S32 s32VencChn, RKADK_S32 s32ThumbVencChn);

RKADK_S32 ThumbnailRequest(RKADK_S32 s32ThumbVencChn);

PIXEL_FORMAT_E ThumbToRKPixFmt(RKADK_THUMB_TYPE_E enType);

RKADK_S32 ThumbnailJpgDecode(RKADK_THUMB_ATTR_S *pstSrcThmAttr,
                             RKADK_THUMB_ATTR_S *pstDstThmAttr, bool *bFree);

RKADK_S32 GetThmChnCreate(RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr);

RKADK_S32 GetThmChnDestory(RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr);

RKADK_S32 BuildInThmToFile(FILE *fd, RKADK_THUMB_ATTR_S *pstThumbAttr);

RKADK_S32 RKADK_THUMB_GetFilePath(char *videoPath, char *thumbDir, char *thumbPath, int thumbPathLen);

#ifdef __cplusplus
}
#endif
#endif
