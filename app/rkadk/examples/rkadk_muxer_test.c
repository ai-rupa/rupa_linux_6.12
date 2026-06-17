/*
 * Copyright (c) 2025 Rockchip, Inc. All Rights Reserved.
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

#include "rk_comm_video.h"
#include "rk_mpi_aenc.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vi.h"

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_muxer.h"
#include "rkadk_media_comm.h"
#include "rkadk_audio_encoder.h"
#include "rkadk_signal.h"

#include "isp/sample_isp.h"
#include "isp/sample_iio_aiq.h"

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MUXER_TEST_DEBUG

extern int optind;
extern char *optarg;

static bool is_quit = false;
#define IQ_FILE_PATH "/etc/iqfiles"

static void *pIData = NULL;
static void *pPData = NULL;
static size_t ISize = 0;
static size_t PSize = 0;

typedef struct {
  RKADK_U32 u32ViChn;
  RKADK_U32 u32VencChn;
} MUXER_TEST_VENC_CHN_S;

typedef struct {
  RKADK_MW_PTR pHandle;
  RKADK_U32 u32MuxerId;
  RKADK_U32 u32VencChn;
  pthread_t videoTid;
  int WriteCnt;
} MUXER_TEST_VIDEO_THREAD_CTX_S;

typedef struct {
  RKADK_MW_PTR pHandle;
  RKADK_U32 u32MuxerId;
  RKADK_U32 u32VencChn;
  pthread_t tid;
  void *pSignal;
  bool bRequestThumb;
} MUXER_TEST_THUMB_THREAD_CTX_S;

typedef struct {
  bool bGetBuffer;
  RKADK_U32 u32ViChn;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  MUXER_TEST_THUMB_THREAD_CTX_S thumbThreads[RKADK_MUXER_STREAM_MAX_CNT];
} MUXER_TEST_THUMB_CTX_S;

typedef struct {
  bool bThirdPartyStream;
  RKADK_U32 u32StreamCnt;
  RKADK_U32 u32PreRecTimeSec;

  RKADK_U32 u32CamId;
  RKADK_U32 u32AiChn;
  RKADK_U32 u32AencChn;
  MUXER_TEST_VENC_CHN_S videoChns[RKADK_MUXER_STREAM_MAX_CNT];

  bool bGetBuffer;
  pthread_t audioTid;
  MUXER_TEST_VIDEO_THREAD_CTX_S vencThreads[RKADK_MUXER_STREAM_MAX_CNT];

  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U32 u32FrameRate;
  RKADK_U32 u32Bitrate;
  RKADK_U32 u32FragKeyFrame;
  RKADK_CODEC_TYPE_E enCodecType;

  bool bEnableAudio;
  char* audioNode;
  AUDIO_BIT_WIDTH_E enBitwidth;
  int channels; // 1: mono, 2: stereo
  int samplerate;
  int samplesPerFrame;
  int audioBitrate;

  int number;
  RKADK_MUXER_ATTR_S stMuxerAttr;
  MUXER_TEST_THUMB_CTX_S stThumbCtx;
} MUXER_TEST_CTX_S;

static MUXER_TEST_CTX_S g_ctx;

#ifdef MUXER_TEST_DEBUG
static int g_u32VideoMallocCnt;
static int g_u32AudioMallocCnt;
static int g_u32BufFreeCnt;
#endif

static RKADK_CHAR optstr[] = "a:W:H:f:d:c:A:t:p:I:P:C:n:kh";

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-a /etc/iqfiles]\n", name);
  printf("\t-a: enable aiq with dirpath provided, eg:-a "
         "/oem/etc/iqfiles/, Default /etc/iqfiles,"
         "without this option aiq should run in other application\n");
  printf("\t-W: width, Default: 1920\n");
  printf("\t-H: height, Default: 1080\n");
  printf("\t-f: framerate, Default: 30\n");
  printf("\t-C: codec type, Default: 0(h264), options: 0(h264), 1(h265)\n");
  printf("\t-d: audio device node, Default: hw:0,0\n");
  printf("\t-k: key frame fragment, Default: disable\n");
  printf("\t-c: stream count, Default: 1, options: 1(main stream), 2(main + sub stream)\n");
  printf("\t-t: process third-party stream, Default: 1(enable), options: 0(disable), 1(enable)\n");
  printf("\t-A: enable audio, Default: 1(enable), options: 0(disable), 1(enable)\n");
  printf("\t-p: pre-record time(s), Default: 0\n");
  printf("\t-I: third-party I frame file, Default: null\n");
  printf("\t-P: third-party P frame file, Default: null\n");
  printf("\t-n: write stream number, Default: -1\n");
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

static void MuxerCtxInit() {
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  memset(pCtx, 0, sizeof(MUXER_TEST_CTX_S));

  pCtx->bThirdPartyStream = true;
  pCtx->u32StreamCnt = 1;
  pCtx->u32CamId = 0;
  pCtx->u32AiChn = 0;
  pCtx->u32AencChn = 0;
  for (int i = 0; i < RKADK_MUXER_STREAM_MAX_CNT; i++) {
    pCtx->videoChns[i].u32ViChn = i;
    pCtx->videoChns[i].u32VencChn = i;
  }

  pCtx->u32Width = 1920;
  pCtx->u32Height = 1080;
  pCtx->u32FrameRate = 30;
  pCtx->u32Bitrate =  4 * 1024 * 1024;

  pCtx->bEnableAudio = true;
  pCtx->audioNode = "hw:0,0";
  pCtx->enBitwidth = AUDIO_BIT_WIDTH_16;
  pCtx->channels = 1; // 1: mono, 2: stereo
  pCtx->samplerate = 16000;
  pCtx->samplesPerFrame = 576;
  pCtx->audioBitrate = 160000;

  pCtx->stThumbCtx.u32Width = 256;
  pCtx->stThumbCtx.u32Height = 176;
  pCtx->stThumbCtx.u32ViChn = RKADK_MUXER_STREAM_MAX_CNT;
  for (int i = 0; i < RKADK_MUXER_STREAM_MAX_CNT; i++) {
    pCtx->stThumbCtx.thumbThreads[i].u32VencChn = RKADK_MUXER_STREAM_MAX_CNT + i;
  }

  pCtx->number = -1;
}

RKADK_U32 MuxerGetStreamBufCnt(bool bIsAudio) {
  RKADK_U32 u32Gop;
  RKADK_U32 u32Integer = 0, u32Remainder = 0;
  RKADK_U32 u32PreRecCacheTime = 0;
  RKADK_U32 u32BufCount = RKADK_MUXER_CELL_MAX_CNT;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  if(pCtx->u32PreRecTimeSec == 0)
    return u32BufCount;

  u32Gop = pCtx->u32FrameRate;

  u32Integer = u32Gop / pCtx->u32FrameRate;
  u32Remainder = u32Gop % pCtx->u32FrameRate;
  u32PreRecCacheTime = pCtx->u32PreRecTimeSec + u32Integer;
  if (u32Remainder)
    u32PreRecCacheTime += 1;

  if (bIsAudio)
    u32BufCount = pCtx->channels * (pCtx->samplerate / pCtx->samplesPerFrame) * (u32PreRecCacheTime + 2);
  else
    u32BufCount = (u32PreRecCacheTime + 2) * pCtx->u32FrameRate;

  return u32BufCount;
}

static RKADK_S32 MuxerViInit(RKADK_U32 u32DevId, RKADK_S32 s32ViChnId, VI_CHN_ATTR_S *pstViChnAttr) {
  int ret = 0;
  VI_DEV_ATTR_S stDevAttr;
  VI_DEV_BIND_PIPE_S stBindPipe;

  RKADK_CHECK_POINTER(pstViChnAttr, RKADK_FAILURE);
  memset(&stDevAttr, 0, sizeof(stDevAttr));
  memset(&stBindPipe, 0, sizeof(stBindPipe));

  if (u32DevId >= VI_MAX_PHY_PIPE_NUM) {
    RKADK_LOGD("u32DevId[%d] don't need enable device, VI_MAX_PHY_PIPE_NUM[%d]", u32DevId, VI_MAX_PHY_PIPE_NUM);
    return 0;
  }

  // 0. get dev config status
  ret = RK_MPI_VI_GetDevAttr(u32DevId, &stDevAttr);
  if (ret == RK_ERR_VI_NOT_CONFIG) {
    // 0-1.config dev
    ret = RK_MPI_VI_SetDevAttr(u32DevId, &stDevAttr);
    if (ret) {
      RKADK_LOGE("RK_MPI_VI_SetDevAttr[%d] failed[%x]", u32DevId, ret);
      return ret;
    }
  } else {
    RKADK_LOGI("RK_MPI_VI_SetDevAttr[%d] already", u32DevId);
  }

  // 1.get dev enable status
  ret = RK_MPI_VI_GetDevIsEnable(u32DevId);
  if (ret) {
    // 1-2.enable dev
    ret = RK_MPI_VI_EnableDev(u32DevId);
    if (ret) {
      RKADK_LOGE("RK_MPI_VI_EnableDev[%d] failed[%x]", u32DevId, ret);
      return ret;
    }

    // 1-3.bind dev/pipe
    stBindPipe.u32Num = 1;
    stBindPipe.PipeId[0] = u32DevId;
    ret = RK_MPI_VI_SetDevBindPipe(u32DevId, &stBindPipe);
    if (ret) {
      RKADK_LOGE("RK_MPI_VI_SetDevBindPipe[%d] failed[%x]", u32DevId, ret);
      RK_MPI_VI_DisableDev(u32DevId);
      return ret;
    }

#if defined(RV1126B) || defined(RK3572)
    VI_PARAM_MOD_S stModParam;
    memset(&stModParam, 0, sizeof(stModParam));
    stModParam.enViModType = VI_EXT_CHN_MODE;
    stModParam.stExtChnParam.mirrorCmsc = 0; // 1 is for vpss offline, 0 is for vpss online(vi ext)
    for (int i = 0; i < VI_MAX_EXT_CHN_NUM; i++) {
      stModParam.stExtChnParam.extChn[i] = 0;
    }
    ret = RK_MPI_VI_SetModParam(&stModParam);
    if (ret) {
      RKADK_LOGE("RK_MPI_VI_SetModParam[%d] failed[%x]", u32DevId, ret);
      RK_MPI_VI_DisableDev(u32DevId);
      return ret;
    }

    memset(&stModParam, 0, sizeof(stModParam));
    stModParam.enViModType = VI_EXT_CHN_MODE;
    ret = RK_MPI_VI_GetModParam(&stModParam);
    if (ret)
      RKADK_LOGE("RK_MPI_VI_GetModParam[%d] failed[%x]", u32DevId, ret);

    RKADK_LOGP("vi[%d] mod:%d mirror:%d ext_chn_mode:%d ext_chn1_mode:%d, "
            "ext_chn2_mode:%d, ext_chn3_mode:%d, ext_chn4_mode:%d, ext_chn5_mode:%d",
            u32DevId, stModParam.enViModType, stModParam.stExtChnParam.mirrorCmsc,
            stModParam.stExtChnParam.extChn[0], stModParam.stExtChnParam.extChn[1],
            stModParam.stExtChnParam.extChn[2], stModParam.stExtChnParam.extChn[3],
            stModParam.stExtChnParam.extChn[4], stModParam.stExtChnParam.extChn[5]);
#endif
  } else {
    RKADK_LOGI("RK_MPI_VI_EnableDev[%d] already", u32DevId);
  }

  //enable vi chn
  ret = RK_MPI_VI_SetChnAttr(u32DevId, s32ViChnId, pstViChnAttr);
  if (ret) {
    RKADK_LOGE("Set VI[%d, %d] attribute failed[%x]", u32DevId, s32ViChnId, ret);
    RK_MPI_VI_DisableDev(u32DevId);
    return ret;
  }

  ret = RK_MPI_VI_EnableChn(u32DevId, s32ViChnId);
  if (ret) {
    RKADK_LOGE("Create VI[%d, %d] failed[%x]", u32DevId, s32ViChnId, ret);
    RK_MPI_VI_DisableDev(u32DevId);
    return ret;
  }

  return 0;
}

static RKADK_S32 MuxerViDeInit(RKADK_U32 u32DevId, RKADK_U32 u32ViChnId) {
  int ret;

  if (u32DevId >= VI_MAX_PHY_PIPE_NUM) {
    RKADK_LOGD("u32DevId[%d] don't need disable device, VI_MAX_PHY_PIPE_NUM[%d]", u32DevId, VI_MAX_PHY_PIPE_NUM);
    return 0;
  }

  ret = RK_MPI_VI_DisableChn(u32DevId, u32ViChnId);
  if (ret)
    RKADK_LOGE("Destory VI[%d, %d] failed[%x]", u32DevId, u32ViChnId, ret);

  return RK_MPI_VI_DisableDev(u32DevId);
}

static int VideoInit(RKADK_U32 u32CamId, RKADK_U32 u32ViChn, RKADK_U32 u32VencChn) {
  int ret;
  VI_CHN_ATTR_S stViChnAttr;
  VENC_CHN_ATTR_S stVencChnAttr;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  //init vi
  memset(&stViChnAttr, 0, sizeof(VI_CHN_ATTR_S));
  stViChnAttr.stIspOpt.stMaxSize.u32Width = pCtx->u32Width;
  stViChnAttr.stIspOpt.stMaxSize.u32Height = pCtx->u32Height;
  stViChnAttr.stIspOpt.u32BufCount = 3;
  stViChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
  stViChnAttr.stSize.u32Width = pCtx->u32Width;
  stViChnAttr.stSize.u32Height = pCtx->u32Height;
  stViChnAttr.enPixelFormat = RK_FMT_YUV420SP;
  stViChnAttr.enCompressMode = COMPRESS_MODE_NONE;
  stViChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stViChnAttr.stFrameRate.s32DstFrameRate = -1;
  ret = MuxerViInit(u32CamId, u32ViChn, &stViChnAttr);
  if (ret) {
    RKADK_LOGE("VI[%d, %d] init failed, ret[%x]", u32CamId, u32ViChn, ret);
    return ret;
  }
  RKADK_LOGP("VI[%d, %d] init ok", u32CamId, u32ViChn);

  //init venc
  memset(&stVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
  stVencChnAttr.stVencAttr.enType = RKADK_MEDIA_GetRkCodecType(pCtx->enCodecType);
  stVencChnAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
  stVencChnAttr.stVencAttr.u32MaxPicWidth = pCtx->u32Width;
  stVencChnAttr.stVencAttr.u32MaxPicHeight = pCtx->u32Height;
  stVencChnAttr.stVencAttr.u32PicWidth = pCtx->u32Width;
  stVencChnAttr.stVencAttr.u32PicHeight = pCtx->u32Height;
  stVencChnAttr.stVencAttr.u32VirWidth = pCtx->u32Width;
  stVencChnAttr.stVencAttr.u32VirHeight = pCtx->u32Height;
  stVencChnAttr.stVencAttr.u32Profile = 100;
  stVencChnAttr.stVencAttr.u32StreamBufCnt = MuxerGetStreamBufCnt(false);
  stVencChnAttr.stVencAttr.u32BufSize = pCtx->u32Width * pCtx->u32Height * 2;
  stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;

  if (stVencChnAttr.stVencAttr.enType == RK_VIDEO_ID_AVC) {
    stVencChnAttr.stRcAttr.stH264Vbr.u32Gop = pCtx->u32FrameRate;
    stVencChnAttr.stRcAttr.stH264Vbr.u32BitRate = pCtx->u32Bitrate / 1000; //4M
    stVencChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRateDen = 1;
    stVencChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRateNum = pCtx->u32FrameRate;
    stVencChnAttr.stRcAttr.stH264Vbr.u32SrcFrameRateDen = 1;
    stVencChnAttr.stRcAttr.stH264Vbr.u32SrcFrameRateNum = pCtx->u32FrameRate;
  } else if (stVencChnAttr.stVencAttr.enType == RK_VIDEO_ID_HEVC) {
    stVencChnAttr.stRcAttr.stH265Vbr.u32Gop = pCtx->u32FrameRate;
    stVencChnAttr.stRcAttr.stH265Vbr.u32BitRate = pCtx->u32Bitrate / 1000; //4M
    stVencChnAttr.stRcAttr.stH265Vbr.fr32DstFrameRateDen = 1;
    stVencChnAttr.stRcAttr.stH265Vbr.fr32DstFrameRateNum = pCtx->u32FrameRate;
    stVencChnAttr.stRcAttr.stH265Vbr.u32SrcFrameRateDen = 1;
    stVencChnAttr.stRcAttr.stH265Vbr.u32SrcFrameRateNum = pCtx->u32FrameRate;
  } else {
    RKADK_LOGE("Codec type[%d] not support", stVencChnAttr.stVencAttr.enType);
    return -1;
  }

  ret = RK_MPI_VENC_CreateChn(u32VencChn, &stVencChnAttr);
  if (ret) {
    RKADK_LOGE("Create VENC[%d] failed[%x]", u32VencChn, ret);
    MuxerViDeInit(u32CamId, u32ViChn);
    return ret;
  }
  RKADK_LOGP("Create VENC[%d] ok", u32VencChn);

  return 0;
}

static int VideoDeInit(RKADK_U32 u32CamId, RKADK_U32 u32ViChn, RKADK_U32 u32VencChn) {
  int ret = 0;

  ret = RK_MPI_VENC_DestroyChn(u32VencChn);
  if (ret)
    RKADK_LOGE("Dstroy VENC[%d] failed[%x]", u32VencChn, ret);

  ret = MuxerViDeInit(u32CamId, u32ViChn);
  if (ret)
    RKADK_LOGE("MuxerViDeInit[%d, %d] failed, ret[%x]", u32CamId, u32ViChn, ret);

  return ret;
}

static RKADK_S32 MuxerAiInit(AUDIO_DEV aiDevId, RKADK_S32 s32AiChnId,
                            AIO_ATTR_S *pstAiAttr, RKADK_U32 micType) {
  int ret = 0;
  RKADK_U32 s32SetTrackMode = 0;
  AI_CHN_PARAM_S pstParams;

  RKADK_CHECK_POINTER(pstAiAttr, RKADK_FAILURE);

  memset(&pstParams, 0, sizeof(AI_CHN_PARAM_S));
  pstParams.enLoopbackMode = AUDIO_LOOPBACK_NONE;
  pstParams.s32UsrFrmDepth = 1;

  ret = RK_MPI_AI_SetPubAttr(aiDevId, pstAiAttr);
  if (ret != 0) {
    RKADK_LOGE("AI[%d] set attr failed[%x]", aiDevId, ret);
    return ret;
  }

  ret = RK_MPI_AI_Enable(aiDevId);
  if (ret != 0) {
    RKADK_LOGE("AI[%d] enable failed[%x]", aiDevId, ret);
    return ret;
  }

  ret = RK_MPI_AI_SetChnParam(aiDevId, s32AiChnId, &pstParams);
  if (ret != 0) {
    RKADK_LOGE("AI[%d, %d] enable chn param failed[%x]", aiDevId, s32AiChnId, ret);
    RK_MPI_AI_Disable(aiDevId);
    return ret;
  }

  if (pstAiAttr->u32ChnCnt == 2) {
    if (micType == RKADK_MIC_TYPE_LEFT) {
      s32SetTrackMode = AUDIO_TRACK_BOTH_LEFT;
    } else if (micType == RKADK_MIC_TYPE_RIGHT) {
      s32SetTrackMode = AUDIO_TRACK_BOTH_RIGHT;
    } else if (micType == RKADK_MIC_TYPE_BOTH) {
      s32SetTrackMode = AUDIO_TRACK_NORMAL;
    } else {
      RKADK_LOGE("AI channel[%d] mic type[%d] not support", pstAiAttr->u32ChnCnt, micType);
    }
  } else if (pstAiAttr->u32ChnCnt == 1) {
    if (micType == RKADK_MIC_TYPE_LEFT) {
      s32SetTrackMode = AUDIO_TRACK_FRONT_LEFT;
    } else if (micType == RKADK_MIC_TYPE_RIGHT) {
      s32SetTrackMode = AUDIO_TRACK_FRONT_RIGHT;
    } else {
      RKADK_LOGE("AI channel[%d] mic type[%d] not support", pstAiAttr->u32ChnCnt, micType);
    }
  } else {
    RKADK_LOGE("AI channel[%d] mic type[%d] not support", pstAiAttr->u32ChnCnt, micType);
  }

  ret = RK_MPI_AI_SetTrackMode(aiDevId, (AUDIO_TRACK_MODE_E)s32SetTrackMode);
  if (ret) {
    RKADK_LOGE("AI[%d, %d] mic type[%d] enable failed[%x]", aiDevId, s32AiChnId, micType, ret);
  }

  ret = RK_MPI_AI_EnableChn(aiDevId, s32AiChnId);
  if (ret) {
    RKADK_LOGE("AI[%d, %d] enable chn failed[%x]", aiDevId, s32AiChnId, ret);
    RK_MPI_AI_Disable(aiDevId);
    return ret;
  }

  ret = RK_MPI_AI_SetVolume(aiDevId, 100);
  if (ret) {
    RKADK_LOGE("AI[%d, %d] set volume failed[%x]", aiDevId, s32AiChnId, ret);
  }

  return 0;
}

static int MuxerAiDeInit(AUDIO_DEV aiDevId, RKADK_S32 s32AiChnId) {
  int ret;

  ret = RK_MPI_AI_DisableChn(aiDevId, s32AiChnId);
  if (ret) 
    RKADK_LOGE("AI[%d, %d] disable chn failed[%x]", aiDevId, s32AiChnId, ret);

  ret = RK_MPI_AI_Disable(aiDevId);
  if (ret != 0) {
    RKADK_LOGE("Ai[%d] disable failed[%x]", aiDevId, ret);
    return ret;
  }

  return 0;
}

static int AidioInit(RKADK_U32 u32AiChn, RKADK_U32 u32AencChn) {
  int ret = 0;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  int bytes = 2; // if the requirement is 16bit
  AUDIO_SOUND_MODE_E soundMode;
  AIO_ATTR_S stAiAttr;
  AENC_CHN_ATTR_S stAencAttr;

  if (!pCtx->bEnableAudio)
    return 0;

  if (RKADK_MEDIA_EnableAencRegister(RKADK_CODEC_TYPE_MP3)) {
    ret = RKADK_AUDIO_ENCODER_Register(RKADK_CODEC_TYPE_MP3);
    if (ret) {
      RKADK_LOGE("RKADK_AUDIO_ENCODER_Register failed(%d)", ret);
      return ret;
    }
  }

  // Create AI
  memset(&stAiAttr, 0, sizeof(AIO_ATTR_S));
  memcpy(stAiAttr.u8CardName, pCtx->audioNode, strlen(pCtx->audioNode));
  stAiAttr.soundCard.channels = 2;
  stAiAttr.soundCard.sampleRate = pCtx->samplerate;
  bytes = RKADK_MEDIA_GetAudioBitWidth(pCtx->enBitwidth) / 8;
  stAiAttr.soundCard.bitWidth = pCtx->enBitwidth;

  stAiAttr.enBitwidth = pCtx->enBitwidth;
  stAiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)pCtx->samplerate;
  soundMode = RKADK_AI_GetSoundMode(pCtx->channels);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    return -1;
}

  stAiAttr.enSoundmode = soundMode;
  stAiAttr.u32FrmNum = 2;
  stAiAttr.u32PtNumPerFrm = bytes * pCtx->samplesPerFrame * pCtx->channels;
  stAiAttr.u32EXFlag = 0;
  stAiAttr.u32ChnCnt = pCtx->channels;
  ret = MuxerAiInit(0, u32AiChn, &stAiAttr, RKADK_MIC_TYPE_LEFT);
  if (ret) {
    RKADK_LOGE("MuxerAiInit failed(%d)", ret);
    RKADK_AUDIO_ENCODER_UnRegister(RKADK_CODEC_TYPE_MP3);
    return ret;
  }
  RKADK_LOGP("AI[%d] init ok", u32AiChn);

  // Create AENC
  memset(&stAencAttr, 0, sizeof(AENC_CHN_ATTR_S));
  stAencAttr.enType = RK_AUDIO_ID_MP3;
  stAencAttr.u32BufCount = MuxerGetStreamBufCnt(true);
  stAencAttr.stCodecAttr.enType = stAencAttr.enType;
  stAencAttr.stCodecAttr.u32Channels = pCtx->channels;
  stAencAttr.stCodecAttr.u32SampleRate = pCtx->samplerate;
  stAencAttr.stCodecAttr.enBitwidth = pCtx->enBitwidth;
  stAencAttr.stCodecAttr.pstResv = RK_NULL;
  stAencAttr.stCodecAttr.u32Resv[0] = pCtx->samplesPerFrame;
  stAencAttr.stCodecAttr.u32Resv[1] = pCtx->audioBitrate;
  ret = RK_MPI_AENC_CreateChn(u32AencChn, &stAencAttr);
  if (ret) {
    RKADK_LOGE("Create AENC[%d] failed[%x]", u32AencChn, ret);
    MuxerAiDeInit(0, u32AiChn);
    RKADK_AUDIO_ENCODER_UnRegister(RKADK_CODEC_TYPE_MP3);
    return ret;
  }
  RKADK_LOGP("AENC[%d] init ok", u32AencChn);

  return 0;
}

static int AidioDeInit(RKADK_U32 u32AiChn, RKADK_U32 u32AencChn) {
  int ret = 0;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  if (!pCtx->bEnableAudio)
    return 0;

  ret = RK_MPI_AENC_DestroyChn(u32AencChn);
  if (ret) {
    RKADK_LOGE("RK_MPI_AENC_DestroyChn failed(%d)", ret);
  }

  ret = MuxerAiDeInit(0, u32AiChn);
  if (ret) {
    RKADK_LOGE("MuxerAiDeInit failed(%d)", ret);
  }

  if (RKADK_MEDIA_EnableAencRegister(RKADK_CODEC_TYPE_MP3)) {
    ret = RKADK_AUDIO_ENCODER_UnRegister(RKADK_CODEC_TYPE_MP3);
    if (ret) {
      RKADK_LOGE("RKADK_AUDIO_ENCODER_UnRegister failed(%d)", ret);
    }
  }

  return ret;
}

static void *MuxerGetThumbMb(void *params) {
  int ret;
  VENC_STREAM_S stFrame;
  VENC_PACK_S stPack;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  RKADK_MUXER_THUMB_INFO_S stThumbInfo;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MUXER_TEST_THUMB_THREAD_CTX_S *pstThumbThreadCtx = (MUXER_TEST_THUMB_THREAD_CTX_S *)params;

  RKADK_CHECK_POINTER(pstThumbThreadCtx, NULL);

  memset(&stRecvParam, 0, sizeof(stRecvParam));
  memset(&stFrame, 0, sizeof(stFrame));
  memset(&stPack, 0, sizeof(stPack));
  stFrame.pstPack = &stPack;

  // drop first frame
  ret = RK_MPI_VENC_GetStream(pstThumbThreadCtx->u32VencChn, &stFrame, 1);
  if (ret == RK_SUCCESS) {
    RK_MPI_VENC_ReleaseStream(pstThumbThreadCtx->u32VencChn, &stFrame);
  } else {
    RKADK_LOGE("RK_MPI_VENC_GetStream[%d] timeout[%x]", pstThumbThreadCtx->u32VencChn, ret);
  }
  RK_MPI_VENC_StopRecvFrame(pstThumbThreadCtx->u32VencChn);
  RK_MPI_VENC_ResetChn(pstThumbThreadCtx->u32VencChn);

  while(pCtx->stThumbCtx.bGetBuffer) {
    RKADK_SIGNAL_Wait(pstThumbThreadCtx->pSignal, -1);
    if (!pCtx->stThumbCtx.bGetBuffer)
      break;

    RKADK_LOGP("u32MuxerId[%d] exit signal wait", pstThumbThreadCtx->u32MuxerId);

    if (pstThumbThreadCtx->bRequestThumb) {
      memset(&stRecvParam, 0, sizeof(stRecvParam));
      stRecvParam.s32RecvPicNum = -1;
      ret = RK_MPI_VENC_StartRecvFrame(pstThumbThreadCtx->u32VencChn, &stRecvParam);
      if (ret)
        RKADK_LOGE("RK_MPI_VENC_StartRecvFrame[%d] failed[%x]", pstThumbThreadCtx->u32VencChn, ret);
      else
        RKADK_LOGP("RK_MPI_VENC_StartRecvFrame[%d] ok", pstThumbThreadCtx->u32VencChn);

      pstThumbThreadCtx->bRequestThumb = false;
      continue;
    }

    ret = RK_MPI_VENC_GetStream(pstThumbThreadCtx->u32VencChn, &stFrame, 1000);
    if (ret == RK_SUCCESS) {
      memset(&stThumbInfo, 0, sizeof(RKADK_MUXER_THUMB_INFO_S));
      stThumbInfo.s32MuxerId = pstThumbThreadCtx->u32MuxerId;
      stThumbInfo.pBuf = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
      stThumbInfo.u32Len = stFrame.pstPack->u32Len;
      ret = RKADK_MUXER_BuildInThumb(pstThumbThreadCtx->pHandle, stThumbInfo);
      if (ret)
        RKADK_LOGE("RKADK_MUXER_BuildInThumb[%d] failed[%x]", pstThumbThreadCtx->u32MuxerId, ret);
      else
        RKADK_LOGP("RKADK_MUXER_BuildInThumb[%d] ok", pstThumbThreadCtx->u32MuxerId);

      ret = RK_MPI_VENC_ReleaseStream(pstThumbThreadCtx->u32VencChn, &stFrame);
      if (ret != RK_SUCCESS)
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream failed[%x]", ret);

      RK_MPI_VENC_StopRecvFrame(pstThumbThreadCtx->u32VencChn);
      RK_MPI_VENC_ResetChn(pstThumbThreadCtx->u32VencChn);
    } else {
      RKADK_LOGP("RK_MPI_VENC_GetStream[%d] timeout[%x]", pstThumbThreadCtx->u32VencChn, ret);
    }
  }

  RKADK_LOGP("Exit get thumb[%d] thread", pstThumbThreadCtx->u32VencChn);
  return NULL;
}

static int MuxerThumbDeInit(RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_U32 u32VencChn;
  MPP_CHN_S stViChn, stVencChn;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  stViChn.enModId = RK_ID_VI;
  stViChn.s32DevId = u32CamId;
  stViChn.s32ChnId = pCtx->stThumbCtx.u32ViChn;

  for (int i = 0; i < pCtx->u32StreamCnt; i++) {
    u32VencChn = pCtx->stThumbCtx.thumbThreads[i].u32VencChn;

    stVencChn.enModId = RK_ID_VENC;
    stVencChn.s32DevId = 0;
    stVencChn.s32ChnId = u32VencChn;

    pCtx->stThumbCtx.bGetBuffer = false;
    RKADK_SIGNAL_Give(pCtx->stThumbCtx.thumbThreads[i].pSignal);
    if (pCtx->stThumbCtx.thumbThreads[i].tid) {
      RKADK_LOGP("Request to cancel venc mb thread...");
      ret = pthread_join(pCtx->stThumbCtx.thumbThreads[i].tid, NULL);
      if (ret)
        RKADK_LOGE("Exit get thumb[%d] mb thread failed!",u32VencChn);
      else
        RKADK_LOGP("Exit get thumb[%d] mb thread ok", u32VencChn);
      pCtx->stThumbCtx.thumbThreads[i].tid = 0;
    }

    // UnBind VI to VENC
    ret = RK_MPI_SYS_UnBind(&stViChn, &stVencChn);
    if (ret)
      RKADK_LOGE("RK_MPI_SYS_UnBind failed(%d)", ret);
    RKADK_LOGP("UnBind VI[%d, %d] to VENC[%d] ok", u32CamId, pCtx->stThumbCtx.u32ViChn, u32VencChn);

    ret = RK_MPI_VENC_DestroyChn(u32VencChn);
    if (ret)
      RKADK_LOGE("Dstroy VENC[%d] failed[%x]", u32VencChn, ret);

    // Destroy signal
    RKADK_SIGNAL_Destroy(pCtx->stThumbCtx.thumbThreads[i].pSignal);
  }

  ret = MuxerViDeInit(u32CamId, pCtx->stThumbCtx.u32ViChn);
  if (ret)
    RKADK_LOGE("MuxerViDeInit[%d] failed(%x)", pCtx->stThumbCtx.u32ViChn, ret);

  return ret;
}

static int MuxerThumbInit(RKADK_U32 u32CamId, RKADK_MW_PTR pHandle) {
  int ret;
  RKADK_U32 u32VencChn;
  VI_CHN_ATTR_S stViChnAttr;
  VENC_CHN_ATTR_S stVencChnAttr;
  MPP_CHN_S stViChn, stVencChn;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_ATTR_JPEG_S *pstAttrJpege;
  char name[RKADK_THREAD_NAME_LEN];
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  stViChn.enModId = RK_ID_VI;
  stViChn.s32DevId = u32CamId;
  stViChn.s32ChnId = pCtx->stThumbCtx.u32ViChn;

  //init vi
  memset(&stViChnAttr, 0, sizeof(VI_CHN_ATTR_S));
  stViChnAttr.stIspOpt.stMaxSize.u32Width = pCtx->stThumbCtx.u32Width;
  stViChnAttr.stIspOpt.stMaxSize.u32Height = pCtx->stThumbCtx.u32Height;
  stViChnAttr.stIspOpt.u32BufCount = 3;
  stViChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
  stViChnAttr.stSize.u32Width = pCtx->stThumbCtx.u32Width;
  stViChnAttr.stSize.u32Height = pCtx->stThumbCtx.u32Height;
  stViChnAttr.enPixelFormat = RK_FMT_YUV420SP;
  stViChnAttr.enCompressMode = COMPRESS_MODE_NONE;
  stViChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stViChnAttr.stFrameRate.s32DstFrameRate = -1;
  ret = MuxerViInit(u32CamId, pCtx->stThumbCtx.u32ViChn, &stViChnAttr);
  if (ret) {
    RKADK_LOGE("VI[%d, %d] init failed, ret[%x]", u32CamId, pCtx->stThumbCtx.u32ViChn, ret);
    return ret;
  }
  RKADK_LOGP("Thumb VI[%d, %d] init ok", u32CamId, pCtx->stThumbCtx.u32ViChn);

  for (int i = 0; i < pCtx->u32StreamCnt; i++) {
    u32VencChn = pCtx->stThumbCtx.thumbThreads[i].u32VencChn;

    stVencChn.enModId = RK_ID_VENC;
    stVencChn.s32DevId = 0;
    stVencChn.s32ChnId = u32VencChn;

    //init signal
    pCtx->stThumbCtx.thumbThreads[i].pSignal = RKADK_SIGNAL_Create(0, 1);
    if (!pCtx->stThumbCtx.thumbThreads[i].pSignal) {
      RKADK_LOGE("RKADK_SIGNAL_Create[%d] failed", i);
      MuxerViDeInit(u32CamId, pCtx->stThumbCtx.u32ViChn);
      return -1;
    }

    //init venc
    memset(&stVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    stVencChnAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
    stVencChnAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = pCtx->stThumbCtx.u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = pCtx->stThumbCtx.u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = pCtx->stThumbCtx.u32Width;
    stVencChnAttr.stVencAttr.u32PicHeight = pCtx->stThumbCtx.u32Height;
    stVencChnAttr.stVencAttr.u32VirWidth = pCtx->stThumbCtx.u32Width;
    stVencChnAttr.stVencAttr.u32VirHeight = pCtx->stThumbCtx.u32Height;
    stVencChnAttr.stVencAttr.u32StreamBufCnt = 1;
    stVencChnAttr.stVencAttr.u32BufSize = pCtx->stThumbCtx.u32Width * pCtx->stThumbCtx.u32Height;

    pstAttrJpege = &(stVencChnAttr.stVencAttr.stAttrJpege);
    pstAttrJpege->bSupportDCF = RK_FALSE;
    pstAttrJpege->stMPFCfg.u8LargeThumbNailNum = 1;
    pstAttrJpege->enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    pstAttrJpege->stMPFCfg.astLargeThumbNailSize[0].u32Width = pCtx->stThumbCtx.u32Width;
    pstAttrJpege->stMPFCfg.astLargeThumbNailSize[0].u32Height = pCtx->stThumbCtx.u32Height;

    ret = RK_MPI_VENC_CreateChn(u32VencChn, &stVencChnAttr);
    if (ret) {
      RKADK_LOGE("Create VENC[%d] failed[%x]", u32VencChn, ret);
      MuxerViDeInit(u32CamId, pCtx->stThumbCtx.u32ViChn);
      return ret;
    }
    RKADK_LOGP("Create VENC[%d] ok", u32VencChn);

    memset(&stRecvParam, 0, sizeof(stRecvParam));
    stRecvParam.s32RecvPicNum = -1;
    ret = RK_MPI_VENC_StartRecvFrame(u32VencChn, &stRecvParam);
    if (ret) {
      RK_MPI_VENC_DestroyChn(u32VencChn);
      MuxerViDeInit(u32CamId, pCtx->stThumbCtx.u32ViChn);
      RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed[%x]", ret);
      return ret;
    }

    // Bind VI to VENC
    ret = RK_MPI_SYS_Bind(&stViChn, &stVencChn);
    if (ret) {
      RK_MPI_VENC_DestroyChn(u32VencChn);
      MuxerViDeInit(u32CamId, pCtx->stThumbCtx.u32ViChn);
      RKADK_LOGE("RK_MPI_SYS_Bind failed(%d)", ret);
      return ret;
    }

    RKADK_LOGP("Bind VI[%d, %d] to VENC[%d] ok", u32CamId, pCtx->stThumbCtx.u32ViChn, u32VencChn);

    // Create thread
    pCtx->stThumbCtx.bGetBuffer = true;
    pCtx->stThumbCtx.thumbThreads[i].pHandle = pHandle;
    ret = pthread_create(&pCtx->stThumbCtx.thumbThreads[i].tid, NULL, MuxerGetThumbMb, &pCtx->stThumbCtx.thumbThreads[i]);
    if (ret) {
      RKADK_LOGE("Create get thumb[%d] mb thread failed[%d]", u32VencChn, ret);
      MuxerThumbDeInit(u32CamId);
      return ret;
    }
    snprintf(name, sizeof(name), "GetThumbMb_%d", u32VencChn);
    pthread_setname_np(pCtx->stThumbCtx.thumbThreads[i].tid, name);
  }

  return 0;
}


static RKADK_VOID MuxerEventCallback(RKADK_MW_PTR pHandle,
                    const RKADK_MUXER_EVENT_INFO_S *pstEventInfo) {
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  switch (pstEventInfo->enEvent) {
  case RKADK_MUXER_EVENT_STREAM_START:
    printf("+++++ RKADK_MUXER_EVENT_STREAM_START +++++\n");
    break;
  case RKADK_MUXER_EVENT_STREAM_STOP:
    printf("+++++ RKADK_MUXER_EVENT_STREAM_STOP +++++\n");
    break;
  case RKADK_MUXER_EVENT_FILE_BEGIN:
    printf("+++++ RKADK_MUXER_EVENT_FILE_BEGIN +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_FILE_END:
    printf("+++++ RKADK_MUXER_EVENT_FILE_END +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_MANUAL_SPLIT_END:
    printf("+++++ RKADK_MUXER_EVENT_MANUAL_SPLIT_END +++++\n");
    printf("\tstFileInfo: %s\n",
           pstEventInfo->unEventInfo.stFileInfo.asFileName);
    printf("\tu32Duration: %d\n",
           pstEventInfo->unEventInfo.stFileInfo.u32Duration);
    break;
  case RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL:
    printf("+++++ RKADK_MUXER_EVENT_ERR_CREATE_FILE_FAIL[%d, %s] +++++\n",
            pstEventInfo->unEventInfo.stErrorInfo.s32ErrorCode,
            strerror(pstEventInfo->unEventInfo.stErrorInfo.s32ErrorCode));
    break;
  case RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL:
    printf("+++++ RKADK_MUXER_EVENT_ERR_WRITE_FILE_FAIL[%d, %s] +++++\n",
            pstEventInfo->unEventInfo.stErrorInfo.s32ErrorCode,
            strerror(pstEventInfo->unEventInfo.stErrorInfo.s32ErrorCode));
    break;
  case RKADK_MUXER_EVENT_FILE_WRITING_SLOW:
    printf("+++++ RKADK_MUXER_EVENT_FILE_WRITINpCtx->SLOW +++++\n");
    break;
  case RKADK_MUXER_EVENT_ERR_CARD_NONEXIST:
    printf("+++++ RKADK_MUXER_EVENT_ERR_CARD_NONEXIST +++++\n");
    break;
  case RKADK_MUXER_EVENT_REQUEST_THUMB:
  {
    int i;
    printf("+++++ RKADK_MUXER_EVENT_REQUEST_THUMB +++++\n");
    printf("\tMuxerId: %d stFileInfo: %s\n", pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId,
            pstEventInfo->unEventInfo.stThumbInfo.asFileName);

    for (i = 0; i < pCtx->u32StreamCnt; i++) {
      if (pCtx->stThumbCtx.thumbThreads[i].u32MuxerId == pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId)
        break;
    }

    if (i == pCtx->u32StreamCnt) {
      RKADK_LOGE("Muxer[%d] no find thumbThreads", pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId);
      break;
    }

    pCtx->stThumbCtx.thumbThreads[i].bRequestThumb = true;
    RKADK_SIGNAL_Give(pCtx->stThumbCtx.thumbThreads[i].pSignal);
    break;
  }
  case RKADK_MUXER_EVENT_BUILD_IN_THUMB:
  {
    int i;
    printf("+++++ RKADK_MUXER_EVENT_BUILD_IN_THUMB +++++\n");
    printf("\tMuxerId: %d stFileInfo: %s\n", pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId,
            pstEventInfo->unEventInfo.stThumbInfo.asFileName);

    for (i = 0; i < pCtx->u32StreamCnt; i++) {
      if (pCtx->stThumbCtx.thumbThreads[i].u32MuxerId == pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId)
        break;
    }

    if (i == pCtx->u32StreamCnt) {
      RKADK_LOGE("Muxer[%d] no find thumbThreads", pstEventInfo->unEventInfo.stThumbInfo.s32MuxerId);
      break;
    }

    RKADK_SIGNAL_Give(pCtx->stThumbCtx.thumbThreads[i].pSignal);
    break;
  }
  default:
    printf("+++++ Unknown event(%d) +++++\n", pstEventInfo->enEvent);
    break;
  }
}

static RKADK_S32 GetMuxerFileName(RKADK_VOID *pHandle, RKADK_CHAR *pcFileName, RKADK_U32 u32MuxerId) {
  int u32FileIdx = 0;
  static RKADK_U32 u32FileIdx_0 = 0;
  static RKADK_U32 u32FileIdx_1 = 0;

  RKADK_LOGP("pHandle:%p, u32MuxerId: %d", pHandle, u32MuxerId);

  if (!pcFileName) {
    RKADK_LOGE("pcFileName is NULL");
    return -1;
  }

  if (u32FileIdx_0 >= 4)
    u32FileIdx_0 = 0;

  if (u32FileIdx_1 >= 4)
    u32FileIdx_1 = 0;

  if (u32MuxerId == 0) {
    u32FileIdx = u32FileIdx_0;
    u32FileIdx_0++;
  } else {
    u32FileIdx = u32FileIdx_1;
    u32FileIdx_1++;
  }

  sprintf(pcFileName, "/tmp/MuxerTest_%u_%u.mp4", u32MuxerId, u32FileIdx);
  return 0;
}

static int MuxerInit(RKADK_U32 u32CamId, RKADK_MW_PTR *ppHandle) {
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  memset(&pCtx->stMuxerAttr, 0, sizeof(RKADK_MUXER_ATTR_S));

  pCtx->stMuxerAttr.enFileType = RKADK_MUXER_TYPE_MP4;
  pCtx->stMuxerAttr.enRecType = RKADK_REC_TYPE_NORMAL;
  pCtx->stMuxerAttr.enMuxerType = RKADK_MUXER_TYPE_THIRD_STREAM;
  pCtx->stMuxerAttr.u32CamId = u32CamId;
  pCtx->stMuxerAttr.u32StreamCnt = pCtx->u32StreamCnt;
  pCtx->stMuxerAttr.pcbRequestFileNames = GetMuxerFileName;
  pCtx->stMuxerAttr.pfnEventCallback = MuxerEventCallback;
  pCtx->stMuxerAttr.u32FragKeyFrame = pCtx->u32FragKeyFrame;
  pCtx->stMuxerAttr.stPreRecordAttr.u32PreRecTimeSec = pCtx->u32PreRecTimeSec;
  pCtx->stMuxerAttr.stPreRecordAttr.enPreRecordMode = RKADK_MUXER_PRE_RECORD_MANUAL_SPLIT;
  if (pCtx->u32PreRecTimeSec)
    pCtx->stMuxerAttr.stPreRecordAttr.u32PreRecCacheTime = pCtx->u32PreRecTimeSec + 1;

  for (int i = 0; i < (int)pCtx->stMuxerAttr.u32StreamCnt; i++) {
    pCtx->vencThreads[i].u32MuxerId = i;
    pCtx->stThumbCtx.thumbThreads[i].u32MuxerId = i;
    pCtx->stMuxerAttr.astStreamAttr[i].s32MuxerId = i;

    if (pCtx->bThirdPartyStream) {
      /* If it is a third-party stream that is not Rockit(such as a network stream),
       * s32ViChn/s32VencChn must set to -1.
      */
      pCtx->stMuxerAttr.astStreamAttr[i].s32ViChn = -1;
      pCtx->stMuxerAttr.astStreamAttr[i].s32VencChn = -1;
    } else {
      pCtx->stMuxerAttr.astStreamAttr[i].s32ViChn = pCtx->videoChns[i].u32ViChn;
      pCtx->stMuxerAttr.astStreamAttr[i].s32VencChn = pCtx->videoChns[i].u32VencChn;
    }
    pCtx->stMuxerAttr.astStreamAttr[i].s32ThumbVencChn = -1;

    pCtx->stMuxerAttr.astStreamAttr[i].enType = RKADK_MUXER_TYPE_MP4;
    pCtx->stMuxerAttr.astStreamAttr[i].u32TimeLenSec = 60;

    if (pCtx->bEnableAudio)
      pCtx->stMuxerAttr.astStreamAttr[i].u32TrackCnt = RKADK_MUXER_TRACK_MAX_CNT; /* a video track and a audio track */
    else
      pCtx->stMuxerAttr.astStreamAttr[i].u32TrackCnt = 1;

    // video track
    RKADK_MUXER_TRACK_SOURCE_S *aHTrackSrcHandle =
        &(pCtx->stMuxerAttr.astStreamAttr[i].aHTrackSrcHandle[0]);
    aHTrackSrcHandle->enTrackType = RKADK_TRACK_SOURCE_TYPE_VIDEO;
    memcpy(aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.cPixFmt, "NV12", strlen("NV12"));
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.enCodecType = pCtx->enCodecType;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32FrameRate = pCtx->u32FrameRate;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u16Level = 41;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u16Profile = 100;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32BitRate = pCtx->u32Bitrate;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Gop = pCtx->u32FrameRate;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Width = pCtx->u32Width;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Height = pCtx->u32Height;

    // if no built-in thumbnail, set the thumbnail resolution to [0, 0]
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32ThumbWidth = pCtx->stThumbCtx.u32Width;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32ThumbHeight = pCtx->stThumbCtx.u32Height;

    if (!pCtx->bEnableAudio)
      continue;

    // audio track, if only video, no need to set audio track
    aHTrackSrcHandle = &(pCtx->stMuxerAttr.astStreamAttr[i].aHTrackSrcHandle[1]);
    aHTrackSrcHandle->enTrackType = RKADK_TRACK_SOURCE_TYPE_AUDIO;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.enCodecType = RKADK_CODEC_TYPE_MP3;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32BitWidth = RKADK_MEDIA_GetAudioBitWidth(pCtx->enBitwidth);
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32ChnCnt = pCtx->channels;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame = pCtx->samplesPerFrame;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32SampleRate = pCtx->samplerate;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32Bitrate = pCtx->audioBitrate;
  }

  if (RKADK_MUXER_Create(&pCtx->stMuxerAttr, ppHandle))
    return -1;

  if (RKADK_MUXER_Enable(&pCtx->stMuxerAttr, *ppHandle)) {
    RKADK_LOGE("RKADK_MUXER_Enable failed");
    RKADK_MUXER_Destroy(*ppHandle);
    return -1;
  }
  return 0;
}

static RKADK_S32 MuxerBufFree(void *buf) {
  if (buf) {
    free(buf);
    buf = NULL;

#ifdef MUXER_TEST_DEBUG
    g_u32BufFreeCnt++;
#endif
  }

  return 0;
}

static void *MuxerGetVencMb(void *params) {
  int ret;
  void *buf, *pThirdBuf = NULL;
  size_t thirdBufSize = 0;
  VENC_PACK_S stPack;
  VENC_STREAM_S stFrame;
  RKADK_MUXER_DATA_S stMuxerData;
  MB_EXT_CONFIG_S stMbExtConfig;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MUXER_TEST_VIDEO_THREAD_CTX_S *pstThreadCtx = (MUXER_TEST_VIDEO_THREAD_CTX_S *)params;

  RKADK_CHECK_POINTER(pstThreadCtx, NULL);

  memset(&stMbExtConfig, 0, sizeof(stMbExtConfig));
  memset(&stPack, 0, sizeof(stPack));
  memset(&stFrame, 0, sizeof(stFrame));
  stFrame.pstPack = &stPack;

  while (pCtx->bGetBuffer) {
    ret = RK_MPI_VENC_GetStream(pstThreadCtx->u32VencChn, &stFrame, 1000);
    if (ret == RK_SUCCESS) {
      memset(&stMuxerData, 0, sizeof(stMuxerData));

      if (pCtx->number > 0 && pstThreadCtx->WriteCnt > pCtx->number) {
        RKADK_LOGP("only write %d frame, current frame = %d", pCtx->number, pstThreadCtx->WriteCnt);
        RK_MPI_VENC_ReleaseStream(pstThreadCtx->u32VencChn, &stFrame);
        break;
      }

      if (pCtx->bThirdPartyStream) {
        /* If it is a third-party stream that is not Rockit(such as a network stream),
         * 1. buf simulates third-party streaming input venc data
         * 2. MB_BLK must be created to store the data, then release after write RKADK_MUXER_WriteVideoFrame,
         * 3. s32VencChn must set to -1.
        */
        if (!pIData || !pPData || ISize == 0 || PSize == 0) {
          thirdBufSize = stFrame.pstPack->u32Len;
          pThirdBuf = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
        } else {
          if (pstThreadCtx->WriteCnt % pCtx->u32FrameRate == 0) {
            pThirdBuf = pIData;
            thirdBufSize = ISize;
            stMuxerData.isKeyFrame = 1;
          } else {
            pThirdBuf = pPData;
            thirdBufSize = PSize;
          }
        }

        buf = malloc(thirdBufSize);
        if (!buf) {
          RKADK_LOGE("malloc video buffer failed, size = %d", thirdBufSize);
          RK_MPI_VENC_ReleaseStream(pstThreadCtx->u32VencChn, &stFrame);
          break;
        }
        memset(buf, 0, thirdBufSize);
        memcpy(buf, pThirdBuf, thirdBufSize);

#ifdef MUXER_TEST_DEBUG
        g_u32VideoMallocCnt++;
#endif

        stMbExtConfig.pFreeCB = MuxerBufFree;
        stMbExtConfig.pOpaque = buf;
        stMbExtConfig.pu8VirAddr = buf;
        stMbExtConfig.u64Size = thirdBufSize;
        ret = RK_MPI_SYS_CreateMB(&stMuxerData.pMbBlk, &stMbExtConfig);
        if (ret) {
          RKADK_LOGE("Create video[%d] MB failed[%x]", pstThreadCtx->u32VencChn, ret);
          free(buf);
          RK_MPI_VENC_ReleaseStream(pstThreadCtx->u32VencChn, &stFrame);
          continue;
        }

        stMuxerData.s32VencChn = -1;
      } else {
        stMuxerData.s32VencChn = pstThreadCtx->u32VencChn;  // must set it, otherwise get wrong MuxerHandle
        stMuxerData.pMbBlk = stFrame.pstPack->pMbBlk;
      }

      if (!pIData || !pPData || ISize == 0 || PSize == 0) {
        if ((stFrame.pstPack->DataType.enH264EType == H264E_NALU_ISLICE ||
            stFrame.pstPack->DataType.enH264EType == H264E_NALU_IDRSLICE) ||
            (stFrame.pstPack->DataType.enH265EType == H265E_NALU_ISLICE ||
            stFrame.pstPack->DataType.enH265EType == H265E_NALU_IDRSLICE))
            stMuxerData.isKeyFrame = 1;
      }

      stMuxerData.s32MuxerId = pstThreadCtx->u32MuxerId; // must set it, otherwise get wrong MuxerHandle
      stMuxerData.u64Pts = stFrame.pstPack->u64PTS;
      stMuxerData.u32Len = thirdBufSize;
      stMuxerData.u32Seq = stFrame.u32Seq;
      RKADK_MUXER_WriteVideoFrame(stMuxerData, pstThreadCtx->pHandle);
      RKADK_LOGD("write pstThreadCtx->WriteCnt: %d, isKeyFrame: %d", pstThreadCtx->WriteCnt, stMuxerData.isKeyFrame);
      pstThreadCtx->WriteCnt++;

      if (pCtx->bThirdPartyStream && stMuxerData.pMbBlk)
        RK_MPI_MB_ReleaseMB(stMuxerData.pMbBlk);

      ret = RK_MPI_VENC_ReleaseStream(pstThreadCtx->u32VencChn, &stFrame);
      if (ret)
        RKADK_LOGE("RK_MPI_VENC_ReleaseStream[%d] failed[%x]", pstThreadCtx->u32VencChn, ret);
    } else {
      RKADK_LOGP("RK_MPI_VENC_GetStream chn[%d] timeout[%x]", pstThreadCtx->u32VencChn, ret);
    }
  }

  RKADK_LOGP("Exit get venc[%d] mb thread", pstThreadCtx->u32VencChn);
  return NULL;
}

static int BindVideoChn(RKADK_U32 u32CamId, int index, RKADK_MW_PTR pHandle) {
  int ret = 0;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MPP_CHN_S stSrcChn, stDstChn;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  char name[RKADK_THREAD_NAME_LEN];
  RKADK_U32 u32ViChn = pCtx->videoChns[index].u32ViChn;
  RKADK_U32 u32VencChn = pCtx->videoChns[index].u32VencChn;

  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = u32CamId;
  stSrcChn.s32ChnId = u32ViChn;

  stDstChn.enModId = RK_ID_VENC;
  stDstChn.s32DevId = 0;
  stDstChn.s32ChnId = u32VencChn;

  memset(&stRecvParam, 0, sizeof(stRecvParam));
  stRecvParam.s32RecvPicNum = -1;
  ret = RK_MPI_VENC_StartRecvFrame(u32VencChn, &stRecvParam);
  if (ret) {
    RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed[%x]", ret);
    return ret;
  }

  pCtx->bGetBuffer = true;
  pCtx->vencThreads[index].u32VencChn = u32VencChn;
  pCtx->vencThreads[index].pHandle = pHandle;
  ret = pthread_create(&pCtx->vencThreads[index].videoTid, NULL, MuxerGetVencMb, &pCtx->vencThreads[index]);
  if (ret) {
    RKADK_LOGE("Create get venc[%d] thread failed[%d]", u32VencChn,ret);
    return ret;
  }
  snprintf(name, sizeof(name), "GetVencMb_%d", u32VencChn);
  pthread_setname_np(pCtx->vencThreads[index].videoTid, name);

  // Bind VI to VENC
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDstChn);
  if (ret) {
    RKADK_LOGE("RK_MPI_SYS_Bind failed(%d)", ret);
    return ret;
  }

  RKADK_LOGP("Bind VI[%d, %d] to VENC[%d] ok", u32CamId, u32ViChn, u32VencChn);
  return 0;
}

static int UnBindVideoChn(RKADK_U32 u32CamId, int index, RKADK_MW_PTR pHandle) {
  int ret = 0;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MPP_CHN_S stSrcChn, stDstChn;
  RKADK_U32 u32ViChn = pCtx->videoChns[index].u32ViChn;
  RKADK_U32 u32VencChn = pCtx->videoChns[index].u32VencChn;

  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = u32CamId;
  stSrcChn.s32ChnId = u32ViChn;

  stDstChn.enModId = RK_ID_VENC;
  stDstChn.s32DevId = 0;
  stDstChn.s32ChnId = u32VencChn;

  pCtx->bGetBuffer = false;
  if (pCtx->vencThreads[index].videoTid) {
    RKADK_LOGP("Request to cancel venc mb thread...");
    ret = pthread_join(pCtx->vencThreads[index].videoTid, NULL);
    if (ret)
      RKADK_LOGE("Exit get venc[%d] mb thread failed!", u32VencChn);
    else
      RKADK_LOGP("Exit get venc[%d] mb thread ok", u32VencChn);
    pCtx->vencThreads[index].videoTid = 0;
  }

  // UnBind VI to VENC
  ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDstChn);
  if (ret) {
    RKADK_LOGE("RK_MPI_SYS_UnBind failed(%d)", ret);
    return ret;
  }
  RKADK_LOGP("UnBind VI[%d, %d] to VENC[%d] ok", u32CamId, u32ViChn, u32VencChn);

  return 0;
}

static void *MuxerGetAencMb(void *params) {
  int ret;
  void *buf;
  AUDIO_STREAM_S stFrame;
  MB_EXT_CONFIG_S stMbExtConfig;
  RKADK_MUXER_DATA_S stMuxerData;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  RKADK_CHECK_POINTER(params, NULL);

  memset(&stMbExtConfig, 0, sizeof(stMbExtConfig));
  memset(&stFrame, 0, sizeof(AUDIO_STREAM_S));

  while (pCtx->bGetBuffer) {
    ret = RK_MPI_AENC_GetStream(pCtx->u32AencChn, &stFrame, 1000);
    if (ret == RK_SUCCESS) {
      memset(&stMuxerData, 0, sizeof(RKADK_MUXER_DATA_S));
      if (pCtx->bThirdPartyStream) {
        /* If it is a third-party stream that is not Rockit(such as a network stream),
         * MB_BLK must be created to store the data, then release after write RKADK_MUXER_WriteVideoFrame.
        */
        buf = malloc(stFrame.u32Len);
        if (!buf) {
          RKADK_LOGE("malloc audio buffer failed, size = %d", stFrame.u32Len);
          RK_MPI_AENC_ReleaseStream(pCtx->u32AencChn, &stFrame);
          continue;
        }
        memset(buf, 0, stFrame.u32Len);
        memcpy(buf, RK_MPI_MB_Handle2VirAddr(stFrame.pMbBlk), stFrame.u32Len);

#ifdef MUXER_TEST_DEBUG
        g_u32AudioMallocCnt++;
#endif

        stMbExtConfig.pFreeCB = MuxerBufFree;
        stMbExtConfig.pOpaque = buf;
        stMbExtConfig.pu8VirAddr = buf;
        stMbExtConfig.u64Size = stFrame.u32Len;
        ret = RK_MPI_SYS_CreateMB(&stMuxerData.pMbBlk, &stMbExtConfig);
        if (ret) {
          RKADK_LOGE("Create audio[%d] MB failed[%x]", pCtx->u32AencChn, ret);
          free(buf);
          RK_MPI_AENC_ReleaseStream(pCtx->u32AencChn, &stFrame);
          continue;
        }
      } else {
        stMuxerData.pMbBlk = stFrame.pMbBlk;
      }

      stMuxerData.u64Pts = stFrame.u64TimeStamp;
      stMuxerData.u32Len = stFrame.u32Len;
      stMuxerData.u32Seq = stFrame.u32Seq;
      RKADK_MUXER_WriteAudioFrame(stMuxerData, params);

      if (pCtx->bThirdPartyStream && stMuxerData.pMbBlk)
        RK_MPI_MB_ReleaseMB(stMuxerData.pMbBlk);

      ret = RK_MPI_AENC_ReleaseStream(pCtx->u32AencChn, &stFrame);
      if (ret)
        RKADK_LOGE("RK_MPI_AENC_ReleaseStream[%d] failed[%x]", pCtx->u32AencChn, ret);
    } else {
        RKADK_LOGE("RK_MPI_AENC_GetStream chn[%d] timeout[%x]", pCtx->u32AencChn, ret);
    }
  }

  RKADK_LOGP("Exit get aenc mb thread");
  return NULL;
}

static int BindAudioChn(RKADK_U32 u32AiChn, RKADK_U32 u32AencChn, RKADK_MW_PTR pHandle) {
  int ret = 0;
  char name[RKADK_THREAD_NAME_LEN];
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MPP_CHN_S stSrcChn, stDstChn;

  if (!pCtx->bEnableAudio)
    return 0;

  stSrcChn.enModId = RK_ID_AI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = u32AiChn;

  stDstChn.enModId = RK_ID_AENC;
  stDstChn.s32DevId = 0;
  stDstChn.s32ChnId = u32AencChn;

  pCtx->bGetBuffer = true;
  ret = pthread_create(&pCtx->audioTid, NULL, MuxerGetAencMb, pHandle);
  if (ret) {
    RKADK_LOGE("Create get aenc thread failed[%d]", ret);
    return ret;
  }
  snprintf(name, sizeof(name), "GetAencMb_%d", u32AencChn);
  pthread_setname_np(pCtx->audioTid, name);

  // Bind AI to AENC
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDstChn);
  if (ret) {
    RKADK_LOGE("RK_MPI_SYS_Bind failed(%d)", ret);
    return ret;
  }
  RKADK_LOGP("Bind AI[%d] to AENC[%d] ok", u32AiChn, u32AencChn);

  return 0;
}

static int UnBindAudioChn(RKADK_U32 u32AiChn, RKADK_U32 u32AencChn, RKADK_MW_PTR pHandle) {
  int ret = 0;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  MPP_CHN_S stSrcChn, stDstChn;

  stSrcChn.enModId = RK_ID_AI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = u32AiChn;

  stDstChn.enModId = RK_ID_AENC;
  stDstChn.s32DevId = 0;
  stDstChn.s32ChnId = u32AencChn;

  if (!pCtx->bEnableAudio)
    return 0;

  pCtx->bGetBuffer = false;
  if (pCtx->audioTid) {
    RKADK_LOGP("Request to cancel aenc mb thread...");
    ret = pthread_join(pCtx->audioTid, NULL);
    if (ret)
      RKADK_LOGE("Exit get aenc mb thread failed!");
    else
      RKADK_LOGP("Exit get aenc mb thread ok");
    pCtx->audioTid = 0;
  }

  // UnBind AI to AENC
  ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDstChn);
  if (ret) {
    RKADK_LOGE("RK_MPI_SYS_UnBind failed(%d)", ret);
    return ret;
  }
  RKADK_LOGP("UnBind AI[%d] to AENC[%d] ok", u32AiChn, u32AencChn);

  return 0;
}

static RKADK_U32 MuxerResetThumb() {
  int ret = 0;
  VENC_PACK_S stPack;
  VENC_STREAM_S stFrame;
  RKADK_U32 u32ThumbChn;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  for (int i = 0; i < (int)pCtx->stMuxerAttr.u32StreamCnt; i++) {
    u32ThumbChn = pCtx->stThumbCtx.thumbThreads[i].u32VencChn;

    //clean thumbnail
    memset(&stFrame, 0, sizeof(stFrame));
    memset(&stPack, 0, sizeof(stPack));
    stFrame.pstPack = &stPack;
    do {
      ret = RK_MPI_VENC_GetStream(u32ThumbChn, &stFrame, 1);
      if (ret == RK_SUCCESS) {
        ret = RK_MPI_VENC_ReleaseStream(u32ThumbChn, &stFrame);
        if (ret != RK_SUCCESS)
          RKADK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", ret);
      } else {
        break;
      }
    } while(1);
    RK_MPI_VENC_ResetChn(u32ThumbChn);

    //make sure thumbnail
    RKADK_LOGI("venc[%d] request thumbnail", u32ThumbChn);
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    stRecvParam.s32RecvPicNum = 1;
    ret = RK_MPI_VENC_StartRecvFrame(u32ThumbChn, &stRecvParam);
    if (ret) {
      RKADK_LOGE("Request thumbnail failed %x", ret);
      break;
    }
  }

  return ret;
}

static int MuxerTestReset(RKADK_U32 u32CamId, RKADK_MW_PTR pHandle) {
  int ret;
  bool bReset = false;
  MPP_CHN_S stSrcChn, stDstChn;
  RKADK_MUXER_TRACK_SOURCE_S *aHTrackSrcHandle;
  VI_CHN_ATTR_S stViAttr[RKADK_MUXER_STREAM_MAX_CNT];
  VENC_CHN_ATTR_S stVencAttr[RKADK_MUXER_STREAM_MAX_CNT];
  MUXER_TEST_CTX_S *pCtx = &g_ctx;

  for (int i = 0; i < (int)pCtx->stMuxerAttr.u32StreamCnt; i++) {
    memset(&stVencAttr[i], 0, sizeof(VENC_CHN_ATTR_S));

    ret = RK_MPI_VENC_GetChnAttr(pCtx->videoChns[i].u32VencChn, &stVencAttr[i]);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VENC_GetChnAttr[%d] failed [%x]", pCtx->videoChns[i].u32VencChn, ret);
      return -1;
    }

    if (stVencAttr[i].stVencAttr.u32PicWidth == pCtx->u32Width &&
        stVencAttr[i].stVencAttr.u32PicHeight == pCtx->u32Height) {
      RKADK_LOGP("venc[%d] resolution has not changed", pCtx->videoChns[i].u32VencChn);
      continue;
    }

    stVencAttr[i].stVencAttr.u32PicWidth = pCtx->u32Width;
    stVencAttr[i].stVencAttr.u32PicHeight = pCtx->u32Height;
    stVencAttr[i].stVencAttr.u32VirWidth = pCtx->u32Width;
    stVencAttr[i].stVencAttr.u32VirHeight = pCtx->u32Height;

    memset(&stViAttr[i], 0, sizeof(VI_CHN_ATTR_S));
    ret = RK_MPI_VI_GetChnAttr(u32CamId, pCtx->videoChns[i].u32ViChn, &stViAttr[i]);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VI_GetChnAttr[%d] failed [%x]", pCtx->videoChns[i].u32ViChn, ret);
      return -1;
    }
    stViAttr[i].stSize.u32Width = pCtx->u32Width;
    stViAttr[i].stSize.u32Height = pCtx->u32Height;

    aHTrackSrcHandle = &(pCtx->stMuxerAttr.astStreamAttr[i].aHTrackSrcHandle[0]);
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Width = pCtx->u32Width;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Height = pCtx->u32Height;
    bReset = true;
  }

  if (!bReset) {
    RKADK_LOGP("No need to reset muxer");
    return 0;
  }

  ret = RKADK_MUXER_Stop(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_MUXER_Stop failed[%d]", ret);
    return -1;
  }

  RKADK_MUXER_SetResetState(pHandle, true);

  ret = RKADK_MUXER_Reset(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_MUXER_Reset failed");
    return -1;
  }

  MuxerResetThumb();

  for (int i = 0; i < (int)pCtx->stMuxerAttr.u32StreamCnt; i++) {
    memset(&stSrcChn, 0, sizeof(MPP_CHN_S));
    memset(&stDstChn, 0, sizeof(MPP_CHN_S));

    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = u32CamId;
    stSrcChn.s32ChnId = pCtx->videoChns[i].u32ViChn;

    stDstChn.enModId = RK_ID_VENC;
    stDstChn.s32DevId = 0;
    stDstChn.s32ChnId = pCtx->videoChns[i].u32VencChn;

    ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDstChn);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("CamId: %d VI[%d] UnBind VENC[%d] failed: %x",
                  u32CamId, index, stSrcChn.s32ChnId, stDstChn.s32ChnId, ret);
      return -1;
    }

    ret = RK_MPI_VENC_SetChnAttr(stDstChn.s32ChnId, &stVencAttr[i]);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("set venc[%d] attr failed %x", stDstChn.s32ChnId, ret);
      return -1;
    }

    ret = RK_MPI_VI_SetChnAttr(u32CamId, stSrcChn.s32ChnId, &stViAttr[i]);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VI_SetChnAttr(%d) failed %x", stSrcChn.s32ChnId, ret);
      return -1;
    }
  }

  ret = RKADK_MUXER_ResetParam(&pCtx->stMuxerAttr, pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_MUXER_ResetParam failed");
    return -1;
  }

  for (int i = 0; i < (int)pCtx->stMuxerAttr.u32StreamCnt; i++) {
    memset(&stSrcChn, 0, sizeof(MPP_CHN_S));
    memset(&stDstChn, 0, sizeof(MPP_CHN_S));

    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = u32CamId;
    stSrcChn.s32ChnId = pCtx->videoChns[i].u32ViChn;

    stDstChn.enModId = RK_ID_VENC;
    stDstChn.s32DevId = 0;
    stDstChn.s32ChnId = pCtx->videoChns[i].u32VencChn;

    ret = RK_MPI_SYS_Bind(&stSrcChn, &stDstChn);
    if(ret != RK_SUCCESS) {
      RKADK_LOGE("VI[%d] Bind VENC [%d] failed[%x]", stSrcChn.s32ChnId, stDstChn.s32ChnId, ret);
      return -1;
    }
  }

  RKADK_MUXER_SetResetState(pHandle, false);
  RKADK_MUXER_Start(pHandle);
  RKADK_LOGP("u32CamId[%d] reset ok", u32CamId);
  return 0;
}

int main(int argc, char *argv[]) {
  int c, i, ret;
  MUXER_TEST_CTX_S *pCtx = &g_ctx;
  RKADK_MW_PTR pHandle = NULL;
  FILE_CACHE_ARG stFileCacheAttr;
  const char *pIFile = NULL;
  const char *pPFile = NULL;
  int bThirdPartyStream;
  int enableAudio;

#ifdef RKAIQ
  const char *tmp_optarg = optarg;
  SAMPLE_ISP_PARAM stIspParam;

  memset(&stIspParam, 0, sizeof(SAMPLE_ISP_PARAM));
  stIspParam.iqFileDir = IQ_FILE_PATH;
#endif

  MuxerCtxInit();

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
#ifdef RKAIQ
    case 'a':
      if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
        tmp_optarg = argv[optind++];
      }

      if (tmp_optarg)
        stIspParam.iqFileDir = (char *)tmp_optarg;
      break;
#endif

    case 'd':
      pCtx->audioNode = optarg;
      break;

    case 'W':
      pCtx->u32Width = atoi(optarg);
      break;

    case 'H':
      pCtx->u32Height = atoi(optarg);
      break;

    case 'f':
      pCtx->u32FrameRate = atoi(optarg);
      break;

    case 'c':
      pCtx->u32StreamCnt = atoi(optarg);
      break;

    case 'C':
      pCtx->enCodecType = (RKADK_CODEC_TYPE_E)atoi(optarg);
      break;

    case 'k':
      pCtx->u32FragKeyFrame = 1;
      break;

    case 't':
      bThirdPartyStream = atoi(optarg);
      if (!bThirdPartyStream)
        pCtx->bThirdPartyStream = false;
      else
        pCtx->bThirdPartyStream = true;
      break;

    case 'A':
      enableAudio = atoi(optarg);
      if (!enableAudio)
        pCtx->bEnableAudio = false;
      else
        pCtx->bEnableAudio = true;
      break;

    case 'p':
      pCtx->u32PreRecTimeSec = atoi(optarg);
      break;

    case 'n':
      pCtx->number = atoi(optarg);
      break;

    case 'I':
      pIFile = optarg;
      break;

    case 'P':
      pPFile = optarg;
      break;

    case 'h':
    default:
      print_usage(argv[0]);
      optind = 0;
      return 0;
    }
  }
  optind = 0;

  if (pCtx->u32StreamCnt > RKADK_MUXER_STREAM_MAX_CNT || pCtx->u32StreamCnt == 0) {
    RKADK_LOGE("invalid stream cnt(%d)", pCtx->u32StreamCnt);
    return -1; 
  }

  RKADK_LOGP("u32StreamCnt = %d", pCtx->u32StreamCnt);
  RKADK_LOGP("u32Width = %d", pCtx->u32Width);
  RKADK_LOGP("u32Height = %d", pCtx->u32Height);
  RKADK_LOGP("u32FrameRate = %d", pCtx->u32FrameRate);
  RKADK_LOGP("u32FragKeyFrame = %d", pCtx->u32FragKeyFrame);
  RKADK_LOGP("enCodecType = %d", pCtx->enCodecType);
  RKADK_LOGP("audioNode = %s", pCtx->audioNode);
  RKADK_LOGP("bThirdPartyStream = %d", pCtx->bThirdPartyStream);
  RKADK_LOGP("bEnableAudio = %d", pCtx->bEnableAudio);
  RKADK_LOGP("u32PreRecTimeSec = %d", pCtx->u32PreRecTimeSec);
  RKADK_LOGP("pIFile = %s", pIFile);
  RKADK_LOGP("pPFile = %s", pPFile);
  RKADK_LOGP("number: %d", pCtx->number);

  pCtx->stThumbCtx.u32ViChn = pCtx->u32StreamCnt;

  if (pCtx->enCodecType > RKADK_CODEC_TYPE_H265) {
    RKADK_LOGE("invalid codec type(%d)", pCtx->enCodecType);
    return -1;
  }

  if (pIFile && pPFile) {
    FILE *IFile = fopen(pIFile, "rb");
    if (IFile) {
      struct stat st;
      if (fstat(fileno(IFile), &st) == 0) {
        ISize = st.st_size;
        pIData = malloc(ISize);
        if (pIData) {
          if (fread(pIData, 1, ISize, IFile) != ISize) {
              RKADK_LOGP("fread %s failed", pIFile);
              free(pIData);
              pIData = NULL;
          } else {
            RKADK_LOGP("read %s ok, size: %d", pIFile, ISize);
          }
        }
      }

      fclose(IFile);
    } else {
      RKADK_LOGE("open %s failed", pIFile);
    }

    FILE *PFile = fopen(pPFile, "rb");
    if (PFile) {
      struct stat st;
      if (fstat(fileno(PFile), &st) == 0) {
        PSize = st.st_size;
        pPData = malloc(PSize);
        if (pPData) {
          if (fread(pPData, 1, PSize, PFile) != PSize) {
              RKADK_LOGE("fread %s failed", pPFile);
              free(pPData);
              pPData = NULL;
          } else {
            RKADK_LOGP("read %s ok, size: %d", pPFile, PSize);
          }
        }
      }

      fclose(PFile);
    } else {
      RKADK_LOGE("open %s failed", pPFile);
    }
  }

#ifdef RKAIQ
  stIspParam.WDRMode = RK_AIQ_WORKING_MODE_NORMAL;
  stIspParam.bMultiCam = false;
  stIspParam.fps = pCtx->u32FrameRate;
  SAMPLE_ISP_Start(pCtx->u32CamId, stIspParam);
  RKADK_BUFINFO("isp[%d] init", pCtx->u32CamId);
#endif

  RKADK_MPI_SYS_Init();

  // init file cache
  memset(&stFileCacheAttr, 0, sizeof(FILE_CACHE_ARG));
  stFileCacheAttr.sdcard_path = "/dev/mmcblk1p1";
  stFileCacheAttr.total_cache = 7 * 1024 * 1024; // 7M
  stFileCacheAttr.write_cache = 1024 * 1024; // 1M
  stFileCacheAttr.write_thread_arg.sched_policy = FILE_SCHED_FIFO;
  stFileCacheAttr.write_thread_arg.priority = 99;
  RKADK_MUXER_FileCacheInit(&stFileCacheAttr);

  for (i = 0; i < pCtx->u32StreamCnt; i++) {
    ret = VideoInit(pCtx->u32CamId, pCtx->videoChns[i].u32ViChn, pCtx->videoChns[i].u32VencChn);
    if (ret) {
      RKADK_LOGE("video init failed");
      goto EXIT;
    }
  }

  ret = AidioInit(pCtx->u32AiChn, pCtx->u32AencChn);
  if (ret) {
    RKADK_LOGE("aidio init failed");
    VideoInit(pCtx->u32CamId, pCtx->videoChns[i].u32ViChn, pCtx->videoChns[i].u32VencChn);
    goto EXIT;
  }

  ret = MuxerInit(pCtx->u32CamId, &pHandle);
  if (ret) {
    RKADK_LOGE("muxer init failed");
    goto EXIT_1;
  }

  //bind vi and venc
  for (i = 0; i < pCtx->u32StreamCnt; i++) {
    ret = BindVideoChn(pCtx->u32CamId, i, pHandle);
    if (ret) {
      RKADK_LOGE("BindVideoChn failed");
      goto EXIT_2;
    }
  }

  //bind ai and aenc
  ret = BindAudioChn(pCtx->u32AiChn, pCtx->u32AencChn, pHandle);
  if (ret) {
    RKADK_LOGE("BindAudioChn failed");
    goto EXIT_2;
  }

  MuxerThumbInit(pCtx->u32CamId, pHandle);
  RKADK_MUXER_Start(pHandle);

  signal(SIGINT, sigterm_handler);
  char cmd[64];
  printf("\n#Usage: input 'quit' to exit programe!\n"
         "peress any other key to quit\n");
  while (!is_quit) {
    fgets(cmd, sizeof(cmd), stdin);
    if (strstr(cmd, "quit") || is_quit) {
      RKADK_LOGP("#Get 'quit' cmd!");
      break;
    } else if (strstr(cmd, "MS")) { //Manual Split
      RKADK_MUXER_MANUAL_SPLIT_ATTR_S stSplitAttr;
      memset(&stSplitAttr, 0, sizeof(stSplitAttr));
      stSplitAttr.enManualType = MUXER_PRE_MANUAL_SPLIT;
      stSplitAttr.u32DurationSec = 30;
      RKADK_MUXER_ManualSplit(pHandle, &stSplitAttr);
    } else if (strstr(cmd, "720")) {
      pCtx->u32Width = 1280;
      pCtx->u32Height = 720;
      MuxerTestReset(pCtx->u32CamId, pHandle);
    } else if (strstr(cmd, "1080")) {
      pCtx->u32Width = 1920;
      pCtx->u32Height = 1080;
      MuxerTestReset(pCtx->u32CamId, pHandle);
    }

    usleep(500000);
  }

  RKADK_MUXER_Stop(pHandle);
  MuxerThumbDeInit(pCtx->u32CamId);

  //unbind ai and aenc
  UnBindAudioChn(pCtx->u32AiChn, pCtx->u32AencChn, pHandle);

EXIT_2:
  //unbind vi and venc
  for (i = 0; i < pCtx->u32StreamCnt; i++)
    UnBindVideoChn(pCtx->u32CamId, i, pHandle);

EXIT_1:
  // Must be called before VideoDeInit
  RKADK_MUXER_Disable(pHandle);

  AidioDeInit(pCtx->u32AiChn, pCtx->u32AencChn);
  for (i = 0; i < pCtx->u32StreamCnt; i++) {
    VideoDeInit(pCtx->u32CamId, pCtx->videoChns[i].u32ViChn, pCtx->videoChns[i].u32VencChn);
  }

  RKADK_MUXER_Destroy(pHandle);

EXIT:
#ifdef RKAIQ
  SAMPLE_ISP_Stop(pCtx->u32CamId);
#endif

  RKADK_MPI_SYS_Exit();
  RKADK_MUXER_FileCacheDeInit();

#ifdef MUXER_TEST_DEBUG
  RKADK_LOGP("video malloc[%d], audio malloc[%d], total[%d]", g_u32VideoMallocCnt, g_u32AudioMallocCnt, g_u32VideoMallocCnt + g_u32AudioMallocCnt);
  RKADK_LOGP("free aenc frame cnt = %d", g_u32BufFreeCnt);
#endif

  if (pIData)
    free(pIData);

  if (pPData)
    free(pPData);

  RKADK_LOGP("Exit");
  return 0;
}