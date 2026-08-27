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

#include "rkadk_param.h"
#include "rkadk_param_map.h"
#include "rkadk_hal.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_ao.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define RKISPP_SCALE0_NV12_WIDTH_MAX 2080
#define RKISPP_SCALE1_WIDTH_MAX 1280

/** parameter context */
static RKADK_PARAM_CONTEXT_S g_stPARAMCtx = {
    .bInit = false, .mutexLock = PTHREAD_MUTEX_INITIALIZER};

static RKADK_S32 RKADK_PARAM_SaveViCfg(char *path, RKADK_U32 viIndex,
                                       RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_PARAM_MEDIA_CFG_S *pstMediaCfg = NULL;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);
  pstMediaCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId];

  switch (viIndex) {
  case 0:
    pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_VI0_MAP);
    break;

  case 1:
    pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_VI1_MAP);
    break;

  case 2:
    pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_VI2_MAP);
    break;

  case 3:
    pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_VI3_MAP);
    break;

  default:
    RKADK_LOGE("Invaild vi index = %d", viIndex);
    return -1;
  }
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, &pstMediaCfg->stViCfg[viIndex],
                         pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save u32CamId[%d] VI[%d] param failed", u32CamId, viIndex);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveVersion(char *path) {
  int ret = 0;
  RKADK_PARAM_VERSION_S *pstVersion = &g_stPARAMCtx.stCfg.stVersion;

  ret = RKADK_Struct2Ini(-1, path, pstVersion, g_stVersionMapTable,
                         sizeof(g_stVersionMapTable) / sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save version failed");

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveCommCfg(char *path) {
  int ret = 0;
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  ret = RKADK_Struct2Ini(-1, path, pstCommCfg, g_stCommCfgMapTable,
                         sizeof(g_stCommCfgMapTable) / sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save common param failed");

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveAudioCfg(char *path) {
  int ret = 0;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioCfg = &g_stPARAMCtx.stCfg.stAudioCfg;

  ret = RKADK_Struct2Ini(-1, path, pstAudioCfg, g_stAudioCfgMapTable,
                         sizeof(g_stAudioCfgMapTable) / sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save audio param failed");

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveThumbCfg(char *path, RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstThumbCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stThumbCfg;

  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_THUMB_MAP);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, pstThumbCfg, pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save thumb param failed");

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveSensorCfg(char *path, RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];

  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_SENSOR_MAP);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, pstSensorCfg, pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save sensor[%d] param failed", u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SavePhotoCfg(char *path, RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstPhotoCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;

  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_PHOTO_MAP);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, pstPhotoCfg, pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save sensor[%d] photo param failed", u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveStreamCfg(char *path, RKADK_U32 u32CamId,
                                           RKADK_STREAM_TYPE_E enStrmType) {
  int ret = 0;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;
  RKADK_PARAM_MAP_TYPE_E enMapType;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stLiveCfg;
    enMapType = RKADK_PARAM_LIVE_MAP;
    break;
  case RKADK_STREAM_TYPE_PREVIEW:
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg;
    enMapType = RKADK_PARAM_PREVIEW_MAP;
    break;
  default:
    RKADK_LOGW("unsupport enStrmType = %d", enStrmType);
    return -1;
  }

  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, enMapType);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, &pstStreamCfg->attribute,
                         pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save sensor[%d] stream(enStrmType = %d) param failed",
               enStrmType, u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveRecCfg(char *path, RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;

  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_REC_MAP);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, pstRecCfg, pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret) {
    RKADK_LOGE("save sensor[%d] record param failed", u32CamId);
    return ret;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SaveRecTime(char *path, RKADK_U32 u32CamId,
                                         RKADK_STREAM_TYPE_E enStrmType) {
  int ret;
  RKADK_U32 index;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;
  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    index = 0;
    pstMapTableCfg =
        RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_REC_MAIN_TIME_MAP);
    break;
  case RKADK_STREAM_TYPE_VIDEO_SUB:
    index = 1;
    pstMapTableCfg =
        RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_REC_SUB_TIME_MAP);
    break;
  default:
    return -1;
  }

  if (!pstMapTableCfg)
    return -1;

  ret = RKADK_Struct2Ini(u32CamId, path, &pstRecCfg->record_time_cfg[index],
                         pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret) {
    RKADK_LOGE("save sensor[%d] record time[%d] param failed", u32CamId, index);
    return ret;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SaveRecAttr(char *path, RKADK_U32 u32CamId,
                                         RKADK_STREAM_TYPE_E enStrmType) {
  int ret;
  RKADK_U32 index;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;
  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    index = 0;
    pstMapTableCfg =
        RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_REC_MAIN_MAP);
    break;
  case RKADK_STREAM_TYPE_VIDEO_SUB:
    index = 1;
    pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_REC_SUB_MAP);
    break;
  default:
    return -1;
  }

  if (!pstMapTableCfg)
    return -1;

  ret = RKADK_Struct2Ini(u32CamId, path, &pstRecCfg->attribute[index],
                         pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret) {
    RKADK_LOGE("save sensor[%d] record attribute[%d] param failed", u32CamId,
               index);
    return ret;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SaveDispCfg(char *path, RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_DISP_CFG_S *pstDispCfg;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstDispCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stDispCfg;
  pstMapTableCfg = RKADK_PARAM_GetMapTable(u32CamId, RKADK_PARAM_DISP_MAP);
  RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

  ret = RKADK_Struct2Ini(u32CamId, path, pstDispCfg, pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
  if (ret)
    RKADK_LOGE("save sensor[%d] display param failed", u32CamId);

  return ret;
}

static bool RKADK_PARAM_CheckCfg(RKADK_U32 *u32Value, RKADK_U32 u32DefValue,
                                 const char *tag) {
  if (*u32Value <= 0) {
    RKADK_LOGW("%s: invalid value(%d), use default(%d)", tag, *u32Value,
               u32DefValue);
    *u32Value = u32DefValue;
    return true;
  } else {
    return false;
  }
}

static bool RKADK_PARAM_CheckCfgU32(RKADK_U32 *u32Value, RKADK_U32 u32Min,
                                    RKADK_U32 u32Max, RKADK_U32 u32DefValue,
                                    const char *tag) {
  if ((*u32Value > u32Max) || (*u32Value < u32Min)) {
    RKADK_LOGW("%s: invalid value(%d)[%d, %d], use default(%d)", tag, *u32Value,
               u32Min, u32Max, u32DefValue);
    *u32Value = u32DefValue;
    return true;
  } else {
    return false;
  }
}

static bool RKADK_PARAM_CheckCfgStr(char *value, const char *defValue,
                                    RKADK_U32 s32ValueLen, const char *tag) {
  if (!strlen(value)) {
    RKADK_LOGW("%s: invalid value(%s), use default(%s)", tag, value, defValue);
    snprintf(value, s32ValueLen, "%s", defValue);
    return true;
  } else {
    return false;
  }
}

static void RKADK_PARAM_CheckCommCfg(char *path) {
  bool change = false;
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  change = RKADK_PARAM_CheckCfgU32(&pstCommCfg->sensor_count, 1,
                                   RKADK_MAX_SENSOR_CNT, 1, "sensor_count");
  change |= RKADK_PARAM_CheckCfgU32(&pstCommCfg->speaker_volume, 0, 100, 70,
                                    "speaker_volume");
  change |= RKADK_PARAM_CheckCfgU32(&pstCommCfg->mic_volume, 0, 100, 70,
                                    "mic_volume");

  if (change)
    RKADK_PARAM_SaveCommCfg(path);
}

static void RKADK_PARAM_CheckAudioCfg(char *path) {
  bool change = false;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioCfg = &g_stPARAMCtx.stCfg.stAudioCfg;

  change = RKADK_PARAM_CheckCfgStr(pstAudioCfg->ai_audio_node, AUDIO_DEVICE_NAME,
                                   RKADK_BUFFER_LEN, "ai_audio_node");
  change |= RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstAudioCfg->bit_width,
                                    AUDIO_BIT_WIDTH_8, AUDIO_BIT_WIDTH_32,
                                    AUDIO_BIT_WIDTH, "bit_width");
  change |=
      RKADK_PARAM_CheckCfg(&pstAudioCfg->channels, AUDIO_CHANNEL, "channels");
  change |= RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstAudioCfg->mic_type,
                                    RKADK_MIC_TYPE_LEFT, RKADK_MIC_TYPE_BOTH,
                                    RKADK_MIC_TYPE_LEFT, "mic_type");
  change |= RKADK_PARAM_CheckCfg(&pstAudioCfg->samplerate, AUDIO_SAMPLE_RATE,
                                 "samplerate");
  change |= RKADK_PARAM_CheckCfg(&pstAudioCfg->samples_per_frame,
                                 AUDIO_FRAME_COUNT, "samples_per_frame");
  change |= RKADK_PARAM_CheckCfg(&pstAudioCfg->ai_depth, AI_DEPTH, "ai_depth");
  change |=
      RKADK_PARAM_CheckCfg(&pstAudioCfg->bitrate, AUDIO_BIT_REAT, "bitrate");
  change |= RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstAudioCfg->vqe_mode,
                                    RKADK_VQE_MODE_AI_TALK, RKADK_VQE_MODE_BUTT,
                                    RKADK_VQE_MODE_AI_RECORD, "vqe_mode");
  change |= RKADK_PARAM_CheckCfgStr(pstAudioCfg->vqe_config_path, AI_VQE_CONFIG_PATH,
                                   RKADK_PATH_LEN, "vqe_config_path");
  change |= RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstAudioCfg->codec_type,
                                    RKADK_CODEC_TYPE_G711A, RKADK_CODEC_TYPE_PCM,
                                    RKADK_CODEC_TYPE_MP3, "codec_type");

  if (change)
    RKADK_PARAM_SaveAudioCfg(path);
}

static void RKADK_PARAM_CheckThumbCfg(char *path, RKADK_U32 u32CamId) {
  bool change = false;
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg =
    &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stThumbCfg;

  change = RKADK_PARAM_CheckCfg(&pstThumbCfg->thumb_width, THUMB_WIDTH, "thumb_width");
  change |= RKADK_PARAM_CheckCfg(&pstThumbCfg->thumb_height, THUMB_HEIGHT, "thumb_height");
  change |= RKADK_PARAM_CheckCfgU32(&pstThumbCfg->photo_venc_chn, 0, VENC_MAX_CHN_NUM,
                                    THUMB_PHOTO_VENC_CHN, "thumb photo_venc_chn");
  change |= RKADK_PARAM_CheckCfgU32(&pstThumbCfg->record_main_venc_chn, 0, VENC_MAX_CHN_NUM,
                                    THUMB_RECORD_MAIN_VENC_CHN, "thumb record_main_venc_chn");
  change |= RKADK_PARAM_CheckCfgU32(&pstThumbCfg->record_sub_venc_chn, 0, VENC_MAX_CHN_NUM,
                                    THUMB_RECORD_SUB_VENC_CHN, "thumb record_sub_venc_chn");

  if (change)
    RKADK_PARAM_SaveThumbCfg(path, u32CamId);
}

static void RKADK_PARAM_CheckSensorCfg(char *path, RKADK_U32 u32CamId) {
  bool change = false;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];

  change = RKADK_PARAM_CheckCfg(&pstSensorCfg->framerate, VIDEO_FRAME_RATE,
                                "sensor fps");

  if (change)
    RKADK_PARAM_SaveSensorCfg(path, u32CamId);
}

static void RKADK_PARAM_CheckStreamCfg(char *path, RKADK_U32 u32CamId,
                                       RKADK_STREAM_TYPE_E enStrmType) {
  bool change = false;
  RKADK_PARAM_VENC_ATTR_S *pstAttribute = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_LIVE:
    pstAttribute = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stLiveCfg.attribute;
    break;
  case RKADK_STREAM_TYPE_PREVIEW:
    pstAttribute =
        &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg.attribute;
    break;
  default:
    RKADK_LOGW("unsupport enStrmType = %d", enStrmType);
    return;
  }

  if (strcmp(pstAttribute->rc_mode, "CBR") &&
      strcmp(pstAttribute->rc_mode, "VBR") &&
      strcmp(pstAttribute->rc_mode, "AVBR")) {
    memcpy(pstAttribute->rc_mode, "CBR", strlen("CBR"));
    change = true;
  }

  change |= RKADK_PARAM_CheckCfg(&pstAttribute->width, STREAM_VIDEO_WIDTH,
                                 "stream width");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->height, STREAM_VIDEO_HEIGHT,
                                 "stream height");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->bufsize,
                                 STREAM_VIDEO_WIDTH * STREAM_VIDEO_HEIGHT * 3 / 2,
                                 "stream bufsize");
  change |= RKADK_PARAM_CheckCfgU32(&pstAttribute->venc_chn, 0,
                                    VENC_MAX_CHN_NUM, 1, "stream venc_chn");
  change |= RKADK_PARAM_CheckCfgU32(&pstAttribute->vpss_chn, 0, VPSS_MAX_CHN_NUM,
                                    1, "stream vpss_chn");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->bitrate, 4 * 1024 * 1024,
                                 "stream bitrate");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->framerate, pstSensorCfg->framerate, "stream fps");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->gop, VIDEO_GOP, "stream gop");
  change |= RKADK_PARAM_CheckCfg(&pstAttribute->profile, VIDEO_PROFILE,
                                 "stream profile");
  change |= RKADK_PARAM_CheckCfgU32(
      (RKADK_U32 *)&pstAttribute->codec_type, RKADK_CODEC_TYPE_H264,
      RKADK_CODEC_TYPE_MJPEG, RKADK_CODEC_TYPE_H264, "stream codec_type");

  if (change)
    RKADK_PARAM_SaveStreamCfg(path, u32CamId, enStrmType);
}

static void RKADK_PARAM_CheckPhotoCfg(char *path, RKADK_U32 u32CamId) {
  bool change = false;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;

  change = RKADK_PARAM_CheckCfg(&pstPhotoCfg->image_width, PHOTO_VIDEO_WIDTH,
                                "photo width");
  change |= RKADK_PARAM_CheckCfg(&pstPhotoCfg->image_height, PHOTO_VIDEO_HEIGHT,
                                 "photo height");
  change |= RKADK_PARAM_CheckCfgU32(&pstPhotoCfg->venc_chn, 0, VENC_MAX_CHN_NUM,
                                    2, "photo venc_chn");
  change |= RKADK_PARAM_CheckCfgU32(&pstPhotoCfg->vpss_chn, 0, VPSS_MAX_CHN_NUM,
                                    2, "photo vpss_chn");

  if (pstPhotoCfg->jpeg_slice) {
    change |= RKADK_PARAM_CheckCfg(&pstPhotoCfg->slice_height, pstPhotoCfg->image_height, "photo slice_height");
    change |= RKADK_PARAM_CheckCfg(&pstPhotoCfg->max_slice_width, JPEG_SLICE_WIDTH_MAX, "photo max_slice_width");
    change |= RKADK_PARAM_CheckCfg(&pstPhotoCfg->max_slice_height, JPEG_SLICE_HEIGHT_MAX, "photo max_slice_height");
  }

  if (change)
    RKADK_PARAM_SavePhotoCfg(path, u32CamId);
}

static void RKADK_PARAM_CheckRecCfg(char *path, RKADK_U32 u32CamId) {
  bool change = false;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];
  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;

  change = RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstRecCfg->record_type,
                                   RKADK_REC_TYPE_NORMAL, RKADK_REC_TYPE_AOV_LAPSE,
                                   RKADK_REC_TYPE_NORMAL, "record_type");

  change |= RKADK_PARAM_CheckCfgU32((RKADK_U32 *)&pstRecCfg->file_type,
                                    RKADK_MUXER_TYPE_MP4, RKADK_MUXER_TYPE_FLV,
                                    RKADK_MUXER_TYPE_MP4, "file_type");

  change |= RKADK_PARAM_CheckCfgU32(
      (RKADK_U32 *)&pstRecCfg->pre_record_mode, RKADK_MUXER_PRE_RECORD_NONE,
      RKADK_MUXER_PRE_RECORD_SINGLE, RKADK_MUXER_PRE_RECORD_NONE,
      "pre_record_mode");
  change |= RKADK_PARAM_CheckCfg(&pstRecCfg->lapse_multiple,
                                    pstRecCfg->attribute[0].framerate, "lapse_multiple");
  change |=
      RKADK_PARAM_CheckCfgU32(&pstRecCfg->file_num, 1, RECORD_FILE_NUM_MAX,
                              RECORD_FILE_NUM_MAX, "file_num");

  if (change)
    RKADK_PARAM_SaveRecCfg(path, u32CamId);

  for (int i = 0; i < (int)pstRecCfg->file_num; i++) {
    RKADK_STREAM_TYPE_E enStrmType = RKADK_STREAM_TYPE_VIDEO_MAIN;
    RKADK_U32 u32DefWidth, u32DefHeight, u32DefChn, u32DefBitrate;
    RKADK_PARAM_VENC_ATTR_S *pstAttribute = &pstRecCfg->attribute[i];
    RKADK_PARAM_REC_TIME_CFG_S *pstRecTimeCfg = &pstRecCfg->record_time_cfg[i];

    if (i == 0) {
      enStrmType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      u32DefWidth = RECORD_VIDEO_WIDTH;
      u32DefHeight = RECORD_VIDEO_HEIGHT;
      u32DefChn = 0;
      u32DefBitrate = 30 * 1024 * 1024;
    } else {
      enStrmType = RKADK_STREAM_TYPE_VIDEO_SUB;
      u32DefWidth = RECORD_VIDEO_WIDTH_S;
      u32DefHeight = RECORD_VIDEO_HEIGHT_S;
      u32DefChn = 1;
      u32DefBitrate = 4 * 1024 * 1024;
    }

    // check record time
    change =
        RKADK_PARAM_CheckCfg(&pstRecTimeCfg->record_time, 60, "record_time");
    change |=
        RKADK_PARAM_CheckCfg(&pstRecTimeCfg->splite_time, 60, "splite_time");
    change |= RKADK_PARAM_CheckCfg(&pstRecTimeCfg->lapse_interval, 60,
                                   "lapse_interval");

    if (change)
      RKADK_PARAM_SaveRecTime(path, u32CamId, enStrmType);

    // check venc attribute
    change = false;
    if (strcmp(pstAttribute->rc_mode, "CBR") &&
        strcmp(pstAttribute->rc_mode, "VBR") &&
        strcmp(pstAttribute->rc_mode, "AVBR")) {
      memcpy(pstAttribute->rc_mode, "CBR", strlen("CBR"));
      change = true;
    }

    change |=
        RKADK_PARAM_CheckCfg(&pstAttribute->width, u32DefWidth, "rec width");
    change |=
        RKADK_PARAM_CheckCfg(&pstAttribute->height, u32DefHeight, "rec height");
    change |=
        RKADK_PARAM_CheckCfg(&pstAttribute->bufsize, u32DefWidth * u32DefHeight * 3 / 2, "rec bufsize");
    change |=
        RKADK_PARAM_CheckCfgU32(&pstAttribute->venc_chn, 0, VENC_MAX_CHN_NUM,
                                u32DefChn, "rec venc_chn");
    change |= RKADK_PARAM_CheckCfgU32(
        &pstAttribute->vpss_chn, 0, VPSS_MAX_CHN_NUM, u32DefChn, "rec vpss_chn");
    change |= RKADK_PARAM_CheckCfg(&pstAttribute->bitrate, u32DefBitrate,
                                   "rec bitrate");
    change |= RKADK_PARAM_CheckCfg(&pstAttribute->framerate, pstSensorCfg->framerate, "rec fps");
    change |= RKADK_PARAM_CheckCfg(&pstAttribute->gop, VIDEO_GOP, "rec gop");
    change |= RKADK_PARAM_CheckCfg(&pstAttribute->profile, VIDEO_PROFILE,
                                   "rec profile");
    change |= RKADK_PARAM_CheckCfgU32(
        (RKADK_U32 *)&pstAttribute->codec_type, RKADK_CODEC_TYPE_H264,
        RKADK_CODEC_TYPE_MJPEG, RKADK_CODEC_TYPE_H264, "rec codec_type");

    if (change)
      RKADK_PARAM_SaveRecAttr(path, u32CamId, enStrmType);
  }
}

static void RKADK_PARAM_CheckViCfg(char *path, RKADK_U32 u32CamId,
                                   RKADK_U32 index) {
  bool change = false;
  RKADK_U32 u32ChnId;
  const char *pixFmt = "NV12";
  const char *deviceName;
  const char *module;
  RKADK_U32 u32DefWidth, u32DefHeight;
  RKADK_PARAM_VI_CFG_S *pstViCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stViCfg[index];
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];

  u32ChnId = index + (u32CamId * RKADK_ISPP_VI_NODE_CNT);
  if (u32ChnId >= VI_MAX_CHN_NUM)
    u32ChnId = VI_MAX_CHN_NUM - 1;

  switch (index) {
  case 0:
    deviceName = DEF_DEVICE_NEME_VI_0;
    module = DEF_MODULE_VI_0;
    u32DefWidth = RECORD_VIDEO_WIDTH;
    u32DefHeight = RECORD_VIDEO_HEIGHT;
    break;
  case 1:
    deviceName = DEF_DEVICE_NEME_VI_1;
    module = DEF_MODULE_VI_1;
    u32DefWidth = DEF_WIDTH_VI_1;
    u32DefHeight = DEF_HEIGHT_VI_1;
    break;
  case 2:
    deviceName = DEF_DEVICE_NEME_VI_2;
    module = DEF_MODULE_VI_2;
    u32DefWidth = DEF_WIDTH_VI_2;
    u32DefHeight = DEF_HEIGHT_VI_2;
    break;
  case 3:
    deviceName = DEF_DEVICE_NEME_VI_3;
    module = DEF_MODULE_VI_3;
    u32DefWidth = STREAM_VIDEO_WIDTH;
    u32DefHeight = STREAM_VIDEO_HEIGHT;
    break;
  default:
    break;
  }

  change = RKADK_PARAM_CheckCfg(&pstViCfg->width, u32DefWidth, "vi width");
  change |= RKADK_PARAM_CheckCfg(&pstViCfg->height, u32DefHeight, "vi height");
  change |= RKADK_PARAM_CheckCfgU32(&pstViCfg->chn_id, 0, VI_MAX_CHN_NUM,
                                    u32ChnId, "vi chn_id");
  change |= RKADK_PARAM_CheckCfgStr(pstViCfg->device_name, deviceName,
                                    MAX_VI_ENTITY_NAME_LEN, "vi device_name");
  change |= RKADK_PARAM_CheckCfgStr(pstViCfg->module, module, RKADK_BUFFER_LEN,
                                    "vi module");
  change |= RKADK_PARAM_CheckCfg(&pstViCfg->buf_cnt, VIDEO_BUFFER_COUNT, "vi buf_cnt");
  change |= RKADK_PARAM_CheckCfgStr(pstViCfg->pix_fmt, pixFmt,
                                    RKADK_PIX_FMT_LEN, "vi pix_fmt");

  if (pstViCfg->depth > (pstViCfg->buf_cnt - 2)) {
    RKADK_LOGW("illogical CamId[%d] vi[%d] depth[%d], buf_cnt[%d]",
                u32CamId, index, pstViCfg->depth, pstViCfg->buf_cnt);
    pstViCfg->depth = pstViCfg->buf_cnt - 2;
    change = true;
  }

#ifdef RV1126_1109
  if (!strcmp(pstViCfg->device_name, "rkispp_scale0")) {
    if ((pstViCfg->width != pstSensorCfg->max_width) &&
        (pstViCfg->width > RKISPP_SCALE0_NV12_WIDTH_MAX)) {
      if (strcmp(pstViCfg->pix_fmt, "NV16")) {
        RKADK_LOGW("rkispp_scale0 resolution[%dx%d] > 2K, must NV16",
                   pstViCfg->width, pstViCfg->height);
        memcpy(pstViCfg->pix_fmt, "NV16", strlen("NV16"));
        change = true;
      }
    }
  }
#endif

  if (change)
    RKADK_PARAM_SaveViCfg(path, index, u32CamId);
}

static void RKADK_PARAM_CheckDispCfg(char *path, RKADK_U32 u32CamId) {
  bool change = false;
  RKADK_PARAM_DISP_CFG_S *pstDispCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stDispCfg;

  change =
      RKADK_PARAM_CheckCfg(&pstDispCfg->width, DISP_WIDTH, "display width");
  change |=
      RKADK_PARAM_CheckCfg(&pstDispCfg->height, DISP_HEIGHT, "display height");

  if (pstDispCfg->rotation != 0 && pstDispCfg->rotation != 1 &&
      pstDispCfg->rotation != 2 && pstDispCfg->rotation != 3) {
    RKADK_LOGW("invalid rotation(%d), use default(0)", pstDispCfg->rotation);
    pstDispCfg->rotation = 0;
    change = true;
  }

  change |= RKADK_PARAM_CheckCfgStr(pstDispCfg->img_type, "RGB888",
                                    RKADK_PIX_FMT_LEN, "display pix_fmt");

  change |= RKADK_PARAM_CheckCfgStr(pstDispCfg->splice_mode, "RGA",
                                    RKADK_SPLICE_MODE_LEN, "display splice_mode");

#if defined(RV1106_1103) || defined(RV1103B)
  change |= RKADK_PARAM_CheckCfgStr(pstDispCfg->intf_type, "default",
                                    RKADK_INTF_FMT_LEN, "display intf_type");
#else
  change |= RKADK_PARAM_CheckCfgStr(pstDispCfg->intf_type, "MIPI",
                                    RKADK_INTF_FMT_LEN, "display intf_type");
#endif

  change |= RKADK_PARAM_CheckCfgU32(&pstDispCfg->vo_chn, 0, VO_MAX_CHN_NUM, 0,
                                    "display vo_chn");
  change |= RKADK_PARAM_CheckCfgU32(&pstDispCfg->vpss_chn, 0, VPSS_MAX_CHN_NUM, 3,
                                    "display vpss_chn");

  if (change)
    RKADK_PARAM_SaveDispCfg(path, u32CamId);
}

static bool RKADK_PARAM_CheckVersion(char *path) {
  int ret;
  char version[RKADK_BUFFER_LEN];
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  memset(version, 0, RKADK_BUFFER_LEN);
  memset(&pstCfg->stVersion, 0, sizeof(RKADK_PARAM_VERSION_S));

  ret = RKADK_Ini2Struct(-1, g_stPARAMCtx.defPath, &pstCfg->stVersion, g_stVersionMapTable,
                          sizeof(g_stVersionMapTable) / sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("load default ini version failed");
    return false;
  }
  strncpy(version, pstCfg->stVersion.version, RKADK_BUFFER_LEN);

  memset(&pstCfg->stVersion, 0, sizeof(RKADK_PARAM_VERSION_S));
  ret = RKADK_Ini2Struct(-1, path, &pstCfg->stVersion, g_stVersionMapTable,
                         sizeof(g_stVersionMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("load setting ini version failed");
    return false;
  }

  if (strcmp(version, pstCfg->stVersion.version)) {
    RKADK_LOGW("default ini version[%s] != setting ini version[%s]", version,
               pstCfg->stVersion.version);
    return false;
  }

  return true;
}

static void RKADK_PARAM_DefCommCfg(char *path) {
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  memset(pstCommCfg, 0, sizeof(RKADK_PARAM_COMM_CFG_S));
  pstCommCfg->sensor_count = 1;
  pstCommCfg->speaker_volume = 70;
  pstCommCfg->mic_volume = 70;
  pstCommCfg->vpss_devcie = 1;
  RKADK_PARAM_SaveCommCfg(path);
}

static void RKADK_PARAM_DefAudioCfg(char *path) {
  RKADK_PARAM_AUDIO_CFG_S *pstAudioCfg = &g_stPARAMCtx.stCfg.stAudioCfg;

  memset(pstAudioCfg, 0, sizeof(RKADK_PARAM_AUDIO_CFG_S));
  memcpy(pstAudioCfg->ai_audio_node, AUDIO_DEVICE_NAME, strlen(AUDIO_DEVICE_NAME));
  pstAudioCfg->ai_depth = AI_DEPTH;
  pstAudioCfg->bit_width = AUDIO_BIT_WIDTH;
  pstAudioCfg->channels = AUDIO_CHANNEL;
  pstAudioCfg->mic_type = AUDIO_MIC_TYPE;
  pstAudioCfg->samplerate = AUDIO_SAMPLE_RATE;
  pstAudioCfg->samples_per_frame = AUDIO_FRAME_COUNT;
  pstAudioCfg->bitrate = AUDIO_BIT_REAT;
  pstAudioCfg->vqe_mode = RKADK_VQE_MODE_BUTT;
  memcpy(pstAudioCfg->vqe_config_path, AI_VQE_CONFIG_PATH, strlen(AI_VQE_CONFIG_PATH));
  pstAudioCfg->codec_type = RKADK_CODEC_TYPE_MP3;
  RKADK_PARAM_SaveAudioCfg(path);
}

static void RKADK_PARAM_DefSensorCfg(RKADK_U32 u32CamId, char *path) {
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[0];

  memset(pstSensorCfg, 0, sizeof(RKADK_PARAM_SENSOR_CFG_S));
  if (u32CamId == 0) {
    pstSensorCfg->max_width = SENSOR_MAX_WIDTH;
    pstSensorCfg->max_height = SENSOR_MAX_HEIGHT;
  }

  pstSensorCfg->used_isp = true;
  pstSensorCfg->framerate = VIDEO_FRAME_RATE;
  pstSensorCfg->flip = false;
  pstSensorCfg->mirror = false;
  pstSensorCfg->enable_wrap = false;
  pstSensorCfg->wrap_buf_line = SENSOR_MAX_HEIGHT / 4;
  pstSensorCfg->aenc_chn = 0;
  pstSensorCfg->mute = false;
  RKADK_PARAM_SaveSensorCfg(path, u32CamId);
}

static void RKADK_PARAM_DefViCfg(RKADK_U32 u32CamId, RKADK_U32 u32ViIndex,
                                 char *path) {
  RKADK_PARAM_VI_CFG_S *pstViCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stViCfg[u32ViIndex];

  memset(pstViCfg, 0, sizeof(RKADK_PARAM_VI_CFG_S));
  pstViCfg->chn_id = u32ViIndex + (u32CamId * RKADK_ISPP_VI_NODE_CNT);
  if (pstViCfg->chn_id >= VI_MAX_CHN_NUM)
    pstViCfg->chn_id = VI_MAX_CHN_NUM - 1;

  pstViCfg->buf_cnt = VIDEO_BUFFER_COUNT;
  memcpy(pstViCfg->pix_fmt, "NV12", strlen("NV12"));

  switch (u32ViIndex) {
  case 0:
    memcpy(pstViCfg->device_name, DEF_DEVICE_NEME_VI_0, strlen(DEF_DEVICE_NEME_VI_0));
    pstViCfg->width = RECORD_VIDEO_WIDTH;
    pstViCfg->height = RECORD_VIDEO_HEIGHT;
    memcpy(pstViCfg->module, DEF_MODULE_VI_0, strlen(DEF_MODULE_VI_0));
    break;
  case 1:
    memcpy(pstViCfg->device_name, DEF_DEVICE_NEME_VI_1, strlen(DEF_DEVICE_NEME_VI_1));
    pstViCfg->width = DEF_WIDTH_VI_1;
    pstViCfg->height = DEF_HEIGHT_VI_1;
    memcpy(pstViCfg->module, DEF_MODULE_VI_1, strlen(DEF_MODULE_VI_1));
    break;
  case 2:
    memcpy(pstViCfg->device_name, DEF_DEVICE_NEME_VI_2, strlen(DEF_DEVICE_NEME_VI_2));
    pstViCfg->width = DEF_WIDTH_VI_2;
    pstViCfg->height = DEF_HEIGHT_VI_2;
    memcpy(pstViCfg->module, DEF_MODULE_VI_2, strlen(DEF_MODULE_VI_2));
    break;
  case 3:
    memcpy(pstViCfg->device_name, DEF_DEVICE_NEME_VI_3, strlen(DEF_DEVICE_NEME_VI_3));
    pstViCfg->width = STREAM_VIDEO_WIDTH;
    pstViCfg->height = STREAM_VIDEO_HEIGHT;
    memcpy(pstViCfg->module, DEF_MODULE_VI_3, strlen(DEF_MODULE_VI_3));
    break;
  default:
    RKADK_LOGE("Nonsupport vi index: %d", u32ViIndex);
    return;
  }

  RKADK_PARAM_SaveViCfg(path, u32ViIndex, u32CamId);
}

static void RKADK_PARAM_DefRecTime(RKADK_U32 u32CamId,
                                   RKADK_STREAM_TYPE_E enStrmType, char *path) {
  RKADK_PARAM_REC_TIME_CFG_S *pstTimeCfg = NULL;

  if (u32CamId >= RKADK_MAX_SENSOR_CNT) {
    RKADK_LOGE("invalid camera id: %d", u32CamId);
    return;
  }

  if (enStrmType == RKADK_STREAM_TYPE_VIDEO_MAIN)
    pstTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg.record_time_cfg[0];
  else
    pstTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg.record_time_cfg[1];

  memset(pstTimeCfg, 0, sizeof(RKADK_PARAM_REC_TIME_CFG_S));
  pstTimeCfg->record_time = 60;
  pstTimeCfg->splite_time = 60;
  pstTimeCfg->lapse_interval = 60;

  RKADK_PARAM_SaveRecTime(path, u32CamId, enStrmType);
}

static void RKADK_PARAM_DefRecAttr(RKADK_U32 u32CamId,
                                   RKADK_STREAM_TYPE_E enStrmType, char *path) {
  RKADK_U32 u32Width, u32Height;
  RKADK_U32 u32Chn, u32VpssGrp, u32VpssChn;
  RKADK_U32 u32Bitrate;
  RKADK_PARAM_VENC_ATTR_S *pstAttr = NULL;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;

  if (u32CamId >= RKADK_MAX_SENSOR_CNT) {
    RKADK_LOGE("invalid camera id: %d", u32CamId);
    return;
  }

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;
  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstAttr = &pstRecCfg->attribute[0];
    u32Width = RECORD_VIDEO_WIDTH;
    u32Height = RECORD_VIDEO_HEIGHT;
    u32Chn = 0;
    u32VpssGrp = 0;
    u32VpssChn = 0;
    u32Bitrate = 30 * 1024 * 1024;
    break;
  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstAttr = &pstRecCfg->attribute[1];
    u32Width = RECORD_VIDEO_WIDTH_S;
    u32Height = RECORD_VIDEO_HEIGHT_S;
    u32Chn = 1;
    u32VpssGrp = 2;
    u32VpssChn = 0;
    u32Bitrate = 4 * 1024 * 1024;
    break;
  default:
    return;
  }

  memset(pstAttr, 0, sizeof(RKADK_PARAM_VENC_ATTR_S));
  pstAttr->max_width = u32Width;
  pstAttr->max_height = u32Height;
  pstAttr->width = u32Width;
  pstAttr->height = u32Height;
  pstAttr->bufsize = u32Width * u32Height * 3 / 2;
  pstAttr->venc_chn = u32Chn;
  pstAttr->enable_vpss = false;
  pstAttr->vpss_grp = u32VpssGrp;
  pstAttr->vpss_chn = u32VpssChn;
  pstAttr->bitrate = u32Bitrate;
  pstAttr->framerate = VIDEO_FRAME_RATE;
  pstAttr->gop = VIDEO_GOP;
  pstAttr->profile = VIDEO_PROFILE;
  pstAttr->codec_type = RKADK_CODEC_TYPE_H264;
  memcpy(pstAttr->rc_mode, "CBR", strlen("CBR"));

  RKADK_PARAM_SaveRecAttr(path, u32CamId, enStrmType);
}

static void RKADK_PARAM_DefRecCfg(RKADK_U32 u32CamId, char *path) {
  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;

  memset(pstRecCfg, 0, sizeof(RKADK_PARAM_REC_CFG_S));
  pstRecCfg->record_type = RKADK_REC_TYPE_NORMAL;
  pstRecCfg->file_type = RKADK_MUXER_TYPE_MP4;
  pstRecCfg->pre_record_time = 0;
  pstRecCfg->pre_record_mode = RKADK_MUXER_PRE_RECORD_NONE;
  pstRecCfg->lapse_multiple = VIDEO_FRAME_RATE;
  pstRecCfg->file_num = 1;
  pstRecCfg->enable_audio = true;
  RKADK_PARAM_SaveRecCfg(path, u32CamId);
}

static void RKADK_PARAM_DefPhotoCfg(RKADK_U32 u32CamId, char *path) {
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;

  memset(pstPhotoCfg, 0, sizeof(RKADK_PARAM_PHOTO_CFG_S));
  pstPhotoCfg->max_width = PHOTO_VIDEO_WIDTH;
  pstPhotoCfg->max_height = PHOTO_VIDEO_HEIGHT;
  pstPhotoCfg->image_width = PHOTO_VIDEO_WIDTH;
  pstPhotoCfg->image_height = PHOTO_VIDEO_HEIGHT;
  pstPhotoCfg->venc_chn = 2;
  pstPhotoCfg->vpss_grp = 0;
  pstPhotoCfg->vpss_chn = 0;
  pstPhotoCfg->enable_combo = false;
  pstPhotoCfg->combo_venc_chn = 0;
  pstPhotoCfg->qfactor = 70;
  pstPhotoCfg->enable_vpss = false;
  pstPhotoCfg->jpeg_slice = false;

  RKADK_PARAM_SavePhotoCfg(path, u32CamId);
}

static void RKADK_PARAM_DefStreamCfg(RKADK_U32 u32CamId, char *path,
                                     RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = NULL;

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stLiveCfg;
    break;
  case RKADK_STREAM_TYPE_PREVIEW:
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg;
    break;
  default:
    RKADK_LOGW("unsupport enStrmType = %d", enStrmType);
    return;
  }

  memset(pstStreamCfg, 0, sizeof(RKADK_PARAM_STREAM_CFG_S));
  pstStreamCfg->attribute.max_width = STREAM_VIDEO_WIDTH;
  pstStreamCfg->attribute.max_height = STREAM_VIDEO_HEIGHT;
  pstStreamCfg->attribute.width = STREAM_VIDEO_WIDTH;
  pstStreamCfg->attribute.height = STREAM_VIDEO_HEIGHT;
  pstStreamCfg->attribute.bufsize = STREAM_VIDEO_WIDTH * STREAM_VIDEO_HEIGHT * 3 / 2;
  pstStreamCfg->attribute.venc_chn = 1;
  pstStreamCfg->attribute.enable_vpss = false;
  pstStreamCfg->attribute.vpss_grp = 2;
  pstStreamCfg->attribute.vpss_chn = 0;
  pstStreamCfg->attribute.framerate = VIDEO_FRAME_RATE;
  pstStreamCfg->attribute.gop = VIDEO_GOP;
  pstStreamCfg->attribute.bitrate = 10 * 1024;
  pstStreamCfg->attribute.profile = VIDEO_PROFILE;
  pstStreamCfg->attribute.codec_type = RKADK_CODEC_TYPE_H264;
  pstStreamCfg->enable_audio = true;
  memcpy(pstStreamCfg->attribute.rc_mode, "VBR", strlen("VBR"));

  RKADK_PARAM_SaveStreamCfg(path, u32CamId, enStrmType);
}

static void RKADK_PARAM_DefDispCfg(RKADK_U32 u32CamId, char *path) {
  RKADK_PARAM_DISP_CFG_S *pstDispCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stDispCfg;

  memset(pstDispCfg, 0, sizeof(RKADK_PARAM_DISP_CFG_S));
  if (u32CamId == 0) {
    pstDispCfg->width = DISP_WIDTH;
    pstDispCfg->height = DISP_HEIGHT;
    pstDispCfg->frame_rate = 30;
    pstDispCfg->rotation = 1; //0: 0, 1: 90, 2: 180, 3: 270
    pstDispCfg->enable_vpss = true;
    pstDispCfg->vpss_grp = 3;
    pstDispCfg->vpss_chn = 0;
    memcpy(pstDispCfg->img_type, "RGB888", strlen("RGB888"));
    memcpy(pstDispCfg->splice_mode, "RGA", strlen("RGA"));
    pstDispCfg->vo_device = VO_DEVICE;
    pstDispCfg->vo_layer = VOP_LAYER;
    pstDispCfg->vo_chn = 0;
#if defined(RV1106_1103) || defined(RV1103B)
    memcpy(pstDispCfg->intf_type, "default", strlen("default"));
#else
    memcpy(pstDispCfg->intf_type, "MIPI", strlen("MIPI"));
#endif
  }

  RKADK_PARAM_SaveDispCfg(path, u32CamId);
}

static void RKADK_PARAM_DefThumbCfg(RKADK_U32 u32CamId, char *path) {
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg =
    &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stThumbCfg;

  memset(pstThumbCfg, 0, sizeof(RKADK_PARAM_THUMB_CFG_S));
  pstThumbCfg->thumb_width = THUMB_WIDTH;
  pstThumbCfg->thumb_height = THUMB_HEIGHT;
  pstThumbCfg->photo_venc_chn = THUMB_PHOTO_VENC_CHN;
  pstThumbCfg->record_main_venc_chn = THUMB_RECORD_MAIN_VENC_CHN;
  pstThumbCfg->record_sub_venc_chn = THUMB_RECORD_SUB_VENC_CHN;
  pstThumbCfg->vpss_grp = THUMB_VPSS_GRP;
  pstThumbCfg->vpss_chn = THUMB_VPSS_CHN;
  pstThumbCfg->qfactor = 50;
  RKADK_PARAM_SaveThumbCfg(path, u32CamId);
}

static void RKADK_PARAM_Dump() {
  int i, j;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  if (get_log_level() < RKADK_LOG_LEVEL_DEBUG)
    return;

  printf("Version: %s\n", pstCfg->stVersion.version);

  printf("Common Config\n");
  printf("\tsensor_count: %d\n", pstCfg->stCommCfg.sensor_count);
  printf("\tspeaker_volume: %d\n", pstCfg->stCommCfg.speaker_volume);
  printf("\tmic_volume: %d\n", pstCfg->stCommCfg.mic_volume);
  printf("\tvpss_devcie: %d\n", pstCfg->stCommCfg.vpss_devcie);

  printf("Audio Config\n");
  printf("\tai_audio_node: %s\n", pstCfg->stAudioCfg.ai_audio_node);
  printf("\tai_depth: %d\n", pstCfg->stAudioCfg.ai_depth);
  printf("\tbit_width: %d\n", pstCfg->stAudioCfg.bit_width);
  printf("\tchannels: %d\n", pstCfg->stAudioCfg.channels);
  printf("\tmic_type: %d\n", pstCfg->stAudioCfg.mic_type);
  printf("\tsamplerate: %d\n", pstCfg->stAudioCfg.samplerate);
  printf("\tsamples_per_frame: %d\n", pstCfg->stAudioCfg.samples_per_frame);
  printf("\tbitrate: %d\n", pstCfg->stAudioCfg.bitrate);
  printf("\tvqe_mode: %d\n", pstCfg->stAudioCfg.vqe_mode);
  printf("\tvqe_mode: %s\n", pstCfg->stAudioCfg.vqe_config_path);
  printf("\tcodec_type: %d\n", pstCfg->stAudioCfg.codec_type);

  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    printf("Sensor[%d] Config\n", i);
    printf("\tsensor[%d] used_isp: %d\n", i, pstCfg->stSensorCfg[i].used_isp);
    printf("\tsensor[%d] max_width: %d\n", i, pstCfg->stSensorCfg[i].max_width);
    printf("\tsensor[%d] max_height: %d\n", i,
           pstCfg->stSensorCfg[i].max_height);
    printf("\tsensor[%d] framerate: %d\n", i, pstCfg->stSensorCfg[i].framerate);
    printf("\tsensor[%d] flip: %d\n", i, pstCfg->stSensorCfg[i].flip);
    printf("\tsensor[%d] mirror: %d\n", i, pstCfg->stSensorCfg[i].mirror);
    printf("\tsensor[%d] enable_wrap: %d\n", i, pstCfg->stSensorCfg[i].enable_wrap);
    printf("\tsensor[%d] wrap_buf_line: %d\n", i, pstCfg->stSensorCfg[i].wrap_buf_line);
    printf("\tsensor[%d] aenc_chn: %d\n", i, pstCfg->stSensorCfg[i].aenc_chn);
    printf("\tsensor[%d] mute: %d\n", i, pstCfg->stSensorCfg[i].mute);
    printf("\tsensor[%d] dev_id: %d\n", i, pstCfg->stSensorCfg[i].dev_id);

    printf("\tVI Config\n");
    for (j = 0; j < RKADK_ISPP_VI_NODE_CNT; j++) {
      printf("\t\tsensor[%d] VI[%d] device_name: %s\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].device_name);
      printf("\t\tsensor[%d] VI[%d] module: %s\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].module);
      printf("\t\tsensor[%d] VI[%d] chn_id: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].chn_id);
      printf("\t\tsensor[%d] VI[%d] buf_cnt: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].buf_cnt);
      printf("\t\tsensor[%d] VI[%d] depth: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].depth);
      printf("\t\tsensor[%d] VI[%d] pix_fmt: %s\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].pix_fmt);
      printf("\t\tsensor[%d] VI[%d] width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].width);
      printf("\t\tsensor[%d] VI[%d] height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].height);
    }

    printf("\tRecord Config\n");
    printf("\t\tsensor[%d] stRecCfg record_type: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.record_type);
    printf("\t\tsensor[%d] stRecCfg file_type: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.file_type);
    printf("\t\tsensor[%d] stRecCfg pre_record_time: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.pre_record_time);
    printf("\t\tsensor[%d] stRecCfg pre_record_mode: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.pre_record_mode);
    printf("\t\tsensor[%d] stRecCfg lapse_multiple: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.lapse_multiple);
    printf("\t\tsensor[%d] stRecCfg file_num: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.file_num);
    printf("\t\tsensor[%d] stRecCfg enable_audio: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.enable_audio);

    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      printf("\t\tsensor[%d] stRecCfg[%d] record_time: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.record_time_cfg[j].record_time);
      printf("\t\tsensor[%d] stRecCfg[%d] splite_time: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.record_time_cfg[j].splite_time);
      printf("\t\tsensor[%d] stRecCfg[%d] lapse_interval: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.record_time_cfg[j].lapse_interval);
      printf("\t\tsensor[%d] stRecCfg[%d] width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].max_width);
      printf("\t\tsensor[%d] stRecCfg[%d] height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].max_height);
      printf("\t\tsensor[%d] stRecCfg[%d] width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].width);
      printf("\t\tsensor[%d] stRecCfg[%d] height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].height);
      printf("\t\tsensor[%d] stRecCfg[%d] bufsize: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].bufsize);
      printf("\t\tsensor[%d] stRecCfg[%d] bitrate: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].bitrate);
      printf("\t\tsensor[%d] stRecCfg[%d] framerate: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].framerate);
      printf("\t\tsensor[%d] stRecCfg[%d] gop: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].gop);
      printf("\t\tsensor[%d] stRecCfg[%d] profile: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].profile);
      printf("\t\tsensor[%d] stRecCfg[%d] venc_chn: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_chn);
      printf("\t\tsensor[%d] stRecCfg[%d] enable_vpss: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].enable_vpss);
      printf("\t\tsensor[%d] stRecCfg[%d] vpss_grp: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].vpss_grp);
      printf("\t\tsensor[%d] stRecCfg[%d] vpss_chn: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].vpss_chn);
      printf("\t\tsensor[%d] stRecCfg[%d] post_aiisp: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].post_aiisp);
      printf("\t\tsensor[%d] stRecCfg[%d] codec_type: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].codec_type);
      printf("\t\tsensor[%d] stRecCfg[%d] rc_mode: %s\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].rc_mode);
      printf("\t\tsensor[%d] stRecCfg[%d] enable_pskip: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].enable_pskip);
    }

    printf("\tPhoto Config\n");
    printf("\t\tsensor[%d] stPhotoCfg image_width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.max_width);
    printf("\t\tsensor[%d] stPhotoCfg image_height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.max_height);
    printf("\t\tsensor[%d] stPhotoCfg image_width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.image_width);
    printf("\t\tsensor[%d] stPhotoCfg image_height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.image_height);
    printf("\t\tsensor[%d] stPhotoCfg venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.venc_chn);
    printf("\t\tsensor[%d] stPhotoCfg enable_vpss: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.enable_vpss);
    printf("\t\tsensor[%d] stPhotoCfg vpss_grp: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vpss_grp);
    printf("\t\tsensor[%d] stPhotoCfg vpss_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vpss_chn);
    printf("\t\tsensor[%d] stPhotoCfg post_aiisp: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.post_aiisp);
    printf("\t\tsensor[%d] stPhotoCfg enable_combo: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.enable_combo);
    printf("\t\tsensor[%d] stPhotoCfg combo_venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.combo_venc_chn);
    printf("\t\tsensor[%d] stPhotoCfg qfactor: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.qfactor);
    printf("\t\tsensor[%d] stPhotoCfg jpeg_slice: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.jpeg_slice);
    printf("\t\tsensor[%d] stPhotoCfg slice_height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.slice_height);
    printf("\t\tsensor[%d] stPhotoCfg max_slice_width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.max_slice_width);
    printf("\t\tsensor[%d] stPhotoCfg max_slice_height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.max_slice_height);

    printf("\tPreview Config\n");
    printf("\t\tsensor[%d] stStreamCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.max_width);
    printf("\t\tsensor[%d] stStreamCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.max_height);
    printf("\t\tsensor[%d] stStreamCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.width);
    printf("\t\tsensor[%d] stStreamCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.height);
    printf("\t\tsensor[%d] stStreamCfg bufsize: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.bufsize);
    printf("\t\tsensor[%d] stStreamCfg bitrate: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.bitrate);
    printf("\t\tsensor[%d] stStreamCfg framerate: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.framerate);
    printf("\t\tsensor[%d] stStreamCfg gop: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.gop);
    printf("\t\tsensor[%d] stStreamCfg profile: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.profile);
    printf("\t\tsensor[%d] stStreamCfg venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_chn);
    printf("\t\tsensor[%d] stStreamCfg vpss_grp: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.vpss_grp);
    printf("\t\tsensor[%d] stStreamCfg vpss_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.vpss_chn);
    printf("\t\tsensor[%d] stStreamCfg post_aiisp: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.post_aiisp);
    printf("\t\tsensor[%d] stStreamCfg codec_type: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.codec_type);
    printf("\t\tsensor[%d] stStreamCfg rc_mode: %s\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.rc_mode);
    printf("\t\tsensor[%d] stStreamCfg enable_pskip: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.enable_pskip);

    printf("\tLive Config\n");
    printf("\t\tsensor[%d] stLiveCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.max_width);
    printf("\t\tsensor[%d] stLiveCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.max_height);
    printf("\t\tsensor[%d] stLiveCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.width);
    printf("\t\tsensor[%d] stLiveCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.height);
    printf("\t\tsensor[%d] stLiveCfg bufsize: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.bufsize);
    printf("\t\tsensor[%d] stLiveCfg bitrate: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.bitrate);
    printf("\t\tsensor[%d] stLiveCfg framerate: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.framerate);
    printf("\t\tsensor[%d] stLiveCfg gop: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.gop);
    printf("\t\tsensor[%d] stLiveCfg profile: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.profile);
    printf("\t\tsensor[%d] stLiveCfg venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.venc_chn);
    printf("\t\tsensor[%d] stLiveCfg vpss_grp: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.vpss_grp);
    printf("\t\tsensor[%d] stLiveCfg vpss_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.vpss_chn);
    printf("\t\tsensor[%d] stLiveCfg post_aiisp: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.post_aiisp);
    printf("\t\tsensor[%d] stLiveCfg codec_type: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.codec_type);
    printf("\t\tsensor[%d] stLiveCfg rc_mode: %s\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.rc_mode);
    printf("\t\tsensor[%d] stLiveCfg enable_pskip: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.attribute.enable_pskip);
    printf("\t\tsensor[%d] stLiveCfg enable_audio: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.enable_audio);

    printf("\tDisplay Config\n");
    printf("\t\tsensor[%d] stDispCfg x: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.x);
    printf("\t\tsensor[%d] stDispCfg y: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.y);
    printf("\t\tsensor[%d] stDispCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.width);
    printf("\t\tsensor[%d] stDispCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.height);
    printf("\t\tsensor[%d] stDispCfg rotation: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.rotation);
    printf("\t\tsensor[%d] stDispCfg enable_vpss: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.enable_vpss);
    printf("\t\tsensor[%d] stDispCfg vpss_grp: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vpss_grp);
    printf("\t\tsensor[%d] stDispCfg vpss_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vpss_chn);
    printf("\t\tsensor[%d] stDispCfg img_type: %s\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.img_type);
    printf("\t\tsensor[%d] stDispCfg splice_mode: %s\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.splice_mode);
    printf("\t\tsensor[%d] stDispCfg intf_type: %s\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.intf_type);
    printf("\t\tsensor[%d] stDispCfg vo_device: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vo_device);
    printf("\t\tsensor[%d] stDispCfg vo_layer: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vo_layer);
    printf("\t\tsensor[%d] stDispCfg vo_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vo_chn);
    printf("\t\tsensor[%d] stDispCfg layer_buflen: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.layer_buflen);

    printf("\tThumb Config\n");
    printf("\t\tsensor[%d] stThumbCfg thumb_width: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.thumb_width);
    printf("\t\tsensor[%d] stThumbCfg thumb_height: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.thumb_height);
    printf("\t\tsensor[%d] stThumbCfg photo_venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.photo_venc_chn);
    printf("\t\tsensor[%d] stThumbCfg record_main_venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.record_main_venc_chn);
    printf("\t\tsensor[%d] stThumbCfg record_sub_venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.record_sub_venc_chn);
    printf("\t\tsensor[%d] stThumbCfg vpss_grp: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vpss_grp);
    printf("\t\tsensor[%d] stThumbCfg vpss_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vpss_chn);
    printf("\t\tsensor[%d] stThumbCfg qfactor: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.qfactor);
  }
}

static void RKADK_PARAM_DumpViAttr() {
  int i, j;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  if (get_log_level() < RKADK_LOG_LEVEL_DEBUG)
    return;

  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      printf("\tRec VI Attribute\n");
      printf("\t\tsensor[%d] stRecCfg[%d] stIspOpt.aEntityName: %s\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.stIspOpt.aEntityName);
      printf("\t\tsensor[%d] stRecCfg[%d] u32ViChn: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].u32ViChn);
      printf("\t\tsensor[%d] stRecCfg[%d] u32Width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.stSize.u32Width);
      printf("\t\tsensor[%d] stRecCfg[%d] u32Height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.stSize.u32Height);
      printf("\t\tsensor[%d] stRecCfg[%d] stIspOpt.u32BufCount: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.stIspOpt.u32BufCount);
      printf("\t\tsensor[%d] stRecCfg[%d] u32Depth: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.u32Depth);
      printf("\t\tsensor[%d] stRecCfg[%d] enPixelFormat: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.enPixelFormat);
      printf("\t\tsensor[%d] stRecCfg[%d] stIspOpt.enMemoryType: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.stIspOpt.enMemoryType);
    }

    printf("\tPhoto VI Attribute\n");
    printf("\t\tsensor[%d] stPhotoCfg stIspOpt.aEntityName: %s\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.stIspOpt.aEntityName);
    printf("\t\tsensor[%d] stPhotoCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stPhotoCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.stSize.u32Width);
    printf("\t\tsensor[%d] stPhotoCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.stSize.u32Height);
    printf("\t\tsensor[%d] stPhotoCfg stIspOpt.u32BufCount: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.stIspOpt.u32BufCount);
    printf("\t\tsensor[%d] stPhotoCfg u32Depth: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.u32Depth);
    printf("\t\tsensor[%d] stPhotoCfg enPixelFormat: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.enPixelFormat);
    printf("\t\tsensor[%d] stPhotoCfg stIspOpt.enMemoryType: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.stIspOpt.enMemoryType);

    printf("\tStream VI Attribute\n");
    printf("\t\tsensor[%d] stStreamCfg stIspOpt.aEntityName: %s\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.stIspOpt.aEntityName);
    printf("\t\tsensor[%d] stStreamCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stStreamCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.stSize.u32Width);
    printf("\t\tsensor[%d] stStreamCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.stSize.u32Height);
    printf("\t\tsensor[%d] stStreamCfg stIspOpt.u32BufCount: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.stIspOpt.u32BufCount);
    printf("\t\tsensor[%d] stStreamCfg u32Depth: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.u32Depth);
    printf("\t\tsensor[%d] stStreamCfg enPixelFormat: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.enPixelFormat);
    printf("\t\tsensor[%d] stStreamCfg stIspOpt.enMemoryType: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.stIspOpt.enMemoryType);

    printf("\tLive VI Attribute\n");
    printf("\t\tsensor[%d] stLiveCfg stIspOpt.aEntityName: %s\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.stIspOpt.aEntityName);
    printf("\t\tsensor[%d] stLiveCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stLiveCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.stSize.u32Width);
    printf("\t\tsensor[%d] stLiveCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.stSize.u32Height);
    printf("\t\tsensor[%d] stLiveCfg stIspOpt.u32BufCount: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.stIspOpt.u32BufCount);
    printf("\t\tsensor[%d] stLiveCfg u32Depth: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.u32Depth);
    printf("\t\tsensor[%d] stLiveCfg enPixelFormat: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.enPixelFormat);
    printf("\t\tsensor[%d] stLiveCfg stIspOpt.enMemoryType: %d\n", i,
           pstCfg->stMediaCfg[i].stLiveCfg.vi_attr.stChnAttr.stIspOpt.enMemoryType);

    printf("\tDisplay VI Attribute\n");
    printf("\t\tsensor[%d] stDispCfg stIspOpt.aEntityName: %s\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.stIspOpt.aEntityName);
    printf("\t\tsensor[%d] stDispCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stDispCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.stSize.u32Width);
    printf("\t\tsensor[%d] stDispCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.stSize.u32Height);
    printf("\t\tsensor[%d] stDispCfg stIspOpt.u32BufCount: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.stIspOpt.u32BufCount);
    printf("\t\tsensor[%d] stDispCfg u32Depth: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.u32Depth);
    printf("\t\tsensor[%d] stDispCfg enPixelFormat: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.enPixelFormat);
    printf("\t\tsensor[%d] stDispCfg stIspOpt.enMemoryType: %d\n", i,
           pstCfg->stMediaCfg[i].stDispCfg.vi_attr.stChnAttr.stIspOpt.enMemoryType);

    printf("\tThumb VI Attribute\n");
    printf("\t\tsensor[%d] stThumbCfg stIspOpt.aEntityName: %s\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.stIspOpt.aEntityName);
    printf("\t\tsensor[%d] stThumbCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stThumbCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.stSize.u32Width);
    printf("\t\tsensor[%d] stThumbCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.stSize.u32Height);
    printf("\t\tsensor[%d] stThumbCfg stIspOpt.u32BufCount: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.stIspOpt.u32BufCount);
    printf("\t\tsensor[%d] stThumbCfg u32Depth: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.u32Depth);
    printf("\t\tsensor[%d] stThumbCfg enPixelFormat: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.enPixelFormat);
    printf("\t\tsensor[%d] stThumbCfg stIspOpt.enMemoryType: %d\n", i,
           pstCfg->stMediaCfg[i].stThumbCfg.vi_attr.stChnAttr.stIspOpt.enMemoryType);
  }
}

static RKADK_S32 RKADK_PARAM_LoadParam(char *path,
                                       char (*sensorPath)[RKADK_PATH_LEN]) {
  int i, j, ret = 0;
  RKADK_MAP_TABLE_CFG_S *pstMapTableCfg = NULL;
  RKADK_PARAM_MAP_TYPE_E enMapType = RKADK_PARAM_MAP_BUTT;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;
  char entry[RKADK_BUFFER_LEN] = {'\0'};

  if (!RKADK_PARAM_CheckVersion(path))
    return -1;

  // load common config
  memset(&pstCfg->stCommCfg, 0, sizeof(RKADK_PARAM_COMM_CFG_S));
  ret = RKADK_Ini2Struct(-1, path, &pstCfg->stCommCfg, g_stCommCfgMapTable,
                         sizeof(g_stCommCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGW("common config param not exist, use default");
    return ret;
  }
  RKADK_PARAM_CheckCommCfg(path);

  // load audio config
  memset(&pstCfg->stAudioCfg, 0, sizeof(RKADK_PARAM_AUDIO_CFG_S));
  ret = RKADK_Ini2Struct(-1, path, &pstCfg->stAudioCfg, g_stAudioCfgMapTable,
                         sizeof(g_stAudioCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGW("audio config param not exist, use default");
    return ret;
  }
  RKADK_PARAM_CheckAudioCfg(path);

  // load sensor config
  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_SENSOR_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    memset(&pstCfg->stSensorCfg[i], 0, sizeof(RKADK_PARAM_SENSOR_CFG_S));
    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stSensorCfg[i],
                           pstMapTableCfg->pstMapTable,
                           pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] config param not exist, use default", i);
      return ret;
    }
    RKADK_PARAM_CheckSensorCfg(sensorPath[i], i);

    // load vi config
    for (j = 0; j < RKADK_ISPP_VI_NODE_CNT; j++) {
      if (j == 0) {
        enMapType = RKADK_PARAM_VI0_MAP;
      } else if (j == 1) {
        enMapType = RKADK_PARAM_VI1_MAP;
      } else if (j == 2) {
        enMapType = RKADK_PARAM_VI2_MAP;
      } else {
        enMapType = RKADK_PARAM_VI3_MAP;
      }

      pstMapTableCfg = RKADK_PARAM_GetMapTable(i, enMapType);
      RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

      memset(&pstCfg->stMediaCfg[i].stViCfg[j], 0,
             sizeof(RKADK_PARAM_VI_CFG_S));
      ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stViCfg[j],
                             pstMapTableCfg->pstMapTable,
                             pstMapTableCfg->u32TableLen);

      if (ret) {
        RKADK_LOGW("sensor[%d] vi[%d] config param not exist, use default", i,
                   j);
        return ret;
      }
      RKADK_PARAM_CheckViCfg(sensorPath[i], i, j);
    }

    // load record config
    memset(&pstCfg->stMediaCfg[i].stRecCfg, 0, sizeof(RKADK_PARAM_REC_CFG_S));
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_REC_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stRecCfg,
                           pstMapTableCfg->pstMapTable,
                           pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] record param not exist, use default", i);
      return ret;
    }

    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      RKADK_MAP_TABLE_CFG_S *pstTimeMapTable = NULL;

      memset(&pstCfg->stMediaCfg[i].stRecCfg.attribute[j], 0,
             sizeof(RKADK_PARAM_VENC_ATTR_S));
      if (j == 0) {
        pstTimeMapTable = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_REC_MAIN_TIME_MAP);
        pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_REC_MAIN_MAP);
        snprintf(entry, RKADK_BUFFER_LEN, "record.0:enable_pskip");
      } else {
        pstTimeMapTable = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_REC_SUB_TIME_MAP);
        pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_REC_SUB_MAP);
        snprintf(entry, RKADK_BUFFER_LEN, "record.1:enable_pskip");
      }

      if (!pstMapTableCfg || !pstTimeMapTable)
        return -1;

      ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stRecCfg.record_time_cfg[j],
                              pstTimeMapTable->pstMapTable, pstTimeMapTable->u32TableLen);
      if (ret) {
        RKADK_LOGW("sensor[%d] rec time[%d] not exist, use default", i, j);
        return ret;
      }

      ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stRecCfg.attribute[j],
                              pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
      if (ret) {
        RKADK_LOGW("sensor[%d] rec attribute[%d] param not exist, use default",
                   i, j);
        return ret;
      }

      // load record enable_pskip
      pstCfg->stMediaCfg[i].stRecCfg.attribute[j].enable_pskip = RKADK_IniGetBool(i, entry, 0);
    }
    RKADK_PARAM_CheckRecCfg(sensorPath[i], i);

    // load preview config
    memset(&pstCfg->stMediaCfg[i].stStreamCfg.attribute, 0,
           sizeof(RKADK_PARAM_VENC_ATTR_S));
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_PREVIEW_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stStreamCfg.attribute,
                            pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] stream config param not exist, use default", i);
      return ret;
    }

    // load preview enable_pskip
    snprintf(entry, RKADK_BUFFER_LEN, "preview:enable_pskip");
    pstCfg->stMediaCfg[i].stStreamCfg.attribute.enable_pskip = RKADK_IniGetBool(i, entry, 0);

    RKADK_PARAM_CheckStreamCfg(sensorPath[i], i, RKADK_STREAM_TYPE_PREVIEW);

    // load live config
    memset(&pstCfg->stMediaCfg[i].stLiveCfg.attribute, 0,
           sizeof(RKADK_PARAM_VENC_ATTR_S));
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_LIVE_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stLiveCfg.attribute,
                            pstMapTableCfg->pstMapTable, pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] live config param not exist, use default", i);
      return ret;
    }

    // load live enable_pskip
    snprintf(entry, RKADK_BUFFER_LEN, "live:enable_pskip");
    pstCfg->stMediaCfg[i].stLiveCfg.attribute.enable_pskip = RKADK_IniGetBool(i, entry, 0);

    // load live enable_audio
    snprintf(entry, RKADK_BUFFER_LEN, "live:enable_audio");
    pstCfg->stMediaCfg[i].stLiveCfg.enable_audio = RKADK_IniGetBool(i, entry, 1);

    RKADK_PARAM_CheckStreamCfg(sensorPath[i], i, RKADK_STREAM_TYPE_LIVE);

    // load photo config
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_PHOTO_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    memset(&pstCfg->stMediaCfg[i].stPhotoCfg, 0,
           sizeof(RKADK_PARAM_PHOTO_CFG_S));
    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stPhotoCfg,
                           pstMapTableCfg->pstMapTable,
                           pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] photo config param not exist, use default", i);
      return ret;
    }
    RKADK_PARAM_CheckPhotoCfg(sensorPath[i], i);

    // load display config
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_DISP_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    memset(&pstCfg->stMediaCfg[i].stDispCfg, 0, sizeof(RKADK_PARAM_DISP_CFG_S));
    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stDispCfg,
                           pstMapTableCfg->pstMapTable,
                           pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("sensor[%d] display config param not exist, use default", i);
      return ret;
    }

    // load live enable_audio
    snprintf(entry, RKADK_BUFFER_LEN, "display:enable_vpss");
    pstCfg->stMediaCfg[i].stDispCfg.enable_vpss = RKADK_IniGetBool(i, entry, 1);
    RKADK_PARAM_CheckDispCfg(sensorPath[i], i);

    // load thumb config
    pstMapTableCfg = RKADK_PARAM_GetMapTable(i, RKADK_PARAM_THUMB_MAP);
    RKADK_CHECK_POINTER(pstMapTableCfg, RKADK_FAILURE);

    memset(&pstCfg->stMediaCfg[i].stThumbCfg, 0, sizeof(RKADK_PARAM_THUMB_CFG_S));
    ret = RKADK_Ini2Struct(i, sensorPath[i], &pstCfg->stMediaCfg[i].stThumbCfg,
                           pstMapTableCfg->pstMapTable,
                           pstMapTableCfg->u32TableLen);
    if (ret) {
      RKADK_LOGW("thumb config param not exist, use default");
      return ret;
    }
    RKADK_PARAM_CheckThumbCfg(sensorPath[i], i);
  }

  return 0;
}

static void RKADK_PARAM_UseDefault() {
  int i;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_STREAM_TYPE_E enStrmType = RKADK_STREAM_TYPE_BUTT;

  // default version
  RKADK_PARAM_VERSION_S *pstVersion = &g_stPARAMCtx.stCfg.stVersion;
  memcpy(pstVersion->version, RKADK_PARAM_VERSION, strlen(RKADK_PARAM_VERSION));
  RKADK_PARAM_SaveVersion(g_stPARAMCtx.path);

  // default common config
  RKADK_PARAM_DefCommCfg(g_stPARAMCtx.path);

  // default audio config
  RKADK_PARAM_DefAudioCfg(g_stPARAMCtx.path);

  // default sensor.0 config
  RKADK_PARAM_DefSensorCfg(0, g_stPARAMCtx.sensorPath[0]);

  // default vi config
  for (i = 0; i < RKADK_ISPP_VI_NODE_CNT; i++)
    RKADK_PARAM_DefViCfg(0, i, g_stPARAMCtx.sensorPath[0]);

  // default sensor.0.rec config
  RKADK_PARAM_DefRecCfg(0, g_stPARAMCtx.sensorPath[0]);

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stRecCfg;
  for (i = 0; i < (int)pstRecCfg->file_num; i++) {
    if (i == 0)
      enStrmType = RKADK_STREAM_TYPE_VIDEO_MAIN;
    else
      enStrmType = RKADK_STREAM_TYPE_VIDEO_SUB;

    RKADK_PARAM_DefRecAttr(0, enStrmType, g_stPARAMCtx.sensorPath[0]);
    RKADK_PARAM_DefRecTime(0, enStrmType, g_stPARAMCtx.sensorPath[0]);
  }

  // default sensor.0.photo config
  RKADK_PARAM_DefPhotoCfg(0, g_stPARAMCtx.sensorPath[0]);

  // default sensor.0.stream config
  RKADK_PARAM_DefStreamCfg(0, g_stPARAMCtx.sensorPath[0],
                           RKADK_STREAM_TYPE_PREVIEW);

  // default sensor.0.live config
  RKADK_PARAM_DefStreamCfg(0, g_stPARAMCtx.sensorPath[0],
                           RKADK_STREAM_TYPE_LIVE);

  // default sensor.0.disp config
  RKADK_PARAM_DefDispCfg(0, g_stPARAMCtx.sensorPath[0]);

  // default sensor.0.thumb config
  RKADK_PARAM_DefThumbCfg(0, g_stPARAMCtx.sensorPath[0]);
}

int RKADK_PARAM_Mkdirs(const char *dir) {
  RKADK_CHECK_POINTER(dir, -1);

  if (strlen(dir) == 0) {
      RKADK_LOGE("strlen(%s) is 0", dir);
      return -1;
  }

  // Copy the path to avoid modifying the original string
  char *path = strdup(dir);
  if (!path)
    return -1;

  char *p = path;
  // Skip the beginning '/' (absolute path)
  if (*p == '/') p++;

  while (*p) {
    // Find the next '/'
    while (*p && *p != '/') p++;

    // Temporary truncation, create current level directory
    char temp = *p;
    *p = '\0';

    // Check and create directory (using full path)
    if (access(path, F_OK) != 0) { // Directory does not exist
      if (mkdir(path, 0777) != 0) {
        RKADK_LOGE("mkdir %s failed: %s", path, strerror(errno));
        free(path);
        return -1;
      }
    }

    // Restore the path and continue processing the next level
    *p = temp;
    if (*p) p++; // Skip '/'
  }

  free(path);
  return 0;
}

static RKADK_S32 RKADK_PARAM_LoadDefault() {
  int ret;
  int bufLen = RKADK_PATH_LEN * 2 + 4;
  char buffer[bufLen];
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  ret = RKADK_PARAM_LoadParam(g_stPARAMCtx.defPath, g_stPARAMCtx.defSensorPath);
  if (ret) {
    RKADK_LOGE("load default ini failed");
    return ret;
  }

  char *ptr = strrchr(g_stPARAMCtx.path, '/');
  if (ptr) {
    memset(buffer, 0, bufLen);
    memcpy(buffer, g_stPARAMCtx.path, strlen(g_stPARAMCtx.path) - strlen(ptr));
    if (access(buffer, F_OK))
      RKADK_PARAM_Mkdirs(buffer);
  }

  memset(buffer, 0, bufLen);
  sprintf(buffer, "cp %s %s", g_stPARAMCtx.defPath, g_stPARAMCtx.path);
  RKADK_LOGD("%s", buffer);
  system(buffer);

  for (int i = 0; i < (int)pstCommCfg->sensor_count; i++) {
    memset(buffer, 0, bufLen);
    sprintf(buffer, "cp %s %s", g_stPARAMCtx.defSensorPath[i], g_stPARAMCtx.sensorPath[i]);
    RKADK_LOGD("%s", buffer);
    system(buffer);
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_MatchViIndex(RKADK_STREAM_TYPE_E enStrmType,
                                          RKADK_S32 s32CamId, RKADK_U32 width,
                                          RKADK_U32 height, bool bEnableVpss) {
  int index;
  int matchViCnt = 0;
  int matchViIndex[RKADK_ISPP_VI_NODE_CNT] = {0};
  char module[RKADK_BUFFER_LEN];
  bool bSaveViCfg = false;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stPhotoCfg;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);
  memset(module, 0, RKADK_BUFFER_LEN);

  if (!bEnableVpss)
    bSaveViCfg = true;

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    memcpy(module, "RECORD_MAIN", strlen("RECORD_MAIN"));
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    memcpy(module, "RECORD_SUB", strlen("RECORD_SUB"));
    break;

  case RKADK_STREAM_TYPE_SNAP:
    memcpy(module, "PHOTO", strlen("PHOTO"));
    if (pstPhotoCfg->jpeg_slice) {
      if (bSaveViCfg)
        RKADK_LOGD("enable jpeg slice, unsupport change vi resolution");
      bSaveViCfg = false;
    }
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
    memcpy(module, "PREVIEW", strlen("PREVIEW"));
    break;

  case RKADK_STREAM_TYPE_LIVE:
    memcpy(module, "LIVE", strlen("LIVE"));
    break;

  case RKADK_STREAM_TYPE_DISP:
    memcpy(module, "DISP", strlen("DISP"));
    break;

  case RKADK_STREAM_TYPE_THUMB:
    memcpy(module, "THUMB", strlen("THUMB"));
    break;

  default:
    RKADK_LOGE("Invaild mode = %d", enStrmType);
    return -1;
  }

  for (index = 0; index < RKADK_ISPP_VI_NODE_CNT; index++) {
    pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
    if (strstr(pstViCfg->module, module)) {
      if (!pstSensorCfg->used_isp) {
        RKADK_LOGD("Sensor[%d] %s[%d x %d] match VI[%d][%d x %d]", s32CamId,
                   module, width, height, index, pstViCfg->width,
                   pstViCfg->height);
        return index;
      } else {
        if(pstViCfg->width == width && pstViCfg->height == height) {
          RKADK_LOGD("Sensor[%d] %s[%d x %d] match VI[%d][%d x %d]", s32CamId,
                     module, width, height, index, pstViCfg->width,
                     pstViCfg->height);
          return index;
        }
        matchViIndex[matchViCnt] = index;
        matchViCnt++;
      }
    }
  }

  if (!matchViCnt) {
    RKADK_LOGE("Sensor[%d] %s[%d x %d] not find matched VI",
               s32CamId, module, width, height);
    return -1;
  } else if (matchViCnt == 1) {
    index = matchViIndex[0];
  } else {
    if (width >= pstSensorCfg->max_width || height >= pstSensorCfg->max_height) {
      index = matchViIndex[0];
#ifdef RV1126_1109
      //1126/1109 nonsupport isp enlarge
      bSaveViCfg = false;
#endif
    } else {
      index = matchViIndex[1];
    }
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
  RKADK_LOGD("Sensor[%d] %s[%d x %d] match vi.%d[%d x %d] bSaveViCfg[%d]\n", s32CamId,
             module, width, height, index, pstViCfg->width, pstViCfg->height, bSaveViCfg);

#ifdef RV1126_1109
   if (!strcmp(pstViCfg->device_name, "rkispp_scale0")) {
     if (!bEnableVpss && ((width != pstSensorCfg->max_width) &&
         (width > RKISPP_SCALE0_NV12_WIDTH_MAX))) {
       if (strcmp(pstViCfg->pix_fmt, "NV16")) {
         RKADK_LOGW("rkispp_scale0 resolution[%d*%d] > 2K, must NV16",
                    pstViCfg->width, pstViCfg->height);
         memcpy(pstViCfg->pix_fmt, "NV16", strlen("NV16"));
         bSaveViCfg = true;
       }
     }
   }
#endif

  if (bSaveViCfg) {
    pstViCfg->width = width;
    pstViCfg->height = height;
    RKADK_PARAM_SaveViCfg(g_stPARAMCtx.sensorPath[s32CamId], index, s32CamId);
  }
  return index;
}

RKADK_VO_INTF_TYPE_E RKADK_PARAM_GetSpliceMode(char *spliceMode) {
  RKADK_VO_SPLICE_MODE_E enVoSpliceMode = SPLICE_MODE_RGA;

  RKADK_CHECK_POINTER(spliceMode, DISPLAY_TYPE_DEFAULT);

  if (!strcmp(spliceMode, "RGA"))
     enVoSpliceMode = SPLICE_MODE_RGA;
  else if (!strcmp(spliceMode, "GPU"))
     enVoSpliceMode = SPLICE_MODE_GPU;
  else if (!strcmp(spliceMode, "BYPASS"))
     enVoSpliceMode = SPLICE_MODE_BYPASS;
  else
    RKADK_LOGE("Invalid spliceMode: %s, use RGA", spliceMode);

  return enVoSpliceMode;
}

RKADK_VO_INTF_TYPE_E RKADK_PARAM_GetIntfType(char *intfType) {
  RKADK_VO_INTF_TYPE_E enIntfType = DISPLAY_TYPE_DEFAULT;

  RKADK_CHECK_POINTER(intfType, DISPLAY_TYPE_DEFAULT);

  if (!strcmp(intfType, "MIPI"))
    enIntfType = DISPLAY_TYPE_MIPI;
  else if (!strcmp(intfType, "HDMI"))
    enIntfType = DISPLAY_TYPE_HDMI;
  else if (!strcmp(intfType, "EDP"))
    enIntfType = DISPLAY_TYPE_EDP;
  else if (!strcmp(intfType, "VGA"))
    enIntfType = DISPLAY_TYPE_VGA;
  else if (!strcmp(intfType, "DP"))
    enIntfType = DISPLAY_TYPE_DP;
  else if (!strcmp(intfType, "HDMI_EDP"))
    enIntfType = DISPLAY_TYPE_HDMI_EDP;
  else if (!strcmp(intfType, "LCD"))
    enIntfType = DISPLAY_TYPE_LCD;
  else if (!strcmp(intfType, "default"))
    enIntfType = DISPLAY_TYPE_DEFAULT;
  else
    RKADK_LOGE("Invalid intfType: %s, use default", intfType);

  return enIntfType;
}

PIXEL_FORMAT_E RKADK_PARAM_GetPixFmt(char *pixFmt,
                                     COMPRESS_MODE_E *enCompressMode) {
  PIXEL_FORMAT_E enPixelFormat = RK_FMT_YUV420SP;

  RKADK_CHECK_POINTER(pixFmt, RK_FMT_BUTT);
  RKADK_CHECK_POINTER(enCompressMode, RK_FMT_BUTT);

  *enCompressMode = COMPRESS_MODE_NONE;

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (!strcmp(pixFmt, "RKFBC0")) {
    enPixelFormat = RK_FMT_YUV420SP;
    *enCompressMode = COMPRESS_RFBC_64x4;
    return enPixelFormat;
  } else if (!strcmp(pixFmt, "RKFBC2")) {
    enPixelFormat = RK_FMT_YUV422SP;
    *enCompressMode = COMPRESS_RFBC_64x4;
    return enPixelFormat;
  }
#endif

  if (!strcmp(pixFmt, "FBC0") || !strcmp(pixFmt, "FBC2")) {
    enPixelFormat = RK_FMT_YUV420SP;
    *enCompressMode = COMPRESS_AFBC_16x16;
  } else if (!strcmp(pixFmt, "NV12"))
    enPixelFormat = RK_FMT_YUV420SP;
  else if (!strcmp(pixFmt, "YUV420SP_10BIT"))
    enPixelFormat = RK_FMT_YUV420SP_10BIT;
  else if (!strcmp(pixFmt, "NV16"))
    enPixelFormat = RK_FMT_YUV422SP;
  else if (!strcmp(pixFmt, "UYVY"))
    enPixelFormat = RK_FMT_YUV422_UYVY;
  else if (!strcmp(pixFmt, "YUV422SP_10BIT"))
    enPixelFormat = RK_FMT_YUV422SP_10BIT;
  else if (!strcmp(pixFmt, "YUV420P"))
    enPixelFormat = RK_FMT_YUV420P;
  else if (!strcmp(pixFmt, "YUV420P_VU"))
    enPixelFormat = RK_FMT_YUV420P_VU;
  else if (!strcmp(pixFmt, "NV21"))
    enPixelFormat = RK_FMT_YUV420SP_VU;
  else if (!strcmp(pixFmt, "YUV422P"))
    enPixelFormat = RK_FMT_YUV422P;
  else if (!strcmp(pixFmt, "YUV422SP_VU"))
    enPixelFormat = RK_FMT_YUV422SP_VU;
  else if (!strcmp(pixFmt, "YUV422_YUYV"))
    enPixelFormat = RK_FMT_YUV422_YUYV;
  else if (!strcmp(pixFmt, "YUV422_UYVY"))
    enPixelFormat = RK_FMT_YUV422_UYVY;
  else if (!strcmp(pixFmt, "YUV400SP"))
    enPixelFormat = RK_FMT_YUV400SP;
  else if (!strcmp(pixFmt, "YUV440SP"))
    enPixelFormat = RK_FMT_YUV440SP;
  else if (!strcmp(pixFmt, "YUV411SP"))
    enPixelFormat = RK_FMT_YUV411SP;
  else if (!strcmp(pixFmt, "YUV444"))
    enPixelFormat = RK_FMT_YUV444;
  else if (!strcmp(pixFmt, "YUV444SP"))
    enPixelFormat = RK_FMT_YUV444SP;
  else if (!strcmp(pixFmt, "YUV422_YVYU"))
    enPixelFormat = RK_FMT_YUV422_YVYU;
  else if (!strcmp(pixFmt, "YUV422_VYUY"))
    enPixelFormat = RK_FMT_YUV422_VYUY;
  else if (!strcmp(pixFmt, "RGB565"))
    enPixelFormat = RK_FMT_RGB565;
  else if (!strcmp(pixFmt, "BGR565"))
    enPixelFormat = RK_FMT_BGR565;
  else if (!strcmp(pixFmt, "RGB555"))
    enPixelFormat = RK_FMT_RGB555;
  else if (!strcmp(pixFmt, "BGR555"))
    enPixelFormat = RK_FMT_BGR555;
  else if (!strcmp(pixFmt, "RGB444"))
    enPixelFormat = RK_FMT_RGB444;
  else if (!strcmp(pixFmt, "BGR444"))
    enPixelFormat = RK_FMT_BGR444;
  else if (!strcmp(pixFmt, "RGB888"))
    enPixelFormat = RK_FMT_RGB888;
  else if (!strcmp(pixFmt, "BGR888"))
    enPixelFormat = RK_FMT_BGR888;
  else if (!strcmp(pixFmt, "RGB101010"))
    enPixelFormat = RK_FMT_RGB101010;
  else if (!strcmp(pixFmt, "BGR101010"))
    enPixelFormat = RK_FMT_BGR101010;
  else if (!strcmp(pixFmt, "ARGB1555"))
    enPixelFormat = RK_FMT_ARGB1555;
  else if (!strcmp(pixFmt, "ABGR1555"))
    enPixelFormat = RK_FMT_ABGR1555;
  else if (!strcmp(pixFmt, "ARGB4444"))
    enPixelFormat = RK_FMT_ARGB4444;
  else if (!strcmp(pixFmt, "ABGR4444"))
    enPixelFormat = RK_FMT_ABGR4444;
  else if (!strcmp(pixFmt, "ARGB8565"))
    enPixelFormat = RK_FMT_ARGB8565;
  else if (!strcmp(pixFmt, "ABGR8565"))
    enPixelFormat = RK_FMT_ABGR8565;
  else if (!strcmp(pixFmt, "ARGB8888"))
    enPixelFormat = RK_FMT_ARGB8888;
  else if (!strcmp(pixFmt, "ABGR8888"))
    enPixelFormat = RK_FMT_ABGR8888;
  else if (!strcmp(pixFmt, "BGRA8888"))
    enPixelFormat = RK_FMT_BGRA8888;
  else if (!strcmp(pixFmt, "RGBA8888"))
    enPixelFormat = RK_FMT_RGBA8888;
  else if (!strcmp(pixFmt, "RGBA5551"))
    enPixelFormat = RK_FMT_RGBA5551;
  else if (!strcmp(pixFmt, "BGRA5551"))
    enPixelFormat = RK_FMT_BGRA5551;
  else if (!strcmp(pixFmt, "BGRA4444"))
    enPixelFormat = RK_FMT_BGRA4444;
  else if (!strcmp(pixFmt, "RGB_BAYER_SBGGR_8BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SBGGR_8BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGBRG_8BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGBRG_8BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGRBG_8BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGRBG_8BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SRGGB_8BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SRGGB_8BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SBGGR_10BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SBGGR_10BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGBRG_10BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGBRG_10BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGRBG_10BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGRBG_10BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SRGGB_10BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SRGGB_10BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SBGGR_12BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SBGGR_12BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGBRG_12BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGBRG_12BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SGRBG_12BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SGRBG_12BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SRGGB_12BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SRGGB_12BPP;
  else if (!strcmp(pixFmt, "RGB_BAYER_SBGGR_16BPP"))
    enPixelFormat = RK_FMT_RGB_BAYER_SBGGR_16BPP;
  else
    RKADK_LOGE("Invalid image type: %s", pixFmt);

  return enPixelFormat;
}

static RKADK_S32 RKADK_PARAM_SetStreamViAttr(RKADK_S32 s32CamId,
                                             RKADK_STREAM_TYPE_E enStrmType) {
  int index;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);

  if (enStrmType == RKADK_STREAM_TYPE_PREVIEW)
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stStreamCfg;
  else
    pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stLiveCfg;

  index = RKADK_PARAM_MatchViIndex(enStrmType, s32CamId,
                                   pstStreamCfg->attribute.width,
                                   pstStreamCfg->attribute.height,
                                   pstStreamCfg->attribute.enable_vpss);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
  pstStreamCfg->vi_attr.index = index;
  pstStreamCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  memcpy(pstStreamCfg->vi_attr.stChnAttr.stIspOpt.aEntityName,
         pstViCfg->device_name, sizeof(pstViCfg->device_name));
  pstStreamCfg->vi_attr.stChnAttr.stIspOpt.u32BufCount = pstViCfg->buf_cnt;
  pstStreamCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  if (!pstSensorCfg->used_isp) {
    //only support MPLANE&&MMAP if not use libv4l2
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
  }
#endif
  if (pstStreamCfg->attribute.enable_vpss) {
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstViCfg->width;
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstViCfg->height;
  } else {
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstStreamCfg->attribute.max_width;
    pstStreamCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstStreamCfg->attribute.max_height;
  }

  pstStreamCfg->vi_attr.stChnAttr.stSize.u32Width = pstViCfg->width;
  pstStreamCfg->vi_attr.stChnAttr.stSize.u32Height = pstViCfg->height;
  pstStreamCfg->vi_attr.stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(
      pstViCfg->pix_fmt, &(pstStreamCfg->vi_attr.stChnAttr.enCompressMode));
  pstStreamCfg->vi_attr.stChnAttr.u32Depth = pstViCfg->depth;
  pstStreamCfg->vi_attr.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  pstStreamCfg->vi_attr.stChnAttr.stFrameRate.s32DstFrameRate = -1;

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (pstStreamCfg->vi_attr.stChnAttr.enCompressMode == COMPRESS_RFBC_64x4 && pstViCfg->chn_id != 2 && pstViCfg->chn_id != 3) {
    RKADK_LOGE("only support chn 2 or 3 channel to use COMPRESS_RFBC_64x4");
    return -1;
  }
#endif

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetPhotoViAttr(RKADK_S32 s32CamId) {
  int index;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stPhotoCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);

  index = RKADK_PARAM_MatchViIndex(RKADK_STREAM_TYPE_SNAP, s32CamId,
                                   pstPhotoCfg->image_width,
                                   pstPhotoCfg->image_height,
                                   pstPhotoCfg->enable_vpss);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
  pstPhotoCfg->vi_attr.index = index;
  pstPhotoCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  memcpy(pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.aEntityName,
         pstViCfg->device_name, sizeof(pstViCfg->device_name));
  pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.u32BufCount = pstViCfg->buf_cnt;
  pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  if (!pstSensorCfg->used_isp) {
    //only support MPLANE&&MMAP if not use libv4l2
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
  }
#endif

  if (pstPhotoCfg->enable_vpss || pstPhotoCfg->jpeg_slice) {
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstViCfg->width;
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstViCfg->height;
  } else {
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstPhotoCfg->max_width;
    pstPhotoCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstPhotoCfg->max_height;
  }
  pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Width = pstViCfg->width;
  pstPhotoCfg->vi_attr.stChnAttr.stSize.u32Height = pstViCfg->height;
  pstPhotoCfg->vi_attr.stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(
      pstViCfg->pix_fmt, &(pstPhotoCfg->vi_attr.stChnAttr.enCompressMode));
  pstPhotoCfg->vi_attr.stChnAttr.u32Depth = pstViCfg->depth;
  pstPhotoCfg->vi_attr.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  pstPhotoCfg->vi_attr.stChnAttr.stFrameRate.s32DstFrameRate = -1;

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (pstPhotoCfg->vi_attr.stChnAttr.enCompressMode == COMPRESS_RFBC_64x4 && pstViCfg->chn_id != 2 && pstViCfg->chn_id != 3) {
    RKADK_LOGE("only support chn 2 or 3 channel to use COMPRESS_RFBC_64x4");
    return -1;
  }
#endif
  return 0;
}

static RKADK_S32 RKADK_PARAM_SetRecViAttr(RKADK_S32 s32CamId) {
  int i, index;
  RKADK_STREAM_TYPE_E enStrmType = RKADK_STREAM_TYPE_BUTT;
  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);

  for (i = 0; i < (int)pstRecCfg->file_num; i++) {
    if (i == 0)
      enStrmType = RKADK_STREAM_TYPE_VIDEO_MAIN;
    else
      enStrmType = RKADK_STREAM_TYPE_VIDEO_SUB;

    index = RKADK_PARAM_MatchViIndex(enStrmType, s32CamId,
                                     pstRecCfg->attribute[i].width,
                                     pstRecCfg->attribute[i].height,
                                     pstRecCfg->attribute[i].enable_vpss);
    if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
      RKADK_LOGE("not find match vi index");
      return -1;
    }

    pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
    pstRecCfg->vi_attr[i].index = index;
    pstRecCfg->vi_attr[i].u32ViChn = pstViCfg->chn_id;
    memcpy(pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.aEntityName,
           pstViCfg->device_name, sizeof(pstViCfg->device_name));
    pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.u32BufCount = pstViCfg->buf_cnt;
    pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
    if (!pstSensorCfg->used_isp) {
      //only support MPLANE&&MMAP if not use libv4l2
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
    }
#endif

    if (pstRecCfg->attribute[i].enable_vpss) {
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.stMaxSize.u32Width = pstViCfg->width;
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.stMaxSize.u32Height = pstViCfg->height;
    } else {
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.stMaxSize.u32Width = pstRecCfg->attribute[i].max_width;
      pstRecCfg->vi_attr[i].stChnAttr.stIspOpt.stMaxSize.u32Height = pstRecCfg->attribute[i].max_height;
    }
    pstRecCfg->vi_attr[i].stChnAttr.stSize.u32Width = pstViCfg->width;
    pstRecCfg->vi_attr[i].stChnAttr.stSize.u32Height = pstViCfg->height;
    pstRecCfg->vi_attr[i].stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(
        pstViCfg->pix_fmt, &(pstRecCfg->vi_attr[i].stChnAttr.enCompressMode));
    pstRecCfg->vi_attr[i].stChnAttr.u32Depth = pstViCfg->depth;
    pstRecCfg->vi_attr[i].stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    pstRecCfg->vi_attr[i].stChnAttr.stFrameRate.s32DstFrameRate = -1;
  }

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (pstRecCfg->vi_attr[i].stChnAttr.enCompressMode == COMPRESS_RFBC_64x4 && pstViCfg->chn_id != 2 && pstViCfg->chn_id != 3) {
    RKADK_LOGE("only support chn 2 or 3 channel to use COMPRESS_RFBC_64x4");
    return -1;
  }
#endif
  return 0;
}

static RKADK_S32 RKADK_PARAM_SetDispViAttr(RKADK_S32 s32CamId) {
  int index;
  bool bSensorHorizontal = false, bDispHorizontal = false;
  RKADK_U32 width, height;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_DISP_CFG_S *pstDispCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stDispCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);

  if (pstSensorCfg->max_width > pstSensorCfg->max_height)
    bSensorHorizontal = true;

  if (pstDispCfg->width > pstDispCfg->height)
    bDispHorizontal = true;

  if ((bSensorHorizontal && bDispHorizontal) ||
      (!bSensorHorizontal && !bDispHorizontal)) {
    width = pstDispCfg->width;
    height = pstDispCfg->height;
  } else {
    width = pstDispCfg->height;
    height = pstDispCfg->width;
  }

  index = RKADK_PARAM_MatchViIndex(RKADK_STREAM_TYPE_DISP, s32CamId, width, height, true);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
  pstDispCfg->vi_attr.index = index;
  pstDispCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  memcpy(pstDispCfg->vi_attr.stChnAttr.stIspOpt.aEntityName,
         pstViCfg->device_name, sizeof(pstViCfg->device_name));
  pstDispCfg->vi_attr.stChnAttr.stIspOpt.u32BufCount = pstViCfg->buf_cnt;
  pstDispCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  if (!pstSensorCfg->used_isp) {
    //only support MPLANE&&MMAP if not use libv4l2
    pstDispCfg->vi_attr.stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
    pstDispCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
    pstDispCfg->vi_attr.stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
  }
#endif
  pstDispCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstViCfg->width;
  pstDispCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstViCfg->height;
  pstDispCfg->vi_attr.stChnAttr.stSize.u32Width = pstViCfg->width;
  pstDispCfg->vi_attr.stChnAttr.stSize.u32Height = pstViCfg->height;
  pstDispCfg->vi_attr.stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(
      pstViCfg->pix_fmt, &(pstDispCfg->vi_attr.stChnAttr.enCompressMode));
  pstDispCfg->vi_attr.stChnAttr.u32Depth = pstViCfg->depth;
  pstDispCfg->vi_attr.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  pstDispCfg->vi_attr.stChnAttr.stFrameRate.s32DstFrameRate = -1;

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (pstDispCfg->vi_attr.stChnAttr.enCompressMode == COMPRESS_RFBC_64x4 && pstViCfg->chn_id != 2 && pstViCfg->chn_id != 3) {
    RKADK_LOGE("only support chn 2 or 3 channel to use COMPRESS_RFBC_64x4");
    return -1;
  }
#endif
  return 0;
}

static RKADK_S32 RKADK_PARAM_SetThumbViAttr(RKADK_S32 s32CamId) {
  int index;
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stThumbCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);

  index = RKADK_PARAM_MatchViIndex(RKADK_STREAM_TYPE_THUMB, s32CamId,
                                   pstThumbCfg->thumb_width,
                                   pstThumbCfg->thumb_height,
                                   true);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[index];
  pstThumbCfg->vi_attr.index = index;
  pstThumbCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  memcpy(pstThumbCfg->vi_attr.stChnAttr.stIspOpt.aEntityName,
         pstViCfg->device_name, sizeof(pstViCfg->device_name));
  pstThumbCfg->vi_attr.stChnAttr.stIspOpt.u32BufCount = pstViCfg->buf_cnt;
  pstThumbCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  if (!pstSensorCfg->used_isp) {
    //only support MPLANE&&MMAP if not use libv4l2
    pstThumbCfg->vi_attr.stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
    pstThumbCfg->vi_attr.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_MMAP;
    pstThumbCfg->vi_attr.stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
  }
#endif
  pstThumbCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Width = pstViCfg->width;
  pstThumbCfg->vi_attr.stChnAttr.stIspOpt.stMaxSize.u32Height = pstViCfg->height;
  pstThumbCfg->vi_attr.stChnAttr.stSize.u32Width = pstViCfg->width;
  pstThumbCfg->vi_attr.stChnAttr.stSize.u32Height = pstViCfg->height;
  pstThumbCfg->vi_attr.stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(
      pstViCfg->pix_fmt, &(pstThumbCfg->vi_attr.stChnAttr.enCompressMode));
  pstThumbCfg->vi_attr.stChnAttr.u32Depth = pstViCfg->depth;
  pstThumbCfg->vi_attr.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  pstThumbCfg->vi_attr.stChnAttr.stFrameRate.s32DstFrameRate = -1;

#if defined(RV1126B) || defined(RK3572) || defined(RK3576)
  if (pstThumbCfg->vi_attr.stChnAttr.enCompressMode == COMPRESS_RFBC_64x4 && pstViCfg->chn_id != 2 && pstViCfg->chn_id != 3) {
    RKADK_LOGE("only support chn 2 or 3 channel to use COMPRESS_RFBC_64x4");
    return -1;
  }
#endif
  return 0;
}

static RKADK_S32 RKADK_PARAM_SetMediaViAttr() {
  int i, ret = 0;

  for (i = 0; i < (int)g_stPARAMCtx.stCfg.stCommCfg.sensor_count; i++) {
    // Must be called before setRecattr
    ret = RKADK_PARAM_SetStreamViAttr(i, RKADK_STREAM_TYPE_PREVIEW);
    if (ret)
      break;

    ret = RKADK_PARAM_SetStreamViAttr(i, RKADK_STREAM_TYPE_LIVE);
    if (ret)
      break;

    // Must be called before SetPhotoAttr
    ret = RKADK_PARAM_SetRecViAttr(i);
    if (ret)
      break;

    ret = RKADK_PARAM_SetPhotoViAttr(i);
    if (ret)
      break;

    ret = RKADK_PARAM_SetDispViAttr(i);
    if (ret)
      break;

    ret = RKADK_PARAM_SetThumbViAttr(i);
    if (ret)
      break;
  }

  RKADK_PARAM_DumpViAttr();
  return ret;
}

static void RKADK_PARAM_SetMicVolume(RKADK_U32 volume) {
  if (volume < 0 || volume > 100) {
    RKADK_LOGE("Set mic input volume failed. Mic input volume range is [0,100]");
    return;
  }

  RKADK_LOGD("volume = %d", volume);
#ifndef OS_RTT
  RK_MPI_AI_SetVolume(0, volume);
#endif
}

static void RKADK_PARAM_SetSpeakerVolume(RKADK_U32 volume) {
  if (volume < 0 || volume > 100) {
    RKADK_LOGE("Set speaker volume failed. Speaker volume range is[0,100]");
    return;
  }

  RKADK_LOGD("volume = %d", volume);
  RK_MPI_AO_SetVolume(0, volume);
}

static void RKADK_PARAM_RecMute(bool mute, RKADK_U32 volume, RKADK_U32 u32AencChn) {
  char buffer[RKADK_VOLUME_LEN];
  volume = (int)(volume * 2.55 + 0.5);
  memset(buffer, 0, RKADK_VOLUME_LEN);
  sprintf(buffer, "%d", volume);

  RKADK_LOGI("RKADK_PARAM_RecMute mute = %d", mute);

#ifndef OS_RTT
  if (mute) {
    RK_MPI_AENC_SetMute(u32AencChn, (RK_BOOL)mute);
  } else {
#if defined(RV1106_1103) || defined(RV1103B)
    RK_MPI_AMIX_SetControl(0, "ADC Digital Left Volume", buffer);
    RK_MPI_AMIX_SetControl(0, "ADC Digital Right Volume", buffer);
#endif
    RK_MPI_AENC_SetMute(u32AencChn, (RK_BOOL)mute);
  }
#endif
}

static void RKADK_PARAM_SetVolume() {
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  RKADK_PARAM_SetMicVolume(pstCommCfg->mic_volume);
  RKADK_PARAM_SetSpeakerVolume(pstCommCfg->speaker_volume);
}

static void RKADK_PARAM_Strtok(char *input, RKADK_S32 *s32Output,
                               RKADK_S32 u32OutputLen, const char *delim) {
  char *p;

  for (int i = 0; i < u32OutputLen; i++) {
    if (!i)
      p = strtok(input, delim);
    else
      p = strtok(NULL, delim);

    if (!p)
      break;

    s32Output[i] = atoi(p);
  }
}

VENC_RC_MODE_E RKADK_PARAM_GetRcMode(char *rcMode,
                                     RKADK_CODEC_TYPE_E enCodecType) {
  VENC_RC_MODE_E enRcMode = VENC_RC_MODE_BUTT;

  RKADK_CHECK_POINTER(rcMode, VENC_RC_MODE_BUTT);

  switch (enCodecType) {
  case RKADK_CODEC_TYPE_H264:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_H264CBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_H264VBR;
    else if (!strcmp(rcMode, "AVBR"))
      enRcMode = VENC_RC_MODE_H264AVBR;
    break;
  case RKADK_CODEC_TYPE_H265:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_H265CBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_H265VBR;
    else if (!strcmp(rcMode, "AVBR"))
      enRcMode = VENC_RC_MODE_H265AVBR;
    break;
  case RKADK_CODEC_TYPE_MJPEG:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_MJPEGCBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_MJPEGVBR;
    break;
  default:
    RKADK_LOGE("Nonsupport codec type: %d", enCodecType);
    break;
  }

  return enRcMode;
}

static RKADK_S32 RKADK_PARAM_SetRcParam(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  const char *strings = NULL;
  bool bSetRcParam = false;
  RKADK_S32 u32StepQp = -1, u32MinQp = -1, u32MaxQp = -1;
  RKADK_S32 u32FrmMinQp = -1, u32FrmMaxQp = -1;
  RKADK_S32 u32FrmMinIQp = -1, u32FrmMaxIQp = -1;
  RKADK_S32 u32MinIQp = -1, u32MaxIQp = -1;
  RKADK_S32 s32FirstFrameStartQp = -1;
  VENC_RC_PARAM_S stRcParam;
  VENC_RC_PARAM2_S stRcParam2;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  memset(&stRcParam, 0, sizeof(VENC_RC_PARAM_S));
  ret = RK_MPI_VENC_GetRcParam(stVencAttr.venc_chn, &stRcParam);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetRcParam failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:frame_min_qp", module);
  u32FrmMinQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32FrmMinQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:frame_max_qp", module);
  u32FrmMaxQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32FrmMaxQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:frame_min_i_qp", module);
  u32FrmMinIQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32FrmMinIQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:frame_max_i_qp", module);
  u32FrmMaxIQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32FrmMaxIQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:step_qp", module);
  u32StepQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32StepQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:max_qp", module);
  u32MaxQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32MaxQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:min_qp", module);
  u32MinQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32MinQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:min_i_qp", module);
  u32MinIQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32MinIQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:i_max_qp", module);
  u32MaxIQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (u32MaxIQp > 0)
    bSetRcParam = true;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:first_frame_qp", module);
  s32FirstFrameStartQp = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32FirstFrameStartQp > 0)
    bSetRcParam = true;

  if (bSetRcParam) {
    RKADK_LOGD("Venc[%d] ini rc param: first_frame_qp[%d] step_qp[%d] frame_min_qp[%d] frame_max_qp[%d] "
                "frame_min_i_qp[%d] frame_max_i_qp[%d] min_qp[%d] max_qp[%d] min_i_qp[%d] i_max_qp[%d]",
                stVencAttr.venc_chn, s32FirstFrameStartQp, u32StepQp, u32FrmMinQp, u32FrmMaxQp,
                u32FrmMinIQp, u32FrmMaxIQp, u32MinQp, u32MaxQp, u32MinIQp, u32MaxIQp);

    if (stVencAttr.codec_type == RKADK_CODEC_TYPE_H264) {
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetRcParam: s32FirstFrameStartQp[%d] u32StepQp[%d] u32FrmMinQp[%d] u32FrmMaxQp[%d] "
                  "u32FrmMinIQp[%d] u32FrmMaxIQp[%d] u32MinQp[%d] u32MaxQp[%d] u32MinIQp[%d] u32MaxIQp[%d]",
                  stVencAttr.venc_chn, stRcParam.s32FirstFrameStartQp, stRcParam.stParamH264.u32StepQp,
                  stRcParam.stParamH264.u32FrmMinQp, stRcParam.stParamH264.u32FrmMaxQp,
                  stRcParam.stParamH264.u32FrmMinIQp, stRcParam.stParamH264.u32FrmMaxIQp,
                  stRcParam.stParamH264.u32MinQp, stRcParam.stParamH264.u32MaxQp,
                  stRcParam.stParamH264.u32MinIQp, stRcParam.stParamH264.u32MaxIQp);

      if (u32FrmMinQp > 0)
        stRcParam.stParamH264.u32FrmMinQp = u32FrmMinQp;

      if (u32FrmMaxQp > 0)
        stRcParam.stParamH264.u32FrmMaxQp = u32FrmMaxQp;

      if (u32FrmMinIQp > 0)
        stRcParam.stParamH264.u32FrmMinIQp = u32FrmMinIQp;

      if (u32FrmMaxIQp > 0)
        stRcParam.stParamH264.u32FrmMaxIQp = u32FrmMaxIQp;

      if (u32StepQp > 0)
        stRcParam.stParamH264.u32StepQp = u32StepQp;

      if (u32MaxQp > 0)
        stRcParam.stParamH264.u32MaxQp = u32MaxQp;

      if (u32MinQp > 0)
        stRcParam.stParamH264.u32MinQp = u32MinQp;

      if (u32MinIQp > 0)
        stRcParam.stParamH264.u32MinIQp = u32MinIQp;

      if (u32MaxIQp > 0)
        stRcParam.stParamH264.u32MaxIQp = u32MaxIQp;
    } else if (stVencAttr.codec_type == RKADK_CODEC_TYPE_H265) {
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetRcParam: s32FirstFrameStartQp[%d] u32StepQp[%d] u32FrmMinQp[%d] u32FrmMaxQp[%d] "
                  "u32FrmMinIQp[%d] u32FrmMaxIQp[%d] u32MinQp[%d] u32MaxQp[%d] u32MinIQp[%d] u32MaxIQp[%d]",
                  stVencAttr.venc_chn, stRcParam.s32FirstFrameStartQp, stRcParam.stParamH265.u32StepQp,
                  stRcParam.stParamH265.u32FrmMinQp, stRcParam.stParamH265.u32FrmMaxQp,
                  stRcParam.stParamH265.u32FrmMinIQp, stRcParam.stParamH265.u32FrmMaxIQp,
                  stRcParam.stParamH265.u32MinQp, stRcParam.stParamH265.u32MaxQp,
                  stRcParam.stParamH265.u32MinIQp, stRcParam.stParamH265.u32MaxIQp);

      if (u32FrmMinQp > 0)
        stRcParam.stParamH265.u32FrmMinQp = u32FrmMinQp;

      if (u32FrmMaxQp > 0)
        stRcParam.stParamH265.u32FrmMaxQp = u32FrmMaxQp;

      if (u32FrmMinIQp > 0)
        stRcParam.stParamH265.u32FrmMinIQp = u32FrmMinIQp;

      if (u32FrmMaxIQp > 0)
        stRcParam.stParamH265.u32FrmMaxIQp = u32FrmMaxIQp;

      if (u32StepQp > 0)
        stRcParam.stParamH265.u32StepQp = u32StepQp;

      if (u32MaxQp > 0)
        stRcParam.stParamH265.u32MaxQp = u32MaxQp;

      if (u32MinQp > 0)
        stRcParam.stParamH265.u32MinQp = u32MinQp;

      if (u32MinIQp > 0)
        stRcParam.stParamH265.u32MinIQp = u32MinIQp;

      if (u32MaxIQp > 0)
        stRcParam.stParamH265.u32MaxIQp = u32MaxIQp;
    } else {
      RKADK_LOGE("unsupported codec_type[%d]", stVencAttr.codec_type);
      return -1;
    }

    if (s32FirstFrameStartQp > 0)
      stRcParam.s32FirstFrameStartQp = s32FirstFrameStartQp;

    ret = RK_MPI_VENC_SetRcParam(stVencAttr.venc_chn, &stRcParam);
    if (ret) {
      RKADK_LOGE("Venc[%d] SetRcParam failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetRcParam ok", stVencAttr.venc_chn);
  }

  // set rc param2
  bSetRcParam = false;
  memset(&stRcParam2, 0, sizeof(VENC_RC_PARAM2_S));
  ret = RK_MPI_VENC_GetRcParam2(stVencAttr.venc_chn, &stRcParam2);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetRcParam2 failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:thrd_i", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini thrd_i: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stRcParam2.u32ThrdI[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetRcParam = true;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:thrd_p", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini thrd_p: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stRcParam2.u32ThrdP[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetRcParam = true;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:aq_step_i", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini aq_step_i: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stRcParam2.s32AqStepI[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetRcParam = true;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:aq_step_p", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini aq_step_p: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stRcParam2.s32AqStepP[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetRcParam = true;
  }

  if (bSetRcParam) {
    ret = RK_MPI_VENC_SetRcParam2(stVencAttr.venc_chn, &stRcParam2);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetRcParam2 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetRcParam2 ok", stVencAttr.venc_chn);
  }

#if defined(RV1103B) || defined(RV1126B)
  // set rc param3
  VENC_RC_PARAM3_S stRcParam3;
  bSetRcParam = false;
  memset(&stRcParam3, 0, sizeof(VENC_RC_PARAM3_S));
  ret = RK_MPI_VENC_GetRcParam3(stVencAttr.venc_chn, &stRcParam3);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetRcParam3 failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:aq_rnge_arr", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini aq_rnge_arr: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stRcParam3.u32AqRange[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetRcParam = true;
  }

  if (bSetRcParam) {
    ret = RK_MPI_VENC_SetRcParam3(stVencAttr.venc_chn, &stRcParam3);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetRcParam3 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetRcParam3 ok", stVencAttr.venc_chn);
  }
#endif

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencFilter(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  VENC_FILTER_S stFilter;
  RKADK_S32 s32StrengthI = -1, s32StrengthP = -1;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  snprintf(entry, RKADK_BUFFER_LEN, "%s:flt_str_i", module);
  s32StrengthI = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:flt_str_p", module);
  s32StrengthP = RKADK_IniGetInt(u32CamId, entry, -1);

  if (s32StrengthI == -1 && s32StrengthP == -1)
    return 0;

  RKADK_LOGD("Venc[%d] ini flt_str_i[%d] flt_str_p[%d]", s32StrengthI, s32StrengthP);

  memset(&stFilter, 0, sizeof(VENC_FILTER_S));
  ret = RK_MPI_VENC_GetFilter(stVencAttr.venc_chn, &stFilter);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetFilter failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }
  RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetFilter: u32StrengthI[%d] u32StrengthP[%d]", stVencAttr.venc_chn, stFilter.u32StrengthI, stFilter.u32StrengthP);

  if (s32StrengthI > 0)
    stFilter.u32StrengthI = s32StrengthI;

  if (s32StrengthP > 0)
    stFilter.u32StrengthP = s32StrengthP;

  ret = RK_MPI_VENC_SetFilter(stVencAttr.venc_chn, &stFilter);
  if (ret) {
    RKADK_LOGE("Venc[%d] SetFilter failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }
  RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetFilter ok", stVencAttr.venc_chn);

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencTrans(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  RKADK_S32 s32ScalingListValid = -1;
  VENC_H264_TRANS_S stH264Trans;
  VENC_H265_TRANS_S stH265Trans;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  snprintf(entry, RKADK_BUFFER_LEN, "%s:scalinglist", module);
  s32ScalingListValid = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32ScalingListValid == -1)
    return 0;

  RKADK_LOGD("Venc[%d] ini scalinglist[%d]", stVencAttr.venc_chn, s32ScalingListValid);
  switch (stVencAttr.codec_type) {
  case RKADK_CODEC_TYPE_H264:
    ret = RK_MPI_VENC_GetH264Trans(stVencAttr.venc_chn, &stH264Trans);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH264Trans failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH264Trans: bScalingListValid[%d]", stVencAttr.venc_chn, stH264Trans.bScalingListValid);

    if (stH264Trans.bScalingListValid == s32ScalingListValid)
      return 0;

    stH264Trans.bScalingListValid = s32ScalingListValid;
    ret = RK_MPI_VENC_SetH264Trans(stVencAttr.venc_chn, &stH264Trans);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH264Trans failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH264Trans ok", stVencAttr.venc_chn);

    break;
  case RKADK_CODEC_TYPE_H265:
    ret = RK_MPI_VENC_GetH265Trans(stVencAttr.venc_chn, &stH265Trans);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Trans failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH265Trans: bScalingListEnabled[%d]", stVencAttr.venc_chn, stH265Trans.bScalingListEnabled);

    if (stH265Trans.bScalingListEnabled == s32ScalingListValid)
      return 0;

    stH265Trans.bScalingListEnabled = s32ScalingListValid;
    ret = RK_MPI_VENC_SetH265Trans(stVencAttr.venc_chn, &stH265Trans);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265Trans failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265Trans ok", stVencAttr.venc_chn);

    break;
  default:
    RKADK_LOGE("Nonsupport codec type: %d", stVencAttr.codec_type);
    return -1;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencVui(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  RKADK_S32 s32FullRange = -1;
  VENC_H264_VUI_S stH264Vui;
  VENC_H265_VUI_S stH265Vui;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  snprintf(entry, RKADK_BUFFER_LEN, "%s:full_range", module);
  s32FullRange = RKADK_IniGetInt(u32CamId, entry, -1);

  if (s32FullRange == -1)
    return 0;

  RKADK_LOGD("Venc[%d] ini full_range[%d]", stVencAttr.venc_chn, s32FullRange);
  switch (stVencAttr.codec_type) {
  case RKADK_CODEC_TYPE_H264:
    ret = RK_MPI_VENC_GetH264Vui(stVencAttr.venc_chn, &stH264Vui);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH264Vui failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH264Vui: video_full_range_flag[%d]", stVencAttr.venc_chn, stH264Vui.stVuiVideoSignal.video_full_range_flag);

    if (stH264Vui.stVuiVideoSignal.video_full_range_flag == s32FullRange)
      return 0;

    stH264Vui.stVuiVideoSignal.video_full_range_flag = s32FullRange;
    ret = RK_MPI_VENC_SetH264Vui(stVencAttr.venc_chn, &stH264Vui);
    if(ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH264Vui failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH264Vui ok", stVencAttr.venc_chn);

    break;

  case RKADK_CODEC_TYPE_H265:
    ret = RK_MPI_VENC_GetH265Vui(stVencAttr.venc_chn, &stH265Vui);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Vui failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH265Vui: video_full_range_flag[%d]", stVencAttr.venc_chn, stH265Vui.stVuiVideoSignal.video_full_range_flag);

    if (stH265Vui.stVuiVideoSignal.video_full_range_flag == s32FullRange)
      return 0;

    stH265Vui.stVuiVideoSignal.video_full_range_flag = s32FullRange;
    ret = RK_MPI_VENC_SetH265Vui(stVencAttr.venc_chn, &stH265Vui);
    if(ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265Vui failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265Vui ok", stVencAttr.venc_chn);

    break;

  default:
    RKADK_LOGE("Nonsupport codec type: %d", stVencAttr.codec_type);
    return -1;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencHierarchicalQp(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  const char *strings = NULL;
  bool bSetParam = false;
  VENC_HIERARCHICAL_QP_S stHierarchicalQp;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  memset(&stHierarchicalQp, 0, sizeof(VENC_HIERARCHICAL_QP_S));
  ret = RK_MPI_VENC_GetHierarchicalQp(stVencAttr.venc_chn, &stHierarchicalQp);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetHierarchicalQp failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:hier_qp_delta", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini hier_qp_delta: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stHierarchicalQp.s32HierarchicalQpDelta[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetParam = true;
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:hier_frame_num", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");
  if (strings && strcmp(strings, "NONE")) {
    RKADK_LOGD("Venc[%d] ini hier_frame_num: %s", stVencAttr.venc_chn, strings);
    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stHierarchicalQp.s32HierarchicalFrameNum[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

    bSetParam = true;
  }

  if (!bSetParam)
    return 0;

  stHierarchicalQp.bHierarchicalQpEn = RK_TRUE;
  ret = RK_MPI_VENC_SetHierarchicalQp(stVencAttr.venc_chn, &stHierarchicalQp);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetHierarchicalQp failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }
  RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetHierarchicalQp ok", stVencAttr.venc_chn);

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencQbias(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  RKADK_S32 enable = -1, s32QbiasI = -1, s32QbiasP = -1;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  snprintf(entry, RKADK_BUFFER_LEN, "%s:qbias_enable", module);
  enable = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:qbias_i", module);
  s32QbiasI = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:qbias_p", module);
  s32QbiasP = RKADK_IniGetInt(u32CamId, entry, -1);

  if (enable == -1 && s32QbiasI == -1 && s32QbiasP == -1)
    return 0;

  RKADK_LOGD("Venc[%d] ini qbias_enable[%d] qbias_i[%d] qbias_p[%d]", stVencAttr.venc_chn, enable, s32QbiasI, s32QbiasP);
  switch (stVencAttr.codec_type) {
  case RKADK_CODEC_TYPE_H264:
  {
    VENC_H264_QBIAS_S qbias;
    memset(&qbias, 0, sizeof(VENC_H264_QBIAS_S));
    ret = RK_MPI_VENC_GetH264Qbias(stVencAttr.venc_chn, &qbias);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH264Qbias failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH264Qbias: bEnable[%d] u32QbiasI[%d] u32QbiasP[%d]", stVencAttr.venc_chn, qbias.bEnable, qbias.u32QbiasI, qbias.u32QbiasP);

		qbias.bEnable = enable == 0 ? RK_FALSE : RK_TRUE;
    if (s32QbiasI > 0)
      qbias.u32QbiasI = s32QbiasI;

    if (s32QbiasP > 0)
      qbias.u32QbiasP = s32QbiasP;

    ret = RK_MPI_VENC_SetH264Qbias(stVencAttr.venc_chn, &qbias);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH264Qbias failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH264Qbias ok", stVencAttr.venc_chn);

    break;
  }

  case RKADK_CODEC_TYPE_H265:
  {
    VENC_H265_QBIAS_S qbias;
    memset(&qbias, 0, sizeof(VENC_H265_QBIAS_S));
    ret = RK_MPI_VENC_GetH265Qbias(stVencAttr.venc_chn, &qbias);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Qbias failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Qbias bEnable[%d] u32QbiasI[%d] u32QbiasP", stVencAttr.venc_chn, qbias.bEnable, qbias.u32QbiasI, qbias.u32QbiasP);

		qbias.bEnable = enable == 0 ? RK_FALSE : RK_TRUE;
    if (s32QbiasI > 0)
      qbias.u32QbiasI = s32QbiasI;

    if (s32QbiasP > 0)
      qbias.u32QbiasP = s32QbiasP;

    ret = RK_MPI_VENC_SetH265Qbias(stVencAttr.venc_chn, &qbias);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265Qbias failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265Qbias ok", stVencAttr.venc_chn);

    break;
  }

  default:
    RKADK_LOGE("Nonsupport codec type: %d", stVencAttr.codec_type);
    return -1;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencQbias2(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
#if defined(RV1103B) || defined(RV1126B)
  int ret = 0, enable = -1;
  const char *strings = NULL;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  snprintf(entry, RKADK_BUFFER_LEN, "%s:qbias_enable", module);
  enable = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:qbias_arr", module);
  strings = RKADK_IniGetString(u32CamId, entry, "NONE");

  if (enable == -1 || !strings || !strcmp(strings, "NONE"))
    return 0;

  RKADK_LOGD("Venc[%d] ini qbias_enable[%d] qbias_arr[%s]", stVencAttr.venc_chn, enable, strings);
  switch (stVencAttr.codec_type) {
  case RKADK_CODEC_TYPE_H264:
  {
    VENC_H264_QBIAS2_S stQbias2;
    memset(&stQbias2, 0, sizeof(VENC_H264_QBIAS2_S));
    ret = RK_MPI_VENC_GetH264Qbias2(stVencAttr.venc_chn, &stQbias2);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH264Qbias2 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }

    stQbias2.bEnable = enable;

    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stQbias2.u32Qbias2[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

		ret = RK_MPI_VENC_SetH264Qbias2(stVencAttr.venc_chn, &stQbias2);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH264Qbias2 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH264Qbias2 ok", stVencAttr.venc_chn);

    break;
  }

  case RKADK_CODEC_TYPE_H265:
  {
    VENC_H265_QBIAS2_S stQbias2;
    memset(&stQbias2, 0, sizeof(VENC_H265_QBIAS2_S));
    ret = RK_MPI_VENC_GetH265Qbias2(stVencAttr.venc_chn, &stQbias2);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Qbias2 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }

    stQbias2.bEnable = enable;

    char *str = strdup(strings);
    if (str) {
      char *tmp = str;
      char *token = strsep(&tmp, ",");
      int i = 0;
      while (token != NULL) {
        stQbias2.u32Qbias2[i++] = atoi(token);
        token = strsep(&tmp, ",");
      }
      free(str);
    }

		ret = RK_MPI_VENC_SetH265Qbias2(stVencAttr.venc_chn, &stQbias2);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265Qbias2 failed[%x]", stVencAttr.venc_chn, ret);
      return -1;
    }
    RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265Qbias2 ok", stVencAttr.venc_chn);

    break;
  }

  default:
    RKADK_LOGE("Nonsupport codec type: %d", stVencAttr.codec_type);
    return -1;
  }
#endif

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencStaticWeight(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
#if defined(RV1103B) || defined(RV1126B)
  int ret = 0;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};
	VENC_STATIC_WEIGHT_S stStaticWeight;
  RKADK_S32 s32FrmNum = -1, s32Madp16Th = -1, s32Skip16Weight = -1, s32Skip32Weight = -1;

  snprintf(entry, RKADK_BUFFER_LEN, "%s:static_frm_num", module);
  s32FrmNum = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:madp16_th", module);
  s32Madp16Th = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:skip16_wgt", module);
  s32Skip16Weight = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:skip32_wgt", module);
  s32Skip32Weight = RKADK_IniGetInt(u32CamId, entry, -1);

  if (s32FrmNum == -1 && s32Madp16Th == -1 && s32Skip16Weight == -1 && s32Skip32Weight == -1)
    return 0;

  RKADK_LOGD("Venc[%d] ini static_frm_num[%d] madp16_th[%d] skip16_wgt[%d] skip32_wgt[%d]", stVencAttr.venc_chn, s32FrmNum, s32Madp16Th, s32Skip16Weight, s32Skip32Weight);
  ret = RK_MPI_VENC_GetStaticWeight(stVencAttr.venc_chn, &stStaticWeight);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetStaticWeight failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }
  RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetStaticWeight: u32FrmNum[%d] u32Madp16Th[%d] u32Skip16Weight[%d] u32Skip32Weight[%d]", stVencAttr.venc_chn, stStaticWeight.u32FrmNum, stStaticWeight.u32Madp16Th, stStaticWeight.u32Skip16Weight, stStaticWeight.u32Skip32Weight);

  if (s32FrmNum > 0)
	  stStaticWeight.u32FrmNum = s32FrmNum;

  if (s32Madp16Th > 0)
    stStaticWeight.u32Madp16Th = s32Madp16Th;

  if (s32Skip16Weight > 0)
    stStaticWeight.u32Skip16Weight = s32Skip16Weight;

  if (s32Skip32Weight > 0)
    stStaticWeight.u32Skip32Weight = s32Skip32Weight;

  ret = RK_MPI_VENC_SetStaticWeight(stVencAttr.venc_chn, &stStaticWeight);
  if (ret) {
    RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetStaticWeight failed[%x]", stVencAttr.venc_chn, ret);
    return -1;
  }
  RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetStaticWeight ok", stVencAttr.venc_chn);
#endif

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetVencParam(RKADK_U32 u32CamId, char *module, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret = 0;
  RKADK_S32 s32Value1 = -1;
  char entry[RKADK_BUFFER_LEN + 1] = {'\0'};

  if (stVencAttr.codec_type == RKADK_CODEC_TYPE_H265) {
    snprintf(entry, RKADK_BUFFER_LEN, "%s:cu_dqp", module);
    s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
    if (s32Value1 != -1) {
      VENC_H265_CU_DQP_S stCuDqp;
      memset(&stCuDqp, 0, sizeof(VENC_H265_CU_DQP_S));
      ret = RK_MPI_VENC_GetH265CuDqp(stVencAttr.venc_chn, &stCuDqp);
      if (ret) {
        RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265CuDqp failed[%x]", stVencAttr.venc_chn, ret);
      } else {
        RKADK_LOGD("Venc[%d] ini cu_dqp[%d]", stVencAttr.venc_chn, s32Value1);
        RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH265CuDqp: u32CuDqp[%d]", stVencAttr.venc_chn, stCuDqp.u32CuDqp);

        if (stCuDqp.u32CuDqp != s32Value1) {
          stCuDqp.u32CuDqp = s32Value1;
          ret = RK_MPI_VENC_SetH265CuDqp(stVencAttr.venc_chn, &stCuDqp);
          if (ret)
            RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265CuDqp failed[%x]", stVencAttr.venc_chn, ret);
          else
            RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265CuDqp ok", stVencAttr.venc_chn);
        }
      }
    }
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:anti_ring", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32Value1 != -1) {
    VENC_ANTI_RING_S stAntiRing;
    memset(&stAntiRing, 0, sizeof(VENC_ANTI_RING_S));
    ret = RK_MPI_VENC_GetAntiRing(stVencAttr.venc_chn, &stAntiRing);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetAntiRing failed[%x]", stVencAttr.venc_chn, ret);
    } else {
      RKADK_LOGD("Venc[%d] ini anti_ring[%d]", stVencAttr.venc_chn, s32Value1);
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetAntiRing: u32AntiRing[%d]", stVencAttr.venc_chn, stAntiRing.u32AntiRing);

      if (stAntiRing.u32AntiRing != s32Value1) {
        stAntiRing.u32AntiRing = s32Value1;
        ret = RK_MPI_VENC_SetAntiRing(stVencAttr.venc_chn, &stAntiRing);
        if (ret)
          RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetAntiRing failed[%x]", stVencAttr.venc_chn, ret);
        else
          RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetAntiRing ok", stVencAttr.venc_chn);
      }
    }
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:anti_line", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32Value1 != -1) {
    VENC_ANTI_LINE_S stAntiLine;
    memset(&stAntiLine, 0, sizeof(VENC_ANTI_LINE_S));
    ret = RK_MPI_VENC_GetAntiLine(stVencAttr.venc_chn, &stAntiLine);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetAntiLine failed[%x]", stVencAttr.venc_chn, ret);
    } else {
      RKADK_LOGD("Venc[%d] ini anti_line[%d]", stVencAttr.venc_chn, s32Value1);
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetAntiLine: u32AntiLine[%d]", stVencAttr.venc_chn, stAntiLine.u32AntiLine);

      if (stAntiLine.u32AntiLine != s32Value1) {
        stAntiLine.u32AntiLine = s32Value1;
        ret = RK_MPI_VENC_SetAntiLine(stVencAttr.venc_chn, &stAntiLine);
        if (ret)
          RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetAntiLine failed[%x]", stVencAttr.venc_chn, ret);
        else
          RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetAntiLine ok", stVencAttr.venc_chn);
      }
    }
  }

#if defined(RV1126B) || defined(RV1103B)
  bool bSaveParam = false;
  RKADK_S32 s32Value2 = -1;
  snprintf(entry, RKADK_BUFFER_LEN, "%s:lambda", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);

  snprintf(entry, RKADK_BUFFER_LEN, "%s:lambda_i", module);
  s32Value2 = RKADK_IniGetInt(u32CamId, entry, -1);

  if (s32Value1 != -1 || s32Value2 != -1) {
    VENC_LAMBDA_S stLambda;
    memset(&stLambda, 0, sizeof(VENC_LAMBDA_S));
    ret = RK_MPI_VENC_GetLambda(stVencAttr.venc_chn, &stLambda);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetLambda failed[%x]", stVencAttr.venc_chn, ret);
    } else {
      RKADK_LOGD("Venc[%d] ini lambda[%d] lambda_i[%d]", stVencAttr.venc_chn, s32Value1,s32Value2);
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetLambda: u32Lambda[%d] u32LambdaI[%d]", stVencAttr.venc_chn, stLambda.u32Lambda, stLambda.u32LambdaI);

      bSaveParam = false;
      if (stLambda.u32Lambda != s32Value1) {
        stLambda.u32Lambda = s32Value1;
        bSaveParam = true;
      }

      if (stLambda.u32LambdaI != s32Value2) {
        stLambda.u32LambdaI = s32Value2;
        bSaveParam = true;
      }

      if (bSaveParam) {
        ret = RK_MPI_VENC_SetLambda(stVencAttr.venc_chn, &stLambda);
        if (ret)
          RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetLambda failed[%x]", stVencAttr.venc_chn, ret);
        else
          RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetLambda ok", stVencAttr.venc_chn);
      }
    }
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:atf_str", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32Value1 != -1) {
    VENC_ANTI_FLICK_S stAntiFlick;
    memset(&stAntiFlick, 0, sizeof(VENC_ANTI_FLICK_S));
    ret = RK_MPI_VENC_GetAntiFlick(stVencAttr.venc_chn, &stAntiFlick);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetAntiFlick failed[%x]", stVencAttr.venc_chn, ret);
    } else {
      RKADK_LOGD("Venc[%d] ini atf_str[%d]", stVencAttr.venc_chn, s32Value1);
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetAntiFlick u32AntiFlick[%d]", stVencAttr.venc_chn, stAntiFlick.u32AntiFlick);

      if (stAntiFlick.u32AntiFlick != s32Value1) {
        stAntiFlick.u32AntiFlick = s32Value1;
        ret = RK_MPI_VENC_SetAntiFlick(stVencAttr.venc_chn, &stAntiFlick);
        if (ret)
          RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetAntiFlick failed[%x]", stVencAttr.venc_chn, ret);
        else
          RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetAntiFlick ok", stVencAttr.venc_chn);
      }
    }
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:lgt_chg_lvl", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32Value1 != -1) {
    VENC_LIGHT_CHANGE_S stLightChange;
    memset(&stLightChange, 0, sizeof(VENC_LIGHT_CHANGE_S));
    ret = RK_MPI_VENC_GetLightChange(stVencAttr.venc_chn, &stLightChange);
    if (ret) {
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetLightChange failed[%x]", stVencAttr.venc_chn, ret);
    } else {
      RKADK_LOGD("Venc[%d] ini lgt_chg_lvl[%d]", stVencAttr.venc_chn, s32Value1);
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetLightChange: u32Level[%d]", stVencAttr.venc_chn, stLightChange.u32Level);

      if (stLightChange.u32Level != s32Value1) {
        stLightChange.u32Level = s32Value1;
        ret = RK_MPI_VENC_SetLightChange(stVencAttr.venc_chn, &stLightChange);
        if (ret)
          RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetLightChange failed[%x]", stVencAttr.venc_chn, ret);
        else
          RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetLightChange ok", stVencAttr.venc_chn);
      }
    }
  }

  if (stVencAttr.codec_type == RKADK_CODEC_TYPE_H265) {
    snprintf(entry, RKADK_BUFFER_LEN, "%s:tmvp_en", module);
    s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
    if (s32Value1 != -1) {
      RK_BOOL bEnable = s32Value1 == 0 ? RK_FALSE : RK_TRUE;
      RKADK_LOGD("Venc[%d] ini tmvp_en[%d] bEnable[%d]", stVencAttr.venc_chn, s32Value1, bEnable);
      ret = RK_MPI_VENC_EnableTmvp(stVencAttr.venc_chn, bEnable);
      if (ret)
        RKADK_LOGE("Venc[%d] RK_MPI_VENC_EnableTmvp failed[%x]", stVencAttr.venc_chn, ret);
      else
        RKADK_LOGD("Venc[%d] RK_MPI_VENC_EnableTmvp ok", stVencAttr.venc_chn);
    }
  }
#endif

#ifdef RV1126B
  if (stVencAttr.codec_type == RKADK_CODEC_TYPE_H265) {
    snprintf(entry, RKADK_BUFFER_LEN, "%s:sao_str_i", module);
    s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
    snprintf(entry, RKADK_BUFFER_LEN, "%s:sao_str_p", module);
    s32Value2 = RKADK_IniGetInt(u32CamId, entry, -1);

    if (s32Value1 != -1 || s32Value2 != -1) {
      VENC_H265_SAO_S stSao;
      memset(&stSao, 0, sizeof(VENC_H265_SAO_S));
      ret = RK_MPI_VENC_GetH265Sao(stVencAttr.venc_chn, &stSao);
      if (ret) {
        RKADK_LOGE("Venc[%d] RK_MPI_VENC_GetH265Sao failed[%x]", stVencAttr.venc_chn, ret);
      } else {
        RKADK_LOGD("Venc[%d] ini sao_str_i[%d] sao_str_p[%d]", stVencAttr.venc_chn, s32Value1, s32Value2);
        RKADK_LOGD("Venc[%d] RK_MPI_VENC_GetH265Sao: u32SaoStrengthI[%d] u32SaoStrengthP[%d]", stVencAttr.venc_chn, stSao.u32SaoStrengthI, stSao.u32SaoStrengthP);

        bSaveParam = false;
        if (stSao.u32SaoStrengthI != s32Value1) {
          stSao.u32SaoStrengthI = s32Value1;
          bSaveParam = true;
        }

        if (stSao.u32SaoStrengthP != s32Value2) {
          stSao.u32SaoStrengthP = s32Value2;
          bSaveParam = true;
        }

        if (bSaveParam) {
          ret = RK_MPI_VENC_SetH265Sao(stVencAttr.venc_chn, &stSao);
          if (ret)
            RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetH265Sao failed[%x]", stVencAttr.venc_chn, ret);
          else
            RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetH265Sao ok", stVencAttr.venc_chn);
        }
      }
    }
  }

  snprintf(entry, RKADK_BUFFER_LEN, "%s:speed", module);
  s32Value1 = RKADK_IniGetInt(u32CamId, entry, -1);
  if (s32Value1 != -1) {
    RKADK_LOGD("Venc[%d] ini speed[%d]", stVencAttr.venc_chn, s32Value1);
    ret = RK_MPI_VENC_SetSpeed(stVencAttr.venc_chn, s32Value1);
    if (ret)
      RKADK_LOGE("Venc[%d] RK_MPI_VENC_SetSpeed failed[%x]", stVencAttr.venc_chn, ret);
    else
      RKADK_LOGD("Venc[%d] RK_MPI_VENC_SetSpeed ok", stVencAttr.venc_chn);
  }

#endif

  return ret;
}

RKADK_S32 RKADK_PARAM_SetVAdvancedParam(RKADK_U32 u32CamId, RKADK_STREAM_TYPE_E enStreamType, RKADK_PARAM_VENC_ATTR_S stVencAttr) {
  int ret;
  char module[RKADK_INTF_FMT_LEN];

  memset(module, 0, RKADK_INTF_FMT_LEN);
  switch(enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    snprintf(module, RKADK_INTF_FMT_LEN - 1, "%s", "record.0");
    break;
  case RKADK_STREAM_TYPE_VIDEO_SUB:
    snprintf(module, RKADK_INTF_FMT_LEN - 1, "%s", "record.1");
    break;
  case RKADK_STREAM_TYPE_PREVIEW:
    snprintf(module, RKADK_INTF_FMT_LEN - 1, "%s", "preview");
    break;
  case RKADK_STREAM_TYPE_LIVE:
    snprintf(module, RKADK_INTF_FMT_LEN - 1, "%s", "live");
    break;
  default:
    RKADK_LOGE("Nonsupport stream type: %d", enStreamType);
    return -1;
  }

  ret = RKADK_PARAM_SetRcParam(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencTrans(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencVui(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencHierarchicalQp(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencFilter(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencQbias(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencQbias2(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencStaticWeight(u32CamId, module, stVencAttr);
  ret |= RKADK_PARAM_SetVencParam(u32CamId, module, stVencAttr);

  return ret;
}

RKADK_PARAM_CONTEXT_S *RKADK_PARAM_GetCtx() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx;
}

RKADK_PARAM_COMM_CFG_S *RKADK_PARAM_GetCommCfg() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx.stCfg.stCommCfg;
}

RKADK_PARAM_WBC_REC_CFG_S *RKADK_PARAM_GetWbcRecCfg() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);

  // load wbc record config
  if (!g_stPARAMCtx.stCfg.pstWbcRecCfg) {
    RKADK_PARAM_WBC_REC_CFG_S *pstWbcRecCfg = NULL;
    char entry[RKADK_BUFFER_LEN] = {'\0'};

    pstWbcRecCfg = malloc(sizeof(RKADK_PARAM_WBC_REC_CFG_S));
    if (!pstWbcRecCfg) {
      RKADK_LOGE("malloc pstWbcRecCfg failed");
      return NULL;
    }

    memset(pstWbcRecCfg, 0, sizeof(RKADK_PARAM_WBC_REC_CFG_S));

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:enable_audio");
    pstWbcRecCfg->enable_audio = RKADK_IniGetBool(-1, entry, 1);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:vo_wbc");
    pstWbcRecCfg->vo_wbc = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:rotation");
    pstWbcRecCfg->rotation = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:record_time");
    pstWbcRecCfg->record_time = RKADK_IniGetInt(-1, entry, 60);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:max_width");
    pstWbcRecCfg->attribute.max_width = RKADK_IniGetInt(-1, entry, 1280);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:max_height");
    pstWbcRecCfg->attribute.max_height = RKADK_IniGetInt(-1, entry, 720);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:width");
    pstWbcRecCfg->attribute.width = RKADK_IniGetInt(-1, entry, 1280);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:height");
    pstWbcRecCfg->attribute.height = RKADK_IniGetInt(-1, entry, 720);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:bufsize");
    pstWbcRecCfg->attribute.bufsize = RKADK_IniGetInt(-1, entry, 1382400);  //1280x720x1.5

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:bitrate");
    pstWbcRecCfg->attribute.bitrate = RKADK_IniGetInt(-1, entry, 2097152);  //2M

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:framerate");
    pstWbcRecCfg->attribute.framerate = RKADK_IniGetInt(-1, entry, 30);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:gop");
    pstWbcRecCfg->attribute.gop = RKADK_IniGetInt(-1, entry, 30);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:profile");
    pstWbcRecCfg->attribute.profile = RKADK_IniGetInt(-1, entry, 100);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:codec_type");
    pstWbcRecCfg->attribute.codec_type = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:venc_chn");
    pstWbcRecCfg->attribute.venc_chn = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:enable_vpss");
    pstWbcRecCfg->attribute.enable_vpss = RKADK_IniGetBool(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:vpss_grp");
    pstWbcRecCfg->attribute.vpss_grp = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:vpss_chn");
    pstWbcRecCfg->attribute.vpss_chn = RKADK_IniGetInt(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:post_aiisp");
    pstWbcRecCfg->attribute.post_aiisp = RKADK_IniGetBool(-1, entry, 0);

    snprintf(entry, RKADK_BUFFER_LEN, "record.wbc:rc_mode");
    const char *strings = RKADK_IniGetString(-1, entry, "VBR");
    memcpy(pstWbcRecCfg->attribute.rc_mode, strings, strlen(strings));

    if (get_log_level() == RKADK_LOG_LEVEL_DEBUG) {
      printf("Wbc Record Config\n");
      printf("\tpstWbcRecCfg enable_audio: %d\n", pstWbcRecCfg->enable_audio);
      printf("\tpstWbcRecCfg vo_wbc: %d\n", pstWbcRecCfg->vo_wbc);
      printf("\tpstWbcRecCfg record_time: %d\n", pstWbcRecCfg->record_time);
      printf("\tpstWbcRecCfg rotation: %d\n", pstWbcRecCfg->rotation);
      printf("\tpstWbcRecCfg width: %d\n", pstWbcRecCfg->attribute.max_width);
      printf("\tpstWbcRecCfg height: %d\n", pstWbcRecCfg->attribute.max_height);
      printf("\tpstWbcRecCfg width: %d\n", pstWbcRecCfg->attribute.width);
      printf("\tpstWbcRecCfg height: %d\n", pstWbcRecCfg->attribute.height);
      printf("\tpstWbcRecCfg bufsize: %d\n", pstWbcRecCfg->attribute.bufsize);
      printf("\tpstWbcRecCfg bitrate: %d\n", pstWbcRecCfg->attribute.bitrate);
      printf("\tpstWbcRecCfg framerate: %d\n", pstWbcRecCfg->attribute.framerate);
      printf("\tpstWbcRecCfg gop: %d\n", pstWbcRecCfg->attribute.gop);
      printf("\tpstWbcRecCfg profile: %d\n", pstWbcRecCfg->attribute.profile);
      printf("\tpstWbcRecCfg venc_chn: %d\n", pstWbcRecCfg->attribute.venc_chn);
      printf("\tpstWbcRecCfg vpss_grp: %d\n", pstWbcRecCfg->attribute.vpss_grp);
      printf("\tpstWbcRecCfg vpss_chn: %d\n", pstWbcRecCfg->attribute.vpss_chn);
      printf("\tpstWbcRecCfg post_aiisp: %d\n",  pstWbcRecCfg->attribute.post_aiisp);
      printf("\tpstWbcRecCfg codec_type: %d\n", pstWbcRecCfg->attribute.codec_type);
      printf("\tpstWbcRecCfg rc_mode: %s\n",  pstWbcRecCfg->attribute.rc_mode);
      printf("\tpstWbcRecCfg enable_pskip: %d\n", pstWbcRecCfg->attribute.enable_pskip);
    }

    g_stPARAMCtx.stCfg.pstWbcRecCfg = pstWbcRecCfg;
  }

  return g_stPARAMCtx.stCfg.pstWbcRecCfg;
}

RKADK_PARAM_REC_CFG_S *RKADK_PARAM_GetRecCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;
}

RKADK_PARAM_SENSOR_CFG_S *RKADK_PARAM_GetSensorCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];
}

RKADK_PARAM_STREAM_CFG_S *
RKADK_PARAM_GetStreamCfg(RKADK_U32 u32CamId, RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);

  if (enStrmType == RKADK_STREAM_TYPE_PREVIEW)
    return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg;
  else
    return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stLiveCfg;
}

RKADK_PARAM_PHOTO_CFG_S *RKADK_PARAM_GetPhotoCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;
}

RKADK_PARAM_DISP_CFG_S *RKADK_PARAM_GetDispCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stDispCfg;
}

RKADK_PARAM_THUMB_CFG_S *RKADK_PARAM_GetThumbCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stThumbCfg;
}

RKADK_PARAM_AUDIO_CFG_S *RKADK_PARAM_GetAudioCfg() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx.stCfg.stAudioCfg;
}

RKADK_S32 RKADK_PARAM_GetVencChnId(RKADK_U32 u32CamId,
                                   RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_S32 s32VencChnId = -1;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg)
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    else
      s32VencChnId = pstRecCfg->attribute[0].venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_VIDEO_SUB: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg)
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    else
      s32VencChnId = pstRecCfg->attribute[1].venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_SNAP: {
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
    if (!pstPhotoCfg)
      RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    else
      s32VencChnId = pstPhotoCfg->venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE: {
    RKADK_PARAM_STREAM_CFG_S *pstStreamCfg =
        RKADK_PARAM_GetStreamCfg(u32CamId, enStrmType);
    if (!pstStreamCfg)
      RKADK_LOGE("RKADK_PARAM_GetStreamCfg failed");
    else
      s32VencChnId = pstStreamCfg->attribute.venc_chn;
    break;
  }
  default:
    RKADK_LOGE("Unsupport stream type: %d", enStrmType);
    break;
  }

  return s32VencChnId;
}

RKADK_S32 RKADK_PARAM_GetThumbChnId(RKADK_U32 u32CamId,
                                   RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_S32 s32VencChnId = -1;
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!pstThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    s32VencChnId = pstThumbCfg->record_main_venc_chn;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    s32VencChnId = pstThumbCfg->record_sub_venc_chn;
    break;

  case RKADK_STREAM_TYPE_SNAP:
    s32VencChnId = pstThumbCfg->photo_venc_chn;
    break;
  default:
    RKADK_LOGE("enStrmType[%d] don't thumbanail", enStrmType);
    break;
  }

  return s32VencChnId;
}

void RKADK_PARAM_GetVpssId(RKADK_U32 u32CamId, RKADK_STREAM_TYPE_E enStrmType,
                                   RKADK_S32 *s32VpssGrp, RKADK_S32 *s32VpssChn) {
  if (u32CamId >= RKADK_MAX_SENSOR_CNT) {
    RKADK_LOGE("invalid camera id: %d", u32CamId);
    return;
  }

  *s32VpssGrp = -1;
  *s32VpssChn = -1;
  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg) {
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    } else {
      *s32VpssGrp = pstRecCfg->attribute[0].vpss_grp;
      *s32VpssChn = pstRecCfg->attribute[0].vpss_chn;
    }
    break;
  }
  case RKADK_STREAM_TYPE_VIDEO_SUB: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg) {
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    } else {
      *s32VpssGrp = pstRecCfg->attribute[1].vpss_grp;
      *s32VpssChn = pstRecCfg->attribute[1].vpss_chn;
    }
    break;
  }
  case RKADK_STREAM_TYPE_SNAP: {
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
    if (!pstPhotoCfg) {
      RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    } else {
      *s32VpssGrp = pstPhotoCfg->vpss_grp;
      *s32VpssChn = pstPhotoCfg->vpss_chn;
    }
    break;
  }
  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE: {
    RKADK_PARAM_STREAM_CFG_S *pstStreamCfg =
        RKADK_PARAM_GetStreamCfg(u32CamId, enStrmType);
    if (!pstStreamCfg) {
      RKADK_LOGE("RKADK_PARAM_GetStreamCfg failed");
    } else {
      *s32VpssGrp = pstStreamCfg->attribute.vpss_grp;
      *s32VpssChn = pstStreamCfg->attribute.vpss_chn;
    }
    break;
  }
  case RKADK_STREAM_TYPE_DISP: {
    RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(u32CamId);
    if (!pstDispCfg) {
      RKADK_LOGE("RKADK_PARAM_GetDispCfg failed");
    } else {
      *s32VpssGrp = pstDispCfg->vpss_grp;
      *s32VpssChn = pstDispCfg->vpss_chn;
    }
    break;
  }
  default:
    RKADK_LOGE("Unsupport stream type: %d", enStrmType);
    break;
  }

  return;
}

RKADK_S32 RKADK_PARAM_GetViDevId(RKADK_U32 u32CamId) {
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  return pstSensorCfg->dev_id;
}

RKADK_PARAM_RES_E RKADK_PARAM_GetResType(RKADK_U32 width, RKADK_U32 height) {
  RKADK_PARAM_RES_E type = RKADK_RES_BUTT;

  if (width == RKADK_WIDTH_480P && height == RKADK_HEIGHT_480P)
    type = RKADK_RES_480P;
  else if (width == RKADK_WIDTH_720P && height == RKADK_HEIGHT_720P)
    type = RKADK_RES_720P;
  else if (width == RKADK_WIDTH_1080P && height == RKADK_HEIGHT_1080P)
    type = RKADK_RES_1080P;
  else if (width == RKADK_WIDTH_1296P && height == RKADK_HEIGHT_1296P)
    type = RKADK_RES_1296P;
  else if (width == RKADK_WIDTH_1440P && height == RKADK_HEIGHT_1440P)
    type = RKADK_RES_1440P;
  else if (width == RKADK_WIDTH_1520P && height == RKADK_HEIGHT_1520P)
    type = RKADK_RES_1520P;
  else if (width == RKADK_WIDTH_1600P && height == RKADK_HEIGHT_1600P)
    type = RKADK_RES_1600P;
  else if (width == RKADK_WIDTH_1620P && height == RKADK_HEIGHT_1620P)
    type = RKADK_RES_1620P;
  else if (width == RKADK_WIDTH_1944P && height == RKADK_HEIGHT_1944P)
    type = RKADK_RES_1944P;
  else if (width == RKADK_WIDTH_2160P && height == RKADK_HEIGHT_2160P)
    type = RKADK_RES_2160P;
  else if (width == RKADK_WIDTH_3456P && height == RKADK_HEIGHT_3456P)
    type = RKADK_RES_3456P;
  else if (width == RKADK_WIDTH_4000P && height == RKADK_HEIGHT_4000P)
    type = RKADK_RES_4000P;
  else if (width == RKADK_WIDTH_4275P && height == RKADK_HEIGHT_4275P)
    type = RKADK_RES_4275P;
  else if (width == RKADK_WIDTH_4320P && height == RKADK_HEIGHT_4320P)
    type = RKADK_RES_4320P;
  else if (width == RKADK_WIDTH_4480P && height == RKADK_HEIGHT_4480P)
    type = RKADK_RES_4480P;
  else if (width == RKADK_WIDTH_3672P && height == RKADK_HEIGHT_3672P)
    type = RKADK_RES_3672P;
  else
    RKADK_LOGE("Unsupport resolution(%d*%d)", width, height);

  return type;
}

RKADK_S32 RKADK_PARAM_GetResolution(RKADK_PARAM_RES_E type, RKADK_U32 *width, RKADK_U32 *height) {
  switch (type) {
  case RKADK_RES_480P:
    *width = RKADK_WIDTH_480P;
    *height = RKADK_HEIGHT_480P;
    break;
  case RKADK_RES_720P:
    *width = RKADK_WIDTH_720P;
    *height = RKADK_HEIGHT_720P;
    break;
  case RKADK_RES_1080P:
    *width = RKADK_WIDTH_1080P;
    *height = RKADK_HEIGHT_1080P;
    break;
  case RKADK_RES_1296P:
    *width = RKADK_WIDTH_1296P;
    *height = RKADK_HEIGHT_1296P;
    break;
  case RKADK_RES_1440P:
    *width = RKADK_WIDTH_1440P;
    *height = RKADK_HEIGHT_1440P;
    break;
  case RKADK_RES_1520P:
    *width = RKADK_WIDTH_1520P;
    *height = RKADK_HEIGHT_1520P;
    break;
  case RKADK_RES_1600P:
    *width = RKADK_WIDTH_1600P;
    *height = RKADK_HEIGHT_1600P;
    break;
  case RKADK_RES_1620P:
    *width = RKADK_WIDTH_1620P;
    *height = RKADK_HEIGHT_1620P;
    break;
  case RKADK_RES_1944P:
    *width = RKADK_WIDTH_1944P;
    *height = RKADK_HEIGHT_1944P;
    break;
  case RKADK_RES_2160P:
    *width = RKADK_WIDTH_2160P;
    *height = RKADK_HEIGHT_2160P;
    break;
  case RKADK_RES_3456P:
    *width = RKADK_WIDTH_3456P;
    *height = RKADK_HEIGHT_3456P;
    break;
  case RKADK_RES_4000P:
    *width = RKADK_WIDTH_4000P;
    *height = RKADK_HEIGHT_4000P;
    break;
  case RKADK_RES_4275P:
    *width = RKADK_WIDTH_4275P;
    *height = RKADK_HEIGHT_4275P;
    break;
  case RKADK_RES_4320P:
    *width = RKADK_WIDTH_4320P;
    *height = RKADK_HEIGHT_4320P;
    break;
  case RKADK_RES_4480P:
    *width = RKADK_WIDTH_4480P;
    *height = RKADK_HEIGHT_4480P;
    break;
  case RKADK_RES_3672P:
    *width = RKADK_WIDTH_3672P;
    *height = RKADK_HEIGHT_3672P;
    break;
  default:
    RKADK_LOGE("Unsupport resolution type: %d, set to 1080P", type);
    *width = RKADK_WIDTH_1080P;
    *height = RKADK_HEIGHT_1080P;
    break;
  }

  return 0;
}

static RKADK_PARAM_RES_E RKADK_PARAM_GetStreamResolution(RKADK_S32 s32CamId, RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_U32 u32Width = 0, u32Height = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(s32CamId);
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, enStrmType);

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    u32Width = pstRecCfg->attribute[0].width;
    u32Height = pstRecCfg->attribute[0].height;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    u32Width = pstRecCfg->attribute[1].width;
    u32Height = pstRecCfg->attribute[1].height;
    break;

  case RKADK_STREAM_TYPE_SNAP:
    u32Width = pstPhotoCfg->image_width;
    u32Height = pstPhotoCfg->image_height;
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    u32Width = pstStreamCfg->attribute.width;
    u32Height = pstStreamCfg->attribute.height;
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStrmType);
    return RKADK_RES_BUTT;
  }

  return RKADK_PARAM_GetResType(u32Width, u32Height);
}

static RKADK_S32 RKADK_PARAM_SetStreamResolution(RKADK_S32 s32CamId, RKADK_PARAM_RES_CFG_S *pstResCfg) {
  bool bEnableVpss = false, bSaveViCfg = false;
  RKADK_U32 *u32Width, *u32Height;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(s32CamId);
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstResCfg->enStreamType);
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(s32CamId);
  char *psensorPath = g_stPARAMCtx.sensorPath[s32CamId];

  switch (pstResCfg->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    bEnableVpss = pstRecCfg->attribute[0].enable_vpss;
    u32Width = &(pstRecCfg->attribute[0].width);
    u32Height = &(pstRecCfg->attribute[0].height);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    bEnableVpss = pstRecCfg->attribute[1].enable_vpss;
    u32Width = &(pstRecCfg->attribute[1].width);
    u32Height = &(pstRecCfg->attribute[1].height);
    break;

  case RKADK_STREAM_TYPE_SNAP:
    bEnableVpss = pstPhotoCfg->enable_vpss;
    u32Width = &(pstPhotoCfg->image_width);
    u32Height = &(pstPhotoCfg->image_height);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    bEnableVpss = pstStreamCfg->attribute.enable_vpss;
    u32Width = &(pstStreamCfg->attribute.width);
    u32Height = &(pstStreamCfg->attribute.height);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstResCfg->enStreamType);
    return -1;
  }

  if (pstSensorCfg->used_isp && !bEnableVpss)
    bSaveViCfg = true;

  RKADK_PARAM_GetResolution(pstResCfg->enResType, u32Width, u32Height);

  switch (pstResCfg->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
  case RKADK_STREAM_TYPE_VIDEO_SUB:
    RKADK_PARAM_SaveRecAttr(psensorPath, s32CamId, pstResCfg->enStreamType);
    if (bSaveViCfg)
      RKADK_PARAM_SetRecViAttr(s32CamId);
    break;

  case RKADK_STREAM_TYPE_SNAP:
    RKADK_PARAM_SavePhotoCfg(psensorPath, s32CamId);
    if (bSaveViCfg)
      RKADK_PARAM_SetPhotoViAttr(s32CamId);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    RKADK_PARAM_SaveStreamCfg(psensorPath, s32CamId, pstResCfg->enStreamType);
    if (bSaveViCfg)
      RKADK_PARAM_SetStreamViAttr(s32CamId, pstResCfg->enStreamType);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstResCfg->enStreamType);
    return -1;
  }

  return 0;
}

static RKADK_CODEC_TYPE_E
RKADK_PARAM_GetCodecType(RKADK_S32 s32CamId, RKADK_STREAM_TYPE_E enStreamType) {
  RKADK_CODEC_TYPE_E enCodecType = RKADK_CODEC_TYPE_BUTT;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    enCodecType = pstRecCfg->attribute[0].codec_type;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    enCodecType = pstRecCfg->attribute[1].codec_type;
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, enStreamType);
    enCodecType = pstStreamCfg->attribute.codec_type;
    break;
  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStreamType);
    break;
  }

  return enCodecType;
}

static RKADK_S32
RKADK_PARAM_SetCodecType(RKADK_S32 s32CamId,
                         RKADK_PARAM_CODEC_CFG_S *pstCodecCfg) {
  RKADK_S32 ret;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (pstCodecCfg->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[0].codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstRecCfg->attribute[0].codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_MAIN);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[1].codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstRecCfg->attribute[1].codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_SUB);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg =
        RKADK_PARAM_GetStreamCfg(s32CamId, pstCodecCfg->enStreamType);
    if (pstStreamCfg->attribute.codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstStreamCfg->attribute.codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveStreamCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                    pstCodecCfg->enStreamType);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstCodecCfg->enStreamType);
    break;
  }

  return ret;
}

static RKADK_U32 RKADK_PARAM_GetBitrate(RKADK_S32 s32CamId,
                                        RKADK_STREAM_TYPE_E enStreamType) {
  RKADK_U32 bitrate = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    bitrate = pstRecCfg->attribute[0].bitrate;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    bitrate = pstRecCfg->attribute[1].bitrate;
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, enStreamType);
    bitrate = pstStreamCfg->attribute.bitrate;
    break;
  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStreamType);
    break;
  }

  return bitrate;
}

static RKADK_S32 RKADK_PARAM_SetBitrate(RKADK_S32 s32CamId,
                                        RKADK_PARAM_BITRATE_S *pstBitrate) {
  RKADK_S32 ret = -1;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (pstBitrate->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[0].bitrate == pstBitrate->u32Bitrate)
      return 0;

    pstRecCfg->attribute[0].bitrate = pstBitrate->u32Bitrate;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_MAIN);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[1].bitrate == pstBitrate->u32Bitrate)
      return 0;

    pstRecCfg->attribute[1].bitrate = pstBitrate->u32Bitrate;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_SUB);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstBitrate->enStreamType);
    if (pstStreamCfg->attribute.bitrate == pstBitrate->u32Bitrate)
      return 0;

    pstStreamCfg->attribute.bitrate = pstBitrate->u32Bitrate;
    ret = RKADK_PARAM_SaveStreamCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                    pstBitrate->enStreamType);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstBitrate->enStreamType);
    break;
  }

  return ret;
}

static RKADK_U32 RKADK_PARAM_GetFps(RKADK_S32 s32CamId,
                                    RKADK_STREAM_TYPE_E enStreamType) {
  RKADK_U32 u32Fps = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;

  switch (enStreamType) {
  case RKADK_STREAM_TYPE_SENSOR:
    pstSensorCfg = RKADK_PARAM_GetSensorCfg(s32CamId);
    u32Fps = pstSensorCfg->framerate;
    break;

  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    u32Fps = pstRecCfg->attribute[0].framerate;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    u32Fps = pstRecCfg->attribute[1].framerate;
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, enStreamType);
    u32Fps = pstStreamCfg->attribute.framerate;
    break;
  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStreamType);
    break;
  }

  return u32Fps;
}

static RKADK_S32 RKADK_PARAM_SetFps(RKADK_S32 s32CamId,
                                    RKADK_PARAM_FPS_S *pstFps) {
  RKADK_S32 ret = -1;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;

  switch (pstFps->enStreamType) {
  case RKADK_STREAM_TYPE_SENSOR:
    pstSensorCfg = RKADK_PARAM_GetSensorCfg(s32CamId);
    if (pstSensorCfg->framerate == pstFps->u32Framerate)
      return 0;

    if (pstSensorCfg->used_isp) {
      pstSensorCfg->framerate = pstFps->u32Framerate;
      ret = RKADK_PARAM_SaveSensorCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId);
    } else {
      RKADK_LOGW("AHD sensor[%d] nonsupport set fps, please set venc fps!", s32CamId);
      return -1;
    }
    break;

  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[0].framerate == pstFps->u32Framerate)
      return 0;

    pstRecCfg->attribute[0].framerate = pstFps->u32Framerate;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_MAIN);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[1].framerate == pstFps->u32Framerate)
      return 0;

    pstRecCfg->attribute[1].framerate = pstFps->u32Framerate;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_SUB);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstFps->enStreamType);
    if (pstStreamCfg->attribute.framerate == pstFps->u32Framerate)
      return 0;

    pstStreamCfg->attribute.framerate = pstFps->u32Framerate;
    ret = RKADK_PARAM_SaveStreamCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                    pstFps->enStreamType);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstFps->enStreamType);
    break;
  }

  return ret;
}

static RKADK_U32 RKADK_PARAM_GetGop(RKADK_S32 s32CamId,
                                    RKADK_STREAM_TYPE_E enStreamType) {
  RKADK_U32 u32Gop = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    u32Gop = pstRecCfg->attribute[0].gop;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    u32Gop = pstRecCfg->attribute[1].gop;
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, enStreamType);
    u32Gop = pstStreamCfg->attribute.gop;
    break;
  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStreamType);
    break;
  }

  return u32Gop;
}

static RKADK_S32 RKADK_PARAM_SetGop(RKADK_S32 s32CamId,
                                    RKADK_PARAM_GOP_S *pstGop) {
  RKADK_S32 ret = -1;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (pstGop->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[0].gop == pstGop->u32Gop)
      return 0;

    pstRecCfg->attribute[0].gop = pstGop->u32Gop;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_MAIN);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[1].gop == pstGop->u32Gop)
      return 0;

    pstRecCfg->attribute[1].gop = pstGop->u32Gop;
    ret = RKADK_PARAM_SaveRecAttr(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                  RKADK_STREAM_TYPE_VIDEO_SUB);
    break;

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstGop->enStreamType);
    if (pstStreamCfg->attribute.gop == pstGop->u32Gop)
      return 0;

    pstStreamCfg->attribute.gop = pstGop->u32Gop;
    ret = RKADK_PARAM_SaveStreamCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                    pstGop->enStreamType);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstGop->enStreamType);
    break;
  }

  return ret;
}

static RKADK_U32 RKADK_PARAM_GetRecTime(RKADK_S32 s32CamId,
                                        RKADK_STREAM_TYPE_E enStreamType,
                                        RKADK_PARAM_TYPE_E enParamType) {
  RKADK_U32 time = -1;
  RKADK_PARAM_REC_TIME_CFG_S *pstRecTimeCfg;

  if (enStreamType == RKADK_STREAM_TYPE_VIDEO_MAIN)
    pstRecTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg.record_time_cfg[0];
  else
    pstRecTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg.record_time_cfg[1];

  switch (enParamType) {
  case RKADK_PARAM_TYPE_RECORD_TIME:
    time = pstRecTimeCfg->record_time;
    break;
  case RKADK_PARAM_TYPE_SPLITTIME:
    time = pstRecTimeCfg->splite_time;
    break;
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL:
    time = pstRecTimeCfg->lapse_interval;
    break;
  default:
    RKADK_LOGE("Invalid enParamType: %d", enParamType);
    break;
  }

  return time;
}

static RKADK_S32 RKADK_PARAM_SetRecTime(RKADK_S32 s32CamId,
                                        RKADK_PARAM_REC_TIME_S *pstRecTime,
                                        RKADK_PARAM_TYPE_E enParamType) {
  RKADK_S32 ret;
  RKADK_PARAM_REC_TIME_CFG_S *pstRecTimeCfg;

  if (pstRecTime->enStreamType == RKADK_STREAM_TYPE_VIDEO_MAIN)
    pstRecTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg.record_time_cfg[0];
  else
    pstRecTimeCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg.record_time_cfg[1];

  switch (enParamType) {
  case RKADK_PARAM_TYPE_RECORD_TIME:
    if (pstRecTimeCfg->record_time == pstRecTime->time)
      return 0;

    pstRecTimeCfg->record_time = pstRecTime->time;
    break;
  case RKADK_PARAM_TYPE_SPLITTIME:
    if (pstRecTimeCfg->splite_time == pstRecTime->time)
      return 0;

    pstRecTimeCfg->splite_time = pstRecTime->time;
    break;
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL:
    if (pstRecTimeCfg->lapse_interval == pstRecTime->time)
      return 0;

    pstRecTimeCfg->lapse_interval = pstRecTime->time;
    break;
  default:
    RKADK_LOGE("Invalid enParamType: %d", enParamType);
    return -1;
  }

  ret = RKADK_PARAM_SaveRecTime(g_stPARAMCtx.sensorPath[s32CamId], s32CamId,
                                pstRecTime->enStreamType);
  return ret;
}

static RKADK_S32 RKADK_PARAM_SetInputFormat(RKADK_S32 s32CamId,
                                        RKADK_PARAM_INPUT_FMT_S *pstFormat) {
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;
  RKADK_PRAAM_VI_ATTR_S *pstViAttr = NULL;

  if (!pstFormat->format) {
    RKADK_LOGE("format is null");
    return -1;
  }

  switch (pstFormat->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    pstViAttr = &pstRecCfg->vi_attr[0];
    break;
  }

  case RKADK_STREAM_TYPE_VIDEO_SUB: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    pstViAttr = &pstRecCfg->vi_attr[1];
    break;
  }

  case RKADK_STREAM_TYPE_SNAP: {
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(s32CamId);
    pstViAttr = &pstPhotoCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE: {
    RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstFormat->enStreamType);
    pstViAttr = &pstStreamCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_DISP: {
    RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(s32CamId);
    pstViAttr = &pstDispCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_THUMB: {
    RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(s32CamId);
    pstViAttr = &pstThumbCfg->vi_attr;
    break;
  }

  default: {
    RKADK_LOGE("Unsupport enStreamType: %d", pstFormat->enStreamType);
    return -1;
  }
  }

  RKADK_LOGD("index[%d] old enPixelFormat[%d] enCompressMode[%d]",
      pstViAttr->index, pstViAttr->stChnAttr.enPixelFormat, pstViAttr->stChnAttr.enCompressMode);
  pstViAttr->stChnAttr.enPixelFormat = RKADK_PARAM_GetPixFmt(pstFormat->format, &(pstViAttr->stChnAttr.enCompressMode));
  RKADK_LOGD("index[%d] new enPixelFormat[%d] enCompressMode[%d]",
      pstViAttr->index, pstViAttr->stChnAttr.enPixelFormat, pstViAttr->stChnAttr.enCompressMode);

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[pstViAttr->index];
  if (!strcmp(pstFormat->format, pstViCfg->pix_fmt)) {
    RKADK_LOGD("vi[%d] format[%s] no change", pstViAttr->index, pstFormat->format);
  } else {
    strncpy(pstViCfg->pix_fmt, pstFormat->format, sizeof(pstFormat->format) + 1);
    RKADK_PARAM_SaveViCfg(g_stPARAMCtx.sensorPath[s32CamId], pstViAttr->index, s32CamId);
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetInputBufCnt(RKADK_S32 s32CamId,
                                        RKADK_PARAM_INPUT_BUFCNT_S *pstBufCnt) {
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;
  RKADK_PRAAM_VI_ATTR_S *pstViAttr = NULL;

  switch (pstBufCnt->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    pstViAttr = &pstRecCfg->vi_attr[0];
    break;
  }

  case RKADK_STREAM_TYPE_VIDEO_SUB: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    pstViAttr = &pstRecCfg->vi_attr[1];
    break;
  }

  case RKADK_STREAM_TYPE_SNAP: {
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(s32CamId);
    pstViAttr = &pstPhotoCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_PREVIEW:
  case RKADK_STREAM_TYPE_LIVE: {
    RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId, pstBufCnt->enStreamType);
    pstViAttr = &pstStreamCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_DISP: {
    RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(s32CamId);
    pstViAttr = &pstDispCfg->vi_attr;
    break;
  }

  case RKADK_STREAM_TYPE_THUMB: {
    RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(s32CamId);
    pstViAttr = &pstThumbCfg->vi_attr;
    break;
  }

  default: {
    RKADK_LOGE("Unsupport enStreamType: %d", pstBufCnt->enStreamType);
    return -1;
  }
  }

  RKADK_LOGD("index[%d] old bufcnt[%d]", pstViAttr->index, pstViAttr->stChnAttr.stIspOpt.u32BufCount);
  pstViAttr->stChnAttr.stIspOpt.u32BufCount = pstBufCnt->u32BufCnt;
  RKADK_LOGD("index[%d] new bufcnt[%d]", pstViAttr->index, pstViAttr->stChnAttr.stIspOpt.u32BufCount);

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stViCfg[pstViAttr->index];
  if (pstBufCnt->u32BufCnt == pstViCfg->buf_cnt) {
    RKADK_LOGD("vi[%d] bufcnt[%d] no change", pstViAttr->index, pstViCfg->buf_cnt);
  } else {
    pstViCfg->buf_cnt = pstBufCnt->u32BufCnt;
    RKADK_PARAM_SaveViCfg(g_stPARAMCtx.sensorPath[s32CamId], pstViAttr->index, s32CamId);
  }

  return 0;
}

RKADK_S32 RKADK_PARAM_GetCamParam(RKADK_S32 s32CamId,
                                  RKADK_PARAM_TYPE_E enParamType,
                                  RKADK_VOID *pvParam) {
  RKADK_PARAM_FPS_S *pstFps;

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);

  // RKADK_LOGD("s32CamId: %d, enParamType: %d, u32_pvParam: %d, b_pvParam: %d",
  // s32CamId, enParamType, *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stPhotoCfg;

  switch (enParamType) {
  case RKADK_PARAM_TYPE_FPS:
    pstFps = (RKADK_PARAM_FPS_S *)pvParam;
    pstFps->u32Framerate = RKADK_PARAM_GetFps(s32CamId, pstFps->enStreamType);
    break;
  case RKADK_PARAM_TYPE_FLIP:
    *(bool *)pvParam = pstSensorCfg->flip;
    break;
  case RKADK_PARAM_TYPE_MIRROR:
    *(bool *)pvParam = pstSensorCfg->mirror;
    break;
  case RKADK_PARAM_TYPE_LDC:
    *(RKADK_U32 *)pvParam = pstSensorCfg->ldc;
    break;
  case RKADK_PARAM_TYPE_ANTIFOG:
    *(RKADK_U32 *)pvParam = pstSensorCfg->antifog;
    break;
  case RKADK_PARAM_TYPE_WDR:
    *(RKADK_U32 *)pvParam = pstSensorCfg->wdr;
    break;
  case RKADK_PARAM_TYPE_HDR:
    *(RKADK_U32 *)pvParam = pstSensorCfg->hdr;
    break;
  case RKADK_PARAM_TYPE_RES:
    *(RKADK_PARAM_RES_E *)pvParam = RKADK_PARAM_GetResType(
        pstRecCfg->attribute[0].width, pstRecCfg->attribute[0].height);
    break;
  case RKADK_PARAM_TYPE_STREAM_RES: {
    RKADK_PARAM_RES_CFG_S *pstResCfg;

    pstResCfg = (RKADK_PARAM_RES_CFG_S *)pvParam;
    pstResCfg->enResType = RKADK_PARAM_GetStreamResolution(s32CamId, pstResCfg->enStreamType);
    break;
  }
  case RKADK_PARAM_TYPE_CODEC_TYPE: {
    RKADK_PARAM_CODEC_CFG_S *pstCodecCfg;

    pstCodecCfg = (RKADK_PARAM_CODEC_CFG_S *)pvParam;
    pstCodecCfg->enCodecType = RKADK_PARAM_GetCodecType(s32CamId, pstCodecCfg->enStreamType);
    break;
  }
  case RKADK_PARAM_TYPE_BITRATE: {
    RKADK_PARAM_BITRATE_S *pstBitrate;

    pstBitrate = (RKADK_PARAM_BITRATE_S *)pvParam;
    pstBitrate->u32Bitrate = RKADK_PARAM_GetBitrate(s32CamId, pstBitrate->enStreamType);
    break;
  }
  case RKADK_PARAM_TYPE_GOP: {
    RKADK_PARAM_GOP_S *pstGop;

    pstGop = (RKADK_PARAM_GOP_S *)pvParam;
    pstGop->u32Gop = RKADK_PARAM_GetGop(s32CamId, pstGop->enStreamType);
    break;
  }
  case RKADK_PARAM_TYPE_RECORD_TIME:
  case RKADK_PARAM_TYPE_SPLITTIME:
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL: {
    RKADK_PARAM_REC_TIME_S *pstRecTime;

    pstRecTime = (RKADK_PARAM_REC_TIME_S *)pvParam;
    pstRecTime->time =
        RKADK_PARAM_GetRecTime(s32CamId, pstRecTime->enStreamType, enParamType);
    break;
  }
  case RKADK_PARAM_TYPE_RECORD_TYPE:
    *(RKADK_REC_TYPE_E *)pvParam = pstRecCfg->record_type;
    break;
  case RKADK_PARAM_TYPE_FILE_CNT:
    *(RKADK_U32 *)pvParam = pstRecCfg->file_num;
    break;
  case RKADK_PARAM_TYPE_LAPSE_MULTIPLE:
    *(RKADK_U32 *)pvParam = pstRecCfg->lapse_multiple;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_TIME:
    *(RKADK_U32 *)pvParam = pstRecCfg->pre_record_time;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_MODE:
    *(RKADK_MUXER_PRE_RECORD_MODE_E *)pvParam = pstRecCfg->pre_record_mode;
    break;
  case RKADK_PARAM_TYPE_ENABLE_AUDIO:
    *(bool *)pvParam = pstRecCfg->enable_audio;
    break;
  case RKADK_PARAM_TYPE_PHOTO_RES:
    *(RKADK_PARAM_RES_E *)pvParam = RKADK_PARAM_GetResType(
        pstPhotoCfg->image_width, pstPhotoCfg->image_height);
    break;
  case RKADK_PARAM_TYPE_JPEG_SLICE:
    *(bool *)pvParam = pstPhotoCfg->jpeg_slice;
    break;
  case RKADK_PARAM_TYPE_SLICE_HEIGHT:
    *(bool *)pvParam = pstPhotoCfg->slice_height;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetCamParam(RKADK_S32 s32CamId,
                                  RKADK_PARAM_TYPE_E enParamType,
                                  const RKADK_VOID *pvParam) {
  RKADK_S32 ret;
  bool bSaveRecCfg = false;
  bool bSavePhotoCfg = false;
  bool bSaveSensorCfg = false;
  RKADK_PARAM_RES_CFG_S stResCfg;

  // RKADK_LOGD("s32CamId: %d, enParamType: %d, u32_pvParam: %d, b_pvParam: %d",
  // s32CamId, enParamType, *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_CHECK_CAMERAID(s32CamId, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stRecCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[s32CamId];
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamId].stPhotoCfg;

  switch (enParamType) {
  case RKADK_PARAM_TYPE_FPS:
    ret = RKADK_PARAM_SetFps(s32CamId, (RKADK_PARAM_FPS_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_FLIP:
    RKADK_CHECK_EQUAL(pstSensorCfg->flip, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->flip = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_MIRROR:
    RKADK_CHECK_EQUAL(pstSensorCfg->mirror, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->mirror = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_LDC:
    RKADK_CHECK_EQUAL(pstSensorCfg->ldc, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->ldc = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_ANTIFOG:
    RKADK_CHECK_EQUAL(pstSensorCfg->antifog, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->antifog = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_WDR:
    RKADK_CHECK_EQUAL(pstSensorCfg->wdr, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->wdr = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_HDR:
    RKADK_CHECK_EQUAL(pstSensorCfg->hdr, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->hdr = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_MUTE:
    RKADK_CHECK_EQUAL(pstSensorCfg->mute, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->mute = *(bool *)pvParam;
#ifndef OS_RTT
    RKADK_PARAM_RecMute((RK_BOOL)pstSensorCfg->mute, pstCommCfg->mic_volume, pstSensorCfg->aenc_chn);
    bSaveSensorCfg = true;
#endif
    break;
  case RKADK_PARAM_TYPE_RES:
    stResCfg.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
    stResCfg.enResType = *(RKADK_PARAM_RES_E *)pvParam;
    ret = RKADK_PARAM_SetStreamResolution(s32CamId, &stResCfg);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    break;
  case RKADK_PARAM_TYPE_STREAM_RES:
    ret = RKADK_PARAM_SetStreamResolution(s32CamId, (RKADK_PARAM_RES_CFG_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_CODEC_TYPE:
    ret = RKADK_PARAM_SetCodecType(s32CamId, (RKADK_PARAM_CODEC_CFG_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_BITRATE:
    ret = RKADK_PARAM_SetBitrate(s32CamId, (RKADK_PARAM_BITRATE_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_GOP:
    ret = RKADK_PARAM_SetGop(s32CamId, (RKADK_PARAM_GOP_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_RECORD_TIME:
  case RKADK_PARAM_TYPE_SPLITTIME:
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL:
    ret = RKADK_PARAM_SetRecTime(s32CamId, (RKADK_PARAM_REC_TIME_S *)pvParam,
                                 enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_RECORD_TYPE:
    RKADK_CHECK_EQUAL(pstRecCfg->record_type, *(RKADK_REC_TYPE_E *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->record_type = *(RKADK_REC_TYPE_E *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_FILE_CNT:
    RKADK_CHECK_EQUAL(pstRecCfg->file_num, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->file_num = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_LAPSE_MULTIPLE:
    RKADK_CHECK_EQUAL(pstRecCfg->lapse_multiple, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->lapse_multiple = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_TIME:
    RKADK_CHECK_EQUAL(pstRecCfg->pre_record_time, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->pre_record_time = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_MODE:
    RKADK_CHECK_EQUAL(pstRecCfg->pre_record_mode,
                      *(RKADK_MUXER_PRE_RECORD_MODE_E *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->pre_record_mode = *(RKADK_MUXER_PRE_RECORD_MODE_E *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_ENABLE_AUDIO:
    RKADK_CHECK_EQUAL(pstRecCfg->enable_audio, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->enable_audio = *(bool *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_PHOTO_RES:
    stResCfg.enStreamType = RKADK_STREAM_TYPE_SNAP;
    stResCfg.enResType = *(RKADK_PARAM_RES_E *)pvParam;
    ret = RKADK_PARAM_SetStreamResolution(s32CamId, &stResCfg);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    break;
  case RKADK_PARAM_TYPE_JPEG_SLICE:
    RKADK_CHECK_EQUAL(pstPhotoCfg->jpeg_slice, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstPhotoCfg->jpeg_slice = *(bool *)pvParam;
    bSavePhotoCfg = true;
    break;
  case RKADK_PARAM_TYPE_SLICE_HEIGHT:
    RKADK_CHECK_EQUAL(pstPhotoCfg->slice_height, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstPhotoCfg->slice_height = *(RKADK_U32 *)pvParam;
    bSavePhotoCfg = true;
    break;
  case RKADK_PARAM_TYPE_INPUT_FMT:
    ret = RKADK_PARAM_SetInputFormat(s32CamId, (RKADK_PARAM_INPUT_FMT_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_INPUT_BUFCNT:
    ret = RKADK_PARAM_SetInputBufCnt(s32CamId, (RKADK_PARAM_INPUT_BUFCNT_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  if (bSaveSensorCfg)
    RKADK_PARAM_SaveSensorCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId);

  if (bSaveRecCfg)
    RKADK_PARAM_SaveRecCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId);

  if (bSavePhotoCfg) {
    RKADK_PARAM_SavePhotoCfg(g_stPARAMCtx.sensorPath[s32CamId], s32CamId);
    if (pstSensorCfg->used_isp)
      RKADK_PARAM_SetPhotoViAttr(s32CamId);
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetCommParam(RKADK_PARAM_TYPE_E enParamType,
                                   const RKADK_VOID *pvParam) {
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;

  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);

  // RKADK_LOGD("enParamType: %d, u32_pvParam: %d, b_pvParam: %d", enParamType,
  // *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  RKADK_LOGI("RKADK_PARAM_SetCommParam enParamType = %d", enParamType);
  switch (enParamType) {
  case RKADK_PARAM_TYPE_REC_MUTE:
    for(int i = 0; i < pstCommCfg->sensor_count; i++) {
      pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[i];
      RKADK_CHECK_EQUAL(pstSensorCfg->mute, *(bool *)pvParam,
                        g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
      pstSensorCfg->mute = *(bool *)pvParam;
#ifndef OS_RTT
      RKADK_PARAM_RecMute((RK_BOOL)pstSensorCfg->mute, pstCommCfg->mic_volume, pstSensorCfg->aenc_chn);
      RKADK_PARAM_SaveSensorCfg(g_stPARAMCtx.sensorPath[i], i);
#endif
    }
    break;
  case RKADK_PARAM_TYPE_VOLUME:
    RKADK_CHECK_EQUAL(pstCommCfg->speaker_volume, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->speaker_volume = *(RKADK_U32 *)pvParam;
    RKADK_PARAM_SetSpeakerVolume(pstCommCfg->speaker_volume);
    break;
  case RKADK_PARAM_TYPE_MIC_VOLUME:
    RKADK_CHECK_EQUAL(pstCommCfg->mic_volume, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->mic_volume = *(RKADK_U32 *)pvParam;
    RKADK_PARAM_SetMicVolume(pstCommCfg->mic_volume);
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_PARAM_SaveCommCfg(g_stPARAMCtx.path);
  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_GetCommParam(RKADK_PARAM_TYPE_E enParamType,
                                   RKADK_VOID *pvParam) {
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);

  // RKADK_LOGD("enParamType: %d, u32_pvParam: %d, b_pvParam: %d", enParamType,
  // *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  switch (enParamType) {
  case RKADK_PARAM_TYPE_VOLUME:
    *(RKADK_U32 *)pvParam = pstCommCfg->speaker_volume;
    break;
  case RKADK_PARAM_TYPE_MIC_VOLUME:
    *(RKADK_U32 *)pvParam = pstCommCfg->mic_volume;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetDefault() {
  RKADK_S32 ret = RKADK_SUCCESS;

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  memset(&g_stPARAMCtx.stCfg, 0, sizeof(RKADK_PARAM_CFG_S));
  ret = RKADK_PARAM_LoadDefault();
  if (ret) {
    RKADK_LOGE("load default ini failed");
  } else {
    ret = RKADK_PARAM_SetMediaViAttr();
    if (ret)
      RKADK_LOGE("set media vi attribute failed");

    RKADK_PARAM_SetVolume();
    RKADK_PARAM_Dump();
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return ret;
}

RKADK_VOID RKADK_PARAM_SetDefPath() {
  int i;
  RKADK_U32 u32PathLen;
  const char *pDefPath = NULL;

  pDefPath = getenv("rkadk_default_ini_path");

  memset(g_stPARAMCtx.defPath, 0, RKADK_PATH_LEN);
  if (pDefPath) {
    u32PathLen = strlen(pDefPath) + strlen("rkadk_defsetting_sensor_0.ini");
    if (u32PathLen < (RKADK_PATH_LEN - 1)) {
      sprintf(g_stPARAMCtx.defPath, "%s/%s", pDefPath, "rkadk_defsetting.ini");
      RKADK_LOGD("defPath: %s", g_stPARAMCtx.defPath);

      for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
        memset(g_stPARAMCtx.defSensorPath[i], 0, RKADK_PATH_LEN);
        sprintf(g_stPARAMCtx.defSensorPath[i], "%s/%s_%d.ini",
                pDefPath, "rkadk_defsetting_sensor", i);

        RKADK_LOGD("defSensorPath[%d]: %s", i, g_stPARAMCtx.defSensorPath[i]);
      }

      return;
    }
  }

  memcpy(g_stPARAMCtx.defPath, RKADK_DEFPARAM_PATH, strlen(RKADK_DEFPARAM_PATH));
  RKADK_LOGD("defPath: %s", g_stPARAMCtx.defPath);

  for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
    memset(g_stPARAMCtx.defSensorPath[i], 0, RKADK_PATH_LEN);
    sprintf(g_stPARAMCtx.defSensorPath[i], "%s_%d.ini",
            RKADK_DEFPARAM_PATH_SENSOR_PREFIX, i);

    RKADK_LOGD("defSensorPath[%d]: %s", i, g_stPARAMCtx.defSensorPath[i]);
  }
}

RKADK_VOID RKADK_PARAM_SetPath(char *path, char **sensorPath) {
  int i;
  RKADK_U32 u32PathLen;
  RKADK_U32 sPathCount = 0;
  char *sPath = NULL;
  const char *pDirPath = NULL;
  bool bDone = false;

  pDirPath = getenv("rkadk_ini_path");

  memset(g_stPARAMCtx.path, 0, RKADK_PATH_LEN);
  if (path) {
    u32PathLen = strlen(path);
    if (u32PathLen < RKADK_PATH_LEN) {
      memcpy(g_stPARAMCtx.path, path, u32PathLen);
      bDone = true;
    }
  } else if (pDirPath) {
    u32PathLen = strlen(pDirPath) + strlen("rkadk_setting.ini");
    if (u32PathLen < (RKADK_PATH_LEN - 1)) {
      sprintf(g_stPARAMCtx.path, "%s/%s", pDirPath, "rkadk_setting.ini");
      bDone = true;
    }
  }

  if (!bDone)
    memcpy(g_stPARAMCtx.path, RKADK_PARAM_PATH, strlen(RKADK_PARAM_PATH));
  else
    bDone = false;

  RKADK_LOGD("path: %s", g_stPARAMCtx.path);

  if (sensorPath) {
    while ((sPath = *sensorPath++) != NULL) {
      if (sPathCount == RKADK_MAX_SENSOR_CNT) {
        RKADK_LOGW("input sensor path count > RKADK_MAX_SENSOR_CNT[%d]", RKADK_MAX_SENSOR_CNT);
        break;
      }

      u32PathLen = strlen(sPath) > (RKADK_PATH_LEN - 1) ? (RKADK_PATH_LEN - 1) : strlen(sPath);
      memset(g_stPARAMCtx.sensorPath[sPathCount], 0, RKADK_PATH_LEN);
      memcpy(g_stPARAMCtx.sensorPath[sPathCount], sPath, u32PathLen);
      RKADK_LOGD("sensorPath[%d]: %s[%d]", sPathCount, g_stPARAMCtx.sensorPath[sPathCount], u32PathLen);

      sPathCount++;
    }

    bDone = true;
  } else if (pDirPath) {
    u32PathLen = strlen(pDirPath) + strlen("rkadk_setting_sensor_0.ini");
    if (u32PathLen < (RKADK_PATH_LEN - 1)) {
      for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
        memset(g_stPARAMCtx.sensorPath[i], 0, RKADK_PATH_LEN);
        sprintf(g_stPARAMCtx.sensorPath[i], "%s/%s_%d.ini", pDirPath, "rkadk_setting_sensor", i);
        RKADK_LOGD("sensorPath[%d]: %s", i, g_stPARAMCtx.sensorPath[i]);
      }

      bDone = true;
    }
  }

  if (!bDone) {
    for (i = 0; i < RKADK_MAX_SENSOR_CNT; i++) {
      memset(g_stPARAMCtx.sensorPath[i], 0, RKADK_PATH_LEN);
      sprintf(g_stPARAMCtx.sensorPath[i], "%s_%d.ini",
              RKADK_PARAM_PATH_SENSOR_PREFIX, i);

      RKADK_LOGD("sensorPath[%d]: %s", i, g_stPARAMCtx.sensorPath[i]);
    }
  }
}

RKADK_S32 RKADK_PARAM_Init(char *globalSetting, char **sesnorSettingArrary) {
  RKADK_S32 ret = RKADK_SUCCESS;

  /* Check Module Init Status */
  if (g_stPARAMCtx.bInit)
    return RKADK_SUCCESS;

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  RKADK_IniInit();

  RKADK_PARAM_SetDefPath();
  RKADK_PARAM_SetPath(globalSetting, sesnorSettingArrary);

  memset(&g_stPARAMCtx.stCfg, 0, sizeof(RKADK_PARAM_CFG_S));

  ret = RKADK_PARAM_LoadParam(g_stPARAMCtx.path, g_stPARAMCtx.sensorPath);
  if (ret) {
    RKADK_LOGE("load setting ini failed, load default ini");
    ret = RKADK_PARAM_LoadDefault();
    if (ret) {
      RKADK_LOGE("load default ini failed, use default param");
      RKADK_PARAM_UseDefault();
    }
  }

  ret = RKADK_PARAM_SetMediaViAttr();
  if (ret) {
    RKADK_LOGE("set media vi attribute failed");
    goto end;
  }

  RKADK_PARAM_SetVolume();
  RKADK_PARAM_Dump();
  g_stPARAMCtx.bInit = true;

end:
  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return ret;
}

RKADK_S32 RKADK_PARAM_Deinit() {
  if (g_stPARAMCtx.stCfg.pstWbcRecCfg) {
    free(g_stPARAMCtx.stCfg.pstWbcRecCfg);
    g_stPARAMCtx.stCfg.pstWbcRecCfg = NULL;
  }

  RKADK_IniDeinit();
  g_stPARAMCtx.bInit = false;
  return 0;
}

RKADK_STREAM_TYPE_E RKADK_PARAM_VencChnMux(RKADK_U32 u32CamId,
                                           RKADK_U32 u32ChnId) {
  int i;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_STREAM_TYPE_BUTT);

  RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (pstRecCfg) {
    for (i = 0; i < (int)pstRecCfg->file_num; i++) {
      if (u32ChnId == pstRecCfg->attribute[i].venc_chn) {
        if (i == 0)
          return RKADK_STREAM_TYPE_VIDEO_MAIN;
        else
          return RKADK_STREAM_TYPE_VIDEO_SUB;
      }
    }
  }

  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg =
      RKADK_PARAM_GetStreamCfg(u32CamId, RKADK_STREAM_TYPE_PREVIEW);
  if (pstStreamCfg && (u32ChnId == pstStreamCfg->attribute.venc_chn))
    return RKADK_STREAM_TYPE_PREVIEW;

  RKADK_PARAM_STREAM_CFG_S *pstLiveCfg =
      RKADK_PARAM_GetStreamCfg(u32CamId, RKADK_STREAM_TYPE_LIVE);
  if (pstLiveCfg && (u32ChnId == pstLiveCfg->attribute.venc_chn))
    return RKADK_STREAM_TYPE_LIVE;

  return RKADK_STREAM_TYPE_BUTT;
}

RKADK_U32 RKADK_PARAM_GetStreamBufCnt(RKADK_U32 u32CamId, bool bIsAudio) {
  RKADK_U32 u32Integer = 0, u32Remainder = 0;
  RKADK_U32 u32PreRecCacheTime = 0;
  RKADK_U32 u32BufCount = RKADK_MUXER_CELL_MAX_CNT;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

  pstAudioParam = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioParam) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return u32BufCount;
  }

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return u32BufCount;
  }

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return u32BufCount;
  }

  if(pstRecCfg->pre_record_time == 0)
    return u32BufCount;

  u32Integer = pstRecCfg->attribute[0].gop / pstRecCfg->attribute[0].framerate;
  u32Remainder = pstRecCfg->attribute[0].gop % pstRecCfg->attribute[0].framerate;
  u32PreRecCacheTime = pstRecCfg->pre_record_time + u32Integer;
  if (u32Remainder)
    u32PreRecCacheTime += 1;

  if (bIsAudio)
    u32BufCount = pstAudioParam->channels * (pstAudioParam->samplerate
                  / pstAudioParam->samples_per_frame) * (u32PreRecCacheTime + 2);
  else
    u32BufCount = (u32PreRecCacheTime + 2) * pstRecCfg->attribute[0].framerate;

  return u32BufCount;
}

int RKADK_PARAM_GetViAttr(RKADK_U32 u32CamId, RKADK_STREAM_TYPE_E enSubStreamType,
                          RKADK_PRAAM_VI_ATTR_S **pViChnAttr) {
  switch(enSubStreamType) {
    case RKADK_STREAM_TYPE_VIDEO_MAIN: {
      RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
      if (!pstRecCfg) {
        RKADK_LOGE("RKADK_PARAM_GetRecCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstRecCfg->vi_attr[0]);
      break;
    }

    case RKADK_STREAM_TYPE_VIDEO_SUB: {
      RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
      if (!pstRecCfg) {
        RKADK_LOGE("RKADK_PARAM_GetRecCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstRecCfg->vi_attr[1]);
      break;
    }

    case RKADK_STREAM_TYPE_SNAP: {
      RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
      if (!pstPhotoCfg) {
        RKADK_LOGE("RKADK_PARAM_GetPhotoCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstPhotoCfg->vi_attr);
      break;
    }

    case RKADK_STREAM_TYPE_PREVIEW: {
      RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(u32CamId, RKADK_STREAM_TYPE_PREVIEW);
      if (!pstStreamCfg) {
        RKADK_LOGE("RKADK_PARAM_GetStreamCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstStreamCfg->vi_attr);
      break;
    }

    case RKADK_STREAM_TYPE_LIVE: {
      RKADK_PARAM_STREAM_CFG_S *pstLiveCfg = RKADK_PARAM_GetStreamCfg(u32CamId, RKADK_STREAM_TYPE_LIVE);
      if (!pstLiveCfg) {
        RKADK_LOGE("RKADK_PARAM_GetStreamCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstLiveCfg->vi_attr);
      break;
    }

    case RKADK_STREAM_TYPE_DISP: {
      RKADK_PARAM_DISP_CFG_S *pstDispCfg = RKADK_PARAM_GetDispCfg(u32CamId);
      if (!pstDispCfg) {
        RKADK_LOGE("RKADK_PARAM_GetDispCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(pstDispCfg->vi_attr);
      break;
    }

    case RKADK_STREAM_TYPE_THUMB: {
      RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
      if (!ptsThumbCfg) {
        RKADK_LOGE("RKADK_PARAM_GetThumbCfg[%d] failed", u32CamId);
        return -1;
      }
      *pViChnAttr = &(ptsThumbCfg->vi_attr);
      break;
    }

    default: {
      RKADK_LOGE("Invalid enSubStreamType[%d]", enSubStreamType);
      return -1;
    }
  }

  return 0;
}
