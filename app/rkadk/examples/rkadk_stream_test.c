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

#include "rkadk_common.h"
#include "rkadk_media_comm.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_stream.h"
#include "isp/sample_isp.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int optind;
extern char *optarg;

#define IQ_FILE_PATH "/etc/iqfiles"

static FILE *g_output_file = NULL;
static FILE *g_output1_file = NULL;
static FILE *g_pcm_file = NULL;
static bool is_quit = false;
static RKADK_CHAR *g_output_path = "/data/test.bin";
static RKADK_CHAR optstr[] = "a:I:M:e:o:p:m:s:h";

//default use ini audio codec type
static RKADK_CODEC_TYPE_E g_enCodecType = RKADK_CODEC_TYPE_BUTT;
static int g_vStreamType = 0;

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-a /etc/iqfiles] [-I 0] [-m audio] [-e g711a] [-o "
         "/tmp/aenc.g711a]\n",
         name);
  printf("\t-a: enable aiq with dirpath provided, eg:-a "
         "/oem/etc/iqfiles/, Default /etc/iqfiles,"
         "without this option aiq should run in other application\n");
  printf("\t-I: Camera id, Default:0\n");
  printf("\t-M: Test mode, Value: audio, video, Default:\"audio\"\n");
  printf("\t-e: Encode type, Value:pcm, g711a, g711u, mp2, mp3, Default:ini audio codec type\n");
  printf("\t-o: Output path, Default: /data/test.bin\"\n");
  printf("\t-p: param ini directory path, Default:/data/rkadk\n");
  printf("\t-m: multiple sensors, Default:0, options: 1(all isp sensors), 2(isp+ahd sensors)\n");
  printf("\t-s: video stream type, Default:0(PREVIEW), options: 0(PREVIEW), 1(LIVE), 2(PREVIEW + LIVE)\n");
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

static RKADK_S32 VencDataCb(RKADK_VIDEO_STREAM_S *pVStreamData) {
  if (g_output1_file && pVStreamData->enStreamType == RKADK_STREAM_TYPE_PREVIEW) {
    if (pVStreamData->u32CamId == 0 && g_output_file) {
      fwrite(pVStreamData->astPack.apu8Addr, 1, pVStreamData->astPack.au32Len, g_output_file);
    } else if (pVStreamData->u32CamId == 1) {
      fwrite(pVStreamData->astPack.apu8Addr, 1, pVStreamData->astPack.au32Len, g_output1_file);
    }
  } else {
    if (g_output_file && pVStreamData->enStreamType == RKADK_STREAM_TYPE_PREVIEW) {
      fwrite(pVStreamData->astPack.apu8Addr, 1, pVStreamData->astPack.au32Len, g_output_file);
    }
  }

  RKADK_LOGD("CamId[%d] enStreamType[%d] seq: %d, pts: %lld, size: %zu",
              pVStreamData->u32CamId, pVStreamData->enStreamType, pVStreamData->u32Seq,
              pVStreamData->astPack.u64PTS, pVStreamData->astPack.au32Len);
  return 0;
}

static int PostIspCallback(RK_VOID *pParam, RK_VOID *pPrivateData) {
  int ret = 0;

#ifdef RKAIQ
  RKADK_U32 u32CamId = (RKADK_U32)pPrivateData;
  rk_ainr_param *pAinrParam = (rk_ainr_param *)pParam;

  if (pAinrParam == RK_NULL) {
    RKADK_LOGE("pAinrParam is nullptr!");
    return -1;
  }

  memset(pAinrParam, 0, sizeof(rk_ainr_param));
  ret = SAMPLE_ISP_GetAINrParams(u32CamId, pAinrParam);
  if (ret) {
    RKADK_LOGE("u32CamId[%d] can't get ainr param!", u32CamId);
    return ret;
  }

  RKADK_LOGD("aiisp cam %d enable %d", u32CamId, ((rk_ainr_param *)pAinrParam)->enable);
#else
  RKADK_LOGW("Don't enable aiq");
#endif

  return ret;
}

static int VideoInitTest(RKADK_U32 u32CamId, RKADK_MW_PTR *pHandle, RKADK_STREAM_TYPE_E enStreamType, RKADK_POST_ISP_ATTR_S *pstPostIspAttr) {
  int ret;
  RKADK_STREAM_VIDEO_ATTR_S stVideoAttr;
  RKADK_VIDEO_INFO_S stVideoInfo;

  if (!pstPostIspAttr) {
    RKADK_LOGE("pstPostIspAttr is null");
    return -1;
  }

  //aiisp param init
  memset(pstPostIspAttr, 0, sizeof(RKADK_POST_ISP_ATTR_S));
  pstPostIspAttr->pModelFilePath = "/oem/usr/lib/";
  pstPostIspAttr->stAiIspCallback.pPrivateData = (void *)u32CamId;
  pstPostIspAttr->stAiIspCallback.pfUpdateCallback = PostIspCallback;
  pstPostIspAttr->u32FrameBufCnt = 2;

  memset(&stVideoAttr, 0, sizeof(RKADK_STREAM_VIDEO_ATTR_S));
  stVideoAttr.pfnDataCB = VencDataCb;
  stVideoAttr.u32CamId = u32CamId;
  stVideoAttr.pstPostIspAttr = pstPostIspAttr;
  stVideoAttr.enStreamType = enStreamType;

  RKADK_STREAM_GetVideoInfoEx(u32CamId, enStreamType, &stVideoInfo);
  RKADK_LOGP("CamId[%d] enStreamType: %d", u32CamId, enStreamType);
  RKADK_LOGP("CamId[%d] stVideoInfo.enCodecType: %d", u32CamId, stVideoInfo.enCodecType);
  RKADK_LOGP("CamId[%d] stVideoInfo.u32BitRate: %d", u32CamId, stVideoInfo.u32BitRate);
  RKADK_LOGP("CamId[%d] stVideoInfo.u32FrameRate: %d", u32CamId, stVideoInfo.u32FrameRate);
  RKADK_LOGP("CamId[%d] stVideoInfo.u32Gop: %d", u32CamId, stVideoInfo.u32Gop);
  RKADK_LOGP("CamId[%d] stVideoInfo.u32Width: %d", u32CamId, stVideoInfo.u32Width);
  RKADK_LOGP("CamId[%d] stVideoInfo.u32Height: %d", u32CamId, stVideoInfo.u32Height);

  ret = RKADK_STREAM_VideoInit(&stVideoAttr, pHandle);
  if (ret) {
    RKADK_LOGE("CamId[%d] enStreamType[%d] RKADK_STREAM_VideoInit failed[%d]", u32CamId, enStreamType, ret);
    return -1;
  }

  ret = RKADK_STREAM_VencStart(*pHandle, -1);
  if (ret) {
    RKADK_LOGE("CamId[%d] enStreamType[%d] RKADK_STREAM_VencStart failed", u32CamId, enStreamType);
    RKADK_STREAM_VideoDeInit(*pHandle);
    return -1;
  }

  return 0;
}

static int VideoDeInitTest(RKADK_MW_PTR pHandle) {
  int ret = 0;

  ret = RKADK_STREAM_VencStop(pHandle);
  if (ret)
    RKADK_LOGE("RKADK_STREAM_VencStop failed[%d]", ret);

  ret = RKADK_STREAM_VideoDeInit(pHandle);
  if (ret)
    RKADK_LOGE("RKADK_STREAM_VideoDeInit failed[%d]", ret);

  return ret;
}

static int VideoTest(RKADK_U32 u32CamId, RKADK_CHAR *pIqfilesPath, RKADK_BOOL bMultiCam, RKADK_BOOL bMultiSensor) {
  RKADK_S32 ret;
  RKADK_MW_PTR pHandle = NULL, pHandle1 = NULL;
  RKADK_MW_PTR pHandleLive = NULL, pHandleLive1 = NULL;
  bool bAiispEnable = false;
  RKADK_PARAM_RES_CFG_S stResCfg;
  RKADK_POST_ISP_ATTR_S stPostIspAttr;
  RKADK_STREAM_TYPE_E enStreamType = RKADK_STREAM_TYPE_PREVIEW;

  g_output_file = fopen(g_output_path, "w");
  if (!g_output_file) {
    RKADK_LOGE("open %s file failed, exit", g_output_path);
    return -1;
  }

  if (bMultiSensor) {
    u32CamId = 0;
    g_output1_file = fopen("/data/venc_cam_1.bin", "w");
    if (!g_output1_file) {
      RKADK_LOGE("open /data/venc_cam_1.bin file failed, exit");
      return -1;
    }
  }

#ifdef RKAIQ
  RKADK_PARAM_FPS_S stFps;
  stFps.enStreamType = RKADK_STREAM_TYPE_SENSOR;
  ret = RKADK_PARAM_GetCamParam(u32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
  if (ret) {
    RKADK_LOGE("RKADK_PARAM_GetCamParam u32CamId[%d] fps failed", u32CamId);
    return -1;
  }

  SAMPLE_ISP_PARAM stIspParam;
  memset(&stIspParam, 0, sizeof(SAMPLE_ISP_PARAM));
  stIspParam.WDRMode = RK_AIQ_WORKING_MODE_NORMAL;
  stIspParam.fps = stFps.u32Framerate;
  stIspParam.bMultiCam = bMultiCam;
  stIspParam.iqFileDir = pIqfilesPath;

stream:
  SAMPLE_ISP_Start(u32CamId, stIspParam);
  RKADK_BUFINFO("isp[%d] init", u32CamId);

  if (bMultiCam) {
    ret = RKADK_PARAM_GetCamParam(1, RKADK_PARAM_TYPE_FPS, &stFps);
    if (ret) {
      RKADK_LOGE("RKADK_PARAM_GetCamParam u32CamId[1] fps failed");
      SAMPLE_ISP_Stop(u32CamId);
      return -1;
    }

    SAMPLE_ISP_Start(1, stIspParam);
    RKADK_BUFINFO("isp[1] init");
  }
#endif

  if (g_vStreamType == 1)
    enStreamType = RKADK_STREAM_TYPE_LIVE;
  else
    enStreamType = RKADK_STREAM_TYPE_PREVIEW;

  ret = VideoInitTest(u32CamId, &pHandle, enStreamType, &stPostIspAttr);
  if (ret) {
    RKADK_LOGE("VideoInitTest u32CamId[%d] enStreamType[%d] failed", u32CamId, enStreamType);
#ifdef RKAIQ
    SAMPLE_ISP_Stop(u32CamId);
    if (bMultiCam)
      SAMPLE_ISP_Stop(1);
#endif
    return -1;
  }

  if (g_vStreamType == 2) {
    ret = VideoInitTest(u32CamId, &pHandleLive, RKADK_STREAM_TYPE_LIVE, &stPostIspAttr);
    if (ret) {
      RKADK_LOGE("VideoInitTest u32CamId[%d] enStreamType[%d] failed", u32CamId, enStreamType);
      VideoDeInitTest(pHandle);
  #ifdef RKAIQ
      SAMPLE_ISP_Stop(u32CamId);
      if (bMultiCam)
        SAMPLE_ISP_Stop(1);
  #endif
      return -1;
    }
  }

  if (bMultiSensor) {
    ret = VideoInitTest(1, &pHandle1, enStreamType, &stPostIspAttr);
    if (ret) {
      RKADK_LOGE("VideoInitTest u32CamId[1] enStreamType[%d] failed = %d", enStreamType, ret);
      VideoDeInitTest(pHandle);
      VideoDeInitTest(pHandleLive);
#ifdef RKAIQ
      SAMPLE_ISP_Stop(u32CamId);
      if (bMultiCam)
        SAMPLE_ISP_Stop(1);
#endif
      return -1;
    }

    ret = VideoInitTest(1, &pHandleLive1, RKADK_STREAM_TYPE_LIVE, &stPostIspAttr);
    if (ret) {
      RKADK_LOGE("VideoInitTest u32CamId[1] LIVE failed = %d", ret);
      VideoDeInitTest(pHandle);
      VideoDeInitTest(pHandleLive);
      VideoDeInitTest(pHandle1);
#ifdef RKAIQ
      SAMPLE_ISP_Stop(u32CamId);
      if (bMultiCam)
        SAMPLE_ISP_Stop(1);
#endif
      return -1;
    }
  }

  RKADK_LOGP("u32CamId[%d] bMultiCam[%d] bMultiSensor[%d] initial finish\n", u32CamId, bMultiCam, bMultiSensor);
  signal(SIGINT, sigterm_handler);

  char cmd[64];
  printf("\n#Usage: input 'quit' to exit programe!\n"
         "peress any other key to manual split file\n");

  while (!is_quit) {
    memset(cmd, 0, sizeof(cmd));
    char *fgetsRet = fgets(cmd, sizeof(cmd), stdin);
    if (fgetsRet == NULL) {
      RKADK_LOGE("fgets failed");
      break;
    }

    if (strstr(cmd, "quit") || is_quit) {
      RKADK_LOGP("#Get 'quit' cmd!");
      break;
    } else if (strstr(cmd, "aiisp")) {
      if (bAiispEnable)
        bAiispEnable = false;
      else
        bAiispEnable = true;

      ret = RKADK_MEDIA_SetPostIspAttr(u32CamId, enStreamType, bAiispEnable, &stPostIspAttr);
      if (ret)
        RKADK_LOGE("RKADK_MEDIA_SetPostIspAttr failed");
    } else if (strstr(cmd, "720")) {
      stResCfg.enResType = RKADK_RES_720P;
      stResCfg.enStreamType = enStreamType;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_STREAM_RES, &stResCfg);
      ret = RKADK_STREAM_VideoReset(pHandle);
      if (ret < 0) {
#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
        RKADK_STREAM_VencStop(pHandle);
        RKADK_STREAM_VideoDeInit(pHandle);
        pHandle = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(u32CamId);
        goto stream;
#endif
#endif
      }
      RKADK_STREAM_VencStart(pHandle, -1);
    } else if (strstr(cmd, "480")) {
      stResCfg.enResType = RKADK_RES_480P;
      stResCfg.enStreamType = enStreamType;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_STREAM_RES, &stResCfg);
      ret = RKADK_STREAM_VideoReset(pHandle);
      if (ret < 0) {
#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
        RKADK_STREAM_VencStop(pHandle);
        RKADK_STREAM_VideoDeInit(pHandle);
        pHandle = NULL;
#ifdef RKAIQ
        SAMPLE_ISP_Stop(u32CamId);
        goto stream;
#endif
#endif
      }
      RKADK_STREAM_VencStart(pHandle, -1);
    }

#if 0
    if (strstr(cmd, "fps")) {
      int fpsTest = 20;
      RKADK_PARAM_GOP_S stGopCfg;
      RKADK_S32 s32VencChn;

      // set ini fps
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_FPS, &fpsTest);

      // set ini gop
      stGopCfg.enStreamType = enStreamType;
      stGopCfg.u32Gop = fpsTest;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_GOP, &stGopCfg);

      // set aiq fps
      SAMPLE_ISP_SET_FrameRate(u32CamId, fpsTest);

      // set mpp fps and gop
      s32VencChn =
          RKADK_PARAM_GetVencChnId(u32CamId, enStreamType);
      RK_MPI_VENC_SetGop(s32VencChn, stGopCfg.u32Gop);
      RK_MPI_VENC_SetFps(s32VencChn, fpsTest, 1, fpsTest, 1);

      // get ini fps and gop
      RKADK_PARAM_GetCamParam(u32CamId, RKADK_PARAM_TYPE_FPS, &fpsTest);
      RKADK_LOGP("fps: %d", fpsTest);
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_GOP, &stGopCfg);
      RKADK_LOGP("gop: %d", stGopCfg.u32Gop);
    }
#endif
    usleep(500000);
  }

  RKADK_LOGP("exit!");

  VideoDeInitTest(pHandle);
  if (g_vStreamType == 2)
    VideoDeInitTest(pHandleLive);

  if (bMultiSensor) {
    VideoDeInitTest(pHandle1);
    if (g_vStreamType == 2)
      VideoDeInitTest(pHandleLive1);
  }

#ifdef RKAIQ
  SAMPLE_ISP_Stop(u32CamId);
  if (bMultiCam)
    SAMPLE_ISP_Stop(1);
#endif

  if (g_output_file) {
    fclose(g_output_file);
    g_output_file = NULL;
  }

  if (g_output1_file) {
    fclose(g_output1_file);
    g_output1_file = NULL;
  }

  return 0;
}

static RKADK_S32 AencDataCb(RKADK_AUDIO_STREAM_S *pAStreamData) {
  if (!g_output_file) {
    g_output_file = fopen(g_output_path, "w");
    if (!g_output_file) {
      RKADK_LOGE("open %s file failed, exit", g_output_path);
      return -1;
    }
  }

  fwrite(pAStreamData->pStream, 1, pAStreamData->u32Len, g_output_file);
  RKADK_LOGP("#Write Aenc seq: %d, pts: %lld, size: %zu", pAStreamData->u32Seq,
             pAStreamData->u64TimeStamp, pAStreamData->u32Len);

  return 0;
}

static RKADK_S32 PcmDataCb(RKADK_AUDIO_STREAM_S *pAStreamData) {

  if (!g_pcm_file) {
    g_pcm_file = fopen("/data/ai.pcm", "w");
    if (!g_pcm_file) {
      RKADK_LOGE("open /data/ai.pcm file failed, exit");
      return -1;
    }
  }

  fwrite(pAStreamData->pStream, 1, pAStreamData->u32Len, g_pcm_file);
  RKADK_LOGP("#Write pcm seq: %d, pts: %lld, size: %zu", pAStreamData->u32Seq,
             pAStreamData->u64TimeStamp, pAStreamData->u32Len);

  return 0;
}

static int AudioTest(RKADK_U32 u32CamId) {
  RKADK_S32 ret;
  RKADK_MW_PTR pHandle = NULL;
  RKADK_STREAM_AUDIO_ATTR_S stAudioAttr;
  RKADK_AUDIO_INFO_S stAudioInfo;

  memset(&stAudioAttr, 0, sizeof(RKADK_STREAM_AUDIO_ATTR_S));
  stAudioAttr.u32CamId = u32CamId;
  stAudioAttr.enCodecType = g_enCodecType;
  stAudioAttr.pfnPcmDataCB = PcmDataCb;
  stAudioAttr.pfnAencDataCB = AencDataCb;
  ret = RKADK_STREAM_AudioInit(&stAudioAttr, &pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_STREAM_AudioInit failed = %d", ret);
    return ret;
  }

  RKADK_STREAM_GetAudioInfo(pHandle, &stAudioInfo);
  RKADK_LOGP("stAudioInfo.enCodecType: %d", stAudioInfo.enCodecType);
  RKADK_LOGP("stAudioInfo.u16SampleBitWidth: %d", stAudioInfo.u16SampleBitWidth);
  RKADK_LOGP("stAudioInfo.u32AvgBytesPerSec: %d", stAudioInfo.u32AvgBytesPerSec);
  RKADK_LOGP("stAudioInfo.u32ChnCnt: %d", stAudioInfo.u32ChnCnt);
  RKADK_LOGP("stAudioInfo.u32SampleRate: %d", stAudioInfo.u32SampleRate);
  RKADK_LOGP("stAudioInfo.u32SamplesPerFrame: %d", stAudioInfo.u32SamplesPerFrame);

  ret = RKADK_STREAM_AencStart(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_STREAM_AencStart failed");
    return -1;
  }

  RKADK_LOGP("initial finish");
  signal(SIGINT, sigterm_handler);

  char cmd[64];
  while (!is_quit) {
    fgets(cmd, sizeof(cmd), stdin);
    if (strstr(cmd, "quit")) {
      RKADK_LOGP("#Get 'quit' cmd!");
      break;
    } else if (strstr(cmd, "start")) {
      RKADK_STREAM_AencStart(pHandle);
    } else if (strstr(cmd, "stop")) {
      RKADK_STREAM_AencStop(pHandle);
    }

    usleep(500000);
  }

  ret = RKADK_STREAM_AencStop(pHandle);
  if (ret) {
    RKADK_LOGE("RKADK_STREAM_AencStop failed");
  }

  ret = RKADK_STREAM_AudioDeInit(pHandle);
  if (ret)
    RKADK_LOGE("RKADK_STREAM_AudioDeInit failed = %d", ret);

  if (g_pcm_file) {
    fclose(g_pcm_file);
    g_pcm_file = NULL;
  }

  if (g_output_file) {
    fclose(g_output_file);
    g_output_file = NULL;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  RKADK_U32 u32CamId = 0;
  RKADK_CHAR *pMode = "audio";
  RKADK_CHAR *pIqfilesPath = IQ_FILE_PATH;
  int c, inCmd = 0;
  const char *iniPath = NULL;
  char path[RKADK_PATH_LEN];
  char sensorPath[RKADK_MAX_SENSOR_CNT][RKADK_PATH_LEN];
  RKADK_BOOL bMultiCam = RKADK_FALSE;
  RKADK_BOOL bMultiSensor = RK_FALSE;

  while ((c = getopt(argc, argv, optstr)) != -1) {
    const char *tmp_optarg = optarg;
    switch (c) {
    case 'a':
      if (!optarg && NULL != argv[optind] && '-' != argv[optind][0]) {
        tmp_optarg = argv[optind++];
      }

      if (tmp_optarg)
        pIqfilesPath = (char *)tmp_optarg;
      break;
    case 'I':
      u32CamId = atoi(optarg);
      break;
    case 'M':
      pMode = optarg;
      break;
    case 'e':
      if (!strcmp(optarg, "pcm")) {
        g_enCodecType = RKADK_CODEC_TYPE_PCM;
        g_output_path = "/data/test.pcm";
      } else if (!strcmp(optarg, "g711a")) {
        g_enCodecType = RKADK_CODEC_TYPE_G711A;
        g_output_path = "/data/test.g711a";
      } else if (!strcmp(optarg, "g711u")) {
        g_enCodecType = RKADK_CODEC_TYPE_G711U;
        g_output_path = "/data/aenc.g711u";
      } else if (!strcmp(optarg, "mp3")) {
        g_enCodecType = RKADK_CODEC_TYPE_MP3;
        g_output_path = "/data/test.mp3";
      } else {
        RKADK_LOGE("unknow encode type: %s", optarg);
        return 0;
      }
      RKADK_LOGP("Encode type: %d", g_enCodecType);
      break;
    case 'o':
      g_output_path = optarg;
      RKADK_LOGP("g_output_path = %s", g_output_path);
      break;
    case 'p':
      iniPath = optarg;
      RKADK_LOGP("iniPath: %s", iniPath);
      break;
    case 'm':
      inCmd = atoi(optarg);
      if (inCmd == 1) {
        bMultiCam = RKADK_TRUE;
        bMultiSensor = RKADK_TRUE;
      } else if (inCmd == 2)
        bMultiSensor = RKADK_TRUE;
      break;
    case 's':
      g_vStreamType = atoi(optarg);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      optind = 0;
      return 0;
    }
  }
  optind = 0;

  RKADK_LOGP("#Test mode: %s", pMode);
  RKADK_LOGP("#Out path: %s", g_output_path);
  RKADK_LOGP("#g_vStreamType: %d", g_vStreamType);

  RKADK_MPI_SYS_Init();

  if (iniPath) {
    memset(path, 0, RKADK_PATH_LEN);
    memset(sensorPath, 0, RKADK_MAX_SENSOR_CNT * RKADK_PATH_LEN);
    sprintf(path, "%s/rkadk_setting.ini", iniPath);
    for (int i = 0; i < RKADK_MAX_SENSOR_CNT; i++)
      sprintf(sensorPath[i], "%s/rkadk_setting_sensor_%d.ini", iniPath, i);

    /*
    lg:
      char *sPath[] = {"/data/rkadk/rkadk_setting_sensor_0.ini",
      "/data/rkadk/rkadk_setting_sensor_1.ini", NULL};
    */
    char *sPath[] = {sensorPath[0], sensorPath[1], NULL};

    RKADK_PARAM_Init(path, sPath);
  } else {
    RKADK_PARAM_Init(NULL, NULL);
  }

  if (!strcmp(pMode, "audio"))
    AudioTest(u32CamId);
  else if (!strcmp(pMode, "video"))
    VideoTest(u32CamId, pIqfilesPath, bMultiCam, bMultiSensor);
  else {
    RKADK_LOGE("Invalid test mode: %s", pMode);
    return -1;
  }
  RKADK_MPI_SYS_Exit();
  RKADK_PARAM_Deinit();
  return 0;
}
