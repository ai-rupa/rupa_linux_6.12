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

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_record.h"
#include "rkadk_disp.h"
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

static RKADK_CHAR optstr[] = "a:I:p:t:f:s:W:H:kh";

static const char *g_fileType = "mp4";
static bool is_quit = false;
#define IQ_FILE_PATH "/etc/iqfiles"

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-a /etc/iqfiles] [-I 0] [-t 0]\n", name);
  printf("\t-a: enable aiq with dirpath provided, eg:-a "
         "/oem/etc/iqfiles/, Default /etc/iqfiles,"
         "without this option aiq should run in other application\n");
  printf("\t-I: Camera id, Default:0\n");
  printf("\t-p: param ini directory path, Default:/data/rkadk\n");
  printf("\t-k: key frame fragment, Default: disable\n");
  printf("\t-t: get thumbnail delay time(ms) when enbale file cache, Default:3000(ms)\n");
  printf("\t-f: file type, Default: mp4, options: mp4, ts, flv\n");
  printf("\t-s: wbc source type, Default: 0(DEV), options: 1(VIDEO), 2(GRAPHIC))\n");
  printf("\t-W: wbc target width, default display width\n");
  printf("\t-H: wbc target height, default display height\n");
}

static RKADK_S32
GetRecordFileName(RKADK_MW_PTR pRecorder, RKADK_U32 u32FileCnt,
                  RKADK_CHAR (*paszFilename)[RKADK_MAX_FILE_PATH_LEN]) {
  static RKADK_U32 u32FileIdx = 0;

  RKADK_LOGP("u32FileCnt:%d, pRecorder:%p", u32FileCnt, pRecorder);

  if (u32FileIdx >= 4)
    u32FileIdx = 0;

  for (RKADK_U32 i = 0; i < u32FileCnt; i++) {
    sprintf(paszFilename[i], "/tmp/RecordTest_wbc_%u.%s", u32FileIdx, g_fileType);
    u32FileIdx++;
  }

  return 0;
}

static RKADK_VOID
RecordEventCallback(RKADK_MW_PTR pRecorder,
                    const RKADK_MUXER_EVENT_INFO_S *pstEventInfo) {
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
    printf("+++++ RKADK_MUXER_EVENT_FILE_WRITING_SLOW +++++\n");
    break;
  case RKADK_MUXER_EVENT_ERR_CARD_NONEXIST:
    printf("+++++ RKADK_MUXER_EVENT_ERR_CARD_NONEXIST +++++\n");
    break;
  default:
    printf("+++++ Unknown event(%d) +++++\n", pstEventInfo->enEvent);
    break;
  }
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

int main(int argc, char *argv[]) {
  int c, ret;
  RKADK_S32 s32CamId = 0;
  RKADK_RECORD_ATTR_S stRecAttr;
  RKADK_MW_PTR pRecorder = NULL;
  RKADK_REC_MANUAL_SPLIT_ATTR_S stSplitAttr;
  const char *iniPath = NULL;
  RKADK_PARAM_FPS_S stFps;
  char path[RKADK_PATH_LEN];
  char sensorPath[RKADK_MAX_SENSOR_CNT][RKADK_PATH_LEN];
  FILE_CACHE_ARG stFileCacheAttr;
  RKADK_U32 u32GetThumbTime = 0;
  RKADK_RECORD_WBC_ATTR_S stWbcAttr;
  RKADK_PARAM_DISP_CFG_S *pstDispCfg;

#ifdef RKAIQ
  RK_BOOL bMultiCam = RK_FALSE;
  const char *tmp_optarg = optarg;
  SAMPLE_ISP_PARAM stIspParam;

  memset(&stIspParam, 0, sizeof(SAMPLE_ISP_PARAM));
  stIspParam.iqFileDir = IQ_FILE_PATH;
#endif

  memset(&stRecAttr, 0, sizeof(RKADK_RECORD_ATTR_S));
  memset(&stWbcAttr, 0, sizeof(RKADK_RECORD_WBC_ATTR_S));
  stWbcAttr.enSourceType = VO_WBC_SOURCE_DEV; 

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
      s32CamId = atoi(optarg);
      break;
    case 'p':
      iniPath = optarg;
      RKADK_LOGP("iniPath: %s", iniPath);
      break;
    case 'k':
      stRecAttr.u32FragKeyFrame = 1;
      break;
    case 't':
      u32GetThumbTime = atoi(optarg);
      break;
    case 's':
      stWbcAttr.enSourceType = atoi(optarg);
      break;
    case 'f':
      g_fileType = (const char *)optarg;
      RKADK_LOGP("g_fileType: %s", g_fileType);
      break;
    case 'W':
      stWbcAttr.u32Width = atoi(optarg);
      break;
    case 'H':
      stWbcAttr.u32Height = atoi(optarg);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      optind = 0;
      return -1;
    }
  }
  optind = 0;

  RKADK_LOGP("#s32CamId: %d", s32CamId);
  RKADK_LOGP("#u32FragKeyFrame: %d", stRecAttr.u32FragKeyFrame);
  RKADK_LOGP("#u32GetThumbTime: %d", u32GetThumbTime);
  RKADK_LOGP("#file type: %s", g_fileType);
  RKADK_LOGP("#wbc source type: %d", stWbcAttr.enSourceType);
  RKADK_LOGP("#u32Width: %d", stWbcAttr.u32Width);
  RKADK_LOGP("#u32Height: %d", stWbcAttr.u32Height);

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

  pstDispCfg = RKADK_PARAM_GetDispCfg(s32CamId);
  if (!pstDispCfg) {
    RKADK_LOGE("pstDispCfg is null");
    return -1;
  }

#ifdef RKAIQ
  stFps.enStreamType = RKADK_STREAM_TYPE_SENSOR;
  ret = RKADK_PARAM_GetCamParam(s32CamId, RKADK_PARAM_TYPE_FPS, &stFps);
  if (ret) {
    RKADK_LOGE("RKADK_PARAM_GetCamParam u32CamId[%d] fps failed", s32CamId);
    return -1;
  }

  stIspParam.WDRMode = RK_AIQ_WORKING_MODE_NORMAL;
  stIspParam.bMultiCam = bMultiCam;
  stIspParam.fps = stFps.u32Framerate;

  SAMPLE_ISP_Start(s32CamId, stIspParam);
#endif

  memset(&stFileCacheAttr, 0, sizeof(FILE_CACHE_ARG));
  stFileCacheAttr.sdcard_path = "/dev/mmcblk1p1";
  stFileCacheAttr.total_cache = 7 * 1024 * 1024; // 7M
  stFileCacheAttr.write_cache = 1024 * 1024; // 1M
  stFileCacheAttr.write_thread_arg.sched_policy = FILE_SCHED_FIFO;
  stFileCacheAttr.write_thread_arg.priority = 99;
  RKADK_RECORD_FileCacheInit(&stFileCacheAttr);

  stWbcAttr.u32SourceId = pstDispCfg->vo_device;

  if (!stWbcAttr.u32Width)
    stWbcAttr.u32Width = pstDispCfg->width;
  if (!stWbcAttr.u32Height)
    stWbcAttr.u32Height = pstDispCfg->height;

  stWbcAttr.Format = RKADK_FMT_YUV420SP; //nv12
  stWbcAttr.s32FrameRate = pstDispCfg->frame_rate;

  stRecAttr.pstWbcAttr = &stWbcAttr;
  stRecAttr.s32CamID = s32CamId;
  stRecAttr.pfnRequestFileNames = GetRecordFileName;
  stRecAttr.pfnEventCallback = RecordEventCallback;
  stRecAttr.u32GetThumbTime[0] = u32GetThumbTime;
  stRecAttr.u32GetThumbTime[1] = u32GetThumbTime;

  ret = RKADK_DISP_Init(s32CamId);
  if (ret) {
    RKADK_LOGE("s32CamId[%d] Create display failed", ret);
    return -1;
  }

  if (RKADK_RECORD_CreateWbc(&stRecAttr, &pRecorder)) {
    RKADK_LOGE("s32CamId[%d] Create recorder failed", s32CamId);
    RKADK_DISP_DeInit(s32CamId);
#ifdef RKAIQ
    SAMPLE_ISP_Stop(s32CamId);
    if (bMultiCam)
      SAMPLE_ISP_Stop(1);
#endif
    return -1;
  }
  RKADK_RECORD_Start(pRecorder);

  RKADK_LOGP("initial finish\n");

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
      RKADK_PARAM_REC_TIME_S stRecTime;
      stRecTime.enStreamType = RKADK_STREAM_TYPE_VIDEO_MAIN;
      stRecTime.time = 20; //default time

      RKADK_PARAM_GetCamParam(s32CamId, RKADK_PARAM_TYPE_SPLITTIME, &stRecTime);
      stSplitAttr.enManualType = MUXER_PRE_MANUAL_SPLIT;
      stSplitAttr.u32DurationSec = stRecTime.time;
      RKADK_RECORD_ManualSplit(pRecorder, &stSplitAttr);
    } else if (strstr(cmd, "start")) {
      RKADK_RECORD_Start(pRecorder);
    } else if (strstr(cmd, "stop")) {
      RKADK_RECORD_Stop(pRecorder);
    }

    usleep(500000);
  }

  RKADK_RECORD_Stop(pRecorder);
  RKADK_RECORD_DestroyWbc(pRecorder);
  RKADK_DISP_DeInit(s32CamId);

#ifdef RKAIQ
  SAMPLE_ISP_Stop(s32CamId);
#endif

  RKADK_RECORD_FileCacheDeInit();

  RKADK_MPI_SYS_Exit();
  RKADK_PARAM_Deinit();
  RKADK_LOGP("exit!");
  return 0;
}
