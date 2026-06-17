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

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_photo.h"
#include "rkadk_thumb.h"

#define THUMB_TEST_SAVE_FILE
//#define THUMB_ONLY_TEST_JPG

extern int optind;
extern char *optarg;

static bool is_quit = false;
static RKADK_CHAR optstr[] = "i:t:T:f:W:H:D:b:sh";

typedef enum {
  RKADK_THUMB_TEST_TYPE_MP4 = 0,
  RKADK_THUMB_TEST_TYPE_TS,
  RKADK_THUMB_TEST_TYPE_FLV,
  RKADK_THUMB_TEST_TYPE_JPG,
} RKADK_THUMB_TEST_FILE_TYPE_E;

static void print_usage(const RKADK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-i /tmp/xxx.mp4] [-t 0]\n", name);
  printf("\t-i: test file\n");
  printf("\t-T: thumbnail type, default JPG, options: NV12, JPG, RGB565, RGBA8888, BGRA8888\n");
  printf("\t-t: JPG thumbnail type, default MFP1, options: DCF, MFP1, MFP2\n");
  printf("\t-f: file type, default mp4, options: mp4, jpg, ts, flv\n");
  printf("\t-W: thumbnail width, default obtained from ini\n");
  printf("\t-H: thumbnail height, default obtained from ini\n");
  printf("\t-s: Whether the vdec channels is created and destroyed separately\n");
  printf("\t-D: thumb save directory, use to repair thumbnail, Default: NULL\n");
  printf("\t-b: whether is built-in thumbnail to the file, Default: 1, options: 0(not build-in), 1(build-in thumbnail)\n");
}

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  is_quit = true;
}

int main(int argc, char *argv[]) {
  int c;
  RKADK_U32 u32CamId = 0;
  unsigned int count = 1;
  RKADK_CHAR *pInuptPath = "/userdata/RecordTest_0.mp4";
  char filePath[RKADK_MAX_FILE_PATH_LEN];
  RKADK_JPG_THUMB_TYPE_E eJpgThumbType = RKADK_JPG_THUMB_TYPE_MFP1;
  RKADK_THUMB_TEST_FILE_TYPE_E enFileType = RKADK_THUMB_TEST_TYPE_MP4;
  const char *postfix = "jpg";
  RKADK_THUMB_CHN_ATTR_S stThumbChnAttr;
  RKADK_THUMB_PARAM_S stThumbParam;

#ifdef THUMB_ONLY_TEST_JPG
  int buf_size = 1024 * 1024;
  RKADK_U32 size = buf_size;
  RKADK_U8 *buffer = NULL;;
#else
  RKADK_THUMB_ATTR_S stThumbAttr;
  memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stThumbAttr.s32VdecChn = -1;
  stThumbAttr.s32VpssGrp = -1;
  stThumbAttr.s32VpssChn = -1;
  stThumbAttr.bChnExCreate = RKADK_FALSE;
  stThumbAttr.pstThumbParam = &stThumbParam;

  memset(&stThumbParam, 0, sizeof(RKADK_THUMB_PARAM_S));
  stThumbParam.bBuildInThm = true;
#endif

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'i':
      pInuptPath = optarg;
      break;
    case 'f':
      if (strstr(optarg, "jpg"))
        enFileType = RKADK_THUMB_TEST_TYPE_JPG;
      if (strstr(optarg, "ts"))
        enFileType = RKADK_THUMB_TEST_TYPE_TS;
      if (strstr(optarg, "flv"))
        enFileType = RKADK_THUMB_TEST_TYPE_FLV;
      break;
#ifdef THUMB_ONLY_TEST_JPG
    case 'W':
    case 'H':
    case 'T':
      break;
#else
    case 'W':
      stThumbAttr.u32Width = atoi(optarg);
      stThumbAttr.u32VirWidth = stThumbAttr.u32Width;
      break;
    case 'H':
      stThumbAttr.u32Height = atoi(optarg);
      stThumbAttr.u32VirHeight = stThumbAttr.u32Height;
      break;
    case 'T':
      if (strstr(optarg, "NV12")) {
        stThumbAttr.enType = RKADK_THUMB_TYPE_NV12;
        postfix = "yuv";
      } else if (strstr(optarg, "RGB565")) {
        stThumbAttr.enType = RKADK_THUMB_TYPE_RGB565;
        postfix = "rgb565";
      } else if (strstr(optarg, "RGBA8888")) {
        stThumbAttr.enType = RKADK_THUMB_TYPE_RGBA8888;
        postfix = "rgba8888";
      } else if (strstr(optarg, "BGRA8888")) {
        stThumbAttr.enType = RKADK_THUMB_TYPE_BGRA8888;
        postfix = "bgra8888";
      }
      break;
#endif
    case 't':
      if (strstr(optarg, "DCF"))
        eJpgThumbType = RKADK_JPG_THUMB_TYPE_DCF;
      else if (strstr(optarg, "MFP2"))
        eJpgThumbType = RKADK_JPG_THUMB_TYPE_MFP2;
      break;
    case 's':
      stThumbAttr.bChnExCreate = RKADK_TRUE;
      break;
    case 'D':
      stThumbParam.pThumbDir = optarg;
      RKADK_LOGP("# stThumbParam.pThumbDir: %s", stThumbParam.pThumbDir);
      break;
    case 'b':
      stThumbParam.bBuildInThm = (bool)atoi(optarg);
      RKADK_LOGP("# stThumbParam.bBuildInThm: %d", stThumbParam.bBuildInThm);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      optind = 0;
      return 0;
    }
  }
  optind = 0;

  if (!pInuptPath) {
    RKADK_LOGE("Please input test file");
    return -1;
  }

  RKADK_LOGP("# get thm file: %s", pInuptPath);

  signal(SIGINT, sigterm_handler);

  RKADK_MPI_SYS_Init();
  RKADK_PARAM_Init(NULL, NULL);

#ifndef THUMB_ONLY_TEST_JPG
  if (stThumbAttr.bChnExCreate) {
    RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
    if (pstThumbCfg == NULL) {
      RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
      return -1;
    }

    memset(&stThumbChnAttr, 0, sizeof(RKADK_THUMB_CHN_ATTR_S));
    stThumbChnAttr.s32VdecChn = -1;
    stThumbChnAttr.s32VpssGrp = -1;
    stThumbChnAttr.s32VpssChn = -1;
    stThumbChnAttr.u32SrcWidth = pstThumbCfg->thumb_width;
    stThumbChnAttr.u32SrcHeight = pstThumbCfg->thumb_height;
    stThumbChnAttr.enDstType = stThumbAttr.enType;
    stThumbChnAttr.u32DstWidth = stThumbAttr.u32Width;
    stThumbChnAttr.u32DstHeight = stThumbAttr.u32Height;
    if (RKADK_GetThmChnCreate(u32CamId, &stThumbChnAttr)) {
      RKADK_LOGE("RKADK_GetThmChnCreate failed");
      return -1;
    }
  }
#endif

  while (!is_quit) {
#ifdef THUMB_ONLY_TEST_JPG
    size = buf_size;
    buffer = (RKADK_U8 *)malloc(buf_size);
    RKADK_LOGD("malloc thumb buffer: %p", buffer);

    if (enFileType == RKADK_THUMB_TEST_TYPE_MP4) {
      if (RKADK_GetThmInMp4(u32CamId, pInuptPath, buffer, &size)) {
        RKADK_LOGE("RKADK_GetThmInMp4 failed");
        RKADK_LOGD("free thumb buffer: %p", buffer);
        free(buffer);
        return -1;
      }
    } else if (enFileType == RKADK_THUMB_TEST_TYPE_JPG) {
      RKADK_LOGP("eJpgThumbType: %d", eJpgThumbType);
      if (RKADK_PHOTO_GetThmInJpg(u32CamId, pInuptPath, eJpgThumbType, buffer, &size)) {
        RKADK_LOGE("RKADK_PHOTO_GetThmInJpg failed");
        RKADK_LOGD("free thumb buffer: %p", buffer);
        free(buffer);
        return -1;
      }
    }
    RKADK_LOGP("%s: size: %d, count: %d", postfix, size, count);

#ifdef THUMB_TEST_SAVE_FILE
    if (size > 0) {
      FILE *file = NULL;
      sprintf(filePath, "/tmp/thm_test_%u.%s", count, "jpg");
      file = fopen(filePath, "w");
      if (!file) {
        RKADK_LOGE("Create file(%s) failed", filePath);
        RKADK_LOGD("free thumb buffer: %p", buffer);
        free(buffer);
        return -1;
      }

      fwrite(buffer, 1, size, file);
      fclose(file);
      RKADK_LOGP("save %s done", filePath);
    }
#endif

    RKADK_LOGD("sfree thumb buffer: %p", buffer);
    free(buffer);
#else
    if (enFileType == RKADK_THUMB_TEST_TYPE_MP4) {
      if (RKADK_GetThmInMp4Ex(u32CamId, pInuptPath, &stThumbAttr)) { //can use RKADK_GetThmInFile
        RKADK_LOGE("RKADK_GetThmInMp4Ex failed");
        goto exit;
      }
    } else if (enFileType == RKADK_THUMB_TEST_TYPE_JPG) {
      RKADK_LOGP("eJpgThumbType: %d", eJpgThumbType);
      if (RKADK_PHOTO_GetThmInJpgEx(u32CamId, pInuptPath, eJpgThumbType, &stThumbAttr)) {
        RKADK_LOGE("RKADK_PHOTO_GetThmInJpgEx failed");
        goto exit;
      }
    } else {
      if (RKADK_GetThmInFile(u32CamId, pInuptPath, &stThumbAttr)) {
        RKADK_LOGE("RKADK_GetThmInFile failed");
        goto exit;
      }
    }
    RKADK_LOGP("%s[%d, %d, %d, %d]: size: %d, count: %d", postfix,
               stThumbAttr.u32Width, stThumbAttr.u32Height,
               stThumbAttr.u32VirWidth, stThumbAttr.u32VirHeight,
               stThumbAttr.u32BufSize, count);

#ifdef THUMB_TEST_SAVE_FILE
    if (stThumbAttr.u32BufSize > 0) {
      FILE *file = NULL;
      sprintf(filePath, "/tmp/thm_test_%u.%s", count, postfix);
      file = fopen(filePath, "w");
      if (!file) {
        RKADK_LOGE("Create file(%s) failed", filePath);
        goto exit;
      }

      fwrite(stThumbAttr.pu8Buf, 1, stThumbAttr.u32BufSize, file);
      fclose(file);
      RKADK_LOGP("save %s done", filePath);
    }
#endif

    if (enFileType == RKADK_THUMB_TEST_TYPE_JPG) {
      RKADK_PHOTO_ThumbBufFree(&stThumbAttr);
    } else {
      RKADK_ThmBufFree(&stThumbAttr);
    }
#endif
    count--;
    if (count <= 0)
      break;

    usleep(2 * 1000 * 1000);
  }

exit:
#ifndef THUMB_ONLY_TEST_JPG
  if (stThumbAttr.bChnExCreate) {
    if (RKADK_GetThmChnDestory(u32CamId, &stThumbChnAttr))
      RKADK_LOGE("RKADK_GetThmChnDestory failed");
  }
#endif

  RKADK_MPI_SYS_Exit();
  RKADK_PARAM_Deinit();
  RKADK_LOGE("Exit");
  return 0;
}
