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
#include "rkadk_disp.h"
#include "rkadk_stream.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "isp/sample_isp.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  bool bThirdPartyStream;
  RKADK_MW_PTR pHandle;
  RKADK_U32 u32VdecChn[RKADK_DISP_SUB_STREAM_NUM];

  int u32MallocCnt;
  int u32FreeCnt;
} DISP_TEST_CTX_S;

extern int optind;
extern char *optarg;

static bool is_quit = false;
static RKADK_CHAR optstr[] = "a:I:p:w:h:r:THVP";

#define IQ_FILE_PATH "/etc/iqfiles"

static DISP_TEST_CTX_S g_ctx;

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-a /etc/iqfiles] [-I 0]\n", name);
  printf("\t-a: enable aiq with dirpath provided, eg:-a "
         "/oem/etc/iqfiles/, Default /etc/iqfiles,"
         "without this option aiq should run in other application\n");
  printf("\t-I: camera id, Default 0\n");
  printf("\t-p: param ini directory path, Default:/data/rkadk\n");
  printf("\t-w: the width of the physical size of the screen, default rk3576/rk3572/rv1126b = 1080, other chip = 720\n");
  printf("\t-h: the height of the physical size of the screen, default rk3576/rk3572/rv1126b = 1920, other chip = 1280\n");
  printf("\t-r: rotation, Default: 1(90), options: 0(0), 1(90), 2(180), 3(270)\n");
  printf("\t-H: enable horizontal splicing display, Default: disable\n");
  printf("\t-V: enable vertical splicing display, Default: disable\n");
  printf("\t-P: enable picture in picture display, Default: disable\n");
  printf("\t-T: enable process third-party stream, Default: disable\n");
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

static void DispCtxInit() {
  DISP_TEST_CTX_S *pCtx = &g_ctx;

  memset(pCtx, 0, sizeof(DISP_TEST_CTX_S));
  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++)
    pCtx->u32VdecChn[i] = i;
}

static RKADK_S32 DispBufFree(void *buf) {
  DISP_TEST_CTX_S *pCtx = &g_ctx;

  if (buf) {
    free(buf);
    buf = NULL;
    pCtx->u32FreeCnt++;
  }

  return 0;
}

static RKADK_S32 VencDataCb(RKADK_VIDEO_STREAM_S *pVStreamData) {
  int ret = 0;
  RKADK_DISP_DATA_S stData;
  DISP_TEST_CTX_S *pCtx = &g_ctx;

  if (!pVStreamData) {
    RKADK_LOGE("pVStreamData is null");
    return -1;
  }

  for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
    memset(&stData, 0, sizeof(RKADK_DISP_DATA_S));
    stData.bEofFlag = pVStreamData->bEndOfStream;
    stData.u32Len = pVStreamData->astPack.au32Len;
    stData.u64Pts = pVStreamData->astPack.u64PTS;
    stData.u32Seq = pVStreamData->u32Seq;
    stData.bBypass = true;             /* if bypass, must set pFreeCB; */
    stData.pFreeCB = DispBufFree;
    stData.data = (char *)malloc(pVStreamData->astPack.au32Len);
    if (!stData.data) {
      RKADK_LOGE("malloc failed");
      return -1;
    }
    memset(stData.data, 0, pVStreamData->astPack.au32Len);
    memcpy(stData.data, pVStreamData->astPack.apu8Addr, pVStreamData->astPack.au32Len);
    pCtx->u32MallocCnt++;

    ret = RKADK_DISP_SendVideoStream(pCtx->pHandle, pCtx->u32VdecChn[i], &stData);
    if (ret) {
      RKADK_LOGE("RKADK_DISP_SendVideoStream[%d] failed", pCtx->u32VdecChn[i]);
      DispBufFree(stData.data);
    }
  }

  //RKADK_LOGP("#Send seq: %d, pts: %lld, size: %zu", pVStreamData->u32Seq,
  //            pVStreamData->astPack.u64PTS, pVStreamData->astPack.au32Len);

  return 0;
}

int main(int argc, char *argv[]) {
  int c, ret;
  RKADK_U32 u32CamId = 0;
  RKADK_DISP_ATTR_S stDispAttr;
  const char *iniPath = NULL;
  char path[RKADK_PATH_LEN];
  char sensorPath[RKADK_MAX_SENSOR_CNT][RKADK_PATH_LEN];
  bool bVerDisp = false, bHorDisp = false, bPipDisp = false;
  RKADK_DISP_CFG_S stDisExAttr;
  RKADK_MW_PTR pHandle = NULL;
  RKADK_U32 u32Width;
  RKADK_U32 u32Height;
  RKADK_U32 rotation = 1;
  RKADK_MW_PTR pStreamHandle = NULL;
  RKADK_STREAM_VIDEO_ATTR_S stVideoAttr;
  RKADK_VIDEO_INFO_S stVideoInfo;
  RKADK_PARAM_RES_CFG_S stResCfg;
  RKADK_PARAM_CODEC_CFG_S stCodecType;
  DISP_TEST_CTX_S *pCtx = &g_ctx;

#if defined(RK3576) || defined(RK3572) || defined(RV1126B)
  u32Width = 1080;
  u32Height = 1920;
#else
  u32Width = 720;
  u32Height = 1280;
#endif

#ifdef RKAIQ
  RKADK_PARAM_FPS_S stFps;
  const char *tmp_optarg = optarg;
  SAMPLE_ISP_PARAM stIspParam;

  memset(&stIspParam, 0, sizeof(SAMPLE_ISP_PARAM));
  stIspParam.iqFileDir = IQ_FILE_PATH;
#endif

  DispCtxInit();

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
    case 'I':
      u32CamId = atoi(optarg);
      break;
    case 'p':
      iniPath = optarg;
      RKADK_LOGP("iniPath: %s", iniPath);
      break;
    case 'w':
      u32Width = atoi(optarg);
      break;
    case 'h':
      u32Height = atoi(optarg);
      break;
    case 'r':
      rotation = atoi(optarg);
      break;
    case 'H':
      bHorDisp = true;
      break;
    case 'V':
      bVerDisp = true;
      break;
    case 'P':
      bPipDisp = true;
      break;
    case 'T':
      g_ctx.bThirdPartyStream = true;
      break;
    default:
      print_usage(argv[0]);
      optind = 0;
      return 0;
    }
  }
  optind = 0;

  RKADK_LOGP("#camera id: %d", u32CamId);
  RKADK_LOGP("#u32Width: %d", u32Width);
  RKADK_LOGP("#u32Height: %d", u32Height);
  RKADK_LOGP("#rotation: %d", rotation);
  RKADK_LOGP("#bHorDisp: %d", bHorDisp);
  RKADK_LOGP("#bVerDisp: %d", bVerDisp);
  RKADK_LOGP("#bPipDisp: %d", bPipDisp);
  RKADK_LOGP("#bThirdPartyStream: %d", g_ctx.bThirdPartyStream);

  memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
  stDispAttr.stVpssCropRect.u32X = 120;
  stDispAttr.stVpssCropRect.u32Y = 120;
  stDispAttr.stVoRect.u32X = 120;
  stDispAttr.stVoRect.u32Y = 120;
  if (rotation == 1 || rotation == 3) {
    stDispAttr.stVpssCropRect.u32Width = u32Width;
    stDispAttr.stVpssCropRect.u32Height = u32Height / 2;
    stDispAttr.stVoRect.u32Width = u32Height / 2;
    stDispAttr.stVoRect.u32Height = u32Width;
  } else {
    stDispAttr.stVpssCropRect.u32Width = u32Width;
    stDispAttr.stVpssCropRect.u32Height = u32Height / 2;
    stDispAttr.stVoRect.u32Width = u32Width / 2;
    stDispAttr.stVoRect.u32Height = u32Height;
  }

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

#ifdef RKAIQ
  stFps.enStreamType = RKADK_STREAM_TYPE_SENSOR;
  ret = RKADK_PARAM_GetCamParam(u32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
  if (ret) {
    RKADK_LOGE("RKADK_PARAM_GetCamParam fps failed");
    return -1;
  }

  stIspParam.WDRMode = RK_AIQ_WORKING_MODE_NORMAL;
  stIspParam.bMultiCam = false;
  stIspParam.fps = stFps.u32Framerate;
  SAMPLE_ISP_Start(u32CamId, stIspParam);
  RKADK_BUFINFO("isp[%d] init", u32CamId);
#endif

  memset(&stDisExAttr, 0, sizeof(stDisExAttr));
  if (g_ctx.bThirdPartyStream) {
    memset(&stVideoAttr, 0, sizeof(RKADK_STREAM_VIDEO_ATTR_S));
    stVideoAttr.pfnDataCB = VencDataCb;
    stVideoAttr.u32CamId = u32CamId;

    ret = RKADK_STREAM_VideoInit(&stVideoAttr, &pStreamHandle);
    if (ret) {
      RKADK_LOGE("RKADK_STREAM_VideoInit[%d] failed", u32CamId);
      return -1;
    }

    RKADK_STREAM_GetVideoInfo(u32CamId, &stVideoInfo);
    RKADK_LOGP("stVideoInfo.enCodecType: %d", stVideoInfo.enCodecType);
    RKADK_LOGP("stVideoInfo.u32BitRate: %d", stVideoInfo.u32BitRate);
    RKADK_LOGP("stVideoInfo.u32FrameRate: %d", stVideoInfo.u32FrameRate);
    RKADK_LOGP("stVideoInfo.u32Gop: %d", stVideoInfo.u32Gop);
    RKADK_LOGP("stVideoInfo.u32Width: %d", stVideoInfo.u32Width);
    RKADK_LOGP("stVideoInfo.u32Height: %d", stVideoInfo.u32Height);

    // Init Vdec
    for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
      stDisExAttr.stSubStream[i].stVdecCfg.u32VdecChn = pCtx->u32VdecChn[i];
      stDisExAttr.stSubStream[i].stVdecCfg.u32SrcWidth = stVideoInfo.u32Width;
      stDisExAttr.stSubStream[i].stVdecCfg.u32SrcHeight = stVideoInfo.u32Height;
      stDisExAttr.stSubStream[i].stVdecCfg.enCodecType = stVideoInfo.enCodecType;
      //stDisExAttr.stSubStream[i].stVdecCfg.u32DecodeMode = VIDEO_MODE_FRAME;
      //stDisExAttr.stSubStream[i].stVdecCfg.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;
    }
  }

  stDisExAttr.u32CamId = u32CamId;
  if (bVerDisp) {
    // CamId 0 display and CamId 0 sub-record vertical splicing display
    stDisExAttr.stSubStream[0].bEnable = true;
    stDisExAttr.stSubStream[0].s32CamId = 0;
    stDisExAttr.stSubStream[0].enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
    stDisExAttr.stSubStream[0].stVoCfg.u32VoChn = 1;
    stDisExAttr.stSubStream[0].stVoCfg.rotation = rotation;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32X = u32Width / 2;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Y = 0;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Width = u32Width / 2;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Height = u32Height;
    ret = RKADK_DISP_InitEx(&stDisExAttr, &pHandle);
  } else if (bHorDisp) {
    // CamId 0 display and CamId 1 display horizontal splicing display
    // vo attr read from ini file when RKADK_STREAM_TYPE_DISP
    stDisExAttr.stSubStream[0].bEnable = true;
    stDisExAttr.stSubStream[0].s32CamId = 1;
    stDisExAttr.stSubStream[0].enStreamType = RKADK_STREAM_TYPE_DISP;
    ret = RKADK_DISP_InitEx(&stDisExAttr, &pHandle);
  } else if (bPipDisp || g_ctx.bThirdPartyStream) {
    // CamId 0 display and CamId 0 sub-record vertical splicing display
    stDisExAttr.stSubStream[0].bEnable = true;
    stDisExAttr.stSubStream[0].s32CamId = 0;
    if (g_ctx.bThirdPartyStream)
      stDisExAttr.stSubStream[0].enStreamType = RKADK_STREAM_TYPE_THIRD_STREAM;
    else
      stDisExAttr.stSubStream[0].enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
    stDisExAttr.stSubStream[0].stVoCfg.u32VoChn = 1;
    stDisExAttr.stSubStream[0].stVoCfg.rotation = rotation;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32X = 0;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Y = 0;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Width = u32Width / 2;
    stDisExAttr.stSubStream[0].stVoCfg.stVoRect.u32Height = u32Height / 2;

    stDisExAttr.stSubStream[1].bEnable = true;
    stDisExAttr.stSubStream[1].s32CamId = 0;
    if (g_ctx.bThirdPartyStream)
      stDisExAttr.stSubStream[1].enStreamType = RKADK_STREAM_TYPE_THIRD_STREAM;
    else
      stDisExAttr.stSubStream[1].enStreamType = RKADK_STREAM_TYPE_VIDEO_SUB;
    stDisExAttr.stSubStream[1].stVoCfg.u32VoChn = 2;
    stDisExAttr.stSubStream[1].stVoCfg.rotation = rotation;
    stDisExAttr.stSubStream[1].stVoCfg.stVoRect.u32X = u32Width / 4 + 100;
    stDisExAttr.stSubStream[1].stVoCfg.stVoRect.u32Y = u32Height / 4 + 100;
    stDisExAttr.stSubStream[1].stVoCfg.stVoRect.u32Width = u32Width / 4;
    stDisExAttr.stSubStream[1].stVoCfg.stVoRect.u32Height = u32Height / 4;

    ret = RKADK_DISP_InitEx(&stDisExAttr, &pHandle);
  } else {
    ret = RKADK_DISP_Init(u32CamId);
  }
  if (ret) {
    RKADK_LOGE("RKADK_DISP_Init failed(%d)", ret);
#ifdef RKAIQ
    SAMPLE_ISP_Stop(u32CamId);
#endif
    return -1;
  }

  pCtx->pHandle = pHandle;
  if (g_ctx.bThirdPartyStream) {
    ret = RKADK_STREAM_VencStart(pStreamHandle, -1);
    if (ret) {
      RKADK_LOGE("RKADK_STREAM_VencStart[%d] failed", u32CamId);
      return -1;
    }
  }

  signal(SIGINT, sigterm_handler);
  char cmd[64];
  printf("\n#Usage: input 'quit' to exit programe!\n"
         "peress any other key to quit\n");

  while (!is_quit) {
    fgets(cmd, sizeof(cmd), stdin);
    if (strstr(cmd, "quit") || is_quit) {
      RKADK_LOGP("#Get 'quit' cmd!");
      break;
    } else if (strstr(cmd, "full")) {
      memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
      stDispAttr.stVpssCropRect.u32X = 0;
      stDispAttr.stVpssCropRect.u32Y = 0;
      stDispAttr.stVpssCropRect.u32Width = u32Height;
      stDispAttr.stVpssCropRect.u32Height = u32Width;
      stDispAttr.stVoRect.u32X = 0;
      stDispAttr.stVoRect.u32Y = 0;
      stDispAttr.stVoRect.u32Width = u32Width;
      stDispAttr.stVoRect.u32Height = u32Height;
      RKADK_DISP_SetAttr(u32CamId, &stDispAttr);
    } else if (strstr(cmd, "move")) {
      memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
      stDispAttr.stVoRect.u32X = 100;
      stDispAttr.stVoRect.u32Y = 100;
      stDispAttr.stVoRect.u32Width = u32Width / 2;
      stDispAttr.stVoRect.u32Height = u32Height / 2;
      RKADK_DISP_SetAttr(u32CamId, &stDispAttr);
    } else if (strstr(cmd, "crop")) { //crop and adjust the display area
      memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
      stDispAttr.stVpssCropRect.u32X = 0;
      stDispAttr.stVpssCropRect.u32Y = 0;
      stDispAttr.stVpssCropRect.u32Width = u32Height / 4;
      stDispAttr.stVpssCropRect.u32Height = u32Width / 4;

      if (bHorDisp) {
        stDispAttr.stSubStreamAttr[0].s32Priority = -1;
        stDispAttr.stSubStreamAttr[0].stVpssCropRect.u32X = 0;
        stDispAttr.stSubStreamAttr[0].stVpssCropRect.u32Y = 0;
        stDispAttr.stSubStreamAttr[0].stVpssCropRect.u32Width = u32Height / 4;
        stDispAttr.stSubStreamAttr[0].stVpssCropRect.u32Height = u32Width / 4;
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);
      } else {
        RKADK_DISP_SetAttr(u32CamId, &stDispAttr);
      }
    } else if (strstr(cmd, "reset-1")) {
      if (bPipDisp || g_ctx.bThirdPartyStream) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = 5;
        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].s32Priority = -1; // not change priority
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_BUTT; // not change codec type
        }
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);
      }
    } else if (strstr(cmd, "reset-2")) {
      if (bPipDisp || g_ctx.bThirdPartyStream) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = 0;
        stDispAttr.stSubStreamAttr[0].s32Priority = 2;
        stDispAttr.stSubStreamAttr[0].stVoRect.u32Width = u32Width / 4;
        stDispAttr.stSubStreamAttr[0].stVoRect.u32Height = u32Height / 4;
        stDispAttr.stSubStreamAttr[1].s32Priority = 1;
        stDispAttr.stSubStreamAttr[1].stVoRect.u32X = 200;
        stDispAttr.stSubStreamAttr[1].stVoRect.u32Y = 200;
        stDispAttr.stSubStreamAttr[1].stVoRect.u32Width = u32Width / 2;
        stDispAttr.stSubStreamAttr[1].stVoRect.u32Height = u32Height / 2;

        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_BUTT; // not change codec type
        }

        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);
      }
    } else if (g_ctx.bThirdPartyStream && strstr(cmd, "720")) {                      // third-party stream resolution change
      stResCfg.enResType = RKADK_RES_720P;
      stResCfg.enStreamType = RKADK_STREAM_TYPE_PREVIEW;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_STREAM_RES, &stResCfg);

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
      RKADK_STREAM_VencStop(pStreamHandle);
      RKADK_STREAM_VideoDeInit(pStreamHandle);
      pStreamHandle = NULL;

      ret = RKADK_STREAM_VideoInit(&stVideoAttr, &pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoInit[%d] failed", u32CamId);
#else
      ret = RKADK_STREAM_VideoReset(pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoReset[%d] failed", u32CamId);
#endif

      if (!ret) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = -1;                      // not change priority
        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].s32Priority = -1; // not change priority
          stDispAttr.stSubStreamAttr[i].stVdecAttr.u32SrcWidth = RKADK_WIDTH_720P;
          stDispAttr.stSubStreamAttr[i].stVdecAttr.u32SrcHeight = RKADK_HEIGHT_720P;
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_BUTT; // not change codec type
        }
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);

        RKADK_STREAM_VencStart(pStreamHandle, -1);
      }
    } else if (g_ctx.bThirdPartyStream && strstr(cmd, "480")) {                     // third-party stream resolution change
      stResCfg.enResType = RKADK_RES_480P;
      stResCfg.enStreamType = RKADK_STREAM_TYPE_PREVIEW;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_STREAM_RES, &stResCfg);

#if !defined(RV1106_1103) && !defined(RV1103B) && !defined(RV1126B)
      RKADK_STREAM_VencStop(pStreamHandle);
      RKADK_STREAM_VideoDeInit(pStreamHandle);
      pStreamHandle = NULL;

      ret = RKADK_STREAM_VideoInit(&stVideoAttr, &pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoInit[%d] failed", u32CamId);
#else
      ret = RKADK_STREAM_VideoReset(pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoReset[%d] failed", u32CamId);
#endif

      if (!ret) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = -1;                      // not change priority
        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].s32Priority = -1; // not change priority
          stDispAttr.stSubStreamAttr[i].stVdecAttr.u32SrcWidth = RKADK_WIDTH_480P;
          stDispAttr.stSubStreamAttr[i].stVdecAttr.u32SrcHeight = RKADK_HEIGHT_480P;
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_BUTT; // not change codec type
        }
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);

        RKADK_STREAM_VencStart(pStreamHandle, -1);
      }
    } else if (g_ctx.bThirdPartyStream && strstr(cmd, "264")) {                      // third-party stream codec type change
      stCodecType.enCodecType = RKADK_CODEC_TYPE_H264;
      stCodecType.enStreamType = RKADK_STREAM_TYPE_PREVIEW;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);

#if !defined(RV1106_1103) && !defined(RV1103B)
      RKADK_STREAM_VencStop(pStreamHandle);
      RKADK_STREAM_VideoDeInit(pStreamHandle);
      pStreamHandle = NULL;

      ret = RKADK_STREAM_VideoInit(&stVideoAttr, &pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoInit[%d] failed", u32CamId);
#else
      ret = RKADK_STREAM_VideoReset(pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoReset h264 failed");
#endif

      if (!ret) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = -1;                      // not change priority
        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].s32Priority = -1; // not change priority
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_H264;
        }
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);

        RKADK_STREAM_VencStart(pStreamHandle, -1);
      }
    } else if (g_ctx.bThirdPartyStream && strstr(cmd, "265")) {                      // third-party stream codec type change
      stCodecType.enCodecType = RKADK_CODEC_TYPE_H265;
      stCodecType.enStreamType = RKADK_STREAM_TYPE_PREVIEW;
      RKADK_PARAM_SetCamParam(u32CamId, RKADK_PARAM_TYPE_CODEC_TYPE, &stCodecType);

#if !defined(RV1106_1103) && !defined(RV1103B)
      RKADK_STREAM_VencStop(pStreamHandle);
      RKADK_STREAM_VideoDeInit(pStreamHandle);
      pStreamHandle = NULL;

      ret = RKADK_STREAM_VideoInit(&stVideoAttr, &pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoInit[%d] failed", u32CamId);
#else
      ret = RKADK_STREAM_VideoReset(pStreamHandle);
      if (ret)
        RKADK_LOGE("RKADK_STREAM_VideoReset h265 failed");
#endif

      if (!ret) {
        memset(&stDispAttr, 0, sizeof(RKADK_DISP_ATTR_S));
        stDispAttr.s32Priority = -1;                      // not change priority
        for (int i = 0; i < RKADK_DISP_SUB_STREAM_NUM; i++) {
          stDispAttr.stSubStreamAttr[i].s32Priority = -1; // not change priority
          stDispAttr.stSubStreamAttr[i].stVdecAttr.enCodecType = RKADK_CODEC_TYPE_H265;
        }
        RKADK_DISP_SetAttrEx(&stDispAttr, pHandle);

        RKADK_STREAM_VencStart(pStreamHandle, -1);
      }
    }

    usleep(500000);
  }

  if (g_ctx.bThirdPartyStream) {
    RKADK_STREAM_VencStop(pStreamHandle);
    RKADK_STREAM_VideoDeInit(pStreamHandle);
  }

  if (bHorDisp || bVerDisp || bPipDisp || g_ctx.bThirdPartyStream)
    RKADK_DISP_DeInitEx(pHandle);
  else
    RKADK_DISP_DeInit(u32CamId);

#ifdef RKAIQ
  SAMPLE_ISP_Stop(u32CamId);
#endif
  RKADK_MPI_SYS_Exit();
  RKADK_PARAM_Deinit();

  RKADK_LOGP("Exit, u32MallocCnt[%d] u32FreeCnt[%d]", pCtx->u32MallocCnt, pCtx->u32FreeCnt);
  return 0;
}
