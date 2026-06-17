/*
 * Copyright (c) 2022 Rockchip, Inc. All Rights Reserved.
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

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_signal.h"
#include "rkadk_thread.h"
#include "rkadk_media_comm.h"
#include "rkadk_player.h"
#include "rkadk_demuxer.h"
#include "rkadk_audio_decoder.h"
#include "rk_debug.h"
#include "rk_defines.h"
#include <math.h>
#include "rk_comm_vdec.h"
#include "rk_mpi_ao.h"
#if defined(RK3506)
#include "rk_mpi_af.h"
#include "rk_mpi_amix.h"
#endif
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#ifndef OS_RTT
#include "rkdemuxer.h"
#else
#include "../../common_algorithm/misc/include/rkdemuxer.h"
#endif
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "rkadk_hal.h"
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

#define PLAYER_SNAPSHOT_MAX_WIDTH 4096
#define PLAYER_SNAPSHOT_MAX_HEIGHT 4096
#define MAX_BUFFER_SZIE 4096

typedef enum {
  RKADK_PLAYER_PAUSE_FALSE = 0x0,
  RKADK_PLAYER_PAUSE_START,
} RKADK_PLAYER_PAUSE_STATUS_E;

typedef enum {
  RKADK_PLAYER_SEEK_NO = 0x0,
  RKADK_PLAYER_SEEK_WAIT,
  RKADK_PLAYER_SEEK_VIDEO_DOING,
  RKADK_PLAYER_SEEK_VIDEO_DONE,
  RKADK_PLAYER_SEEK_DONE,
} RKADK_PLAYER_SEEK_STATUS_E;

typedef enum {
  RKADK_PLAYER_AF_OP_RESAMPLE = 0x0,
  RKADK_PLAYER_AF_OP_MAP,
  RKADK_PLAYER_AF_OP_CACHE,
} RKADK_PLAYER_AF_OP_E;

typedef struct {
  RKADK_U32   srcWidth;
  RKADK_U32   srcHeight;
  RKADK_U32   chnIndex;
  RKADK_VDEC_DECODE_MODE_E   inputMode;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_U32   compressMode;
  RKADK_U32   frameBufferCnt;
  RKADK_U32   streamBufferCnt;
  RKADK_S32   chnFd;
  PIXEL_FORMAT_E outputPixFmt;
  RKADK_BOOL  bEnableDei;
  RKADK_BOOL  bEnableColmv;
  bool bEnableMbPool;
  MB_POOL s32Pool;
} RKADK_PLAYER_VDEC_CTX_S;

typedef struct {
  RKADK_S32  enIntfSync;
  RKADK_U32  u32Width;
  RKADK_U32  u32Height;
} VO_Timing_S;

typedef struct {
  RKADK_U32 u32Screen0VoLayer;
  RKADK_U32 u32Screen1VoLayer;

  RKADK_U32 u32Screen0Rows;
  RKADK_U32 u32Screen1Rows;
  RK_BOOL bDoubleScreen;
} VO_CFG_S;

typedef struct {
  RKADK_U32 mode;
  VO_INTF_SYNC_E enIntfSync;
} VO_MODE_S;

typedef struct {
  RKADK_U32 u32VoLay;
  RKADK_U32 u32VoDev;
  RKADK_U32 u32VoChn;
  RKADK_VO_INTF_TYPE_E  enIntfType;
  RKADK_VO_INTF_SYNC_E  enIntfSync;
  RKADK_U32  x;
  RKADK_U32  y;
  RKADK_U32  dispWidth;
  RKADK_U32  dispHeight;
  RKADK_U32  imgeWidth;
  RKADK_U32  imageHeight;
  PIXEL_FORMAT_E pixFormat;
  RKADK_U32  dispFrmRt;
  MIRROR_E    enMirror;
  ROTATION_E  enRotation;
  RKADK_VO_SPLICE_MODE_E enVoSpliceMode;
  RKADK_U32  u32DispBufLen;
  RKADK_U32  u32ChnPriority;
} RKADK_PLAYER_VO_CTX_S;

typedef struct {
  RKADK_S32      chnNum;
  RKADK_S32      sampleRate;
  RKADK_S32      channel;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_S32      decMode;
  RK_BOOL        bBlock;
  RKADK_S32      chnIndex;
  RKADK_S32      clrChnBuf;
  RKADK_S32      adecBufCnt;
} RKADK_PLAYER_ADEC_CTX_S;

typedef struct {
  const RKADK_CHAR *dstFilePath;
  RKADK_S32 chnNum;
  RKADK_S32 sampleRate;        //device samplerate
  RKADK_S32 reSmpSampleRate;   //file demuxer samplerate
  RKADK_S32 channel;           //file demuxer channel
  RKADK_S32 deviceChannel;     //device channel
  RKADK_S32 bitWidth;
  RKADK_S32 devId;
  RKADK_S32 periodCount;
  RKADK_S32 periodSize;
  RKADK_CHAR cardName[RKADK_BUFFER_LEN];
  RKADK_S32 chnIndex;
  RKADK_S32 setVolume;
  RKADK_S32 setMute;
  RKADK_S32 setFadeRate;
  RKADK_S32 setTrackMode;
  RKADK_S32 getVolume;
  RKADK_S32 getMute;
  RKADK_S32 getTrackMode;
  RKADK_S32 queryChnStat;
  RKADK_S32 pauseResumeChn;
  RKADK_S32 saveFile;
  RKADK_S32 queryFileStat;
  RKADK_S32 clrChnBuf;
  RKADK_S32 clrPubAttr;
  RKADK_S32 getPubAttr;
  RKADK_U32 u32SpeakerVolume;
} RKADK_PLAYER_AO_CTX_S;

typedef struct {
  RKADK_S32      s32GrpId;
  RKADK_S32      s32PipeId;
  RKADK_S32      s32ChnId;
  RKADK_S32      s32PeriodSize;
  RKADK_S32      s32PipeNum;
  RKADK_S32      s32ChnNum;
  RKADK_S32      s32InSampleRate;
  RKADK_U32      u32InChannel;
} RKADK_PLAYER_AMIX_CTX_S;

typedef struct {
  RKADK_S32      s32ChnIndex;
  RKADK_S32      s32ChnCnt;
  RKADK_S32      s32InputRate;
  RKADK_S32      s32InputCh;
  RKADK_S32      s32InputBitWidth;
  RKADK_S32      s32OutRate;
  RKADK_S32      s32OutCh;
  RKADK_S32      s32OutputBitWidth;
  RKADK_S32      s32ReSampleIndex;
  RKADK_S32      s32MapIndex;
  RKADK_S32      s32CacheIndex;
  RKADK_S32      s32CacheSize;
  RKADK_S32      s32InFrameSize;
} RKADK_PLAYER_AF_CTX_S;

typedef struct {
  PIXEL_FORMAT_E enPixelFormat;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U32 u32VirWidth;
  RKADK_U32 u32VirHeight;
  MB_BLK pMbBlk;
} PLAYER_SNAPSHOT_FRAME_S;

typedef struct {
  bool bSnapshot;
  bool bVencChnExist;
  RKADK_U32 u32VencChn;
  RKADK_U32 u32MaxWidth;
  RKADK_U32 u32MaxHeight;
  PLAYER_SNAPSHOT_FRAME_S stFrame;

  void *pThread;
  void *pSignal;
  pthread_mutex_t mutex;
  RKADK_PPLAYER_SNAPSHOT_RECV_FN pfnDataCallback;
} RKADK_PLAYER_SNAPSHOT_PARAM_S;

typedef struct {
  RKADK_BOOL bEnableFadeIn;
  RKADK_U32 u32FadeInFrames;
  RKADK_U32 u32VoChn;
  RKADK_U32 u32SetDataSourceCnt;
  RKADK_U32 u32Priority;
} RKADK_PLAYER_FADEIN_PARAM_S;

typedef struct {
  bool bEnableSmoothPlay;
  void *pSignal;
} RKADK_PLAYER_SMOOTHPLAY_PARAM_S;

typedef struct {
  RKADK_CHAR pFilePath[RKADK_PATH_LEN];
  RKADK_PLAYER_STATE_E enStatus;
  pthread_mutex_t mutex;
  RKADK_BOOL bIsNetRTStream;    //network real time stream
  RKADK_BOOL bEnableBlackBackground;
  RKADK_BOOL bStopSendStream;

  RKADK_BOOL bEnableVideo;
  RKADK_BOOL bVideoExist;
  RKADK_S64 videoTimeStamp;
  RKADK_PLAYER_VDEC_CTX_S stVdecCtx;
  RKADK_PLAYER_VO_CTX_S stVoCtx;

  RKADK_BOOL bEnableAudio;
  RKADK_BOOL bAudioExist;
  RKADK_PLAYER_ADEC_CTX_S stAdecCtx;
  RKADK_PLAYER_AO_CTX_S stAoCtx;
  RKADK_PLAYER_AF_CTX_S stAfCtx;
  RKADK_PLAYER_AMIX_CTX_S stAmixCtx;

  RKADK_VOID *pDemuxerCfg;
  RKADK_DEMUXER_PARAM_S stDemuxerParam;
  RKADK_BOOL bEnableThirdDemuxer;
  RKADK_BOOL bVideoSendStreamFail;
  RKADK_S32 videoStreamCount;

  RKADK_PLAYER_SEEK_STATUS_E enSeekStatus;
  RKADK_S64 seekTimeStamp;
  RKADK_U32 duration;
  RKADK_S32 frameCount;
  RKADK_S64 positionTimeStamp;
#ifndef OS_RTT
  pthread_t tidDataSend;
#else
  rt_thread_t tidDataSend;
  RKADK_BOOL  bSendEnd;
#endif
  RKADK_PLAYER_EVENT_FN pfnPlayerCallback;

  RKADK_PLAYER_SNAPSHOT_PARAM_S stSnapshotParam;

  RKADK_U32 u32VdecWaterline; /* frames = left frames waiting for decode + pics waiting for output */
  RKADK_PLAYER_FADEIN_PARAM_S stFadeInParam;
  RKADK_PLAYER_SMOOTHPLAY_PARAM_S stSmoothPlayParam;
  RKADK_S32 s32AdecChnCnt;
} RKADK_PLAYER_HANDLE_S;

typedef struct {
  RKADK_S32 s32PlayerPrepareCnt;
  RKADK_S32 s32AoSampleRate;
  RKADK_S32 s32AoChannels;
  RKADK_S32 s32AoBitWidth;
  RKADK_S32 s32PtNumPerFrm;
} RKADK_MULTI_PLAYER_CTX_S;

static RKADK_MULTI_PLAYER_CTX_S gMultiPlayerCtx = {0};

static void RKADK_PLAYER_ProcessEvent(RKADK_PLAYER_HANDLE_S *pstPlayer,
                                            RKADK_PLAYER_EVENT_E enEvent,
                                            RKADK_VOID *pData) {
  RKADK_CHECK_POINTER_N(pstPlayer);

  switch (pstPlayer->enSeekStatus) {
    case RKADK_PLAYER_SEEK_NO:
    case RKADK_PLAYER_SEEK_DONE:
      break;
    default:
      return;
  }

  if (pstPlayer->pfnPlayerCallback)
    pstPlayer->pfnPlayerCallback((RKADK_MW_PTR)pstPlayer, enEvent, pData);
}

static RKADK_S32 VdecCtxInit(RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx, RKADK_PLAYER_VDEC_CFG_S stVdecCfg) {
  memset(pstVdecCtx, 0, sizeof(RKADK_PLAYER_VDEC_CTX_S));

  pstVdecCtx->inputMode = stVdecCfg.u32DecodeMode;
  pstVdecCtx->compressMode = COMPRESS_MODE_NONE;
  pstVdecCtx->frameBufferCnt = stVdecCfg.u32FrameBufCnt;
  pstVdecCtx->streamBufferCnt = stVdecCfg.u32StreamBufCnt;
  pstVdecCtx->chnIndex = stVdecCfg.u32VdecChnId;
  pstVdecCtx->bEnableColmv = RKADK_TRUE;
  pstVdecCtx->outputPixFmt = RK_FMT_YUV420SP;
  pstVdecCtx->eCodecType = RKADK_CODEC_TYPE_H264;
  pstVdecCtx->srcWidth = 320;
  pstVdecCtx->srcHeight = 240;
  pstVdecCtx->s32Pool = MB_INVALID_POOLID;

  if (pstVdecCtx->frameBufferCnt <= 0)
    pstVdecCtx->frameBufferCnt = 3;

  if (pstVdecCtx->streamBufferCnt <= 0)
    pstVdecCtx->streamBufferCnt = 8;

  return RKADK_SUCCESS;
}

static RKADK_S32 CreateVdec(RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx) {
  RKADK_S32 ret = RKADK_SUCCESS;
  VDEC_CHN_ATTR_S stAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
  MB_PIC_CAL_S stMbPicCalResult;
  VDEC_MOD_PARAM_S stModParam;

  memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
  memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
  memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));
  memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

  RKADK_LOGI("found video width %d height %d pixfmt %d",
              pstVdecCtx->srcWidth, pstVdecCtx->srcHeight, pstVdecCtx->outputPixFmt);

  if (pstVdecCtx->bEnableMbPool) {
    stModParam.enVdecMBSource = MB_SOURCE_USER;
    ret = RK_MPI_VDEC_SetModParam(&stModParam);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("vdec %d SetModParam failed[%x]", pstVdecCtx->chnIndex, ret);
      return ret;
    }
  }

  stVdecPicBufAttr.enCodecType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stVdecPicBufAttr.stPicBufAttr.u32Width = pstVdecCtx->srcWidth;
  stVdecPicBufAttr.stPicBufAttr.u32Height = pstVdecCtx->srcHeight;
  stVdecPicBufAttr.stPicBufAttr.enPixelFormat = pstVdecCtx->outputPixFmt;
  stVdecPicBufAttr.stPicBufAttr.enCompMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;
  ret = RK_MPI_CAL_VDEC_GetPicBufferSize(&stVdecPicBufAttr, &stMbPicCalResult);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("get picture buffer size failed[%x]", ret);
    return ret;
  }

  stAttr.enMode = RKADK_MEDIA_GetRkDecodeMode(pstVdecCtx->inputMode);
  stAttr.enType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stAttr.u32PicWidth = pstVdecCtx->srcWidth;
  stAttr.u32PicHeight = pstVdecCtx->srcHeight;
  if (pstVdecCtx->bEnableDei) {
    // iep must remain 5 buffers for deinterlace
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt + 5;
  } else
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt;

  stAttr.u32StreamBufCnt = pstVdecCtx->streamBufferCnt;
  /*
    * if decode 10bit stream, need specify the u32FrameBufSize,
    * other conditions can be set to 0, calculated internally.
    */
  stAttr.u32FrameBufSize = stMbPicCalResult.u32MBSize;

  if (!pstVdecCtx->bEnableColmv)
    stAttr.stVdecVideoAttr.bTemporalMvpEnable = RK_FALSE;

  stAttr.u32FrameBufDepth = stAttr.u32StreamBufCnt;
  ret = RK_MPI_VDEC_CreateChn(pstVdecCtx->chnIndex, &stAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Create vdec chn[%d] failed[%x]", pstVdecCtx->chnIndex, ret);
    return ret;
  }

  stVdecParam.stVdecVideoParam.enCompressMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;
  if (pstVdecCtx->bEnableDei)
    stVdecParam.stVdecVideoParam.bDeiEn = (RK_BOOL)pstVdecCtx->bEnableDei;

  // it is only effective to disable MV when decoding sequence output
  if (!pstVdecCtx->bEnableColmv)
    stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;

  ret = RK_MPI_VDEC_SetChnParam(pstVdecCtx->chnIndex, &stVdecParam);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Set chn[%d] param failed[%x]", pstVdecCtx->chnIndex, ret);
    goto __FAILED;
  }

  pstVdecCtx->chnFd = RK_MPI_VDEC_GetFd(pstVdecCtx->chnIndex);
  if (pstVdecCtx->chnFd <= 0) {
    RKADK_LOGE("Get chn[%d] fd[%d] failed", pstVdecCtx->chnIndex, pstVdecCtx->chnFd);
    goto __FAILED;
  }

  if (pstVdecCtx->bEnableMbPool) {
    MB_POOL_CONFIG_S stMbPoolCfg;

    memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    stMbPoolCfg.u64MBSize = pstVdecCtx->srcWidth * pstVdecCtx->srcHeight * 3; //stMbPicCalResult.u32MBSize;
    stMbPoolCfg.u32MBCnt  = stAttr.u32FrameBufCnt;
    stMbPoolCfg.enRemapMode = MB_REMAP_MODE_CACHED;
    stMbPoolCfg.bPreAlloc = RK_TRUE;
    pstVdecCtx->s32Pool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
    if (pstVdecCtx->s32Pool == MB_INVALID_POOLID) {
      RKADK_LOGE("create pool failed[%x]!", ret);
      goto __FAILED;
    }

    ret = RK_MPI_VDEC_AttachMbPool(pstVdecCtx->chnIndex, pstVdecCtx->s32Pool);
    if (ret != RK_SUCCESS) {
      RK_LOGE("attach vdec mb pool %d failed[%x]!", pstVdecCtx->chnIndex, ret);
      goto __FAILED;
    }
  }

  ret = RK_MPI_VDEC_StartRecvStream(pstVdecCtx->chnIndex);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("start recv chn[%d] failed[%x]", pstVdecCtx->chnIndex, ret);
    RK_MPI_VDEC_DetachMbPool(pstVdecCtx->chnIndex);
    goto __FAILED;
  }

  return RKADK_SUCCESS;

__FAILED:
  if (pstVdecCtx->chnFd > 0)
    RK_MPI_VDEC_CloseFd(pstVdecCtx->chnIndex);

  if (pstVdecCtx->s32Pool != MB_INVALID_POOLID)
    RK_MPI_MB_DestroyPool(pstVdecCtx->s32Pool);

  RK_MPI_VDEC_DestroyChn(pstVdecCtx->chnIndex);
  return RKADK_FAILURE;
}

static RKADK_S32 DestroyVdec(RKADK_PLAYER_VDEC_CTX_S *ctx) {
  RKADK_S32 ret = 0;

  ret = RK_MPI_VDEC_StopRecvStream(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("Stop Vdec stream failed[%x]", ret);
    return RKADK_FAILURE;
  }

  if (ctx->chnFd > 0) {
    ret = RK_MPI_VDEC_CloseFd(ctx->chnIndex);
    if (ret) {
      RKADK_LOGE("close Vdec fd failed[%x]", ret);
      return RKADK_FAILURE;
    }
  }

#if 0
  if (ctx->bEnableMbPool) {
    ret = RK_MPI_VDEC_DetachMbPool(ctx->chnIndex);
    if (ret) {
      RKADK_LOGE("Detach Vdec[%d] pool failed[%x]", ctx->chnIndex, ret);
      return RKADK_FAILURE;
    }
  }
#endif

  ret = RK_MPI_VDEC_DestroyChn(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("Destroy Vdec channel[%d] failed[%x]", ctx->chnIndex, ret);
    return RKADK_FAILURE;
  }

  if (ctx->s32Pool != MB_INVALID_POOLID) {
    ret = RK_MPI_MB_DestroyPool(ctx->s32Pool);
    if (ret) {
      RKADK_LOGE("Destroy Vdec[%d] pool failed[%x]", ctx->chnIndex, ret);
      return RKADK_FAILURE;
    }
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 SetVoCtx(RKADK_PLAYER_VO_CTX_S *pstVoCtx, RKADK_PLAYER_FRAME_INFO_S *pstFrameInfo) {
  memset(pstVoCtx, 0, sizeof(RKADK_PLAYER_VO_CTX_S));

  RKADK_MPI_VO_CheckDefaultID((RKADK_S32 *)&pstFrameInfo->u32VoDev, (RKADK_S32 *)&pstFrameInfo->u32VoLay);
  pstVoCtx->u32VoDev = pstFrameInfo->u32VoDev;
  pstVoCtx->u32VoLay = pstFrameInfo->u32VoLay;
  pstVoCtx->u32VoChn = pstFrameInfo->u32VoChn;
  pstVoCtx->x = pstFrameInfo->u32FrmInfoX;
  pstVoCtx->y = pstFrameInfo->u32FrmInfoY;
  pstVoCtx->dispWidth = pstFrameInfo->u32DispWidth;
  pstVoCtx->dispHeight = pstFrameInfo->u32DispHeight;
  pstVoCtx->imgeWidth = pstFrameInfo->u32ImgWidth;
  pstVoCtx->imageHeight = pstFrameInfo->u32ImgHeight;
  pstVoCtx->dispFrmRt = pstFrameInfo->stSyncInfo.u16FrameRate;
  pstVoCtx->enIntfType = pstFrameInfo->u32EnIntfType;
  pstVoCtx->enVoSpliceMode = pstFrameInfo->enVoSpliceMode;
  pstVoCtx->u32DispBufLen = pstFrameInfo->u32DispBufLen;

  switch (pstFrameInfo->u32VoFormat) {
    case VO_FORMAT_RGB888:
      pstVoCtx->pixFormat = RK_FMT_RGB888;
      break;
    case VO_FORMAT_NV12:
      pstVoCtx->pixFormat = RK_FMT_YUV420SP;
      break;
    case VO_FORMAT_RGB565:
      pstVoCtx->pixFormat = RK_FMT_RGB565;
      break;
    case VO_FORMAT_RGB444:
      pstVoCtx->pixFormat = RK_FMT_RGB444;
      break;
    case VO_FORMAT_BGR565:
      pstVoCtx->pixFormat = RK_FMT_BGR565;
      break;
    default:
      RKADK_LOGW("unsupport pix format[%d], use default[%d]", pstVoCtx->pixFormat, RK_FMT_RGB888);
      pstVoCtx->pixFormat = RK_FMT_RGB888;
  }

  pstVoCtx->enIntfSync = pstFrameInfo->enIntfSync;
  if (pstVoCtx->enIntfSync >= RKADK_VO_OUTPUT_BUTT) {
    RKADK_LOGE("Unsupport IntfSync[%d]", pstVoCtx->enIntfSync);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->bMirror && !pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_HORIZONTAL;
  else if (pstFrameInfo->bFlip && !pstFrameInfo->bMirror)
    pstVoCtx->enMirror = MIRROR_VERTICAL;
  else if (pstFrameInfo->bMirror && pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_BOTH;
  else
    pstVoCtx->enMirror = MIRROR_NONE;

  pstVoCtx->enRotation = (ROTATION_E)pstFrameInfo->u32Rotation;
  return RKADK_SUCCESS;
}

static RKADK_S32 AdecCtxInit(RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx, RKADK_PLAYER_AUDIO_CFG_S *pstAudioCfg) {
  memset(pstAdecCtx, 0, sizeof(RKADK_PLAYER_ADEC_CTX_S));

  pstAdecCtx->chnIndex = pstAudioCfg->u32AdecChnId;
  pstAdecCtx->sampleRate = 16000;
  pstAdecCtx->channel = 2;
  pstAdecCtx->decMode = ADEC_MODE_STREAM;
  pstAdecCtx->eCodecType = RKADK_CODEC_TYPE_MP3;
  pstAdecCtx->bBlock = RK_TRUE;
  pstAdecCtx->adecBufCnt = pstAudioCfg->u32AdecBufCnt;
  return RKADK_SUCCESS;
}

static RKADK_S32 CreateAdec(RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx) {
  RKADK_S32 ret = 0;
  ADEC_CHN_ATTR_S pstChnAttr;

  memset(&pstChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
  pstChnAttr.stCodecAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.stCodecAttr.u32Channels = pstAdecCtx->channel;
  pstChnAttr.stCodecAttr.u32SampleRate = pstAdecCtx->sampleRate;
  pstChnAttr.stCodecAttr.u32BitPerCodedSample = 4;
  pstChnAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.enMode = (ADEC_MODE_E)pstAdecCtx->decMode;
  if (pstAdecCtx->adecBufCnt) {
    pstChnAttr.u32BufCount = pstAdecCtx->adecBufCnt;
    pstChnAttr.u32Depth = pstAdecCtx->adecBufCnt;
  } else {
    pstChnAttr.u32BufCount = 8;
    pstChnAttr.u32Depth = 8;
  }
  pstChnAttr.u32BufSize = 50 * 1024;

  ret = RK_MPI_ADEC_CreateChn(pstAdecCtx->chnIndex, &pstChnAttr);
  if (ret)
    RKADK_LOGE("create adec chn[%d] err[%x]", pstAdecCtx->chnIndex, ret);

  return ret;
}

static void AoCtxInit(RKADK_PLAYER_AO_CTX_S *pstAoCtx, RKADK_PLAYER_AUDIO_CFG_S *pstAudioCfg) {
  memset(pstAoCtx, 0, sizeof(RKADK_PLAYER_AO_CTX_S));

  pstAoCtx->dstFilePath     = RKADK_NULL;
  pstAoCtx->chnNum          = 1;
  pstAoCtx->reSmpSampleRate = 0;
  pstAoCtx->channel         = 2;
  pstAoCtx->bitWidth        = AUDIO_BIT_WIDTH_16;
  pstAoCtx->periodCount     = 4;
  pstAoCtx->periodSize      = 4096;
  pstAoCtx->devId           = pstAudioCfg->u32AoDevId;
  pstAoCtx->chnIndex        = pstAudioCfg->u32AoChnId;
  pstAoCtx->setVolume       = 100;
  pstAoCtx->setMute         = 0;
  pstAoCtx->setTrackMode    = 0;
  pstAoCtx->setFadeRate     = 0;
  pstAoCtx->getVolume       = 1;
  pstAoCtx->getMute         = 0;
  pstAoCtx->getTrackMode    = 0;
  pstAoCtx->queryChnStat    = 0;
  pstAoCtx->pauseResumeChn  = 0;
  pstAoCtx->saveFile        = 0;
  pstAoCtx->queryFileStat   = 0;
  pstAoCtx->clrChnBuf       = 0;
  pstAoCtx->clrPubAttr      = 0;
  pstAoCtx->getPubAttr      = 0;
  pstAoCtx->u32SpeakerVolume = pstAudioCfg->u32SpeakerVolume;
  RKADK_LOGD("u32SpeakerVolume: %d", pstAoCtx->u32SpeakerVolume);

  if (pstAudioCfg->pSoundCard) {
    memcpy(pstAoCtx->cardName, pstAudioCfg->pSoundCard, strlen(pstAudioCfg->pSoundCard));
  } else {
    memcpy(pstAoCtx->cardName, "hw:0,0", strlen("hw:0,0"));
    RKADK_LOGI("Ao use default soundcard[hw:0,0]");
  }

  if (pstAudioCfg->u32AoChannle > 0) {
    pstAoCtx->deviceChannel = pstAudioCfg->u32AoChannle;
  } else {
    pstAoCtx->deviceChannel = 2;
    RKADK_LOGI("Ao use default device channle[2]");
  }

  if (pstAudioCfg->u32AoSampleRate > 0) {
    pstAoCtx->sampleRate = pstAudioCfg->u32AoSampleRate;
  } else {
    pstAoCtx->sampleRate = 16000;
    RKADK_LOGI("Ao use default sampleRate[16000]");
  }
  gMultiPlayerCtx.s32AoSampleRate = pstAudioCfg->u32AoSampleRate;
}

static RKADK_VOID QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 ret = 0;
  AO_CHN_STATE_S pstStat;

  memset(&pstStat, 0, sizeof(AO_CHN_STATE_S));
  ret = RK_MPI_AO_QueryChnStat(aoDevId, aoChn, &pstStat);
  if (ret == RKADK_SUCCESS) {
    RKADK_LOGI("query ao flow status:");
    RKADK_LOGI("total number of channel buffer : %d", pstStat.u32ChnTotalNum);
    RKADK_LOGI("free number of channel buffer : %d", pstStat.u32ChnFreeNum);
    RKADK_LOGI("busy number of channel buffer : %d", pstStat.u32ChnBusyNum);
  }
}

static AUDIO_SOUND_MODE_E FindSoundMode(RKADK_S32 ch) {
  AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
  switch (ch) {
    case 1:
      channel = AUDIO_SOUND_MODE_MONO;
      break;
    case 2:
      channel = AUDIO_SOUND_MODE_STEREO;
      break;
    default:
      RKADK_LOGE("Unsupport channel[%d]", ch);
      return AUDIO_SOUND_MODE_BUTT;
  }

  return channel;
}

static AUDIO_BIT_WIDTH_E FindBitWidth(RKADK_S32 bit) {
  AUDIO_BIT_WIDTH_E bitWidth = AUDIO_BIT_WIDTH_BUTT;
  switch (bit) {
    case 8:
      bitWidth = AUDIO_BIT_WIDTH_8;
      break;
    case 16:
      bitWidth = AUDIO_BIT_WIDTH_16;
      break;
    case 24:
      bitWidth = AUDIO_BIT_WIDTH_24;
      break;
    default:
      RKADK_LOGE("Unsupport bitwidth[%d]", bit);
      return AUDIO_BIT_WIDTH_BUTT;
  }

  return bitWidth;
}

static RKADK_S32 SetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 ret = 0;
  AO_CHN_PARAM_S pstParams;

  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));

  //aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0)
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  else if (aoChn == 1)
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;

  ret = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("Ao set channel[%d] params failed[%x]", aoChn, ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 CreateDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  RKADK_S32 ret = 0;
  AUDIO_SOUND_MODE_E soundMode;
  AIO_ATTR_S aoAttr;

  memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

  if (strlen(ctx->cardName)) {
    snprintf((RKADK_CHAR *)(aoAttr.u8CardName),
              sizeof(aoAttr.u8CardName), "%s", ctx->cardName);
  }
  aoAttr.soundCard.channels = ctx->deviceChannel;
  aoAttr.soundCard.sampleRate = ctx->sampleRate;
  aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

  aoAttr.enBitwidth = FindBitWidth(ctx->bitWidth);
  if (aoAttr.enBitwidth == AUDIO_BIT_WIDTH_BUTT) {
    RKADK_LOGE("Unsupport audio bitWidth[%d]", aoAttr.enBitwidth);
    return RKADK_FAILURE;
  }

  aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->reSmpSampleRate;
  soundMode = FindSoundMode(ctx->channel);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    RKADK_LOGE("Unsupport audio soundMode[%d]", soundMode);
    return RKADK_FAILURE;
  }
  aoAttr.enSoundmode = soundMode;

  aoAttr.u32FrmNum = ctx->periodCount;
  aoAttr.u32PtNumPerFrm = ctx->periodSize;
  aoAttr.u32EXFlag = 0;
  aoAttr.u32ChnCnt = 2;
  ret = RK_MPI_AO_SetPubAttr(ctx->devId, &aoAttr);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_SetPubAttr failde[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_Enable(ctx->devId);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_Enable failed[%x]", ret);
    return RKADK_FAILURE;
  }

  if (USE_AO_MIXER)
    SetAoChannelMode(ctx->devId, ctx->chnIndex);

  if (ctx->channel >= 2)
    ctx->setTrackMode = AUDIO_TRACK_NORMAL;
  else if (ctx->channel == 1)
    ctx->setTrackMode = AUDIO_TRACK_OUT_STEREO;
  else {
    RKADK_LOGE("illegal input channel[%d]", ctx->channel);
    goto __FAILED;
  }

  ret = RK_MPI_AO_SetTrackMode(ctx->devId, (AUDIO_TRACK_MODE_E)ctx->setTrackMode);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_SetTrackMode failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    goto __FAILED;
  }

  ret = RK_MPI_AO_EnableChn(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_EnableChn failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    goto __FAILED;
  }

  // set sample rate of input data
  ret = RK_MPI_AO_EnableReSmp(ctx->devId, ctx->chnIndex,
                                (AUDIO_SAMPLE_RATE_E)ctx->reSmpSampleRate);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_EnableReSmp failed[%x] aoChn[%d]", ret, ctx->chnIndex);
    RK_MPI_AO_DisableChn(ctx->devId, ctx->chnIndex);
    goto __FAILED;
  }

  return RKADK_SUCCESS;

__FAILED:
  RK_MPI_AO_Disable(ctx->devId);
  return RKADK_FAILURE;
}

static RKADK_S32 DestoryDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  RKADK_S32 ret;

  ret = RK_MPI_AO_DisableReSmp(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_DisableReSmp failed[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_DisableChn(ctx->devId, ctx->chnIndex);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_DisableChn failed[%x]", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_Disable(ctx->devId);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_Disable failed[%x]", ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static void AfCtxInit(RKADK_PLAYER_AF_CTX_S *pstAfCtx, RKADK_PLAYER_AUDIO_CFG_S *pstAudioCfg) {
  memset(pstAfCtx, 0, sizeof(RKADK_PLAYER_AF_CTX_S));

  pstAfCtx->s32InputBitWidth    = 16;
  pstAfCtx->s32OutRate          = pstAudioCfg->u32AoSampleRate;
  pstAfCtx->s32OutCh            = pstAudioCfg->u32AoChannle;
  pstAfCtx->s32OutputBitWidth   = 16;
  pstAfCtx->s32ReSampleIndex    = -1;
  pstAfCtx->s32MapIndex         = -1;
  pstAfCtx->s32CacheIndex       = -1;

  if (pstAudioCfg->u32AoChannle > 0) {
    pstAfCtx->s32OutCh = pstAudioCfg->u32AoChannle;
  } else {
    pstAfCtx->s32OutCh = 2;
    RKADK_LOGI("Af use default device channle[2]");
  }

  if (pstAudioCfg->u32AoSampleRate > 0) {
    pstAfCtx->s32OutRate = pstAudioCfg->u32AoSampleRate;
  } else {
    pstAfCtx->s32OutRate = 16000;
    RKADK_LOGI("Af use default sampleRate[16000]");
  }
}

static RKADK_S32 CreateAfOperation(RKADK_PLAYER_AF_CTX_S *ctx, RKADK_PLAYER_AF_OP_E opt) {
  RKADK_S32 ret = 0;

#if defined(RK3506)
  AF_ATTR_S attr;
  RKADK_S32 inputLen = 0, tmpLen = 0;

  memset(&attr, 0, sizeof(AF_ATTR_S));
  attr.u32InBufCount  = 2;
  attr.u32OutBufCount = 2;
  attr.u32Depth = 0;
  if (ctx->s32InFrameSize > gMultiPlayerCtx.s32PtNumPerFrm)
    inputLen = ctx->s32InFrameSize;
  else
    inputLen = gMultiPlayerCtx.s32PtNumPerFrm;

  switch (opt) {
    case RKADK_PLAYER_AF_OP_RESAMPLE:
      attr.enType = AUDIO_FILTER_RESAMPLE;

      attr.stResAttr.u32InRate      = ctx->s32InputRate;
      attr.stResAttr.u32InChn       = ctx->s32InputCh;
      attr.stResAttr.enInBitWidth   = FindBitWidth(ctx->s32InputBitWidth);

      attr.stResAttr.u32OutRate     = gMultiPlayerCtx.s32AoSampleRate;
      attr.stResAttr.u32OutChn      = ctx->s32InputCh;
      attr.stResAttr.enOutBitWidth  = FindBitWidth(ctx->s32OutputBitWidth);

      ctx->s32ReSampleIndex = ctx->s32ChnIndex + ctx->s32ChnCnt * RKADK_PLAYER_AF_OP_RESAMPLE;
      attr.u32MaxInputSize = inputLen;
      ret = RK_MPI_AF_Create(ctx->s32ReSampleIndex, &attr);
      if (ret != RKADK_SUCCESS)
        RKADK_LOGE("create af chn %d err:0x%x\n", 0, ret);
      break;
    case RKADK_PLAYER_AF_OP_MAP:
      attr.enType = AUDIO_FILTER_MAP;

      attr.stMapAttr.u32SampleRate = gMultiPlayerCtx.s32AoSampleRate;
      attr.stMapAttr.enBitWidth    = FindBitWidth(ctx->s32OutputBitWidth);
      attr.stMapAttr.u32InChn      = ctx->s32InputCh;
      attr.stMapAttr.u8MapOutChns  = ctx->s32OutCh;

      ctx->s32MapIndex = ctx->s32ChnIndex + ctx->s32ChnCnt * RKADK_PLAYER_AF_OP_MAP;
      if (gMultiPlayerCtx.s32AoSampleRate / ctx->s32InputRate > 0) {
        tmpLen = inputLen * ( gMultiPlayerCtx.s32AoSampleRate / ctx->s32InputRate + 1);
        if (tmpLen > inputLen)
          inputLen = tmpLen;
      }
      attr.u32MaxInputSize = inputLen;
      ret = RK_MPI_AF_Create(ctx->s32MapIndex, &attr);
      if (ret != RKADK_SUCCESS)
        RKADK_LOGE("create af chn %d err:0x%x\n", 0, ret);
      break;
    case RKADK_PLAYER_AF_OP_CACHE:
      attr.enType = AUDIO_FILTER_CACHE;

      attr.stCacheAttr.stBasicAttr.u32SampleRate = gMultiPlayerCtx.s32AoSampleRate;
      attr.stCacheAttr.stBasicAttr.enBitWidth = FindBitWidth(ctx->s32OutputBitWidth);
      attr.stCacheAttr.stBasicAttr.u32Channel = ctx->s32OutCh;
      attr.stCacheAttr.s32Size = ctx->s32CacheSize;

      ctx->s32CacheIndex = ctx->s32ChnIndex + ctx->s32ChnCnt * RKADK_PLAYER_AF_OP_CACHE;
      if (gMultiPlayerCtx.s32AoSampleRate / ctx->s32InputRate > 0 || ctx->s32OutCh / ctx->s32InputCh > 0) {
        tmpLen = inputLen * (gMultiPlayerCtx.s32AoSampleRate * ctx->s32OutCh / (ctx->s32InputRate * ctx->s32InputCh) + 1);
        if (tmpLen > inputLen)
          inputLen = tmpLen;
      }
      attr.u32MaxInputSize = inputLen;
      ret = RK_MPI_AF_Create(ctx->s32CacheIndex, &attr);
      if (ret != RKADK_SUCCESS)
        RKADK_LOGE("create af chn %d err:0x%x\n", 0, ret);
      break;
    default:
      ret = RKADK_FAILURE;
      break;
  }
#endif

  return ret;
}

static RKADK_S32 CreateAf(RKADK_PLAYER_AF_CTX_S *ctx) {
  RKADK_S32 ret = 0;

  ret = CreateAfOperation (ctx, RKADK_PLAYER_AF_OP_RESAMPLE);
  ret = CreateAfOperation (ctx, RKADK_PLAYER_AF_OP_MAP);
  ret = CreateAfOperation (ctx, RKADK_PLAYER_AF_OP_CACHE);

  return ret;
}

static RKADK_S32 DestroyAf(RKADK_PLAYER_AF_CTX_S *ctx) {
  RKADK_S32 ret = 0;
#if defined(RK3506)
  if (ctx != RKADK_NULL) {
    if (ctx->s32ReSampleIndex >= 0)
      ret = RK_MPI_AF_Destroy(ctx->s32ReSampleIndex);
    if (ctx->s32MapIndex >= 0)
      ret = RK_MPI_AF_Destroy(ctx->s32MapIndex);
    if (ctx->s32CacheIndex >= 0)
      ret = RK_MPI_AF_Destroy(ctx->s32CacheIndex);
  }
#endif
  return ret;
}

static void AmixCtxInit(RKADK_PLAYER_AMIX_CTX_S *pstAmixCtx, RKADK_PLAYER_AUDIO_CFG_S *pstAudioCfg) {
  memset(pstAmixCtx, 0, sizeof(RKADK_PLAYER_AMIX_CTX_S));

  pstAmixCtx->s32PipeNum          = pstAudioCfg->s32AdecChnCnt;
  pstAmixCtx->s32GrpId            = 0;
  pstAmixCtx->s32PeriodSize       = 4096;
  pstAmixCtx->s32InSampleRate     = pstAudioCfg->u32AoSampleRate;
  pstAmixCtx->u32InChannel        = pstAudioCfg->u32AoChannle;
  pstAmixCtx->s32ChnNum           = 1;

  if (pstAudioCfg->u32AoChannle > 0) {
    pstAmixCtx->u32InChannel = pstAudioCfg->u32AoChannle;
  } else {
    pstAmixCtx->u32InChannel = 2;
    RKADK_LOGI("Af use default device channle[2]");
  }

  if (pstAudioCfg->u32AoSampleRate > 0) {
    pstAmixCtx->s32InSampleRate = pstAudioCfg->u32AoSampleRate;
  } else {
    pstAmixCtx->s32InSampleRate = 16000;
    RKADK_LOGI("Af use default sampleRate[16000]");
  }
}

static RKADK_S32 CreateAmix(RKADK_PLAYER_AMIX_CTX_S *ctx) {
  RKADK_S32    ret = 0;

#if defined(RK3506)
  AMIX_GRP  AmixGrp = 0;
  AMIX_PIPE AmixPipe = 0;
  AMIX_CHN  AmixChn = 0;
  AMIX_GRP_ATTR_S   stGrpAttr;
  AMIX_PIPE_ATTR_S  stPipeAttr[AMIX_MAX_PIPE_NUM];
  AMIX_CHN_ATTR_S   stChnAttr[AMIX_MAX_CHN_NUM];
  AUDIO_FRAME_S stFrame;
  memset(&stGrpAttr,  0, sizeof(stGrpAttr));
  memset(&stPipeAttr, 0, sizeof(stPipeAttr));
  memset(&stChnAttr,  0, sizeof(stChnAttr));
  memset(&stFrame, 0, sizeof(stFrame));

  stGrpAttr.enMode = AMIX_MODE_MIX;
  stGrpAttr.u32PeriodSize = ctx->s32PeriodSize;
  // stGrpAttr.u32StartDelay = ctx->s32PeriodSize / ctx->s32InSampleRate;
  AmixGrp = ctx->s32GrpId;
  ret = RK_MPI_AMIX_CreateGrp(AmixGrp, &stGrpAttr);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("amix [%d, %d] RK_MPI_AMIX_CreateGrp failed: %#x!",
            AmixGrp, 0, ret);
    return ret;
  }

  RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_CreateGrp already.", AmixGrp, 0);
  for (AmixPipe = 0; AmixPipe < ctx->s32PipeNum; AmixPipe++) {
    stPipeAttr[AmixPipe].stAttr.s32SampleRate = ctx->s32InSampleRate;
    stPipeAttr[AmixPipe].stAttr.u32Channels = ctx->u32InChannel;
    stPipeAttr[AmixPipe].stAttr.enBitWidth = AUDIO_BIT_WIDTH_16;
    stPipeAttr[AmixPipe].u32InBufCnt = 4;
    ret = RK_MPI_AMIX_SetPipeAttr(AmixGrp, AmixPipe, &stPipeAttr[AmixPipe]);
    if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("amix [%d, %d] RK_MPI_AMIX_SetPipeAttr failed: %#x!",
                AmixGrp, AmixPipe, ret);
        return ret;
    }
    RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_SetPipeAttr already.", AmixGrp, AmixPipe);

    ret = RK_MPI_AMIX_EnablePipe(AmixGrp, AmixPipe);
    if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_AMIX_EnablePipe failed: %#x!", AmixPipe, ret);
        return ret;
    }
    RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_EnablePipe already.", AmixGrp, AmixPipe);
  }

  for (AmixChn = 0; AmixChn < ctx->s32ChnNum; AmixChn++) {
    stChnAttr[AmixChn].stAttr.s32SampleRate = ctx->s32InSampleRate;
    stChnAttr[AmixChn].stAttr.u32Channels = ctx->u32InChannel;
    stChnAttr[AmixChn].stAttr.enBitWidth = AUDIO_BIT_WIDTH_16;
    stChnAttr[AmixChn].u32OutBufCnt = 4;
    stChnAttr[AmixChn].u32Depth = 1;
    ctx->s32ChnId = AmixChn;
    ret = RK_MPI_AMIX_SetChnAttr(AmixGrp, AmixChn, &stChnAttr[AmixChn]);
    if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("amix [%d, %d]RK_MPI_AMIX_SetChnAttr failed: %#x!",
                AmixGrp, AmixChn, ret);
        return ret;
    }
    RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_SetChnAttr already.", AmixGrp, AmixChn);

    ret = RK_MPI_AMIX_EnableChn(AmixGrp, AmixChn);
    if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_AMIX_EnableChn failed: %#x!", AmixChn, ret);
        return ret;
    }
    RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_EnableChn already.", AmixGrp, AmixChn);
  }

  ret = RK_MPI_AMIX_StartGrp(AmixGrp);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_AMIX_StartGrp failed: %#x!", AmixGrp, ret);
    return ret;
  }
  RKADK_LOGD("amix [%d, %d] RK_MPI_AMIX_StartGrp already.", AmixGrp, 0);
#endif

  return ret;
}

static RKADK_S32 DestroyAmix(RKADK_PLAYER_AMIX_CTX_S *ctx) {
  RKADK_S32    ret = 0;

#if defined(RK3506)
  if (ctx != RKADK_NULL) {
    ret = RK_MPI_AMIX_StopGrp(ctx->s32GrpId);
    RKADK_LOGD("RK_MPI_AMIX_StopGrp ret: %#x!", 0, ret);
    if (ret != RKADK_SUCCESS)
      RKADK_LOGE("RK_MPI_AMIX_StopGrp failed: %#x!", 0, ret);

    ret = RK_MPI_AMIX_DestroyGrp(ctx->s32GrpId);
    RKADK_LOGD("RK_MPI_AMIX_DestroyGrp ret: %#x!", 0, ret);
    if (ret != RKADK_SUCCESS)
      RKADK_LOGE("RK_MPI_AMIX_DestroyGrp failed: %#x!", 0, ret);
  }
#endif

  return ret;
}

static RKADK_S32 AfBindAmix(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_S32 ret = 0;

#if defined(RK3506)
  MPP_CHN_S stAfReSampleChn, stAfMapChn, stAfCacheChn, stAmixPipeChn;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);

  stAfReSampleChn.enModId = RK_ID_AF;
  stAfReSampleChn.s32DevId = 0;
  stAfReSampleChn.s32ChnId = pstPlayer->stAfCtx.s32ReSampleIndex;

  stAfMapChn.enModId = RK_ID_AF;
  stAfMapChn.s32DevId = 0;
  stAfMapChn.s32ChnId = pstPlayer->stAfCtx.s32MapIndex;

  stAfCacheChn.enModId = RK_ID_AF;
  stAfCacheChn.s32DevId = 0;
  stAfCacheChn.s32ChnId = pstPlayer->stAfCtx.s32CacheIndex;

  stAmixPipeChn.enModId = RK_ID_AMIX;
  stAmixPipeChn.s32DevId = 0;
  stAmixPipeChn.s32ChnId = pstPlayer->stAmixCtx.s32PipeId;

  ret = RK_MPI_SYS_Bind(&stAfReSampleChn, &stAfMapChn);
  RKADK_LOGI("Bind stAfReSampleChn[%d, %d] to stAfMapChn[%d, %d] ret[%x]", stAfReSampleChn.s32DevId,
                stAfReSampleChn.s32ChnId, stAfMapChn.s32DevId, stAfMapChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("Bind stAfReSampleChn[%d, %d] to stAfReSampleChn[%d] failed[%x]", stAfReSampleChn.s32DevId,
                stAfReSampleChn.s32ChnId, stAfMapChn.s32ChnId, ret);
  }

  ret = RK_MPI_SYS_Bind(&stAfMapChn, &stAfCacheChn);
  RKADK_LOGI("Bind stAfMapChn[%d, %d] to stAfCacheChn[%d, %d] ret[%x]", stAfMapChn.s32DevId,
                stAfMapChn.s32ChnId, stAfCacheChn.s32DevId, stAfCacheChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("Bind stAfMapChn[%d, %d] to stAfCacheChn[%d] failed[%x]", stAfMapChn.s32DevId,
                stAfMapChn.s32ChnId, stAfCacheChn.s32ChnId, ret);
  }

  ret = RK_MPI_SYS_Bind(&stAfCacheChn, &stAmixPipeChn);
  RKADK_LOGI("Bind stAfCacheChn[%d, %d] to stAmixPipeChn[%d, %d] ret[%x]", stAfCacheChn.s32DevId,
                stAfCacheChn.s32ChnId, stAmixPipeChn.s32DevId, stAmixPipeChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("Bind stAfCacheChn[%d, %d] to stAmixPipeChn[%d] failed[%x]", stAfCacheChn.s32DevId,
                stAfCacheChn.s32ChnId, stAmixPipeChn.s32ChnId, ret);
  }
#endif

  return ret;
}

static RKADK_S32 AfUnBindAmix(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_S32 ret = 0;

#if defined(RK3506)
  MPP_CHN_S stAfReSampleChn, stAfMapChn, stAfCacheChn, stAmixPipeChn;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);

  stAfReSampleChn.enModId = RK_ID_AF;
  stAfReSampleChn.s32DevId = 0;
  stAfReSampleChn.s32ChnId = pstPlayer->stAfCtx.s32ReSampleIndex;

  stAfMapChn.enModId = RK_ID_AF;
  stAfMapChn.s32DevId = 0;
  stAfMapChn.s32ChnId = pstPlayer->stAfCtx.s32MapIndex;

  stAfCacheChn.enModId = RK_ID_AF;
  stAfCacheChn.s32DevId = 0;
  stAfCacheChn.s32ChnId = pstPlayer->stAfCtx.s32CacheIndex;

  stAmixPipeChn.enModId = RK_ID_AMIX;
  stAmixPipeChn.s32DevId = 0;
  stAmixPipeChn.s32ChnId = pstPlayer->stAmixCtx.s32PipeId;

  ret = RK_MPI_SYS_UnBind(&stAfCacheChn, &stAmixPipeChn);
  RKADK_LOGI("UnBind stAfCacheChn[%d, %d] to stAmixPipeChn[%d, %d] ret[%x]", stAfCacheChn.s32DevId,
                stAfCacheChn.s32ChnId, stAmixPipeChn.s32DevId, stAmixPipeChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("UnBind stAfCacheChn[%d, %d] to stAmixPipeChn[%d] failed[%x]", stAfCacheChn.s32DevId,
                stAfCacheChn.s32ChnId, stAmixPipeChn.s32ChnId, ret);
  }
  ret = RK_MPI_SYS_UnBind(&stAfMapChn, &stAfCacheChn);
  RKADK_LOGI("UnBind stAfMapChn[%d, %d] to stAfCacheChn[%d, %d] ret[%x]", stAfMapChn.s32DevId,
                stAfMapChn.s32ChnId, stAfCacheChn.s32DevId, stAfCacheChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("UnBind stAfMapChn[%d, %d] to stAfCacheChn[%d] failed[%x]", stAfMapChn.s32DevId,
                stAfMapChn.s32ChnId, stAfCacheChn.s32ChnId, ret);
  }
  ret = RK_MPI_SYS_UnBind(&stAfReSampleChn, &stAfMapChn);
  RKADK_LOGI("UnBind stAfReSampleChn[%d, %d] to stAfMapChn[%d, %d] ret[%x]", stAfReSampleChn.s32DevId,
                stAfReSampleChn.s32ChnId, stAfMapChn.s32DevId, stAfMapChn.s32ChnId, ret);
  if (ret) {
    RKADK_LOGE("UnBind stAfReSampleChn[%d, %d] to stAfReSampleChn[%d] failed[%x]", stAfReSampleChn.s32DevId,
                stAfReSampleChn.s32ChnId, stAfMapChn.s32ChnId, ret);
  }
#endif

  return ret;
}

static RKADK_S32 AmixBindAo(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_S32 ret = 0;

#if defined(RK3506)
  MPP_CHN_S stAmixGrpChn, stAoChn;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);

  stAmixGrpChn.enModId = RK_ID_AMIX;
  stAmixGrpChn.s32DevId = 0;
  stAmixGrpChn.s32ChnId = 0;

  stAoChn.enModId = RK_ID_AO;
  stAoChn.s32DevId = 0;
  stAoChn.s32ChnId = pstPlayer->stAoCtx.chnIndex;

  ret = RK_MPI_SYS_Bind(&stAmixGrpChn, &stAoChn);
  if (ret) {
    RKADK_LOGE("Bind amix grp[%d, %d] to stAoChn[%d] failed[%x]", stAmixGrpChn.s32DevId,
                stAmixGrpChn.s32ChnId, stAoChn.s32ChnId, ret);
  }
#endif

  return RKADK_SUCCESS;
}

static RKADK_S32 AmixUnBindAo(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_S32 ret = 0;

#if defined(RK3506)
  MPP_CHN_S stAmixGrpChn, stAoChn;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);

  stAmixGrpChn.enModId = RK_ID_AMIX;
  stAmixGrpChn.s32DevId = 0;
  stAmixGrpChn.s32ChnId = 0;

  stAoChn.enModId = RK_ID_AO;
  stAoChn.s32DevId = 0;
  stAoChn.s32ChnId = pstPlayer->stAoCtx.chnIndex;

  ret = RK_MPI_SYS_UnBind(&stAmixGrpChn, &stAoChn);
  if (ret) {
    RKADK_LOGE("UnBind amix grp[%d, %d] to stAoChn[%d] failed[%x]", stAmixGrpChn.s32DevId,
                stAmixGrpChn.s32ChnId, stAoChn.s32ChnId, ret);
  }
#endif

  return RKADK_SUCCESS;
}

//Query whether there is a vdec stream
static RKADK_S32 VdecPollEvent(RKADK_S32 timeoutMsec, RKADK_S32 fd) {
  RKADK_S32 num_fds = 1;
  struct pollfd pollFds[num_fds];
  RKADK_S32 ret = 0;

  RK_ASSERT(fd > 0);
  memset(pollFds, 0, sizeof(pollFds));
  pollFds[0].fd = fd;
  pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

  ret = poll(pollFds, num_fds, timeoutMsec);
  if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
    RKADK_LOGE("fd[%d] polled error", fd);
    return -1;
  }

  return ret;
}

static bool SnapshotProc(void *pHandle) {
  int ret;
  RKADK_PLAYER_SNAPSHOT_S stData;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_CHN_ATTR_S stAttr;
  VIDEO_FRAME_INFO_S stFrame;
  VENC_STREAM_S stStream;
  VENC_PACK_S stPack;
  PLAYER_SNAPSHOT_FRAME_S stSnapshotFrame;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pHandle;

  memset(&stSnapshotFrame, 0, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  if (!pstPlayer) {
    RKADK_LOGE("pstPlayer is null");
    goto __EXIT;
  }

  RKADK_SIGNAL_Wait(pstPlayer->stSnapshotParam.pSignal, -1);

  if (!pstPlayer->stSnapshotParam.bSnapshot)
    return false;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP)
    goto __EXIT;

  if (!pstPlayer->stSnapshotParam.pfnDataCallback) {
    RKADK_LOGE("Unregistered snapshot data callback");
    goto __EXIT;
  }

  if (!pstPlayer->stSnapshotParam.stFrame.pMbBlk) {
    RKADK_LOGE("Snapshot pMbBlk is null");
    goto __EXIT;
  }

  pthread_mutex_lock(&pstPlayer->stSnapshotParam.mutex);
  memcpy(&stSnapshotFrame, &pstPlayer->stSnapshotParam.stFrame, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  RK_MPI_MB_AddUserCnt(stSnapshotFrame.pMbBlk);
  pthread_mutex_unlock(&pstPlayer->stSnapshotParam.mutex);

  if (!pstPlayer->stSnapshotParam.bVencChnExist) {
    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

    stAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
    stAttr.stVencAttr.enPixelFormat = stSnapshotFrame.enPixelFormat;
    stAttr.stVencAttr.u32MaxPicWidth = pstPlayer->stSnapshotParam.u32MaxWidth;
    stAttr.stVencAttr.u32MaxPicHeight = pstPlayer->stSnapshotParam.u32MaxHeight;
    stAttr.stVencAttr.u32PicWidth = stSnapshotFrame.u32Width;
    stAttr.stVencAttr.u32PicHeight = stSnapshotFrame.u32Height;
    stAttr.stVencAttr.u32VirWidth = stSnapshotFrame.u32VirWidth;
    stAttr.stVencAttr.u32VirHeight = stSnapshotFrame.u32VirHeight;
    stAttr.stVencAttr.u32StreamBufCnt = 1;
    stAttr.stVencAttr.u32BufSize =
        stAttr.stVencAttr.u32MaxPicWidth * stAttr.stVencAttr.u32MaxPicHeight;

    ret = RK_MPI_VENC_CreateChn(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGD("Create snapshot venc[%d] failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      goto __EXIT;
    }

    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    stRecvParam.s32RecvPicNum = -1;
    ret = RK_MPI_VENC_StartRecvFrame(pstPlayer->stSnapshotParam.u32VencChn, &stRecvParam);
    if (ret != RK_SUCCESS) {
      RKADK_LOGD("Snapshot ven[%d] start recv frame failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      RK_MPI_VENC_DestroyChn(pstPlayer->stSnapshotParam.u32VencChn);
      goto __EXIT;
    }

    pstPlayer->stSnapshotParam.bVencChnExist = true;
  } else {
    ret = RK_MPI_VENC_GetChnAttr(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("Get snapshot venc[%d] attr failed[%x]", pstPlayer->stSnapshotParam.u32VencChn, ret);
      goto __EXIT;
    }

    if (stAttr.stVencAttr.enPixelFormat != stSnapshotFrame.enPixelFormat)
      RKADK_LOGW("venc chn pix[%x] != snapshot pix[%x]",stAttr.stVencAttr.enPixelFormat,
                  stSnapshotFrame.enPixelFormat);

    if (stAttr.stVencAttr.u32PicWidth != stSnapshotFrame.u32Width
        || stAttr.stVencAttr.u32PicHeight != stSnapshotFrame.u32Height) {
      RKADK_LOGD("Reset vencAttr[%d, %d], cell[%d, %d]", stAttr.stVencAttr.u32PicWidth,
                  stAttr.stVencAttr.u32PicHeight, stSnapshotFrame.u32Width, stSnapshotFrame.u32Height);
      stAttr.stVencAttr.u32PicWidth = stSnapshotFrame.u32Width;
      stAttr.stVencAttr.u32PicHeight = stSnapshotFrame.u32Height;
      stAttr.stVencAttr.u32VirWidth = stSnapshotFrame.u32VirWidth;
      stAttr.stVencAttr.u32VirHeight = stSnapshotFrame.u32VirHeight;

      ret = RK_MPI_VENC_SetChnAttr(pstPlayer->stSnapshotParam.u32VencChn, &stAttr);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("Set snapshot venc[%d] attr failed[%x]",
                    pstPlayer->stSnapshotParam.u32VencChn, ret);
        goto __EXIT;
      }
    }
  }

  memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  stFrame.stVFrame.pMbBlk = stSnapshotFrame.pMbBlk;
  stFrame.stVFrame.u32Width = stSnapshotFrame.u32Width;
  stFrame.stVFrame.u32Height = stSnapshotFrame.u32Height;
  stFrame.stVFrame.u32VirWidth = stSnapshotFrame.u32VirWidth;
  stFrame.stVFrame.u32VirHeight = stSnapshotFrame.u32VirHeight;
  stFrame.stVFrame.enPixelFormat = stSnapshotFrame.enPixelFormat;

  ret = RK_MPI_VENC_SendFrame(pstPlayer->stSnapshotParam.u32VencChn, &stFrame, -1);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Snapshot venc[%d] send frame failed[%x]",
                pstPlayer->stSnapshotParam.u32VencChn, ret);
    goto __EXIT;
  }

  ret = RK_MPI_MB_ReleaseMB(stSnapshotFrame.pMbBlk);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);
    goto __EXIT;
  }
  stSnapshotFrame.pMbBlk = NULL;

  memset(&stStream, 0, sizeof(VENC_STREAM_S));
  memset(&stPack, 0, sizeof(VENC_PACK_S));
  stStream.pstPack = &stPack;
  ret = RK_MPI_VENC_GetStream(pstPlayer->stSnapshotParam.u32VencChn, &stStream, -1);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Snapshot venc[%d] get stream failed[%x]",
                pstPlayer->stSnapshotParam.u32VencChn, ret);
    goto __EXIT;
  }

  memset(&stData, 0, sizeof(RKADK_PLAYER_SNAPSHOT_S));
  stData.u32Width = stSnapshotFrame.u32Width;
  stData.u32Height = stSnapshotFrame.u32Height;
  stData.u32DataLen = stStream.pstPack->u32Len;
  stData.pu8DataBuf = (RKADK_U8 *)RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
  pstPlayer->stSnapshotParam.pfnDataCallback(&stData);
  RKADK_LOGD("Snapshot done, w*h[%d, %d], pu8DataBuf[%p, %d]",
              stData.u32Width, stData.u32Height, stData.pu8DataBuf, stData.u32DataLen);

  RK_MPI_VENC_ReleaseStream(pstPlayer->stSnapshotParam.u32VencChn, &stStream);
  pstPlayer->stSnapshotParam.bSnapshot = false;
  return true;

__EXIT:
  if (stSnapshotFrame.pMbBlk) {
    ret = RK_MPI_MB_ReleaseMB(stSnapshotFrame.pMbBlk);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_MB_ReleaseMB failed[%x]", ret);
      return -1;
    }
  }

  pstPlayer->stSnapshotParam.bSnapshot = false;
  return false;
}

static int SnapshotEnable(RKADK_PLAYER_HANDLE_S *pstPlayer, RKADK_PLAYER_SNAPSHOT_CFG_S stSnapshotCfg) {
  char name[20];

   pstPlayer->stSnapshotParam.u32VencChn = stSnapshotCfg.u32VencChn;
  if (stSnapshotCfg.u32MaxWidth > 0)
    pstPlayer->stSnapshotParam.u32MaxWidth = stSnapshotCfg.u32MaxWidth;
  else
    pstPlayer->stSnapshotParam.u32MaxWidth = PLAYER_SNAPSHOT_MAX_WIDTH;

  if (stSnapshotCfg.u32MaxHeight > 0)
    pstPlayer->stSnapshotParam.u32MaxHeight = stSnapshotCfg.u32MaxHeight;
  else
    pstPlayer->stSnapshotParam.u32MaxHeight = PLAYER_SNAPSHOT_MAX_HEIGHT;

  // Create signal
  pstPlayer->stSnapshotParam.pSignal = RKADK_SIGNAL_Create(0, 1);
  if (!pstPlayer->stSnapshotParam.pSignal) {
    RKADK_LOGE("Create snapshot signal failed");
    return -1;
  }

  // Create thread
  snprintf(name, sizeof(name), "%s", "snapshot");
  pstPlayer->stSnapshotParam.pThread = RKADK_THREAD_Create(SnapshotProc, pstPlayer, name);
  if (!pstPlayer->stSnapshotParam.pThread) {
    RKADK_LOGE("Create snapshot thread failed");
    RKADK_SIGNAL_Destroy(pstPlayer->stSnapshotParam.pSignal);
    return -1;
  }

  pthread_mutex_init(&pstPlayer->stSnapshotParam.mutex, NULL);
  pstPlayer->stSnapshotParam.pfnDataCallback = stSnapshotCfg.pfnDataCallback;
  return 0;
}

static int SnapshotDisable(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  int ret;

  // Exit thread
  if (pstPlayer->stSnapshotParam.pThread)
    RKADK_THREAD_SetExit(pstPlayer->stSnapshotParam.pThread);

  if (pstPlayer->stSnapshotParam.pSignal)
    RKADK_SIGNAL_Give(pstPlayer->stSnapshotParam.pSignal);

  // Destroy thread
  if (pstPlayer->stSnapshotParam.pThread) {
    RKADK_THREAD_Destory(pstPlayer->stSnapshotParam.pThread);
    pstPlayer->stSnapshotParam.pThread = NULL;
  }

  // Destroy signal
  if (pstPlayer->stSnapshotParam.pSignal)
    RKADK_SIGNAL_Destroy(pstPlayer->stSnapshotParam.pSignal);

  // Destory mutex
  pthread_mutex_destroy(&pstPlayer->stSnapshotParam.mutex);

  if (pstPlayer->stSnapshotParam.bVencChnExist) {
    ret = RK_MPI_VENC_DestroyChn(pstPlayer->stSnapshotParam.u32VencChn);
    if (ret != RK_SUCCESS)
      RKADK_LOGE("Destory snapshot venc chn failed[%x]", ret);
  }

  return 0;
}

static void SendBlackBackground(RKADK_PLAYER_HANDLE_S *pstPlayer, VIDEO_FRAME_INFO_S *tFrame) {
  RKADK_S32 ret = 0;
  RKADK_BOOL bEnableSendVo = RKADK_FALSE;
  RK_U8 *lastFrame = RK_NULL;

  if (pstPlayer->bEnableBlackBackground) {
    lastFrame = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(tFrame->stVFrame.pMbBlk);
    if (!lastFrame) {
      RKADK_LOGE("Last frame is NULL, it is not support send black background!");
      return;
    }

    if (tFrame->stVFrame.enPixelFormat != RK_FMT_YUV420SP) {
      RKADK_LOGE("Pixel format %d does not support send black background!", tFrame->stVFrame.enPixelFormat);
      return;
    }

    ret = RK_MPI_VO_PauseChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
    if (ret != 0)
      RKADK_LOGE("vo pause chn failed[%x]", ret);

    if (tFrame->stVFrame.enPixelFormat == RK_FMT_YUV420SP) {
      memset(lastFrame, 0, tFrame->stVFrame.u32VirWidth * tFrame->stVFrame.u32VirHeight);
      lastFrame += tFrame->stVFrame.u32VirWidth * tFrame->stVFrame.u32VirHeight;
      memset(lastFrame, 128, tFrame->stVFrame.u32VirWidth * tFrame->stVFrame.u32VirHeight / 2);
      bEnableSendVo = RKADK_TRUE;
    }

    ret = RK_MPI_VO_ResumeChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
    if (ret != 0)
      RKADK_LOGE("vo resume chn failed[%x]", ret);

    if (bEnableSendVo) {
      ret = RK_MPI_SYS_MmzFlushCache(tFrame->stVFrame.pMbBlk, false);
      if (ret != 0)
        RKADK_LOGE("sys mmz flush cache failed[%x]", ret);

      ret = RK_MPI_VO_SendFrame(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, tFrame, MAX_TIME_OUT_MS);
      if (ret != 0)
        RKADK_LOGE("black backgound send fail[%x]", ret);
    }
  }

  return;
}

static RKADK_S32 RKADK_PLAYER_SendVoFrame(RKADK_PLAYER_HANDLE_S *pstPlayer, VIDEO_FRAME_INFO_S *sFrame, RKADK_S32 s32MilliSec) {
  RKADK_S32 ret = 0;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(sFrame, RKADK_FAILURE);
  RKADK_CHECK_POINTER(sFrame->stVFrame.pMbBlk, RKADK_FAILURE);

  ret = RK_MPI_SYS_MmzFlushCache(sFrame->stVFrame.pMbBlk, false);
  if (ret != RK_SUCCESS)
    RKADK_LOGE("sys mmz flush cache failed[%x]", ret);

  // save snapshot frame info
  if (pstPlayer->stSnapshotParam.pfnDataCallback) {
    pthread_mutex_lock(&pstPlayer->stSnapshotParam.mutex);
    if (pstPlayer->stSnapshotParam.stFrame.u32Width && pstPlayer->stSnapshotParam.stFrame.u32Height
      && pstPlayer->stSnapshotParam.stFrame.pMbBlk) {
      RK_MPI_MB_ReleaseMB(pstPlayer->stSnapshotParam.stFrame.pMbBlk);
      memset(&pstPlayer->stSnapshotParam.stFrame, 0, sizeof(PLAYER_SNAPSHOT_FRAME_S));
    }
    pstPlayer->stSnapshotParam.stFrame.u32Width = sFrame->stVFrame.u32Width;
    pstPlayer->stSnapshotParam.stFrame.u32Height = sFrame->stVFrame.u32Height;
    pstPlayer->stSnapshotParam.stFrame.u32VirWidth = sFrame->stVFrame.u32VirWidth;
    pstPlayer->stSnapshotParam.stFrame.u32VirHeight = sFrame->stVFrame.u32VirHeight;
    pstPlayer->stSnapshotParam.stFrame.pMbBlk = sFrame->stVFrame.pMbBlk;
    pstPlayer->stSnapshotParam.stFrame.enPixelFormat = sFrame->stVFrame.enPixelFormat;
    RK_MPI_MB_AddUserCnt(pstPlayer->stSnapshotParam.stFrame.pMbBlk);
    pthread_mutex_unlock(&pstPlayer->stSnapshotParam.mutex);
  }

  ret = RK_MPI_VO_SendFrame(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, sFrame, s32MilliSec);
  if (ret != RK_SUCCESS)
    RKADK_LOGE("send vo failed[%x]", ret);

  return ret;
}

static void AoVolumeControl(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  AUDIO_FADE_S aFade;
  aFade.bFade = RK_FALSE;
  RK_BOOL mute;

  aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)pstPlayer->stAoCtx.setFadeRate;
  aFade.enFadeInRate = (AUDIO_FADE_RATE_E)pstPlayer->stAoCtx.setFadeRate;
  mute = (pstPlayer->stAoCtx.setMute == 0) ? RK_FALSE : RK_TRUE;
  RKADK_LOGI("AO volume = %d, mute = %d", pstPlayer->stAoCtx.u32SpeakerVolume, mute);
  RK_MPI_AO_SetMute(pstPlayer->stAoCtx.devId, mute, &aFade);
  RK_MPI_AO_SetVolume(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.u32SpeakerVolume);
}

static void SendDataDebugLevel1(RKADK_PLAYER_HANDLE_S *pstPlayer, VIDEO_FRAME_INFO_S sFrame) {
  RK_U64 debugCount = 0;
  RK_U32 ret = 0, costtime = 0;
  struct timespec t_begin, t_end;

  ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, -1);
  if (ret == 0) {
    debugCount++;
    clock_gettime(CLOCK_MONOTONIC, &t_begin);
  }

  while (1) {
    ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, -1);
    if (ret == 0) {
      debugCount++;
      clock_gettime(CLOCK_MONOTONIC, &t_end);
      costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
      if (debugCount % 100 == 0)
        RKADK_LOGI("vdec get frame rate : %lld.", debugCount * 1000000 / costtime);

      pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;
      if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
        costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
        RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
        RKADK_LOGI("chn %d reach eos frame.", pstPlayer->stVdecCtx.chnIndex);
        break;
      }
      RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
    }
  }
  return;
}

static void SendVideoData(RKADK_VOID *ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  VIDEO_FRAME_INFO_S sFrame;
  VIDEO_FRAME_INFO_S tFrame;

  struct timespec t_begin, t_end;
  struct timespec t_begin_debug, t_end_debug;
  RKADK_S32 ret = 0;
  RKADK_S32 flagGetTframe = 0;
  RKADK_S32 voSendTime = 0, frameTime = 0, costtime = 0;
  VDEC_CHN_STATUS_S stStatus;
  bool bWaterline = true;
  RKADK_S32 enableSendDataDebug = 0;
  RK_U64 debugCount = 0;

  if (pstPlayer->stDemuxerParam.videoAvgFrameRate <= 0) {
    RKADK_LOGE("Invalid video framerate[%d]", pstPlayer->stDemuxerParam.videoAvgFrameRate);
    return;
  }

  frameTime = 1000000 / pstPlayer->stDemuxerParam.videoAvgFrameRate;
  memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  memset(&tFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

  if (getenv("send_data_debug_level")) {
    enableSendDataDebug = atoi(getenv("send_data_debug_level"));
    RKADK_LOGI("enableSendDataDebug value : %d, env %s.", enableSendDataDebug, getenv("send_data_debug_level"));
  }

  clock_gettime(CLOCK_MONOTONIC, &t_begin_debug);
  while (!pstPlayer->bStopSendStream) {
    if (enableSendDataDebug == 1) {
      SendDataDebugLevel1(pstPlayer, sFrame);
      break;
    }

    ret = RK_MPI_VDEC_QueryStatus(pstPlayer->stVdecCtx.chnIndex, &stStatus);
    if (ret == RK_SUCCESS) {
      if ((stStatus.u32LeftStreamFrames + stStatus.u32LeftPics) < pstPlayer->u32VdecWaterline)
        bWaterline = false;
      else
        bWaterline = true;
    } else {
      RKADK_LOGE("Query vdec status failed[%x]", ret);
    }

    if ((pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE && bWaterline) || pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE) {
      if (pstPlayer->stVdecCtx.chnFd > 0) {
        ret = VdecPollEvent(MAX_TIME_OUT_MS, pstPlayer->stVdecCtx.chnFd);
        if (ret < 0)
          continue;
      }

      ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, MAX_TIME_OUT_MS);
      if (ret == 0) {
        pstPlayer->frameCount++;
        if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE)
          pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;

        if (pstPlayer->bEnableBlackBackground) {
          if (!flagGetTframe) {
            tFrame.stVFrame.u32Width = sFrame.stVFrame.u32Width;
            tFrame.stVFrame.u32Height = sFrame.stVFrame.u32Height;
            tFrame.stVFrame.u32VirWidth = sFrame.stVFrame.u32VirWidth;
            tFrame.stVFrame.u32VirHeight = sFrame.stVFrame.u32VirHeight;
            tFrame.stVFrame.enPixelFormat = sFrame.stVFrame.enPixelFormat;
            tFrame.stVFrame.enCompressMode = sFrame.stVFrame.enCompressMode;
            flagGetTframe = 1;
          }
        }

        if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
          SendBlackBackground(pstPlayer, &tFrame);
          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);

          RKADK_LOGI("chn %d reach eos frame", pstPlayer->stVdecCtx.chnIndex);
          break;
        }

        if (!pstPlayer->bAudioExist) {
          if (sFrame.stVFrame.u64PTS == 0)
            pstPlayer->positionTimeStamp += frameTime;
          else
            pstPlayer->positionTimeStamp = sFrame.stVFrame.u64PTS;
        }

        if (RKADK_MEDIA_GetVoLayerBindMode(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev) == VO_LAYER_MODE_GRAPHIC) {
          clock_gettime(CLOCK_MONOTONIC, &t_end);
          if (pstPlayer->videoTimeStamp >= 0) {
            costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
            if ((RKADK_S64)sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp > (RKADK_S64)costtime) {
              voSendTime = sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp - costtime;

              if (!pstPlayer->bIsNetRTStream) {
#ifndef OS_RTT
                usleep(voSendTime);
#else
                rkos_msleep(voSendTime / 1000);
#endif
              }
            }
            voSendTime = frameTime;
          } else {
            voSendTime = frameTime;
          }
        }
        pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;

        if (pstPlayer->bEnableBlackBackground)
          tFrame.stVFrame.pMbBlk = sFrame.stVFrame.pMbBlk;

        if (enableSendDataDebug == 3) {
          debugCount++;
          clock_gettime(CLOCK_MONOTONIC, &t_end_debug);
          costtime = (t_end_debug.tv_sec - t_begin_debug.tv_sec) * 1000000 + (t_end_debug.tv_nsec - t_begin_debug.tv_nsec) / 1000;
          if (debugCount % 100 == 0)
            RKADK_LOGI("vdec get frame rate : %lld.", debugCount * 1000000 / costtime);
        }
        ret = RKADK_PLAYER_SendVoFrame(pstPlayer, &sFrame, -1);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("send vo failed[%x]", ret);

        if (RKADK_MEDIA_GetVoLayerBindMode(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev) == VO_LAYER_MODE_GRAPHIC) {
          clock_gettime(CLOCK_MONOTONIC, &t_begin);
          if (!pstPlayer->bIsNetRTStream) {
#ifndef OS_RTT
            usleep(voSendTime);
#else
            rkos_msleep(voSendTime / 1000);
#endif
          }
        }
        RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
      } else {
        //RKADK_LOGW("RK_MPI_VDEC_GetFrame timeout[%x]", ret);
      }
    } else {
#ifndef OS_RTT
      usleep(1000);
#else
      rkos_msleep(1);
#endif
    }
  }

  if (pstPlayer->stSnapshotParam.stFrame.u32Width && pstPlayer->stSnapshotParam.stFrame.u32Height
    && pstPlayer->stSnapshotParam.stFrame.pMbBlk) {
    RK_MPI_MB_ReleaseMB(pstPlayer->stSnapshotParam.stFrame.pMbBlk);
    memset(&pstPlayer->stSnapshotParam.stFrame, 0, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  }

  return;
}

static void SendAudioData(RKADK_VOID *ptr) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  RKADK_S32 s32MilliSec = -1, result = 0, size = 0, s32MsSleepTime = 0;
  AUDIO_FRAME_INFO_S stFrmInfo;

  memset(&stFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));

  #ifdef WRITE_DECODER_FILE
  FILE *fp = RKADK_NULL;
  RKADK_CHAR name[128] = {0};
  mkdir("tmp", 0777);
  snprintf(name, sizeof(name), "/tmp/audio_out.pcm");

  fp = fopen(name, "wb");
  if (fp == RKADK_NULL) {
    RKADK_LOGE("can't open output file %s\n", name);
    return;
  }
  #endif

  while (!pstPlayer->bStopSendStream) {
    ret = RK_MPI_ADEC_GetFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo, pstPlayer->stAdecCtx.bBlock);
    if (!ret) {
      size = stFrmInfo.pstFrame->u32Len;
      if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
        if (size > 0)
          pstPlayer->positionTimeStamp = stFrmInfo.pstFrame->u64TimeStamp;

        if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
          result = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfo, s32MilliSec);
          RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
          if (result < 0)
            RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                        result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
        } else {
          result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfo.pstFrame, s32MilliSec);
          if (result < 0)
            RKADK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                        result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
        }
      }

      #ifdef WRITE_DECODER_FILE
      MB_BLK bBlk = stFrmInfo.pstFrame->pMbBlk;
      RK_VOID *pstFrame = RK_MPI_MB_Handle2VirAddr(bBlk);
      fwrite(pstFrame, 1, size, fp);
      #endif

      RK_MPI_ADEC_ReleaseFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo);

      if (size <= 0) {
        if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
          RKADK_LOGI("audio data send eof");

        break;
      }
    } else {
      RKADK_LOGE("RK_MPI_ADEC_GetFrame failed[%x]", ret);
      break;
    }

    if (pstPlayer->stAdecCtx.clrChnBuf) {
      RKADK_LOGI("adec clear chn(%d) buf", pstPlayer->stAdecCtx.chnIndex);
      RK_MPI_ADEC_ClearChnBuf(pstPlayer->stAdecCtx.chnIndex);
      pstPlayer->stAdecCtx.clrChnBuf = 0;
    }
  }

  #ifdef WRITE_DECODER_FILE
  if (fp)
    fclose(fp);
  #endif

  if ((pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) && (!ret)) {
    RKADK_LOGD("wait ao eos!");
    if (pstPlayer->s32AdecChnCnt > 1) {
      if (gMultiPlayerCtx.s32PlayerPrepareCnt == 1)
        RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);
      else {
        s32MsSleepTime = 1000 * pstPlayer->stAfCtx.s32CacheSize /
            (pstPlayer->stAfCtx.s32OutCh * pstPlayer->stAfCtx.s32OutputBitWidth / 8) / gMultiPlayerCtx.s32AoSampleRate;
        RKADK_LOGD("wait ao eos s32MsSleepTime %d", s32MsSleepTime);
#ifndef OS_RTT
        usleep(s32MsSleepTime * 1000llu);
#else
        rkos_msleep(s32MsSleepTime *1llu);
#endif
      }
    } else
      RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);
  }

  return;
}

static void SendDataDebugLevel2(RKADK_PLAYER_HANDLE_S *pstPlayer, struct timespec t_begin, struct timespec t_end, RK_U64 debugCount) {
  RK_U32 costtime = 0;
  costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
  if (debugCount % 100 == 0)
    RKADK_LOGI("vdec get frame rate : %lld.", debugCount * 1000000 / costtime);
  return;
}

static void SendDataDebugLevel3(RKADK_PLAYER_HANDLE_S *pstPlayer, VIDEO_FRAME_INFO_S sFrame, struct timespec t_begin, struct timespec t_end, RK_U64 debugCount) {
  RK_U32 ret = 0, costtime = 0;

  ret = RK_MPI_SYS_MmzFlushCache(sFrame.stVFrame.pMbBlk, false);
  if (ret != 0)
    RKADK_LOGE("sys mmz flush cache failed[%x]", ret);

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    ret = RK_MPI_VO_SendFrame(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, &sFrame, 0);
    if (ret != 0)
      RKADK_LOGE("send frame to vo failed[%x]", ret);
  }

  costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
  if (debugCount % 100 == 0)
    RKADK_LOGI("vdec get frame rate : %lld.", debugCount * 1000000 / costtime);

  RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
}

static void SendData(RKADK_VOID *ptr) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  RKADK_S32 s32MilliSec = -1, result = 0, size = 0;
  AUDIO_FRAME_INFO_S stFrmInfo;
  AUDIO_FRAME_INFO_S stFrmInfoCache;
  AUDIO_FRAME_S stFrmCache;
  VIDEO_FRAME_INFO_S sFrame;
  VIDEO_FRAME_INFO_S tFrame;
  struct timespec t_begin, t_end;
  struct timespec t_begin_1, t_end_1;
  RKADK_S32 flagGetFirstframe = 0, flagVideoEnd = 0, flagAudioEnd = 0, flagsSendNullFrame = 0;
  RKADK_S32 frameTime = 0, costtime = 0, maxNullFrameCount = 0;
  RKADK_S32 audioBytes = 0, sampleNum = 0;
  RKADK_S32 enableSendDataDebug = 0;
  RKADK_S32 voSendTime = 0;
  RK_U64 debugCount = 0;
  RK_U32 cacheBufferLen = 0, copySize = 0, secondPartLen = 0, s32MsSleepTime = 0;
  RK_U32 bufferOffset = 0, originOffset = 0;
  RK_U64 firstAudioTimeStamp = -1;
  RK_U8 *cacheFrame = RK_NULL, *originFrame = RK_NULL;
  VDEC_CHN_STATUS_S stStatus;
  bool bWaterline = true;

  memset(&stFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));
  memset(&stFrmInfoCache, 0, sizeof(AUDIO_FRAME_INFO_S));
  memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  memset(&tFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

  #ifdef WRITE_DECODER_FILE
  FILE *fp = RKADK_NULL;
  RKADK_CHAR name[128] = {0};
  mkdir("tmp", 0777);
  snprintf(name, sizeof(name), "/tmp/audio_out.pcm");

  fp = fopen(name, "wb");
  if (fp == RKADK_NULL) {
    RKADK_LOGE("can't open output file %s\n", name);
    return;
  }
  #endif

  if (pstPlayer->stDemuxerParam.videoAvgFrameRate <= 0) {
    RKADK_LOGE("video frame rate(%d) is out of range",
                pstPlayer->stDemuxerParam.videoAvgFrameRate);
    return;
  }
  frameTime = 1000000 / pstPlayer->stDemuxerParam.videoAvgFrameRate;

  stFrmInfoCache.pstFrame = &stFrmCache;
  stFrmInfoCache.pstFrame->u32Len = 0;
  stFrmInfoCache.pstFrame->u64TimeStamp = 0;

  audioBytes = pstPlayer->stAoCtx.bitWidth / 8;
  cacheBufferLen = audioBytes * pstPlayer->stDemuxerParam.audioChannels *
    pstPlayer->stDemuxerParam.audioSampleRate / pstPlayer->stDemuxerParam.videoAvgFrameRate;
  // Divisor correction
  sampleNum = cacheBufferLen / (audioBytes * pstPlayer->stDemuxerParam.audioChannels);
  cacheBufferLen = sampleNum * audioBytes * pstPlayer->stDemuxerParam.audioChannels;
  maxNullFrameCount = 2 * (1 + pstPlayer->stAoCtx.periodCount) * pstPlayer->stAoCtx.periodCount * pstPlayer->stAoCtx.periodSize / cacheBufferLen;

  if (getenv("send_data_debug_level")) {
    enableSendDataDebug = atoi(getenv("send_data_debug_level"));
    RKADK_LOGI("enableSendDataDebug value : %d, env %s.", enableSendDataDebug, getenv("send_data_debug_level"));
    RKADK_LOGI("video frame rate(%d), frametime[%d], sample rate[%d], channel[%d], cacheBufferLen[%d], enBlackBackground[%d]",
              pstPlayer->stDemuxerParam.videoAvgFrameRate, frameTime, pstPlayer->stDemuxerParam.audioSampleRate,
              pstPlayer->stDemuxerParam.audioChannels, cacheBufferLen, pstPlayer->bEnableBlackBackground);
  }

  cacheFrame = (RK_U8 *)(calloc(cacheBufferLen, sizeof(RK_U8)));
  memset(cacheFrame, 0, cacheBufferLen);

  MB_EXT_CONFIG_S extConfig;
  memset(&extConfig, 0, sizeof(extConfig));
  extConfig.pOpaque = cacheFrame;
  extConfig.pu8VirAddr = cacheFrame;
  extConfig.u64Size = cacheBufferLen;
  RK_MPI_SYS_CreateMB(&(stFrmCache.pMbBlk), &extConfig);

  while (!pstPlayer->bStopSendStream) {
    if (enableSendDataDebug == 1) {
      SendDataDebugLevel1(pstPlayer, sFrame);
      break;
    }

    if (pstPlayer->videoTimeStamp == -1 && !flagVideoEnd) {
      while (pstPlayer->videoTimeStamp < pstPlayer->seekTimeStamp) {
        ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, -1);
        if (ret == 0) {
          pstPlayer->frameCount++;
          pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;
          if (enableSendDataDebug) {
            debugCount++;
            clock_gettime(CLOCK_MONOTONIC, &t_begin_1);
          }

          if (pstPlayer->videoTimeStamp < pstPlayer->seekTimeStamp) {
            RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
            continue;
          }

          if (pstPlayer->bEnableBlackBackground) {
            tFrame.stVFrame.u32Width = sFrame.stVFrame.u32Width;
            tFrame.stVFrame.u32Height = sFrame.stVFrame.u32Height;
            tFrame.stVFrame.u32VirWidth = sFrame.stVFrame.u32VirWidth;
            tFrame.stVFrame.u32VirHeight = sFrame.stVFrame.u32VirHeight;
            tFrame.stVFrame.enPixelFormat = sFrame.stVFrame.enPixelFormat;
            tFrame.stVFrame.enCompressMode = sFrame.stVFrame.enCompressMode;
          }
          flagGetFirstframe = 1;
        }
      }
    }

    ret = RK_MPI_VDEC_QueryStatus(pstPlayer->stVdecCtx.chnIndex, &stStatus);
    if (ret == RK_SUCCESS) {
      if ((stStatus.u32LeftStreamFrames + stStatus.u32LeftPics) < pstPlayer->u32VdecWaterline)
        bWaterline = false;
      else
        bWaterline = true;
    } else {
      RKADK_LOGE("Query vdec status failed[%x]", ret);
    }

    if ((pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE && bWaterline) || pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE) {
      if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE) {
        if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {
          //send sFrame to vo
          ret = RKADK_PLAYER_SendVoFrame(pstPlayer, &sFrame, 0);
          if (ret != 0)
            RKADK_LOGE("send frame to vo failed[%x]", ret);

          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
          pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;
          flagGetFirstframe = 0;
          continue;
        } else {
          pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;
        }
      }

      // Send first video frames
      if (flagGetFirstframe == 1) {
        if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
          ret = RKADK_PLAYER_SendVoFrame(pstPlayer, &sFrame, 0);
          if (ret != 0)
            RKADK_LOGE("first video frame failed[%x]", ret);
        }
#if defined(RV1126B) || defined(RK3506)
        RK_MPI_VO_SetLayerCleanMode(pstPlayer->stVoCtx.u32VoLay, VO_LAYER_CLEAN_MODE_STOP);
#endif
        RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
      }

      if (!flagsSendNullFrame) {
        // Send audio null frames
        while(abs(frameTime - costtime) > 0.1 * frameTime) {
          stFrmInfoCache.pstFrame->u32Len = cacheBufferLen;
          stFrmInfoCache.pstFrame->u64TimeStamp += frameTime;
          stFrmInfoCache.pstFrame->enBitWidth = FindBitWidth(pstPlayer->stAoCtx.bitWidth);
          stFrmInfoCache.pstFrame->enSoundMode = FindSoundMode(pstPlayer->stDemuxerParam.audioChannels);
          stFrmInfoCache.pstFrame->s32SampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
          stFrmInfoCache.pstFrame->u32Seq++;
          stFrmInfoCache.pstFrame->bBypassMbBlk = false;
          clock_gettime(CLOCK_MONOTONIC, &t_begin);
          if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
            result = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfoCache, s32MilliSec);
            RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
            if (result < 0)
              RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                          result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
          } else {
            result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfoCache.pstFrame, 1);
            if (result)
              break;
          }
          clock_gettime(CLOCK_MONOTONIC, &t_end);
          costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
          maxNullFrameCount--;
          if (maxNullFrameCount <= 0)
            break;
        }
        stFrmInfoCache.pstFrame->u32Len = 0;
        flagsSendNullFrame = 1;
      }

      ret = RK_MPI_ADEC_GetFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo, false);
      if (!ret) {
        if (firstAudioTimeStamp == -1)
          firstAudioTimeStamp = stFrmInfo.pstFrame->u64TimeStamp;

        size = stFrmInfo.pstFrame->u32Len;
        if (size > 0) {
          stFrmInfoCache.pstFrame->enBitWidth = stFrmInfo.pstFrame->enBitWidth;
          stFrmInfoCache.pstFrame->enSoundMode = stFrmInfo.pstFrame->enSoundMode;
          stFrmInfoCache.pstFrame->s32SampleRate = stFrmInfo.pstFrame->s32SampleRate;
          stFrmInfoCache.pstFrame->bBypassMbBlk = false;

          originFrame = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(stFrmInfo.pstFrame->pMbBlk);
          copySize  = cacheBufferLen - stFrmInfoCache.pstFrame->u32Len;
          secondPartLen = stFrmInfo.pstFrame->u32Len - copySize;
          if (copySize >= stFrmInfo.pstFrame->u32Len) {
            copySize = stFrmInfo.pstFrame->u32Len;
            secondPartLen = 0;
          }

          memcpy(cacheFrame + bufferOffset, originFrame, copySize);
          bufferOffset += copySize;
          stFrmInfoCache.pstFrame->u32Len += copySize;
          originOffset = copySize;

__RETRY:
          if (stFrmInfoCache.pstFrame->u32Len == cacheBufferLen) {
            stFrmInfoCache.pstFrame->u64TimeStamp += frameTime;
            stFrmInfoCache.pstFrame->u32Seq++;
            if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
              if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
                result = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfoCache, s32MilliSec);
                RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
                if (result < 0)
                  RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                              result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
              } else {
                result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfoCache.pstFrame, s32MilliSec);
              }
              if (pstPlayer->positionTimeStamp == 0)
                pstPlayer->positionTimeStamp = firstAudioTimeStamp + frameTime;
              else
                pstPlayer->positionTimeStamp += frameTime;

              if (result < 0)
                RKADK_LOGE("send frame failed[%x], TimeStamp[%lld], s32MilliSec[%d]",
                          result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
            }
            bufferOffset = 0;
            stFrmInfoCache.pstFrame->u32Len = 0;

            // video process start
            if (!flagGetFirstframe) {
              if (flagVideoEnd == 0) {
                if (pstPlayer->stVdecCtx.chnFd > 0) {
                  ret = VdecPollEvent(0, pstPlayer->stVdecCtx.chnFd);
                  if (ret < 0)
                    continue;
                }

                if (pstPlayer->positionTimeStamp - pstPlayer->videoTimeStamp >= 0.5 * frameTime || (pstPlayer->stSmoothPlayParam.bEnableSmoothPlay == true)) {
__GETVDEC:
                  ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, 0);
                  if (ret == 0) {
                    if (pstPlayer->bIsNetRTStream && (sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp > 5 * frameTime))
                      pstPlayer->positionTimeStamp = sFrame.stVFrame.u64PTS;
                    pstPlayer->frameCount++;
                    if (sFrame.stVFrame.u64PTS == 0)
                      pstPlayer->videoTimeStamp += frameTime;
                    else
                      pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;
                    // RKADK_LOGD("pstPlayer->positionTimeStamp %lld, pstPlayer->videoTimeStamp %lld, sFrame.stVFrame.u64PTS %lld, frameTime %d\n",
                    //     pstPlayer->positionTimeStamp, pstPlayer->videoTimeStamp, sFrame.stVFrame.u64PTS, frameTime);
                    if (enableSendDataDebug) {
                      debugCount++;
                      clock_gettime(CLOCK_MONOTONIC, &t_end_1);
                    }

                    if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
                      SendBlackBackground(pstPlayer, &tFrame);
                      RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
                      RKADK_LOGI("chn %d reach eos frame", pstPlayer->stVdecCtx.chnIndex);
                      flagVideoEnd = 1;
                    } else {
                      if (pstPlayer->bIsNetRTStream) {
                        if ((pstPlayer->videoStreamCount - pstPlayer->frameCount) >= (RKADK_S32)(pstPlayer->stVdecCtx.streamBufferCnt + pstPlayer->stVdecCtx.frameBufferCnt - 1)) {
                          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
                          goto __GETVDEC;
                        }
                      }

                      if (enableSendDataDebug == 3) {
                        SendDataDebugLevel3(pstPlayer, sFrame, t_begin_1, t_end_1, debugCount);
                      } else {
                        if (abs(pstPlayer->positionTimeStamp - pstPlayer->videoTimeStamp) <= 0.5 * frameTime) {
                          // normal video send
                          if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
                            if (pstPlayer->bEnableBlackBackground)
                              tFrame.stVFrame.pMbBlk = sFrame.stVFrame.pMbBlk;

                            if (enableSendDataDebug != 2) {
                              ret = RKADK_PLAYER_SendVoFrame(pstPlayer, &sFrame, 0);
                              if (ret != 0)
                                RKADK_LOGE("send frame to vo failed[%x]", ret);
                            } else {
                              SendDataDebugLevel2(pstPlayer, t_begin_1, t_end_1, debugCount);
                            }
                          }

                          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
                        } else if (pstPlayer->positionTimeStamp - pstPlayer->videoTimeStamp > 0.5 * frameTime) {
                          // video is slower than audio
                          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
                          goto __GETVDEC;
                        } else {
                          RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
                        }
                      }
                    }
                  }
                }
              }
            }
            // video process end
          }

          if (secondPartLen > 0 && secondPartLen < cacheBufferLen) {
            memcpy(cacheFrame, originFrame + originOffset, secondPartLen);
            bufferOffset += secondPartLen;
            stFrmInfoCache.pstFrame->u32Len += secondPartLen;
          } else if(secondPartLen >= cacheBufferLen) {
            originOffset = stFrmInfo.pstFrame->u32Len - secondPartLen;
            memcpy(cacheFrame, originFrame + originOffset, cacheBufferLen);
            stFrmInfoCache.pstFrame->u32Len += cacheBufferLen;
            originOffset += cacheBufferLen;
            secondPartLen -= cacheBufferLen;
            goto __RETRY;
          }

        }

        if (size <= 0) {
          flagAudioEnd = 1;
          if (stFrmInfoCache.pstFrame->u32Len > 0) {
            stFrmInfoCache.pstFrame->u64TimeStamp += frameTime;
            if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
              result = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfoCache, s32MilliSec);
              RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
              if (result < 0)
                RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                            result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
            } else {
              result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfoCache.pstFrame, s32MilliSec);
            }
            if (result != 0)
              RKADK_LOGE("send frame to ao fail , %x.", ret);
          }

          stFrmInfoCache.pstFrame->enBitWidth = stFrmInfo.pstFrame->enBitWidth;
          stFrmInfoCache.pstFrame->enSoundMode = stFrmInfo.pstFrame->enSoundMode;
          stFrmInfoCache.pstFrame->s32SampleRate = stFrmInfo.pstFrame->s32SampleRate;
          stFrmInfoCache.pstFrame->u32Seq++;
          stFrmInfoCache.pstFrame->bBypassMbBlk = false;
          stFrmInfoCache.pstFrame->u32Len = 0;
          if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
            result = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfoCache, s32MilliSec);
            RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
            if (result < 0)
              RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                          result, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
          } else {
            result = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, stFrmInfoCache.pstFrame, s32MilliSec);
          }
          if (result != 0)
            RKADK_LOGE("send eof frame to ao fail , %x.", ret);
          else
            RKADK_LOGI("send eof frame to ao success");

          if(pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
            RKADK_LOGI("audio data send eof");
          RK_MPI_ADEC_ReleaseFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo);

          if (flagAudioEnd == 1 && flagVideoEnd == 1)
            pstPlayer->bStopSendStream = true;
          continue;
        }

        #ifdef WRITE_DECODER_FILE
        MB_BLK bBlk = stFrmInfo.pstFrame->pMbBlk;
        RK_VOID *pstFrame = RK_MPI_MB_Handle2VirAddr(bBlk);
        fwrite(pstFrame, 1, size, fp);
        #endif

        RK_MPI_ADEC_ReleaseFrame(pstPlayer->stAdecCtx.chnIndex, &stFrmInfo);
      } else {
        if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP || flagAudioEnd) {
          // audio sync
          ret = RK_MPI_VDEC_GetFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame, MAX_TIME_OUT_MS);
          if (ret == 0) {
            if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
              SendBlackBackground(pstPlayer, &tFrame);
              RKADK_LOGI("chn %d reach eos frame", pstPlayer->stVdecCtx.chnIndex);
              flagVideoEnd = 1;
            }

            if (pstPlayer->bEnableBlackBackground)
              tFrame.stVFrame.pMbBlk = sFrame.stVFrame.pMbBlk;

            if (pstPlayer->duration * 1000 - pstPlayer->positionTimeStamp > frameTime && flagAudioEnd && !flagVideoEnd) {
              clock_gettime(CLOCK_MONOTONIC, &t_end);
              if (pstPlayer->videoTimeStamp >= 0) {
                costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
                if ((RKADK_S64)sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp > (RKADK_S64)costtime) {
                  voSendTime = sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp - costtime;

                  if (!pstPlayer->bIsNetRTStream)
                    usleep(voSendTime);
                }
                voSendTime = frameTime;
              } else {
                voSendTime = frameTime;
              }
              pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;

              ret = RKADK_PLAYER_SendVoFrame(pstPlayer, &sFrame, 0);
              if (ret != 0)
                RKADK_LOGE("send frame to vo failed[%x]", ret);
              else {
                if (sFrame.stVFrame.u64PTS > pstPlayer->positionTimeStamp)
                  pstPlayer->positionTimeStamp = sFrame.stVFrame.u64PTS;
              }

              clock_gettime(CLOCK_MONOTONIC, &t_begin);
              if (!pstPlayer->bIsNetRTStream)
                usleep(voSendTime);
            }

            RK_MPI_VDEC_ReleaseFrame(pstPlayer->stVdecCtx.chnIndex, &sFrame);
          }

          if (flagAudioEnd == 1 && flagVideoEnd == 1)
            pstPlayer->bStopSendStream = true;
        } else {
#ifndef OS_RTT
          usleep(1000);
#else
          rkos_msleep(1);
#endif
        }
      }

      if (flagGetFirstframe == 1) {
        flagGetFirstframe = 0;
      }
    } else {
#ifndef OS_RTT
      usleep(1000);
#else
      rkos_msleep(1);
#endif
    }
  }

  if (pstPlayer->stSnapshotParam.stFrame.u32Width && pstPlayer->stSnapshotParam.stFrame.u32Height
    && pstPlayer->stSnapshotParam.stFrame.pMbBlk) {
    RK_MPI_MB_ReleaseMB(pstPlayer->stSnapshotParam.stFrame.pMbBlk);
    memset(&pstPlayer->stSnapshotParam.stFrame, 0, sizeof(PLAYER_SNAPSHOT_FRAME_S));
  }

  RK_MPI_MB_ReleaseMB(stFrmCache.pMbBlk);
  free(cacheFrame);

  #ifdef WRITE_DECODER_FILE
  if (fp)
    fclose(fp);
  #endif

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP && enableSendDataDebug == 0) {
    RKADK_LOGD("wait ao eos!");
    if (pstPlayer->s32AdecChnCnt > 1) {
      if (gMultiPlayerCtx.s32PlayerPrepareCnt == 1)
        RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);
      else {
        s32MsSleepTime = 1000 * pstPlayer->stAfCtx.s32CacheSize /
            (pstPlayer->stAfCtx.s32OutCh * pstPlayer->stAfCtx.s32OutputBitWidth / 8) / gMultiPlayerCtx.s32AoSampleRate;
        RKADK_LOGD("wait ao eos s32MsSleepTime %d", s32MsSleepTime);
#ifndef OS_RTT
        usleep(s32MsSleepTime * 1000llu);
#else
        rkos_msleep(s32MsSleepTime *1llu);
#endif
      }
    }
  }

  return;
}

static RKADK_VOID* SendDataThread(RKADK_VOID *ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;

#ifndef OS_RTT
  prctl(PR_SET_NAME, "rkplayer_send_data");
#endif
  if (pstPlayer->bVideoExist && pstPlayer->bAudioExist) {
    SendData(pstPlayer);
  } else if (!pstPlayer->bVideoExist && pstPlayer->bAudioExist) {
    SendAudioData(pstPlayer);
  } else if (pstPlayer->bVideoExist && !pstPlayer->bAudioExist) {
    SendVideoData(pstPlayer);
  }

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    if (pstPlayer->duration != 0)
      pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_EOF, NULL);
    pstPlayer->enStatus = RKADK_PLAYER_STATE_EOF;
  }

  RKADK_LOGI("Exit send data thread");
#ifdef OS_RTT
  pstPlayer->bSendEnd = 1;
#endif

  return RKADK_NULL;
}

static void AoDebugProcess(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  if (pstPlayer->stAoCtx.getVolume) {
    RKADK_S32 volume = 0;
    RK_MPI_AO_GetVolume(pstPlayer->stAoCtx.devId, &volume);
    RKADK_LOGI("info : get volume = %d", volume);
    pstPlayer->stAoCtx.getVolume = 0;
  }

  if (pstPlayer->stAoCtx.getMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(pstPlayer->stAoCtx.devId, &mute, &fade);
    RKADK_LOGI("info : is mute = %d", mute);
    pstPlayer->stAoCtx.getMute = 0;
  }

  if (pstPlayer->stAoCtx.getTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(pstPlayer->stAoCtx.devId, &trackMode);
    RKADK_LOGI("info : get track mode = %d", trackMode);
    pstPlayer->stAoCtx.getTrackMode = 0;
  }

  if (pstPlayer->stAoCtx.queryChnStat) {
    QueryAoFlowGraphStat(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    pstPlayer->stAoCtx.queryChnStat = 0;
  }

  if (pstPlayer->stAoCtx.saveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pstPlayer->stAoCtx.dstFilePath) {
      saveFile.bCfg = RK_TRUE;
      saveFile.u32FileSize = 1024 * 1024;
      snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
      snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", pstPlayer->stAoCtx.dstFilePath);
    }
    RK_MPI_AO_SaveFile(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &saveFile);
    pstPlayer->stAoCtx.saveFile = 0;
  }

  if (pstPlayer->stAoCtx.queryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &fileStat);
    RKADK_LOGI("info : query save file status = %d", fileStat.bSaving);
    pstPlayer->stAoCtx.queryFileStat = 0;
  }

  if (pstPlayer->stAoCtx.pauseResumeChn) {
#ifndef OS_RTT
    usleep(500 * 1000);
#else
    rkos_msleep(500);
#endif
    RK_MPI_AO_PauseChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    RKADK_LOGI("AO pause");
#ifndef OS_RTT
    usleep(1000 * 1000);
#else
    rkos_msleep(1000);
#endif
    RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    RKADK_LOGI("AO resume");
    pstPlayer->stAoCtx.pauseResumeChn = 0;
  }

  if (pstPlayer->stAoCtx.clrChnBuf) {
    RK_MPI_AO_ClearChnBuf(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    pstPlayer->stAoCtx.clrChnBuf = 0;
  }

  if (pstPlayer->stAoCtx.clrPubAttr) {
    RK_MPI_AO_ClrPubAttr(pstPlayer->stAoCtx.devId);
    pstPlayer->stAoCtx.clrPubAttr = 0;
  }

  if (pstPlayer->stAoCtx.getPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(pstPlayer->stAoCtx.devId, &pstAttr);
    RKADK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RKADK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RKADK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RKADK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    pstPlayer->stAoCtx.getPubAttr = 0;
  }
}

static RKADK_S32 BufferFree(RKADK_VOID *opaque) {
  if (opaque) {
    free(opaque);
    opaque = NULL;
  }

  return 0;
}

static RKADK_VOID DoPullDemuxerVideoPacket(RKADK_VOID* pHandle) {
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;
  RKADK_S32 ret = 0;
  VDEC_STREAM_S stStream;
  MB_BLK buffer = RKADK_NULL;
  MB_EXT_CONFIG_S stMbExtConfig;

  if (pstDemuxerPacket->s8EofFlag || (pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_WAIT
        && pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_VIDEO_DOING)
        || (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING)) {
    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING) {
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_VIDEO_DONE;
    }

    if (pstDemuxerPacket->s8EofFlag && pstPlayer->stSmoothPlayParam.bEnableSmoothPlay && pstPlayer->stSmoothPlayParam.pSignal) {
      if (!pstPlayer->bEnableAudio) {
        RKADK_SIGNAL_Give(pstPlayer->stSmoothPlayParam.pSignal);
      }
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }
      return;
    }

    if (!pstPlayer->bAudioExist && pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE) {
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
      RKADK_PLAYER_ProcessEvent(pstPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
    }

    memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stMbExtConfig.pFreeCB = BufferFree;
    stMbExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    stMbExtConfig.pu8VirAddr = (RK_U8 *)pstDemuxerPacket->s8PacketData;
    stMbExtConfig.u64Size = pstDemuxerPacket->s32PacketSize;

    RK_MPI_SYS_CreateMB(&buffer, &stMbExtConfig);

    stStream.u64PTS = pstDemuxerPacket->s64Pts;
    stStream.pMbBlk = buffer;
    stStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stStream.bEndOfStream = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bEndOfFrame = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bBypassMbBlk = RK_TRUE;

__RETRY:
    if (pstPlayer->bIsNetRTStream) {
      if (!pstPlayer->bVideoSendStreamFail || pstDemuxerPacket->s8SpecialFlag) {
        if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE)
          ret = RK_MPI_VDEC_SendStream(pstPlayer->stVdecCtx.chnIndex, &stStream, 0);
      } else
        ret = RKADK_FAILURE;

      if (ret) {
        pstPlayer->bVideoSendStreamFail = true;
      } else {
        pstPlayer->bVideoSendStreamFail = false;
        if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE)
          pstPlayer->videoStreamCount++;
      }
      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
    } else {
      ret = RK_MPI_VDEC_SendStream(pstPlayer->stVdecCtx.chnIndex, &stStream, -1);
      if (ret) {
        if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
          RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
          return;
        }

        RKADK_LOGE("RK_MPI_VDEC_SendStream failed[%x]", ret);

#ifndef OS_RTT
        usleep(1000llu);
#else
        rkos_msleep(1llu);
#endif
        goto  __RETRY;
      }

      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
    }
  } else {
    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }
  }

  return;
}

static RKADK_VOID DoPullDemuxerAudioPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  RKADK_S32 enableSendDataDebug = 0;
  MB_EXT_CONFIG_S stExtConfig;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_STREAM_S stAudioStream;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;

  if (getenv("send_data_debug_level")) {
    enableSendDataDebug = atoi(getenv("send_data_debug_level"));
    if (enableSendDataDebug == 1) {
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }
      return;
    }
  }

  if ((pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT)
    || (pstDemuxerPacket->s32PacketSize && pstDemuxerPacket->s64Pts < pstPlayer->seekTimeStamp)) {
    if (pstDemuxerPacket->s8PacketData) {
      free(pstDemuxerPacket->s8PacketData);
      pstDemuxerPacket->s8PacketData = NULL;
    }
    return;
  }

  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DOING || pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_VIDEO_DONE
       || pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE) {
    pstPlayer->positionTimeStamp = pstDemuxerPacket->s64Pts;
    while(pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_DONE) {
#ifndef OS_RTT
      usleep(1000);
#else
      rkos_msleep(1);
#endif

      if (pstPlayer->bStopSendStream) {
        break;
      }
    }
  }

  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE) {
    pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
    RKADK_PLAYER_ProcessEvent(pstPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
  }

  if (pstDemuxerPacket->s8EofFlag) {
    if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
      RKADK_LOGI("read eos packet, send eos to adec!");

    if (pstPlayer->stSmoothPlayParam.bEnableSmoothPlay && pstPlayer->stSmoothPlayParam.pSignal) {
      RKADK_SIGNAL_Give(pstPlayer->stSmoothPlayParam.pSignal);
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }
      return;
    }
    RK_MPI_ADEC_SendEndOfStream(pstPlayer->stAdecCtx.chnIndex, RK_FALSE);
  } else {
    stAudioStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stAudioStream.u64TimeStamp = pstDemuxerPacket->s64Pts;
    stAudioStream.u32Seq = pstDemuxerPacket->s32Series;
    stAudioStream.bBypassMbBlk = RK_TRUE;

    memset(&stExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stExtConfig.pFreeCB = BufferFree;
    stExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    stExtConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
    stExtConfig.u64Size    = pstDemuxerPacket->s32PacketSize;
    RK_MPI_SYS_CreateMB(&(stAudioStream.pMbBlk), &stExtConfig);

__RETRY:
    if (pstPlayer->bIsNetRTStream) {
      if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {

      } else {
        ret = RK_MPI_ADEC_SendStream(pstPlayer->stAdecCtx.chnIndex, &stAudioStream, RK_FALSE);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_ADEC_SendStream failed[%x]", ret);
      }
    } else {
      ret = RK_MPI_ADEC_SendStream(pstPlayer->stAdecCtx.chnIndex, &stAudioStream, RK_TRUE);
      if (ret != RK_SUCCESS) {
        RKADK_LOGE("RK_MPI_ADEC_SendStream failed[%x]", ret);
        goto __RETRY;
      }
    }
    RK_MPI_MB_ReleaseMB(stAudioStream.pMbBlk);
  }

  return;
}

static RKADK_VOID DoPullDemuxerWavPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_S32 s32MilliSec = -1, s32MsSleepTime = 0;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_FRAME_S frame;
  AUDIO_FRAME_INFO_S stFrmInfo;
  memset(&stFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;

  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT) {
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }

      return;
    } else {
      if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_DONE) {
        pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
        RKADK_PLAYER_ProcessEvent(pstPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
      }

      if (!pstDemuxerPacket->s8EofFlag)
        pstPlayer->positionTimeStamp = pstDemuxerPacket->s64Pts;

      frame.u32Len = pstDemuxerPacket->s32PacketSize;
      frame.u64TimeStamp = pstDemuxerPacket->s64Pts;
      frame.enBitWidth = FindBitWidth(pstPlayer->stAoCtx.bitWidth);
      frame.enSoundMode = FindSoundMode(pstPlayer->stAoCtx.channel);
      frame.bBypassMbBlk = RK_FALSE;

      memset(&stMbExtConfig, 0, sizeof(stMbExtConfig));
      stMbExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
      stMbExtConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
      stMbExtConfig.u64Size = pstDemuxerPacket->s32PacketSize;
      RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &stMbExtConfig);

__RETRY:
      if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
        frame.enSoundMode = FindSoundMode(pstPlayer->stDemuxerParam.audioChannels);
        stFrmInfo.pstFrame = &frame;
        ret = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfo, s32MilliSec);
        RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
        if (ret < 0)
          RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                      ret, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
      } else {
        ret = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &frame, s32MilliSec);
        if (ret < 0) {
          RKADK_LOGE("RK_MPI_AO_SendFrame failed[%x], TimeStamp[%lld], s32MilliSec[%d]",
                    ret, frame.u64TimeStamp, s32MilliSec);
          goto __RETRY;
        }
      }

      RK_MPI_MB_ReleaseMB(frame.pMbBlk);
      if (pstDemuxerPacket->s8PacketData) {
        free(pstDemuxerPacket->s8PacketData);
        pstDemuxerPacket->s8PacketData = NULL;
      }

      if (pstDemuxerPacket->s32PacketSize <= 0) {
        RKADK_LOGI("read eos packet, wait ao eos!");
        if (pstPlayer->s32AdecChnCnt > 1) {
          if (gMultiPlayerCtx.s32PlayerPrepareCnt == 1)
            RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);
          else {
            s32MsSleepTime = 1000 * pstPlayer->stAfCtx.s32CacheSize /
                (pstPlayer->stAfCtx.s32OutCh * pstPlayer->stAfCtx.s32OutputBitWidth / 8) / gMultiPlayerCtx.s32AoSampleRate;
            RKADK_LOGD("wait ao eos s32MsSleepTime %d", s32MsSleepTime);
#ifndef OS_RTT
            usleep(s32MsSleepTime * 1000llu);
#else
            rkos_msleep(s32MsSleepTime *1llu);
#endif
          }
        } else
          RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);

        if (pstPlayer->duration != 0)
          pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback((RKADK_MW_PTR)pstPlayer, RKADK_PLAYER_EVENT_EOF, NULL);
      }
    }
  } else {
    if (pstDemuxerPacket->s8PacketData) {
     free(pstDemuxerPacket->s8PacketData);
     pstDemuxerPacket->s8PacketData = NULL;
    }
  }

  return;
}

static RKADK_S32 SendWavPacket(RKADK_MW_PTR pPlayer, RKADK_PLAYER_PACKET *pstPacket) {
  RKADK_S32 ret = 0;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_S32 s32MilliSec = -1, s32MsSleepTime = 0;
  AUDIO_FRAME_S frame;
  AUDIO_FRAME_INFO_S stFrmInfo;
  memset(&stFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstPacket, RKADK_FAILURE);

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
    if (pstPacket->bBypass && pstPacket->s8PacketData) {
      pstPacket->pFreeCB((void *)pstPacket->s8PacketData);
      pstPacket->s8PacketData = NULL;
    }

    return 0;
  }

  if (!pstPacket->bEofFlag)
    pstPlayer->positionTimeStamp = pstPacket->s64Pts;

  frame.u32Len = pstPacket->s32PacketSize;
  frame.u64TimeStamp = pstPacket->s64Pts;
  frame.enBitWidth = FindBitWidth(pstPlayer->stAoCtx.bitWidth);
  frame.enSoundMode = FindSoundMode(pstPlayer->stAoCtx.channel);
  frame.bBypassMbBlk = pstPacket->bBypass;

  memset(&stMbExtConfig, 0, sizeof(stMbExtConfig));
  stMbExtConfig.pOpaque = (RKADK_VOID *)pstPacket->s8PacketData;
  stMbExtConfig.pu8VirAddr = (RK_U8*)pstPacket->s8PacketData;
  stMbExtConfig.u64Size = pstPacket->s32PacketSize;
  if (frame.bBypassMbBlk)
    stMbExtConfig.pFreeCB = pstPacket->pFreeCB;

  ret = RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &stMbExtConfig);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_SYS_CreateMB failed[%x]", ret);
    return -1;
  }

__RETRY:
  if (pstPlayer->s32AdecChnCnt > 1) {
#if defined(RK3506)
    frame.enSoundMode = FindSoundMode(pstPlayer->stDemuxerParam.audioChannels);
    stFrmInfo.pstFrame = &frame;
    ret = RK_MPI_AF_SendFrame(pstPlayer->stAfCtx.s32ReSampleIndex, &stFrmInfo, s32MilliSec);
    RKADK_LOGD("RK_MPI_AF_SendFrame chn index %d, ret %x", pstPlayer->stAfCtx.s32ReSampleIndex, ret);
    if (ret < 0)
      RKADK_LOGE("send af frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                  ret, stFrmInfo.pstFrame->u64TimeStamp, s32MilliSec);
#endif
  } else {
    ret = RK_MPI_AO_SendFrame(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, &frame, s32MilliSec);
    if (ret < 0) {
      RKADK_LOGE("RK_MPI_AO_SendFrame failed[%x], TimeStamp[%lld], s32MilliSec[%d]",
                ret, frame.u64TimeStamp, s32MilliSec);
      goto __RETRY;
    }
  }
  if (ret != RK_SUCCESS) {
    if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
      RK_MPI_MB_ReleaseMB(frame.pMbBlk);
      return 0;
    }

    RKADK_LOGE("RK_MPI_AO_SendFrame failed[%x], TimeStamp[%lld], s32MilliSec[%d]",
              ret, frame.u64TimeStamp, s32MilliSec);
    goto __RETRY;
  }

  RK_MPI_MB_ReleaseMB(frame.pMbBlk);

  if (pstPacket->s32PacketSize <= 0) {
    RKADK_LOGI("read eos packet, wait ao eos!");
    if (pstPlayer->s32AdecChnCnt > 1) {
      if (gMultiPlayerCtx.s32PlayerPrepareCnt == 1)
        RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);
      else {
        s32MsSleepTime = 1000 * pstPlayer->stAfCtx.s32CacheSize /
            (pstPlayer->stAfCtx.s32OutCh * pstPlayer->stAfCtx.s32OutputBitWidth / 8) / gMultiPlayerCtx.s32AoSampleRate;
        RKADK_LOGD("wait ao eos s32MsSleepTime %d", s32MsSleepTime);
#ifndef OS_RTT
        usleep(s32MsSleepTime * 1000llu);
#else
        rkos_msleep(s32MsSleepTime *1llu);
#endif
      }
    } else
      RK_MPI_AO_WaitEos(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex, s32MilliSec);

    if (pstPlayer->duration != 0)
      pstPlayer->positionTimeStamp = (RKADK_S64)(pstPlayer->duration * 1000);

    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_EOF, NULL);
  }

  return 0;
}

RKADK_S32 RKADK_PLAYER_SendAudioPacket(RKADK_MW_PTR pPlayer,
                              RKADK_PLAYER_PACKET *pstPacket) {
  RKADK_S32 ret = 0;
  MB_EXT_CONFIG_S stExtConfig;
  AUDIO_STREAM_S stAudioStream;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPacket->bBypass && !pstPacket->pFreeCB) {
    RKADK_LOGE("Bypass mode, must set pFreeCB");
    return -1;
  }

  if (pstPlayer->stAdecCtx.eCodecType == RKADK_CODEC_TYPE_PCM)
    return SendWavPacket(pPlayer, pstPacket);

  if (pstPacket->bEofFlag) {
    if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP)
      RKADK_LOGI("read eos packet, send eos to adec!");

    RK_MPI_ADEC_SendEndOfStream(pstPlayer->stAdecCtx.chnIndex, RK_FALSE);

    if (pstPacket->bBypass && pstPacket->s8PacketData) {
      pstPacket->pFreeCB((void *)pstPacket->s8PacketData);
      pstPacket->s8PacketData = NULL;
    }
  } else {
    stAudioStream.u32Len = pstPacket->s32PacketSize;
    stAudioStream.u64TimeStamp = pstPacket->s64Pts;
    stAudioStream.u32Seq = pstPacket->u32Seq;
    stAudioStream.bBypassMbBlk = pstPacket->bBypass? RK_TRUE : RK_FALSE;

    memset(&stExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    stExtConfig.pOpaque = (RKADK_VOID *)pstPacket->s8PacketData;
    stExtConfig.pu8VirAddr = (RK_U8*)pstPacket->s8PacketData;
    stExtConfig.u64Size    = pstPacket->s32PacketSize;
    if (pstPacket->bBypass)
      stExtConfig.pFreeCB = pstPacket->pFreeCB;

    ret = RK_MPI_SYS_CreateMB(&(stAudioStream.pMbBlk), &stExtConfig);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_SYS_CreateMB failed[%x]", ret);
      return -1;
    }

__RETRY:
    ret = RK_MPI_ADEC_SendStream(pstPlayer->stAdecCtx.chnIndex, &stAudioStream, RK_TRUE);
    if (ret != RK_SUCCESS) {
      if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
        RK_MPI_MB_ReleaseMB(stAudioStream.pMbBlk);
        return 0;
      }

      RKADK_LOGE("RK_MPI_ADEC_SendStream failed[%x]", ret);
      goto __RETRY;
    }
    RK_MPI_MB_ReleaseMB(stAudioStream.pMbBlk);
  }

  return 0;
}

RKADK_S32 RKADK_PLAYER_SendVideoPacket(RKADK_MW_PTR pPlayer,
                              RKADK_PLAYER_PACKET *pstPacket) {
  RKADK_S32 ret = 0;
  VDEC_STREAM_S stStream;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPacket->bBypass && !pstPacket->pFreeCB) {
    RKADK_LOGE("Bypass mode, must set pFreeCB");
    return -1;
  }

  stStream.u64PTS = pstPacket->s64Pts;
  stStream.u32Len = pstPacket->s32PacketSize;
  stStream.bEndOfStream = pstPacket->bEofFlag ? RK_TRUE : RK_FALSE;
  stStream.bEndOfFrame = pstPacket->bEofFlag ? RK_TRUE : RK_FALSE;
  stStream.bBypassMbBlk = pstPacket->bBypass? RK_TRUE : RK_FALSE;

  memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
  stMbExtConfig.pOpaque = (RKADK_VOID *)pstPacket->s8PacketData;
  stMbExtConfig.pu8VirAddr = (RK_U8 *)pstPacket->s8PacketData;
  stMbExtConfig.u64Size = pstPacket->s32PacketSize;
  if (stStream.bBypassMbBlk)
    stMbExtConfig.pFreeCB = pstPacket->pFreeCB;

  ret = RK_MPI_SYS_CreateMB(&stStream.pMbBlk, &stMbExtConfig);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_SYS_CreateMB failed[%x]", ret);
    return -1;
  }

__RETRY:
  ret = RK_MPI_VDEC_SendStream(pstPlayer->stVdecCtx.chnIndex, &stStream, -1);
  if (ret != RK_SUCCESS) {
    if (pstPlayer->enStatus == RKADK_PLAYER_STATE_STOP) {
      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
      return 0;
    }

    RKADK_LOGE("RK_MPI_VDEC_SendStream failed[%x]", ret);
#ifndef OS_RTT
    usleep(1000llu);
#else
    rkos_msleep(1llu);
#endif
    goto  __RETRY;
  }

  RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
  return 0;
}

static void FadeInInit(RKADK_PLAYER_HANDLE_S *pstPlayer, RKADK_PLAYER_FADEIN_CFG_S stFadeInCfg) {
  memset(&pstPlayer->stFadeInParam, 0, sizeof(RKADK_PLAYER_FADEIN_PARAM_S));
  pstPlayer->stFadeInParam.bEnableFadeIn = RKADK_TRUE;
  pstPlayer->stFadeInParam.u32FadeInFrames = stFadeInCfg.u32FadeInFrames;
  pstPlayer->stFadeInParam.u32VoChn = stFadeInCfg.u32VoChn;
  pstPlayer->stFadeInParam.u32Priority = stFadeInCfg.u32VoChn;
  return;
}

static int FadeChangeVoChn(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_U32 tmpChn;
  RKADK_U32 tmpChnPriority;
  VO_CHN_ATTR_S stChnAttr;
  RKADK_S32 ret = 0;
  RKADK_BOOL bChangePriority = RKADK_FALSE;

  // exchange vo chn id
  tmpChn = pstPlayer->stVoCtx.u32VoChn;
  pstPlayer->stVoCtx.u32VoChn = pstPlayer->stFadeInParam.u32VoChn;
  pstPlayer->stFadeInParam.u32VoChn = tmpChn;

  // if last player display chn u32ChnPriority less than stFadeInParam chn priority, exchange
  if (pstPlayer->stVoCtx.u32ChnPriority < pstPlayer->stFadeInParam.u32Priority) {
    tmpChnPriority = pstPlayer->stVoCtx.u32ChnPriority;
    pstPlayer->stVoCtx.u32ChnPriority = pstPlayer->stFadeInParam.u32Priority;
    pstPlayer->stFadeInParam.u32Priority = tmpChnPriority;
    bChangePriority = RKADK_TRUE;
  }

  stChnAttr.stRect.s32X = pstPlayer->stVoCtx.x;
  stChnAttr.stRect.s32Y = pstPlayer->stVoCtx.y;
  stChnAttr.stRect.u32Width = pstPlayer->stVoCtx.dispWidth;
  stChnAttr.stRect.u32Height = pstPlayer->stVoCtx.dispHeight;
  stChnAttr.u32FgAlpha = 255;
  stChnAttr.u32BgAlpha = 0;
  stChnAttr.enMirror = pstPlayer->stVoCtx.enMirror;
  stChnAttr.enRotation = pstPlayer->stVoCtx.enRotation;
  stChnAttr.u32Priority = pstPlayer->stVoCtx.u32ChnPriority;
  RKADK_LOGD("VO player chn %d, priority %d\n", pstPlayer->stVoCtx.u32VoChn, stChnAttr.u32Priority);
#if defined(RV1126B) || defined(RK3506)
  if (pstPlayer->stFadeInParam.u32FadeInFrames)
    stChnAttr.u32FadeInFrames = pstPlayer->stFadeInParam.u32FadeInFrames;
#endif

  // set new player display chn attr to fade in
  ret = RK_MPI_VO_SetChnAttr(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, &stChnAttr);
  if (ret) {
    RKADK_LOGE("New display chn set RK_MPI_VO_SetChnAttr[%d, %d] failed[%x]", pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, ret);
    goto fadein_err;
  }

  stChnAttr.u32Priority = pstPlayer->stFadeInParam.u32Priority;
#if defined(RV1126B) || defined(RK3506)
  stChnAttr.u32FadeInFrames = 0;
#endif
  RKADK_LOGD("VO stFadeInParam chn %d, priority %d\n", pstPlayer->stFadeInParam.u32VoChn, stChnAttr.u32Priority);
  // set last player display chn as background
  ret = RK_MPI_VO_SetChnAttr(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stFadeInParam.u32VoChn, &stChnAttr);
  if (ret) {
    RKADK_LOGE("Background chn set RK_MPI_VO_SetChnAttr[%d, %d] failed[%x]", pstPlayer->stVoCtx.u32VoLay, pstPlayer->stFadeInParam.u32VoChn, ret);
    goto fadein_err;
  }

  return 0;

fadein_err:
  tmpChn = pstPlayer->stVoCtx.u32VoChn;
  pstPlayer->stVoCtx.u32VoChn = pstPlayer->stFadeInParam.u32VoChn;
  pstPlayer->stFadeInParam.u32VoChn = tmpChn;
  if (bChangePriority) {
    tmpChnPriority = pstPlayer->stVoCtx.u32ChnPriority;
    pstPlayer->stVoCtx.u32ChnPriority = pstPlayer->stFadeInParam.u32Priority;
    pstPlayer->stFadeInParam.u32Priority = tmpChnPriority;
    bChangePriority = RKADK_FALSE;
  }

  return -1;
}

static RKADK_S32 CreateDeviceVo(RKADK_PLAYER_HANDLE_S *pstPlayer, RKADK_PLAYER_FRAME_INFO_S *pstFrameInfo) {
  RKADK_S32 ret = 0;
  VO_VIDEO_LAYER_ATTR_S    stLayerAttr;
  VO_PUB_ATTR_S            stPubAttr;
  VO_CHN_ATTR_S stChnAttr;

  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);

  memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VO_CHN_ATTR_S));
  memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));

  stPubAttr.enIntfType = RKADK_MEDIA_GetRkVoIntfTpye(pstPlayer->stVoCtx.enIntfType);
  stPubAttr.enIntfSync = pstFrameInfo->enIntfSync;
  stPubAttr.stSyncInfo.u16FrameRate = pstFrameInfo->stSyncInfo.u16FrameRate;

  stLayerAttr.enPixFormat = pstPlayer->stVoCtx.pixFormat;
  stLayerAttr.u32DispFrmRt = pstPlayer->stVoCtx.dispFrmRt;
  if (pstFrameInfo->enVoSpliceMode == SPLICE_MODE_BYPASS) {
    stLayerAttr.stDispRect.s32X = pstPlayer->stVoCtx.x;
    stLayerAttr.stDispRect.s32Y = pstPlayer->stVoCtx.y;
    stLayerAttr.stDispRect.u32Width = pstPlayer->stVoCtx.dispWidth;
    stLayerAttr.stDispRect.u32Height = pstPlayer->stVoCtx.dispHeight;
  }

  stChnAttr.stRect.s32X = pstPlayer->stVoCtx.x;
  stChnAttr.stRect.s32Y = pstPlayer->stVoCtx.y;
  stChnAttr.stRect.u32Width = pstPlayer->stVoCtx.dispWidth;
  stChnAttr.stRect.u32Height = pstPlayer->stVoCtx.dispHeight;
  stChnAttr.u32FgAlpha = 255;
  stChnAttr.u32BgAlpha = 0;
  stChnAttr.enMirror = pstPlayer->stVoCtx.enMirror;
  stChnAttr.enRotation = pstPlayer->stVoCtx.enRotation;
  pstPlayer->stVoCtx.u32ChnPriority = pstPlayer->stVoCtx.u32VoChn;
  if (pstPlayer->stFadeInParam.u32FadeInFrames) {
    RKADK_U32 tmpChnPriority;
    if (pstPlayer->stVoCtx.u32ChnPriority < pstPlayer->stFadeInParam.u32Priority) {
      tmpChnPriority = pstPlayer->stVoCtx.u32ChnPriority;
      pstPlayer->stVoCtx.u32ChnPriority = pstPlayer->stFadeInParam.u32Priority;
      pstPlayer->stFadeInParam.u32Priority = tmpChnPriority;
    }
  }

  RKADK_LOGE("rect: [%d %d %d %d]",
            stChnAttr.stRect.s32X, stChnAttr.stRect.s32Y = pstPlayer->stVoCtx.y,
            stChnAttr.stRect.u32Width, stChnAttr.stRect.u32Height);

  // DispBufLen set to 3 to resolve display frame rate insufficiency
  if (pstFrameInfo->u32DispBufLen <= 0)
    pstFrameInfo->u32DispBufLen = 3;

  stChnAttr.u32Priority = pstPlayer->stVoCtx.u32ChnPriority;
  if (pstPlayer->bIsNetRTStream)
    ret = RKADK_MPI_VO_Init(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stVoCtx.u32VoChn, &stPubAttr, &stLayerAttr, &stChnAttr,
                        pstFrameInfo->enVoSpliceMode, pstFrameInfo->u32DispBufLen,
                        VO_LAYER_MODE_GRAPHIC);
  else
    ret = RKADK_MPI_VO_Init(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stVoCtx.u32VoChn, &stPubAttr, &stLayerAttr, &stChnAttr,
                        pstFrameInfo->enVoSpliceMode, pstFrameInfo->u32DispBufLen,
                        VO_LAYER_MODE_VIDEO);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_Vo_Init failed[%x]", ret);
    return ret;
  }

  if (pstPlayer->stFadeInParam.u32FadeInFrames) {
    stChnAttr.u32Priority = pstPlayer->stFadeInParam.u32Priority;
#if defined(RV1126B) || defined(RK3506)
    stChnAttr.u32FadeInFrames = 0;
#endif
    ret = RKADK_MPI_VO_Init(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stFadeInParam.u32VoChn, &stPubAttr, &stLayerAttr, &stChnAttr,
                        pstFrameInfo->enVoSpliceMode, pstFrameInfo->u32DispBufLen,
                        VO_LAYER_MODE_VIDEO);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_Vo_Init for Player fade in config failed[%x]", ret);
      ret = RKADK_MPI_VO_DeInit(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                                pstPlayer->stVoCtx.u32VoChn);
      if (ret)
        RKADK_LOGE("RKADK_MPI_Vo_DeInit failed[%x]", ret);
      return RKADK_FAILURE;
    }
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 DestroyDeviceVo(RKADK_PLAYER_HANDLE_S *pstPlayer) {
  RKADK_CHECK_POINTER(pstPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;

  if (pstPlayer->stFadeInParam.bEnableFadeIn) {
    ret = RKADK_MPI_VO_DeInit(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                          pstPlayer->stFadeInParam.u32VoChn);
    if (ret != RKADK_SUCCESS)
      RKADK_LOGE("RK_MPI_VO_DisableChn failed[%x]", ret);
    pstPlayer->stFadeInParam.bEnableFadeIn = RKADK_FALSE;
  }

  ret = RKADK_MPI_VO_DeInit(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoDev,
                        pstPlayer->stVoCtx.u32VoChn);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_DisableChn failed[%x]", ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Create(RKADK_MW_PTR *pPlayer,
                              RKADK_PLAYER_CFG_S *pstPlayCfg) {
  RKADK_DEMUXER_INPUT_S stDemuxerInput;
  bool bSysInit = false;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pstPlayCfg, RKADK_FAILURE);

  if (*pPlayer) {
    RKADK_LOGE("player has been created");
    return RKADK_FAILURE;
  }

  bSysInit = RKADK_MPI_SYS_CHECK();
  if (!bSysInit) {
    RKADK_LOGE("System is not initialized");
    return RKADK_FAILURE;
  }

  RKADK_LOGI("Create Player[%d, %d] Start...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  if (pstPlayCfg->bEnableVideo == RKADK_FALSE && pstPlayCfg->bEnableAudio == RKADK_FALSE) {
    RKADK_LOGE("bEnableVideo and bEnableAudio are not enable");
    return RKADK_FAILURE;
  }

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)malloc(sizeof(RKADK_PLAYER_HANDLE_S));
  if (!pstPlayer) {
    RKADK_LOGE("malloc pstPlayer failed");
    return RKADK_FAILURE;
  }

  memset(pstPlayer, 0, sizeof(RKADK_PLAYER_HANDLE_S));
  pstPlayer->pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;
  pstPlayer->bEnableVideo = pstPlayCfg->bEnableVideo;
  pstPlayer->bEnableAudio = pstPlayCfg->bEnableAudio;
  pstPlayer->bEnableBlackBackground = pstPlayCfg->bEnableBlackBackground;
  pstPlayer->enStatus = RKADK_PLAYER_STATE_BUTT;
  pstPlayer->bVideoSendStreamFail = false;
  pstPlayer->videoStreamCount = 0;

  pstPlayer->bEnableThirdDemuxer = pstPlayCfg->bEnableThirdDemuxer;
  if (!pstPlayCfg->bEnableThirdDemuxer) {
    stDemuxerInput.ptr = (RKADK_VOID *)pstPlayer;
    stDemuxerInput.readModeFlag = DEMUXER_TYPE_PASSIVE;
    stDemuxerInput.videoEnableFlag = pstPlayer->bEnableVideo;
    stDemuxerInput.audioEnableFlag = pstPlayer->bEnableAudio;
    stDemuxerInput.transport = pstPlayCfg->stNetStreamCfg.transport;
    stDemuxerInput.u32IoTimeout = pstPlayCfg->stNetStreamCfg.u32IoTimeout;
    if (RKADK_DEMUXER_Create(&pstPlayer->pDemuxerCfg, &stDemuxerInput)) {
      RKADK_LOGE("RKADK_DEMUXER_Create failed");
      free(pstPlayer);
      return RKADK_FAILURE;
    }
  }

  if (pstPlayer->bEnableVideo) {
    if (VdecCtxInit(&pstPlayer->stVdecCtx, pstPlayCfg->stVdecCfg)) {
      RKADK_LOGE("Create VDEC ctx failed");
      goto __FAILED;
    }

    pstPlayer->stFadeInParam.bEnableFadeIn = RKADK_FALSE;
    if (pstPlayCfg->stFadeInCfg.u32FadeInFrames) {
      FadeInInit(pstPlayer, pstPlayCfg->stFadeInCfg);
    }

    if (SetVoCtx(&(pstPlayer->stVoCtx), &pstPlayCfg->stFrmInfo)) {
      RKADK_LOGE("Create VO ctx failed");
      goto __FAILED;
    }

    if (CreateDeviceVo(pstPlayer, &pstPlayCfg->stFrmInfo)) {
      RKADK_LOGE("Create VO failed");
      goto __FAILED;
    }
  }

  if (pstPlayer->bEnableAudio) {
    if (RKADK_AUDIO_DECODER_Register(RKADK_CODEC_TYPE_MP3)) {
      RKADK_LOGE("register mp3 failed");
      goto __FAILED;
    }

    if (AdecCtxInit(&pstPlayer->stAdecCtx, &pstPlayCfg->stAudioCfg)) {
      RKADK_LOGE("Create ADEC ctx failed");
      goto __FAILED;
    }

    AoCtxInit(&pstPlayer->stAoCtx, &pstPlayCfg->stAudioCfg);
    if (pstPlayCfg->stAudioCfg.s32AdecChnCnt > 1) {
      AmixCtxInit(&pstPlayer->stAmixCtx, &pstPlayCfg->stAudioCfg);
      AfCtxInit(&pstPlayer->stAfCtx, &pstPlayCfg->stAudioCfg);
      pstPlayer->s32AdecChnCnt = pstPlayCfg->stAudioCfg.s32AdecChnCnt;
    }
  }

  pthread_mutex_init(&(pstPlayer->mutex), NULL);

  if (pstPlayCfg->stSnapshotCfg.pfnDataCallback) {
    if (SnapshotEnable(pstPlayer, pstPlayCfg->stSnapshotCfg)) {
      RKADK_LOGE("Enable snapshot failed");
      goto __FAILED;
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_IDLE;
  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  *pPlayer = (RKADK_MW_PTR)pstPlayer;
  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->bEnableVideo)
    DestroyDeviceVo(pstPlayer);

  if (pstPlayer->pDemuxerCfg)
    RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);

  RKADK_PLAYER_ProcessEvent((RKADK_MW_PTR)pstPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  if (pstPlayer)
    free(pstPlayer);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_LOGI("Destory Player Start...");
  ret = RKADK_PLAYER_Stop(pPlayer);
  if (ret && ret != RKADK_STATE_ERR) {
    RKADK_LOGE("RKADK_PLAYER_Stop failed");
    return RKADK_FAILURE;
  }

  if (pstPlayer->bEnableVideo == RKADK_TRUE) {
    ret = DestroyDeviceVo(pstPlayer);
    if (ret) {
      RKADK_LOGE("DestroyDeviceVo failed");
      return RKADK_FAILURE;
    }
  }

  if (pstPlayer->bEnableAudio)
    if (RKADK_AUDIO_DECODER_UnRegister(RKADK_CODEC_TYPE_MP3)) {
      RKADK_LOGE("unregister mp3 failed");
      return RKADK_FAILURE;
    }

  if (pstPlayer->pDemuxerCfg)
    RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);

  pthread_mutex_destroy(&(pstPlayer->mutex));

  if (pstPlayer->stSnapshotParam.pfnDataCallback)
    if (SnapshotDisable(pstPlayer))
      RKADK_LOGE("Disable snapshot failed");

  if (pstPlayer)
    free(pstPlayer);

  RKADK_LOGI("Destory Player End...");
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_SetDataParam(RKADK_MW_PTR pPlayer,
                                      RKADK_PLAYER_DATA_PARAM_S *pstDataParam) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstDataParam, RKADK_FAILURE);

#if defined(RV1106_1103) || defined(RV1103B)
  if (pstDataParam->enVCodecType == RKADK_CODEC_TYPE_H265) {
    RKADK_LOGE("rv1106 nonsupport h265");
    return -1;
  }
#endif

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  pstPlayer->videoTimeStamp = -1;
  pstPlayer->bStopSendStream = false;
  pstPlayer->bIsNetRTStream = pstDataParam->bIsNetRTStream;

  if (pstDataParam->pFilePath) {
    memset(pstPlayer->pFilePath, 0, RKADK_PATH_LEN);
    memcpy(pstPlayer->pFilePath, pstDataParam->pFilePath, strlen(pstDataParam->pFilePath));
  }

  if (pstDataParam->bVideoExist) {
    if (pstDataParam->enVCodecType != RKADK_CODEC_TYPE_H264
        && pstDataParam->enVCodecType != RKADK_CODEC_TYPE_H265) {
      RKADK_LOGE("Nonsupport video codec type[%d]", pstDataParam->enVCodecType);
      return -1;
    }
    pstPlayer->stVdecCtx.eCodecType = pstDataParam->enVCodecType;

    if (pstDataParam->u32Width <= 0 || pstDataParam->u32Height <= 0) {
      RKADK_LOGE("Invalid video resolution[%d x %d]", pstDataParam->u32Width, pstDataParam->u32Height);
      return -1;
    }
    pstPlayer->stVdecCtx.srcWidth = pstDataParam->u32Width;
    pstPlayer->stVdecCtx.srcHeight = pstDataParam->u32Height;

    pstPlayer->stVdecCtx.outputPixFmt = RKADK_MEDIA_GetRkPixelFormat(pstDataParam->enPixFmt);
    if (RK_FMT_BUTT == pstPlayer->stVdecCtx.outputPixFmt) {
      RKADK_LOGE("Nonsupport output pixel fmt[%d]", pstDataParam->enPixFmt);
      return -1;
    }

    pstPlayer->stDemuxerParam.videoAvgFrameRate = pstDataParam->u32FrameRate;
    pstPlayer->bVideoExist = pstDataParam->bVideoExist;

    RKADK_LOGD("Video[%d x %d]: codecType[%d], pixFmt[%d], fps[%d]",
                pstDataParam->u32Width, pstDataParam->u32Height,
                pstDataParam->enVCodecType, pstDataParam->enPixFmt,
                pstDataParam->u32FrameRate);
  }

  if (pstDataParam->bAudioExist) {
    if (pstDataParam->enACodecType != RKADK_CODEC_TYPE_MP3
        && pstDataParam->enACodecType != RKADK_CODEC_TYPE_ACC
        && pstDataParam->enACodecType != RKADK_CODEC_TYPE_PCM
        && pstDataParam->enACodecType != RKADK_CODEC_TYPE_G711A) {
      RKADK_LOGE("Nonsupport audio codec type[%d]", pstDataParam->enACodecType);
      return -1;
    }
    pstPlayer->stAdecCtx.eCodecType = pstDataParam->enACodecType;
    pstPlayer->stAdecCtx.sampleRate = pstDataParam->u32SampleRate;
    pstPlayer->stAdecCtx.channel = pstDataParam->u32Channel;

    if (pstDataParam->u32BitWidth != 8 && pstDataParam->u32BitWidth != 16) {
      RKADK_LOGE("Nonsupport bitwidth[%d]", pstDataParam->u32BitWidth);
      return -1;
    }
    pstPlayer->stAoCtx.bitWidth = pstDataParam->u32BitWidth;
    pstPlayer->stAoCtx.reSmpSampleRate = pstDataParam->u32SampleRate;
    pstPlayer->stAoCtx.channel = pstDataParam->u32Channel;
    pstPlayer->stDemuxerParam.audioChannels = pstDataParam->u32Channel;
    pstPlayer->stDemuxerParam.audioSampleRate = pstDataParam->u32SampleRate;
    pstPlayer->bAudioExist = pstDataParam->bAudioExist;

    RKADK_LOGD("Audio CodecType[%d], sampleRate[%d], channel[%d], bitWidth[%d]",
                pstDataParam->enACodecType, pstDataParam->u32SampleRate,
                pstDataParam->u32Channel, pstDataParam->u32BitWidth);
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_INIT;
  return 0;
}

RKADK_S32 RKADK_PLAYER_SetDataSource(RKADK_MW_PTR pPlayer,
                                     const RKADK_CHAR *pszfilePath) {
  const char *suffix = NULL;
  RKADK_PLAYER_HANDLE_S *pstPlayer;
  RKADK_CODEC_TYPE_E eAudioCodecType = RKADK_CODEC_TYPE_BUTT;
  int ret;

  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->bEnableThirdDemuxer) {
    RKADK_LOGE("Enable third-party demuxer, please use RKADK_PLAYER_SetDataParam");
    return -1;
  }

  if ((strlen(pszfilePath) <= 0) || (strlen(pszfilePath) >= RKADK_PATH_LEN)) {
    RKADK_LOGE("Invalid pszfilePath[%s] lenght[%d]", pszfilePath, strlen(pszfilePath));
    goto __FAILED;
  }

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_IDLE
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  RKADK_LOGI("SetDataSource[%s] start...", pszfilePath);

  if (strstr(pszfilePath, "rtsp://"))
    pstPlayer->bIsNetRTStream = RKADK_TRUE;
  else
    suffix = strrchr(pszfilePath, '.');

  if (!suffix && !pstPlayer->bIsNetRTStream) {
    RKADK_LOGD("Non-file format or rtsp: %s", pszfilePath);
    goto __FAILED;
  }

  pstPlayer->bVideoExist = RKADK_FALSE;
  pstPlayer->bAudioExist = RKADK_FALSE;
  pstPlayer->videoTimeStamp = -1;
  pstPlayer->bStopSendStream = false;

  if (pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_WAIT) {
    memset(pstPlayer->pFilePath, 0, RKADK_PATH_LEN);
    memcpy(pstPlayer->pFilePath, pszfilePath, strlen(pszfilePath));
  }

  if (pstPlayer->stFadeInParam.u32FadeInFrames > 0) {
    if (pstPlayer->stFadeInParam.u32SetDataSourceCnt > 0) {
      ret = FadeChangeVoChn(pstPlayer);
      if (ret) {
        RKADK_LOGE("Fade in change vo chn fail");
        goto __FAILED;
      }
      pstPlayer->stFadeInParam.u32FadeInFrames = 0;
    }
    pstPlayer->stFadeInParam.u32SetDataSourceCnt++;
  }

  if((suffix && !strcmp(suffix, ".mp4")) || (suffix && !strcmp(suffix, ".ts")) || (suffix && !strcmp(suffix, ".flv")) || pstPlayer->bIsNetRTStream) {
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;

    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->bEnableVideo == RKADK_TRUE) {
      if (pstPlayer->stDemuxerParam.pVideoCodec != NULL) {
        if (strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264") && strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h265")) {
          RKADK_LOGE("Unsupported video format(%s)", pstPlayer->stDemuxerParam.pVideoCodec);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.videoWidth <= 0 || pstPlayer->stDemuxerParam.videoHeigh <= 0) {
          RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->stDemuxerParam.videoWidth,
                      pstPlayer->stDemuxerParam.videoHeigh);
          goto __FAILED;
        }

        if (!strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264")) {
          pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H264;
        } else if (!strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h265")) {
#if defined(RV1106_1103) || defined(RV1103B)
          RKADK_LOGE("rv1106 nonsupport h265");
          goto __FAILED;
#else
          pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H265;
#endif
        }

        pstPlayer->bVideoExist = RKADK_TRUE;
        pstPlayer->stVdecCtx.srcWidth = pstPlayer->stDemuxerParam.videoWidth;
        pstPlayer->stVdecCtx.srcHeight = pstPlayer->stDemuxerParam.videoHeigh;

        if (pstPlayer->stDemuxerParam.VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
          pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP_10BIT;
        else
          pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP;
      } else if (pstPlayer->bEnableAudio){
        RKADK_LOGW("Video codec not find, try to play audio");
      } else {
        RKADK_LOGE("Video codec not find");
        goto __FAILED;
      }
    } else
      RKADK_LOGW("Video is unable");

    if (pstPlayer->bEnableAudio == RKADK_TRUE) {
      if (pstPlayer->stDemuxerParam.pAudioCodec != NULL) {
        if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "mp3"))
          eAudioCodecType = RKADK_CODEC_TYPE_MP3;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "wav"))
          eAudioCodecType = RKADK_CODEC_TYPE_PCM;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "pcm_alaw"))
          eAudioCodecType = RKADK_CODEC_TYPE_G711A;
        else {
          RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->stDemuxerParam.pAudioCodec);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.audioFormat == 0) {
          pstPlayer->stAoCtx.bitWidth = 8;
          if (pstPlayer->s32AdecChnCnt > 1) {
            pstPlayer->stAfCtx.s32InputBitWidth = pstPlayer->stAoCtx.bitWidth;
            if (!gMultiPlayerCtx.s32AoBitWidth)
              gMultiPlayerCtx.s32AoBitWidth = pstPlayer->stAoCtx.bitWidth;
          }
        } else if (pstPlayer->stDemuxerParam.audioFormat == 1) {
          pstPlayer->stAoCtx.bitWidth = 16;
          if (pstPlayer->s32AdecChnCnt > 1) {
            pstPlayer->stAfCtx.s32InputBitWidth = pstPlayer->stAoCtx.bitWidth;
            if (!gMultiPlayerCtx.s32AoBitWidth)
              gMultiPlayerCtx.s32AoBitWidth = pstPlayer->stAoCtx.bitWidth;
          }
        } else {
          RKADK_LOGE("Unsupported audioFormat = %d", pstPlayer->stDemuxerParam.audioFormat);
          goto __FAILED;
        }

        if (pstPlayer->stDemuxerParam.audioChannels <= 0 ||
            pstPlayer->stDemuxerParam.audioSampleRate <= 0) {
          RKADK_LOGE("Invalid channel(%d) or reSmpSampleRate(%d)",
                      pstPlayer->stDemuxerParam.audioChannels, pstPlayer->stDemuxerParam.audioSampleRate);
          goto __FAILED;
        }

        pstPlayer->bAudioExist = RKADK_TRUE;
        pstPlayer->stAdecCtx.sampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
        pstPlayer->stAdecCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
        pstPlayer->stAdecCtx.eCodecType = eAudioCodecType;
        pstPlayer->stAoCtx.reSmpSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
        pstPlayer->stAoCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
        if (pstPlayer->s32AdecChnCnt > 1) {
          if (!gMultiPlayerCtx.s32AoSampleRate)
            gMultiPlayerCtx.s32AoSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
          if (!gMultiPlayerCtx.s32AoChannels)
            gMultiPlayerCtx.s32AoChannels = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAoCtx.reSmpSampleRate = gMultiPlayerCtx.s32AoSampleRate;
          pstPlayer->stAoCtx.channel = gMultiPlayerCtx.s32AoChannels;
          pstPlayer->stAfCtx.s32InputRate = pstPlayer->stDemuxerParam.audioSampleRate;
          pstPlayer->stAfCtx.s32InputCh = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAfCtx.s32OutRate = pstPlayer->stDemuxerParam.audioSampleRate;
          pstPlayer->stAfCtx.s32OutCh = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAfCtx.s32OutputBitWidth = gMultiPlayerCtx.s32AoBitWidth;
          pstPlayer->stAmixCtx.s32InSampleRate = gMultiPlayerCtx.s32AoSampleRate;
          pstPlayer->stAmixCtx.u32InChannel = gMultiPlayerCtx.s32AoChannels;
        }
      } else if (pstPlayer->bEnableVideo) {
        RKADK_LOGE("Audio codec not find, try to play video");
      } else {
        RKADK_LOGE("Audio codec not find");
        goto __FAILED;
      }
    } else
      RKADK_LOGW("audio is unable");
  } else if (suffix && (!strcmp(suffix, ".h264") || !strcmp(suffix, ".h265"))) {
    if (!pstPlayer->bEnableVideo) {
      RKADK_LOGE("video is unable");
      goto __FAILED;
    }

    if (!strcmp(suffix, ".h264")) {
      pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H264;
    } else if (!strcmp(suffix, ".h265")) {
#if defined(RV1106_1103) || defined(RV1103B)
      RKADK_LOGE("1106 nonsupport h265");
      goto __FAILED;
#else
      pstPlayer->stVdecCtx.eCodecType = RKADK_CODEC_TYPE_H265;
#endif
    }

    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->stDemuxerParam.pVideoCodec != NULL) {
#if defined(RV1106_1103) || defined(RV1103B)
      if (strcmp(pstPlayer->stDemuxerParam.pVideoCodec, "h264")) {
        RKADK_LOGE("Unsupported video format(%s)", pstPlayer->stDemuxerParam.pVideoCodec);
        goto __FAILED;
      }
#endif

      if (pstPlayer->stDemuxerParam.videoWidth <= 0 || pstPlayer->stDemuxerParam.videoHeigh <= 0) {
        RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->stDemuxerParam.videoWidth,
                    pstPlayer->stDemuxerParam.videoHeigh);
        goto __FAILED;
      }

      pstPlayer->bVideoExist = RKADK_TRUE;
      pstPlayer->stVdecCtx.srcWidth = pstPlayer->stDemuxerParam.videoWidth;
      pstPlayer->stVdecCtx.srcHeight = pstPlayer->stDemuxerParam.videoHeigh;

      if (pstPlayer->stDemuxerParam.VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
        pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP_10BIT;
      else
        pstPlayer->stVdecCtx.outputPixFmt = RK_FMT_YUV420SP;
    }else {
      RKADK_LOGE("Video codec not find");
      goto __FAILED;
    }
  } else if (suffix && (!strcmp(suffix, ".mp3") || !strcmp(suffix, ".wav"))) {
    if (!pstPlayer->bEnableAudio) {
      RKADK_LOGE("audio is unable");
      goto __FAILED;
    }

    pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
    if (!strcmp(suffix, ".wav"))
      pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerWavPacket;
    else
      pstPlayer->stDemuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;

    if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
      RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
      goto __FAILED;
    }

    if (pstPlayer->stDemuxerParam.pAudioCodec != NULL) {
      if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "mp3"))
        eAudioCodecType = RKADK_CODEC_TYPE_MP3;
      else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "wav"))
        eAudioCodecType = RKADK_CODEC_TYPE_PCM;
        else if (!strcmp(pstPlayer->stDemuxerParam.pAudioCodec, "pcm_alaw"))
          eAudioCodecType = RKADK_CODEC_TYPE_G711A;
      else {
        RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->stDemuxerParam.pAudioCodec);
        goto __FAILED;
      }

      if (pstPlayer->stDemuxerParam.audioFormat == 0) {
        pstPlayer->stAoCtx.bitWidth = 8;
        if (pstPlayer->s32AdecChnCnt > 1) {
          pstPlayer->stAfCtx.s32InputBitWidth = pstPlayer->stAoCtx.bitWidth;
          if (!gMultiPlayerCtx.s32AoBitWidth)
            gMultiPlayerCtx.s32AoBitWidth = pstPlayer->stAoCtx.bitWidth;
        }
      } else if (pstPlayer->stDemuxerParam.audioFormat == 1) {
        pstPlayer->stAoCtx.bitWidth = 16;
        if (pstPlayer->s32AdecChnCnt > 1) {
            pstPlayer->stAfCtx.s32InputBitWidth = pstPlayer->stAoCtx.bitWidth;
            if (!gMultiPlayerCtx.s32AoBitWidth)
              gMultiPlayerCtx.s32AoBitWidth = pstPlayer->stAoCtx.bitWidth;
          }
      }
      else {
        RKADK_LOGE("Unsupported audioFormat = %d", pstPlayer->stDemuxerParam.audioFormat);
        goto __FAILED;
      }

      if (pstPlayer->stDemuxerParam.audioChannels <= 0 ||
          pstPlayer->stDemuxerParam.audioSampleRate <= 0) {
        RKADK_LOGE("Invalid channel(%d) or reSmpSampleRate(%d)",
                    pstPlayer->stDemuxerParam.audioChannels, pstPlayer->stDemuxerParam.audioSampleRate);
        goto __FAILED;
      }

      pstPlayer->bAudioExist = RKADK_TRUE;
      pstPlayer->stAdecCtx.sampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
      pstPlayer->stAdecCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
      pstPlayer->stAdecCtx.eCodecType = eAudioCodecType;

      pstPlayer->stAoCtx.reSmpSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
      pstPlayer->stAoCtx.channel = pstPlayer->stDemuxerParam.audioChannels;
      if (pstPlayer->s32AdecChnCnt > 1) {
          if (!gMultiPlayerCtx.s32AoSampleRate)
            gMultiPlayerCtx.s32AoSampleRate = pstPlayer->stDemuxerParam.audioSampleRate;
          if (!gMultiPlayerCtx.s32AoChannels)
            gMultiPlayerCtx.s32AoChannels = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAoCtx.reSmpSampleRate = gMultiPlayerCtx.s32AoSampleRate;
          pstPlayer->stAoCtx.channel = gMultiPlayerCtx.s32AoChannels;
          pstPlayer->stAfCtx.s32InputRate = pstPlayer->stDemuxerParam.audioSampleRate;
          pstPlayer->stAfCtx.s32InputCh = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAfCtx.s32OutRate = pstPlayer->stDemuxerParam.audioSampleRate;
          pstPlayer->stAfCtx.s32OutCh = pstPlayer->stDemuxerParam.audioChannels;
          pstPlayer->stAfCtx.s32OutputBitWidth = gMultiPlayerCtx.s32AoBitWidth;
          pstPlayer->stAmixCtx.s32InSampleRate = gMultiPlayerCtx.s32AoSampleRate;
          pstPlayer->stAmixCtx.u32InChannel = gMultiPlayerCtx.s32AoChannels;
        }
    } else {
      RKADK_LOGE("Audio codec not find");
    }
  } else {
    RKADK_LOGE("Unsupported file format(%s)", pszfilePath);
    goto __FAILED;
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_INIT;
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_LOGI("SetDataSource[%s] end...", pszfilePath);
  return RKADK_SUCCESS;

__FAILED:
  pstPlayer->bAudioExist = RKADK_FALSE;
  memset(pstPlayer->pFilePath, 0, RKADK_PATH_LEN);
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Prepare(RKADK_MW_PTR pPlayer) {
  int ret;
  RKADK_S32 audioBytes = 0, sampleNum = 0, tmpNUm = 0, cacheBufferLen = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_INIT) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  RKADK_LOGI("Prepare start...");

  if (pstPlayer->bVideoExist) {
    ret = CreateVdec(&pstPlayer->stVdecCtx);
    if (ret) {
      RKADK_LOGE("Create VDEC failed");
      goto __FAILED;
    }
  } else {
    RKADK_LOGW("video stream not exist, bEnableVideo: %d", pstPlayer->bEnableVideo);
  }

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) {
      if (CreateAdec(&pstPlayer->stAdecCtx)) {
        RKADK_LOGE("Create ADEC failed");
        goto __FAILED;
      }
    }

    if (pstPlayer->bVideoExist) {
      audioBytes = pstPlayer->stAoCtx.bitWidth / 8;
      cacheBufferLen = audioBytes * pstPlayer->stDemuxerParam.audioChannels *
        pstPlayer->stDemuxerParam.audioSampleRate / pstPlayer->stDemuxerParam.videoAvgFrameRate;

      // Divisor correction
      sampleNum = cacheBufferLen / (audioBytes * pstPlayer->stDemuxerParam.audioChannels);
      cacheBufferLen = sampleNum * audioBytes * pstPlayer->stDemuxerParam.audioChannels;
      pstPlayer->stAoCtx.periodSize = cacheBufferLen;

      if (pstPlayer->stAoCtx.sampleRate != pstPlayer->stAoCtx.reSmpSampleRate) {
          if (((pstPlayer->stAoCtx.sampleRate > pstPlayer->stAoCtx.reSmpSampleRate) &&
              ((pstPlayer->stAoCtx.sampleRate % pstPlayer->stAoCtx.reSmpSampleRate) ||
              ((pstPlayer->stAoCtx.sampleRate / pstPlayer->stAoCtx.reSmpSampleRate) % 2))) ||
              ((pstPlayer->stAoCtx.sampleRate < pstPlayer->stAoCtx.reSmpSampleRate) &&
              (pstPlayer->stAoCtx.sampleRate % pstPlayer->stAoCtx.reSmpSampleRate ||
              ((pstPlayer->stAoCtx.reSmpSampleRate / pstPlayer->stAoCtx.sampleRate) % 2)))) {
              tmpNUm = (RK_S32)(sampleNum *
                                  ((float)(pstPlayer->stAoCtx.sampleRate) / (float)(pstPlayer->stAoCtx.reSmpSampleRate)) + 1);
          }
      }

      if (tmpNUm)
        pstPlayer->stAoCtx.periodCount = MAX_BUFFER_SZIE / tmpNUm;
    } else {
      if (pstPlayer->stAdecCtx.eCodecType == RKADK_CODEC_TYPE_MP3)
        pstPlayer->stAoCtx.periodSize = 1152 * pstPlayer->stDemuxerParam.audioChannels * pstPlayer->stAoCtx.bitWidth / 8; // MP3
      else
        pstPlayer->stAoCtx.periodSize = 1024 * pstPlayer->stDemuxerParam.audioChannels * pstPlayer->stAoCtx.bitWidth / 8;
    }

    if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_NO) {
      if (pstPlayer->s32AdecChnCnt > 0) {
        pstPlayer->stAmixCtx.s32PipeId = gMultiPlayerCtx.s32PlayerPrepareCnt;
        if (gMultiPlayerCtx.s32PlayerPrepareCnt == 0) {
          pstPlayer->stAoCtx.channel = gMultiPlayerCtx.s32AoChannels;
          ret = CreateDeviceAo(&pstPlayer->stAoCtx);
          if (ret) {
            RKADK_LOGE("Create Amix failed");
            goto __FAILED;
          }
          pstPlayer->stAmixCtx.u32InChannel = gMultiPlayerCtx.s32AoChannels;
          // Compute AMIX period size from AO u32PtNumPerFrm
          gMultiPlayerCtx.s32PtNumPerFrm = pstPlayer->stAoCtx.periodSize;
          pstPlayer->stAmixCtx.s32PeriodSize = pstPlayer->stAoCtx.periodSize / (pstPlayer->stDemuxerParam.audioChannels * pstPlayer->stAoCtx.bitWidth / 8);
          ret = CreateAmix(&pstPlayer->stAmixCtx);
          if (ret) {
            RKADK_LOGE("Create AO failed");
            goto __FAILED;
          }
        }
        pstPlayer->stAfCtx.s32ChnIndex = gMultiPlayerCtx.s32PlayerPrepareCnt;
        pstPlayer->stAfCtx.s32OutRate = gMultiPlayerCtx.s32AoSampleRate;
        pstPlayer->stAfCtx.s32ChnCnt = pstPlayer->s32AdecChnCnt;
        pstPlayer->stAfCtx.s32InputCh = pstPlayer->stDemuxerParam.audioChannels;
        pstPlayer->stAfCtx.s32OutCh = gMultiPlayerCtx.s32AoChannels;
        // Set AF cache size to the same as AO u32PtNumPerFrm
        pstPlayer->stAfCtx.s32CacheSize = gMultiPlayerCtx.s32PtNumPerFrm;
        pstPlayer->stAfCtx.s32InFrameSize = pstPlayer->stAoCtx.periodSize;
        ret = CreateAf(&pstPlayer->stAfCtx);
        if (ret) {
          RKADK_LOGE("Create AF failed");
          goto __FAILED;
        }
        AfBindAmix(pstPlayer);
        if (gMultiPlayerCtx.s32PlayerPrepareCnt == pstPlayer->s32AdecChnCnt - 1) {
          AmixBindAo(pstPlayer);
        }
      } else {
        ret = CreateDeviceAo(&pstPlayer->stAoCtx);
        if (ret) {
          RKADK_LOGE("Create AO failed");
          goto __FAILED;
        }
      }
    }
  }

  gMultiPlayerCtx.s32PlayerPrepareCnt++;
  pstPlayer->enStatus = RKADK_PLAYER_STATE_PREPARED;
  pthread_mutex_unlock(&pstPlayer->mutex);

  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PREPARED, NULL);
  RKADK_LOGI("Prepare end...");
  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->bVideoExist)
    DestroyVdec(&pstPlayer->stVdecCtx);

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM)
      RK_MPI_ADEC_DestroyChn(pstPlayer->stAdecCtx.chnIndex);

    DestoryDeviceAo(&pstPlayer->stAoCtx);
  }

  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_S64 startPts = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PREPARED
      && pstPlayer->enStatus != RKADK_PLAYER_STATE_PAUSE) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  RKADK_LOGI("Play start...");

  pstPlayer->frameCount = 0;
  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT)
    startPts = pstPlayer->seekTimeStamp;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PREPARED) {
    if (!pstPlayer->bEnableThirdDemuxer) {
      ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg, startPts);
      if (ret != 0) {
        RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
        goto __FAILED;
      }
    }

    if ((pstPlayer->bAudioExist && pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) || pstPlayer->bVideoExist) {
#ifndef OS_RTT
      ret = pthread_create(&pstPlayer->tidDataSend, RKADK_NULL, SendDataThread, pPlayer);
      if (ret) {
        RKADK_LOGE("Create send data thread failed [%d]", ret);
        goto __FAILED;
      }
#else
      pstPlayer->tidDataSend = rt_thread_create("playerSend", SendDataThread, pstPlayer, 90112, 16, 10);
      if (pstPlayer->tidDataSend)
          rt_thread_startup(pstPlayer->tidDataSend);
      else {
        RKADK_LOGE("Create send data thread failed [%d]", ret);
        goto __FAILED;
      }
#endif
    }
    AoVolumeControl(pstPlayer);
    AoDebugProcess(pstPlayer);
  } else if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {
    if (pstPlayer->bAudioExist) {
      ret = RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
      if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_AO_ResumeChn failed[%x]", ret);
       goto __FAILED;
      }
    }
    if (pstPlayer->bVideoExist) {
      ret = RK_MPI_VO_ResumeChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
      if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_VO_ResumeChn failed, ret = %X\n", ret);
        goto __FAILED;
      }
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_PLAY;
  pthread_mutex_unlock(&pstPlayer->mutex);

  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PLAY, NULL);
  RKADK_LOGI("Play end...");
  return RKADK_SUCCESS;

__FAILED:
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Stop(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0, ret1 = 0;
  RKADK_PLAYER_STATE_E enStatus = RKADK_PLAYER_STATE_BUTT;
  RKADK_PLAYER_SEEK_STATUS_E enSeekStatus = RKADK_PLAYER_SEEK_NO;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_IDLE
      || pstPlayer->enStatus == RKADK_PLAYER_STATE_INIT) {
    pstPlayer->enStatus = RKADK_PLAYER_STATE_STOP;
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_SUCCESS;
  }

  if (pstPlayer->enStatus < RKADK_PLAYER_STATE_PREPARED
      || pstPlayer->enStatus > RKADK_PLAYER_STATE_PAUSE) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  RKADK_LOGI("Stop start...");

  enStatus = pstPlayer->enStatus;
  pstPlayer->enStatus = RKADK_PLAYER_STATE_STOP;

  if (pstPlayer->enSeekStatus == RKADK_PLAYER_SEEK_WAIT)
    enSeekStatus = pstPlayer->enSeekStatus;

  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
  pstPlayer->seekTimeStamp = 0;

  if (enStatus == RKADK_PLAYER_STATE_PAUSE) {
    if (pstPlayer->bVideoExist) {
        ret = RK_MPI_VO_ResumeChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
        if (ret != RKADK_SUCCESS) {
          RKADK_LOGE("RK_MPI_VO_ResumeChn failed[%x]", ret);
          goto __FAILED;
        }
    }

    if (pstPlayer->bAudioExist) {
      ret = RK_MPI_AO_ResumeChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
      if (ret) {
        RKADK_LOGE("RK_MPI_AO_ResumeChn failed[%x]", ret);
        ret1 |= RKADK_FAILURE;
      }
    }
  }

  if (pstPlayer->bVideoExist) {
    ret = RK_MPI_VO_PauseChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_PauseChn failed[%x]", ret);
      goto __FAILED;
    }

    RK_MPI_VO_ClearChnBuffer(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn, RKADK_TRUE);

    ret = RK_MPI_VO_ResumeChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_ResumeChn failed[%x]", ret);
      goto __FAILED;
    }
  }

  if (pstPlayer->pDemuxerCfg)
    RKADK_DEMUXER_ReadPacketStop(pstPlayer->pDemuxerCfg);
  pstPlayer->bStopSendStream = true;

  if (pstPlayer->tidDataSend) {
#ifndef OS_RTT
    pthread_join(pstPlayer->tidDataSend, RKADK_NULL);
#else
    while (!pstPlayer->bSendEnd)
      rkos_msleep(10);

    pstPlayer->bSendEnd = 0;
#endif
    pstPlayer->tidDataSend = 0;
  }

  if (enSeekStatus != RKADK_PLAYER_SEEK_WAIT)
    pstPlayer->positionTimeStamp = 0;

  pstPlayer->frameCount = 0;
  pstPlayer->stSnapshotParam.bSnapshot = false;
  pstPlayer->stSnapshotParam.stFrame.pMbBlk = NULL;
  pstPlayer->bVideoSendStreamFail = false;
  pstPlayer->videoStreamCount = 0;

  if (pstPlayer->bVideoExist) {
      if (DestroyVdec(&pstPlayer->stVdecCtx))
        ret1 |= RKADK_FAILURE;
  }

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->stAdecCtx.eCodecType != RKADK_CODEC_TYPE_PCM) {
      ret = RK_MPI_ADEC_DestroyChn(pstPlayer->stAdecCtx.chnIndex);
      if (ret) {
        RKADK_LOGE("destory Adec channel failed[%x]", ret);
        ret1 |= RKADK_FAILURE;
      }
    }
    if (enSeekStatus == RKADK_PLAYER_SEEK_NO) {
      if (pstPlayer->s32AdecChnCnt > 0) {
        if (gMultiPlayerCtx.s32PlayerPrepareCnt == pstPlayer->s32AdecChnCnt) {
          AmixUnBindAo(pstPlayer);
        }
        AfUnBindAmix(pstPlayer);
        RKADK_LOGD("gMultiPlayerCtx.s32PlayerPrepareCnt %d", gMultiPlayerCtx.s32PlayerPrepareCnt);
        DestroyAf(&pstPlayer->stAfCtx);
        if (gMultiPlayerCtx.s32PlayerPrepareCnt == 1) {
          DestroyAmix(&pstPlayer->stAmixCtx);
          if (DestoryDeviceAo(&pstPlayer->stAoCtx))
            ret1 |= RKADK_FAILURE;
        }
        gMultiPlayerCtx.s32PlayerPrepareCnt--;
      } else {
        if (DestoryDeviceAo(&pstPlayer->stAoCtx))
          ret1 |= RKADK_FAILURE;
      }
    }
  }

  if (ret1)
    goto __FAILED;

  pstPlayer->enSeekStatus = enSeekStatus;
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);
  RKADK_LOGI("Stop end...");
  return ret1;

__FAILED:
  if (enStatus != RKADK_PLAYER_STATE_BUTT)
    pstPlayer->enStatus = enStatus;

  if (enSeekStatus != RKADK_PLAYER_SEEK_NO)
    pstPlayer->enSeekStatus = enSeekStatus;

  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->mutex);
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_PLAY && pstPlayer->enStatus != RKADK_PLAYER_STATE_EOF) {
    RKADK_LOGW("Err state[%d]", pstPlayer->enStatus);
    pthread_mutex_unlock(&pstPlayer->mutex);
    return RKADK_STATE_ERR;
  }

  RKADK_LOGI("Pause start...");

  if (pstPlayer->bAudioExist) {
    ret = RK_MPI_AO_PauseChn(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.chnIndex);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_AO_PauseChn failed, ret = %X\n", ret);
      goto __FAILED;
    }
  }

  if (pstPlayer->bVideoExist) {
    ret = RK_MPI_VO_PauseChn(pstPlayer->stVoCtx.u32VoLay, pstPlayer->stVoCtx.u32VoChn);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_PauseChn failed, ret = %X\n", ret);
      goto __FAILED;
    }
  }

  pstPlayer->enStatus = RKADK_PLAYER_STATE_PAUSE;
  pstPlayer->frameCount = 0;
  pstPlayer->videoStreamCount = 0;
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_PAUSED, NULL);
  RKADK_LOGI("Pause end...");
  return RKADK_SUCCESS;

__FAILED:
  pthread_mutex_unlock(&pstPlayer->mutex);
  RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_STATE_E enStatus = RKADK_PLAYER_STATE_BUTT;
  RKADK_S64 maxSeekTimeInMs = (RKADK_S64)pow(2, 63) / 1000;
  RKADK_S64 seekDelta;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->bEnableThirdDemuxer) {
    RKADK_LOGE("Enable third-party demuxer, nonsupport seek");
    return -1;
  }

  if (pstPlayer->enSeekStatus != RKADK_PLAYER_SEEK_NO) {
    RKADK_LOGE("Seek operation has not done for last time");
    return RKADK_FAILURE;
  }

  if (pstPlayer->enStatus < RKADK_PLAYER_STATE_PREPARED
      || pstPlayer->enStatus > RKADK_PLAYER_STATE_STOP) {
    RKADK_LOGW("State[%d] err", pstPlayer->enStatus);
    return RKADK_STATE_ERR;
  }

  if (strlen(pstPlayer->pFilePath) <= 0) {
    RKADK_LOGE("Invalid pFilePath[%s] lenght[%d]", pstPlayer->pFilePath, strlen(pstPlayer->pFilePath));
    return RKADK_FAILURE;
  }

  if (strstr(pstPlayer->pFilePath, "rtsp://") || strstr(pstPlayer->pFilePath, "http://")) {
    RKADK_LOGE("Nonsupport network stream seek");
    return RKADK_FAILURE;
  }

  if (pstPlayer->duration > 0 && (pstPlayer->duration < s64TimeInMs)) {
    RKADK_LOGE("Invalid s64TimeInMs[%lld] > duration[%d]", s64TimeInMs, pstPlayer->duration);
    return RKADK_FAILURE;
  }

  if (s64TimeInMs > maxSeekTimeInMs) {
    RKADK_LOGE("Invalid s64TimeInMs[%lld]", s64TimeInMs);
    return RKADK_FAILURE;
  }

  seekDelta = RKADK_ABS(s64TimeInMs - pstPlayer->positionTimeStamp / 1000);
  if (pstPlayer->bVideoExist && seekDelta < 500) {
    RKADK_LOGW("mini seek margin is 500ms, s64TimeInMs: %lld, position: %lld, seekDelta: %lld",
                s64TimeInMs, pstPlayer->positionTimeStamp / 1000, seekDelta);
    RKADK_PLAYER_ProcessEvent(pPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);
    return RKADK_SUCCESS;
  }

  RKADK_LOGI("Seek[%s, %d] start...", pstPlayer->pFilePath, s64TimeInMs);

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE)
    enStatus = RKADK_PLAYER_STATE_PAUSE;

  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_WAIT;
  if (pstPlayer->enStatus != RKADK_PLAYER_STATE_STOP) {
    ret = RKADK_PLAYER_Stop(pPlayer);
    if (ret && ret != RKADK_STATE_ERR) {
      RKADK_LOGE("RKADK_PLAYER_Stop failed");
      goto __FAILED;
    }
  }

  ret = RKADK_PLAYER_SetDataSource(pstPlayer, pstPlayer->pFilePath);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_SetDataSource failed");
    goto __FAILED;
  }

  ret = RKADK_PLAYER_Prepare(pstPlayer);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_Prepare failed");
    goto __FAILED;
  }

  pstPlayer->seekTimeStamp = s64TimeInMs * 1000;
  ret = RKADK_PLAYER_Play(pstPlayer);
  if (ret) {
    RKADK_LOGD("RKADK_PLAYER_Prepare failed");
    goto __FAILED;
  }

  if (enStatus == RKADK_PLAYER_STATE_PAUSE)
    RKADK_PLAYER_Pause(pstPlayer);

  if (pstPlayer->bVideoExist)
    pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_VIDEO_DOING;

  if (pstPlayer->bAudioExist && !pstPlayer->bVideoExist)
      pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_DONE;

  RKADK_LOGI("Seek[%s, %d] end...", pstPlayer->pFilePath, s64TimeInMs);
  return RKADK_SUCCESS;

__FAILED:
  pstPlayer->enSeekStatus = RKADK_PLAYER_SEEK_NO;
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetPlayStatus(RKADK_MW_PTR pPlayer,
                                     RKADK_PLAYER_STATE_E *penState) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(penState, RKADK_FAILURE);

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  *penState = pstPlayer->enStatus;
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_GetSendFrameNum(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);

  return pstPlayer->frameCount;
}

RKADK_S32 RKADK_PLAYER_GetDuration(RKADK_MW_PTR pPlayer, RKADK_U32 *pDuration) {
  RKADK_S32 ret = 0;
  void *demuxerCfg;
  RKADK_DEMUXER_INPUT_S demuxerInput;
  RKADK_DEMUXER_PARAM_S demuxerParam;
  RKADK_S64 vDuration = 0, aDuration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_CHECK_POINTER(pDuration, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);

  if (strlen(pstPlayer->pFilePath) == 0) {
    RKADK_LOGE("Invalid pFilePath[%s] length", pstPlayer->pFilePath);
    return RKADK_FAILURE;
  }

  if (strstr(pstPlayer->pFilePath, "rtsp://") || strstr(pstPlayer->pFilePath, "http://")) {
    RKADK_LOGE("Nonsupport get network stream duration");
    return RKADK_FAILURE;
  }

  demuxerInput.ptr = RK_NULL;
  demuxerInput.readModeFlag = DEMUXER_TYPE_ACTIVE;
  demuxerInput.videoEnableFlag = pstPlayer->bVideoExist;
  demuxerInput.audioEnableFlag = pstPlayer->bAudioExist;
  demuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = NULL;
  demuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = NULL;

  if (RKADK_DEMUXER_Create(&demuxerCfg, &demuxerInput)) {
    RKADK_LOGE("RKADK_DEMUXER_Create failed");
    return RKADK_FAILURE;
  }

  if (RKADK_DEMUXER_GetParam(demuxerCfg, pstPlayer->pFilePath, &demuxerParam)) {
    RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
    goto __FAILED;
  }

  if (!pstPlayer->bVideoExist && !pstPlayer->bAudioExist) {
    RKADK_LOGE("get duration failed, bAudioExist: %d, bVideoExist: %d", pstPlayer->bAudioExist, pstPlayer->bVideoExist);
    goto __FAILED;
  }

  if (pstPlayer->bAudioExist) {
    ret = RKADK_DEMUXER_ReadAudioDuration(demuxerCfg, &aDuration);
    if (ret)
      RKADK_LOGE("get audio duration failed");
    else
      aDuration = aDuration / 1000;
  }

  if (pstPlayer->bVideoExist) {
    ret = RKADK_DEMUXER_ReadVideoDuration(demuxerCfg, &vDuration);
    if (ret)
      RKADK_LOGE("get video duration failed");
    else
      vDuration = vDuration / 1000;
  }

  if (!aDuration && !vDuration)
    goto __FAILED;

  *pDuration = fmax(aDuration, vDuration);
  pstPlayer->duration = *pDuration;
  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_SUCCESS;

__FAILED:
  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_FAILURE;
}

RKADK_S64 RKADK_PLAYER_GetCurrentPosition(RKADK_MW_PTR pPlayer) {
  RKADK_S64 duration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->enStatus == RKADK_PLAYER_STATE_PLAY
      || pstPlayer->enStatus == RKADK_PLAYER_STATE_PAUSE) {
    duration = pstPlayer->positionTimeStamp / 1000;
    return duration;
  } else {
    return RKADK_FAILURE;
  }
}

RKADK_S32 RKADK_PLAYER_Snapshot(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (!pstPlayer->bVideoExist) {
    RKADK_LOGW("%s no video track", pstPlayer->pFilePath);
    return -1;
  }

  if (!pstPlayer->stSnapshotParam.bSnapshot) {
    pstPlayer->stSnapshotParam.bSnapshot = true;
    RKADK_SIGNAL_Give(pstPlayer->stSnapshotParam.pSignal);
  } else {
    RKADK_LOGD("Have been in snapshot");
  }

  return 0;
}

RKADK_S32 RKADK_PLAYER_SetVdecWaterline(RKADK_MW_PTR pPlayer, RKADK_U32 u32VdecWaterline) {
  RKADK_PLAYER_HANDLE_S *pstPlayer;
  RKADK_U32 u32VdecTotalLeftFrames = 0;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  u32VdecTotalLeftFrames = pstPlayer->stVdecCtx.frameBufferCnt + pstPlayer->stVdecCtx.streamBufferCnt;
  if (u32VdecWaterline > u32VdecTotalLeftFrames) {
    RKADK_LOGE("Invalid vdec waterline: %d, frameBufferCnt: %d, streamBufferCnt: %d",
      u32VdecWaterline, pstPlayer->stVdecCtx.frameBufferCnt, pstPlayer->stVdecCtx.streamBufferCnt);
    return -1;
  }

  pstPlayer->u32VdecWaterline = u32VdecWaterline;
  return 0;
}

RKADK_S32 RKADK_PLAYER_SetAoVolume(RKADK_MW_PTR pPlayer, RKADK_S32 s32Volume) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  if (s32Volume < 0) {
    RKADK_LOGE("Invalid volume: %d", s32Volume);
    return -1;
  }

  pstPlayer->stAoCtx.u32SpeakerVolume = s32Volume;
  if (pstPlayer->bEnableAudio)
    RK_MPI_AO_SetVolume(pstPlayer->stAoCtx.devId, pstPlayer->stAoCtx.u32SpeakerVolume);
  return 0;
}

RKADK_S32 RKADK_PLAYER_SetFadeInFrames(RKADK_MW_PTR pPlayer, RKADK_U32 u32FadeInFrames) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pstPlayer->stFadeInParam.u32FadeInFrames = u32FadeInFrames;
  return 0;
}

RKADK_S32 RKADK_PLAYER_SmoothPlayEnable(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pstPlayer->stSmoothPlayParam.bEnableSmoothPlay = true;
  if (!pstPlayer->stSmoothPlayParam.pSignal) {
    pstPlayer->stSmoothPlayParam.pSignal = RKADK_SIGNAL_Create(0, 1);
    if (!pstPlayer->stSmoothPlayParam.pSignal) {
      RKADK_LOGE("Create smooth loop signal failed");
      return -1;
    }
  }

  return 0;
}

RKADK_S32 RKADK_PLAYER_SmoothPlayDisable(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pstPlayer->stSmoothPlayParam.bEnableSmoothPlay = false;
  if (pstPlayer->stSmoothPlayParam.pSignal) {
    RKADK_SIGNAL_Destroy(pstPlayer->stSmoothPlayParam.pSignal);
    pstPlayer->stSmoothPlayParam.pSignal = RKADK_NULL;
  }

  return 0;
}

RKADK_S32 RKADK_PLAYER_SmoothPlay(RKADK_MW_PTR pPlayer, const RKADK_CHAR *pszfilePath) {
  RKADK_S32 ret = 0;
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (!pstPlayer->stSmoothPlayParam.bEnableSmoothPlay) {
    RKADK_LOGI("SmoothPlay is disable");
    return ret;
  }

  if ((strlen(pszfilePath) <= 0) || (strlen(pszfilePath) >= RKADK_PATH_LEN)) {
    RKADK_LOGE("Invalid pszfilePath[%s] lenght[%d]", pszfilePath, strlen(pszfilePath));
    return -1;
  }

  RKADK_SIGNAL_Wait(pstPlayer->stSmoothPlayParam.pSignal, -1);

  if (pstPlayer->pDemuxerCfg)
    ret = RKADK_DEMUXER_ReadPacketStop(pstPlayer->pDemuxerCfg);

  pstPlayer->positionTimeStamp = 0;

  if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, &pstPlayer->stDemuxerParam)) {
    RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
  }

  ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg, 0);
  if (ret != 0) {
    RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
  }

  return ret;
}
