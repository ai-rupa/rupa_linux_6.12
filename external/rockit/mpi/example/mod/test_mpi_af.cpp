/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#ifndef HAVE_C_COMPILER
#include <sys/poll.h>
#else
#include "rk_osal.h"
#endif
#include <signal.h>
#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_comm_af.h"
#include "rk_mpi_af.h"
#include "test_comm_argparse.h"

typedef enum rkTEST_FILTER_TYPE_E {
    TEST_FILTER_RESAMPLE = 0,
    TEST_FILTER_3A       = 1,
    TEST_FILTER_BUTT
} TEST_FILTER_TYPE_E;

typedef enum rkTEST_FILTER_SED_TYPE_E {
    TEST_FILTER_SED_MODULE_AGC = (1 << 0),
    TEST_FILTER_SED_MODULE_AED = (1 << 1),
    TEST_FILTER_SED_MODULE_SED = (1 << 2),
    TEST_FILTER_SED_MODULE_FIR = (1 << 3),
    TEST_FILTER_SED_MODULE_BUTT
} TEST_FILTER_SED_MODULE_E;

typedef struct _rkTEST_AF_CTX_S {
    RK_S32      s32LoopCount;
    const char *srcFilePath;
    const char *dstFilePath;
    const char *chAfType;
    const char *agCfgsPath;
    RK_S32      s32InputRate;
    RK_S32      s32OutRate;
    RK_S32      s32InputCh;
    RK_S32      s32OutCh;
    RK_S32      s32InputBitWidth;
    RK_S32      s32OutputBitWidth;
    RK_S32      s32InputChnLayout;
    RK_S32      s32InputRefLayout;
    RK_S32      s32InputRecLayout;
    RK_S32      s32ChnNum;
    RK_S32      s32ChnIndex;
    RK_S32      bBlock;
    RK_S32      s32ReadSize;
    RK_S32      s32CacheSize;
    RK_S32      s32Volume;
    RK_S32      s32Update;
    RK_S32      s32VqeType;
    const char *reloadCfgPath;
    const char *pBcdModelPath;
    RK_S32      s32AedCnt;
    RK_S32      s32LsdCnt;
    RK_S32      s32BcdCnt;
    RK_S32      s32FdGetEnable;
    RK_S32      s32DevFd;
} TEST_AF_CTX_S;

static RK_BOOL gAfExit = RK_FALSE;
static void sigterm_handler(int sig) {
    RK_PRINT("Catched SIGINT %d\n", sig);
    gAfExit = RK_TRUE;
}

static AUDIO_BIT_WIDTH_E find_af_bit_width(RK_S32 bit) {
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
        RK_LOGE("bitwidth(%d) not support", bit);
        return AUDIO_BIT_WIDTH_BUTT;
    }

    return bitWidth;
}

static AUDIO_SOUND_MODE_E find_af_sound_mode(RK_S32 ch) {
    AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
    switch (ch) {
      case 1:
        channel = AUDIO_SOUND_MODE_MONO;
        break;
      case 2:
        channel = AUDIO_SOUND_MODE_STEREO;
        break;
      default:
        channel = (AUDIO_SOUND_MODE_E)ch;
    }

    return channel;
}

RK_S32 test_init_af_resample(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_RESAMPLE;
    attr->stResAttr.u32InRate      = ctx->s32InputRate;
    attr->stResAttr.u32InChn       = ctx->s32InputCh;
    attr->stResAttr.enInBitWidth   = find_af_bit_width(ctx->s32InputBitWidth);

    attr->stResAttr.u32OutRate     = ctx->s32OutRate;
    attr->stResAttr.u32OutChn      = ctx->s32OutCh;
    attr->stResAttr.enOutBitWidth  = find_af_bit_width(ctx->s32OutputBitWidth);
    return RK_SUCCESS;
}

RK_S32 test_init_af_ag(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_3A;
    snprintf(reinterpret_cast<char *>(attr->st3AAttr.cfgPath),
            sizeof(attr->st3AAttr.cfgPath), "%s", ctx->agCfgsPath);
    attr->st3AAttr.u32SampleRate = ctx->s32InputRate;
    attr->st3AAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
    attr->st3AAttr.u32Channels = ctx->s32InputCh;
    attr->st3AAttr.u32ChnLayout = ctx->s32InputChnLayout;
    attr->st3AAttr.u32RecLayout = ctx->s32InputRecLayout;
    attr->st3AAttr.u32RefLayout = ctx->s32InputRefLayout;
    attr->st3AAttr.enTpye       = (AUDIO_AF_VQE_TYPE_E)ctx->s32VqeType;
    return RK_SUCCESS;
}

static RK_S32 test_init_af_map(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_MAP;
    attr->stMapAttr.u32SampleRate = ctx->s32InputRate;
    attr->stMapAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
    attr->stMapAttr.u32InChn = ctx->s32InputCh;
    attr->stMapAttr.u8MapOutChns = ctx->s32OutCh;
    return RK_SUCCESS;
}

static RK_S32 test_init_af_cache(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_CACHE;
    attr->stCacheAttr.stBasicAttr.u32SampleRate = ctx->s32InputRate;
    attr->stCacheAttr.stBasicAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
    attr->stCacheAttr.stBasicAttr.u32Channel = ctx->s32InputCh;
    attr->stCacheAttr.s32Size = ctx->s32CacheSize;
    return RK_SUCCESS;
}

static RK_S32 test_init_af_volume(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_VOLUME;
    attr->stVolumeAttr.stBasicAttr.u32SampleRate = ctx->s32InputRate;
    attr->stVolumeAttr.stBasicAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
    attr->stVolumeAttr.stBasicAttr.u32Channel = ctx->s32InputCh;
    attr->stVolumeAttr.s32Volume = ctx->s32Volume;
    return RK_SUCCESS;
}

static RK_S32 test_init_af_sed(TEST_AF_CTX_S *ctx, AF_ATTR_S *attr) {
    attr->enType = AUDIO_FILTER_SED;
    attr->stSedAttr.stBasicAttr.u32SampleRate = ctx->s32InputRate;
    attr->stSedAttr.stBasicAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
    attr->stSedAttr.stBasicAttr.u32Channel = ctx->s32InputCh;

    attr->stSedAttr.s32ModelEnabled |= TEST_FILTER_SED_MODULE_AGC;
    attr->stSedAttr.stAgcParam.fAttackTime = 200.0;
    attr->stSedAttr.stAgcParam.fReleaseTime = 400.0;
    attr->stSedAttr.stAgcParam.fMaxGain = 30;
    attr->stSedAttr.stAgcParam.fMaxPeak = -3.0;
    attr->stSedAttr.stAgcParam.fRk0 = 2;
    attr->stSedAttr.stAgcParam.fRth0 = -70;
    attr->stSedAttr.stAgcParam.fRth1 = -45;
    attr->stSedAttr.stAgcParam.fRth2 = -40;
    attr->stSedAttr.stAgcParam.fAttenuateTime = 1000;
    attr->stSedAttr.stAgcParam.fRk1 = 0.8;
    attr->stSedAttr.stAgcParam.fRk2 = 0.4;
    attr->stSedAttr.stAgcParam.fLineGainDb = -25.0f;
    attr->stSedAttr.stAgcParam.s32SwSmL0 = 40;
    attr->stSedAttr.stAgcParam.s32SwSmL1 = 80;
    attr->stSedAttr.stAgcParam.s32SwSmL2 = 80;


    attr->stSedAttr.s32ModelEnabled |= TEST_FILTER_SED_MODULE_AED;
    attr->stSedAttr.stAedParam.fSnrDB = 10.0f;
    attr->stSedAttr.stAedParam.fLsdDB = -35.0f;
    attr->stSedAttr.stAedParam.s32Policy = 1;
    attr->stSedAttr.stAedParam.fSmoothParam = 10.0f;

    attr->stSedAttr.s32ModelEnabled |= TEST_FILTER_SED_MODULE_SED;
    attr->stSedAttr.stSedLenParam.s32FrameLen = 60;
    attr->stSedAttr.stSedLenParam.s32Nclass = 1;
    attr->stSedAttr.stSedLenParam.fBabycryConfirmProb = 0.7f;
    attr->stSedAttr.stSedLenParam.s32BabyCryDecisionLen = 35;
    if (ctx->pBcdModelPath != RK_NULL)
        memcpy(attr->stSedAttr.stSedLenParam.fBcdModelPath, ctx->pBcdModelPath, strlen(ctx->pBcdModelPath));

    attr->stSedAttr.s32ModelEnabled |= TEST_FILTER_SED_MODULE_FIR;

    switch (attr->stSedAttr.stBasicAttr.u32Channel) {
    case 4:
        // just for example: 2mic + 2ref
        attr->stSedAttr.s64RecChannelType = 0x03;
        break;
    case 6:
        // just for example: 4mic + 2ref
        attr->stSedAttr.s64RecChannelType = 0x0f;
        break;
    case 8:
        // just for example: 6mic + 2ref
        attr->stSedAttr.s64RecChannelType = 0x3f;
        break;
    default:
        // by default is 1mic + 1ref, it will be set by internal if is not specified.
        attr->stSedAttr.s64RecChannelType = 0x01;
        break;
    }

    return RK_SUCCESS;
}

RK_S32 test_init_af(TEST_AF_CTX_S *ctx) {
    RK_S32 ret = 0;
    AF_ATTR_S attr;
    memset(&attr, 0, sizeof(AF_ATTR_S));
    attr.u32InBufCount  = 2;
    attr.u32OutBufCount = 2;
    attr.u32Depth = 2;
    attr.u32MaxInputSize = 8192;

    if (!ctx->chAfType) {
        RK_LOGE("unsupport type = %s", 0, ctx->chAfType);
        return RK_FAILURE;
    }

    if (strstr(ctx->chAfType, "resample")) {
        test_init_af_resample(ctx, &attr);
    } else if (strstr(ctx->chAfType, "3a")) {
        test_init_af_ag(ctx, &attr);
    } else if (strstr(ctx->chAfType, "map")) {
        test_init_af_map(ctx, &attr);
    } else if (strstr(ctx->chAfType, "cache")) {
        test_init_af_cache(ctx, &attr);
    } else if (strstr(ctx->chAfType, "volume")) {
        test_init_af_volume(ctx, &attr);
    } else if (strstr(ctx->chAfType, "sed")) {
        test_init_af_sed(ctx, &attr);
    } else {
        RK_LOGE("unsupport type = %s", 0, ctx->chAfType);
        return RK_FAILURE;
    }

    ret = RK_MPI_AF_Create(ctx->s32ChnIndex, &attr);
    if (ret != RK_SUCCESS) {
        RK_LOGE("create af chn %d err:0x%x", 0, ret);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 test_deinit_af(TEST_AF_CTX_S *ctx) {
    if (ctx != RK_NULL) {
        RK_MPI_AF_Destroy(ctx->s32ChnIndex);
    }
    return RK_SUCCESS;
}

static void* send_af_thread(void * ptr) {
    TEST_AF_CTX_S *params = reinterpret_cast<TEST_AF_CTX_S *>(ptr);
    MB_POOL_CONFIG_S pool_config;
    // set default value for struct
    RK_U8 *srcData = RK_NULL;
    AUDIO_FRAME_S frame;
    RK_U64 timeStamp = 0;
    RK_S32 s32MilliSec = 1000;
    RK_S32 size = 0;
    RK_S32 result = 0;
    RK_U32 seq = 0;

    AUDIO_FRAME_S stFrame;
    memset(&stFrame, 0, sizeof(AUDIO_FRAME_S));

    AUDIO_FRAME_INFO_S inFrmInfo;
    memset(&inFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));

    FILE *file = RK_NULL;
    if (params->srcFilePath) {
        file = fopen(params->srcFilePath, "rb");
    }

    if (file == RK_NULL) {
        RK_LOGE("open save file %s failed because %s.", params->srcFilePath, strerror(errno));
        goto __EXIT;
    }

    srcData = reinterpret_cast<RK_U8 *>(calloc(params->s32ReadSize, sizeof(RK_U8)));
    if (!srcData)
        goto __EXIT;

    while (!gAfExit) {
        memset(srcData, 0, params->s32ReadSize);
        size = fread(srcData, 1, params->s32ReadSize, file);

        stFrame.u32Len = size;
        stFrame.enBitWidth = AUDIO_BIT_WIDTH_16;
        stFrame.u64TimeStamp = timeStamp;
        stFrame.u32Seq = seq;
        stFrame.enSoundMode = find_af_sound_mode(params->s32InputCh);
        stFrame.bBypassMbBlk = RK_FALSE;

        MB_EXT_CONFIG_S extConfig;
        memset(&extConfig, 0, sizeof(extConfig));
        extConfig.pOpaque = srcData;
        extConfig.pu8VirAddr = srcData;
        extConfig.u64Size = size;
        RK_MPI_SYS_CreateMB(&(stFrame.pMbBlk), &extConfig);

        inFrmInfo.pstFrame = &stFrame;
        inFrmInfo.u32Id = 0;

        result = RK_MPI_AF_SendFrame(params->s32ChnIndex, &inFrmInfo, s32MilliSec);
        if (result != RK_SUCCESS) {
            RK_LOGE("send frame error, seq = %d", seq);
        }
        RK_MPI_MB_ReleaseMB(stFrame.pMbBlk);
        timeStamp += size * 1000000LL / (params->s32InputCh * 2 *params->s32InputRate);
        seq++;

        if (params->s32Update)
            usleep(10000);

        if (size <= 0) {
            RK_LOGI("eof");
            break;
        }
    }

__EXIT:
    if (file) {
        fclose(file);
        file = RK_NULL;
    } else {
        gAfExit = RK_TRUE;
    }

    if (srcData)
        free(srcData);
    return RK_NULL;
}

static void* receive_af_thread(void * ptr) {
    TEST_AF_CTX_S *params = reinterpret_cast<TEST_AF_CTX_S *>(ptr);
    MB_POOL_CONFIG_S pool_config;
    // set default value for struct
    AUDIO_FRAME_S frame;
    RK_U64 timeStamp = 0, prePts = 0;
    RK_S32 s32MilliSec = 1000;
    RK_S32 size = 0;
    RK_S32 result = 0;
    RK_U32 seq = 0;
    RK_S32 eos = 0;
    RK_VOID *outdata = RK_NULL;
    fd_set readfds, writefds;
    struct timeval timeout;
    AF_EXTRA_INFO_S stExtraInfo;
    memset(&stExtraInfo, 0, sizeof(AF_EXTRA_INFO_S));

    const RK_CHAR *sed = strstr(params->chAfType, "sed");
    AUDIO_FRAME_S stFrame;
    memset(&stFrame, 0, sizeof(AUDIO_FRAME_S));

    AUDIO_FRAME_INFO_S outFrmInfo;
    memset(&outFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));

    if (params->s32FdGetEnable) {
        RK_LOGI("%s - fd get enable", __func__);
        params->s32DevFd = RK_MPI_AF_GetFd(params->s32ChnIndex);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }

    FILE *file = RK_NULL;
    if (params->dstFilePath) {
        RK_CHAR     aFilePath[MAX_AUDIO_FILE_PATH_LEN];
        snprintf(aFilePath, sizeof(aFilePath), "%s", params->dstFilePath);

        if (params->s32ChnIndex > 0)
            snprintf(aFilePath, sizeof(aFilePath), "%s%d", params->dstFilePath, params->s32ChnIndex);

        file = fopen(aFilePath, "wb+");
        if (file == RK_NULL) {
            RK_LOGE("failed to open output file %s, error: %s.", aFilePath, strerror(errno));
            goto __EXIT;
        } else {
            RK_LOGI("open output file %s", aFilePath);
        }
    }

    if (file == RK_NULL) {
        RK_LOGE("open save file %s failed because %s.", params->srcFilePath, strerror(errno));
        goto __EXIT;
    }

    while (!gAfExit) {
        outFrmInfo.pstFrame = RK_NULL;
        if (!params->s32FdGetEnable) {
            if (sed)
                result = RK_MPI_AF_GetFrameExtraInfo(params->s32ChnIndex, &outFrmInfo, s32MilliSec, &stExtraInfo);
            else
                result = RK_MPI_AF_GetFrame(params->s32ChnIndex, &outFrmInfo, s32MilliSec);
        } else {
            FD_ZERO(&readfds);
            FD_SET(params->s32DevFd, &readfds);
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            result = select(params->s32DevFd + 1, &readfds, NULL, NULL, &timeout);
            if (result == -1) {
                perror("select");
                exit(EXIT_FAILURE);
            } else if (result) {
                if (FD_ISSET(params->s32DevFd, &readfds)) {
                    if (sed)
                        result = RK_MPI_AF_GetFrameExtraInfo(params->s32ChnIndex, &outFrmInfo, s32MilliSec, &stExtraInfo);
                    else
                        result = RK_MPI_AF_GetFrame(params->s32ChnIndex, &outFrmInfo, s32MilliSec);
                }
            } else {
                printf("fd get timeout occurred %d\n", result);
                continue;
            }
        }

        if (result != RK_SUCCESS) {
            RK_LOGE("get frame error, seq = %d", seq);
        } else {
            outdata = RK_MPI_MB_Handle2VirAddr(outFrmInfo.pstFrame->pMbBlk);
            size = outFrmInfo.pstFrame->u32Len;
            eos = (size <= 0) ? 1 : 0;
            RK_LOGI("get chn(%d) frame (size: %d, outdata: %p, seq: %d, pts: %llu, dPts: %lld)",
                    params->s32ChnIndex, size, outdata, outFrmInfo.pstFrame->u32Seq, outFrmInfo.pstFrame->u64TimeStamp,
                    outFrmInfo.pstFrame->u64TimeStamp - prePts);

            if (sed) {
                if (stExtraInfo.bAed)
                    params->s32AedCnt++;

                if (stExtraInfo.bLsd)
                    params->s32LsdCnt++;

                if (stExtraInfo.bBcd)
                    params->s32BcdCnt++;
                RK_LOGI("extra: %d, aed: %d, lsd: %d, lsd result: %f bcd: %d",
                        stExtraInfo.bExtra, stExtraInfo.bAed,
                        stExtraInfo.bLsd, stExtraInfo.fLsdResult, stExtraInfo.bBcd);
            }

            prePts = outFrmInfo.pstFrame->u64TimeStamp;
            if (file != RK_NULL && size >= 0) {
                fwrite(outdata, 1, size, file);
            }

            RK_MPI_AF_ReleaseFrame(params->s32ChnIndex, &outFrmInfo);
        }
        if (eos) {
            RK_LOGI("get eos stream.");
            break;
        }
    }

__EXIT:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }

    if (params->s32Update)
        gAfExit = RK_TRUE;
    return RK_NULL;
}

static void* update_af_thread(void * ptr) {
    TEST_AF_CTX_S *ctx = reinterpret_cast<TEST_AF_CTX_S *>(ptr);
    int cnt = 0;
    while(!gAfExit) {
        if (strstr(ctx->chAfType, "volume")) {
            int volume = 0;
            RK_MPI_AF_GetVolume(ctx->s32ChnIndex, &volume);
            RK_LOGI("get volume = %d", volume);
            if (cnt % 3 == 0)
                volume = 100;
            else if (cnt % 3 == 1)
                volume = 50;
            else if (cnt % 3 == 2)
                volume = 0;

            RK_MPI_AF_SetVolume(ctx->s32ChnIndex, volume);
        } else if (strstr(ctx->chAfType, "3a")) {
            AF_3A_ATTR_S st3AAttr = {0};
            st3AAttr.u32SampleRate = ctx->s32InputRate;
            st3AAttr.enBitWidth = find_af_bit_width(ctx->s32InputBitWidth);
            st3AAttr.u32Channels = ctx->s32InputCh;
            st3AAttr.u32ChnLayout = ctx->s32InputChnLayout;
            st3AAttr.u32RecLayout = ctx->s32InputRecLayout;
            st3AAttr.u32RefLayout = ctx->s32InputRefLayout;
            st3AAttr.enTpye       = (AUDIO_AF_VQE_TYPE_E)ctx->s32VqeType;
            if (cnt % 2 == 0)
                snprintf(reinterpret_cast<char *>(st3AAttr.cfgPath),
                        sizeof(st3AAttr.cfgPath), "%s", ctx->agCfgsPath);
            else
                snprintf(reinterpret_cast<char *>(st3AAttr.cfgPath),
                        sizeof(st3AAttr.cfgPath), "%s", ctx->reloadCfgPath);

            RK_MPI_AF_ReloadVqe(ctx->s32ChnIndex, &st3AAttr);
        } else {
            RK_LOGE("unsupport type = %s", 0, ctx->chAfType);
            break;
        }

        cnt++;
        usleep(1000000);
    }

    return NULL;
}

RK_S32 unit_test_mpi_af(TEST_AF_CTX_S *ctx) {
    RK_S32 i = 0;
    TEST_AF_CTX_S afCtx[AF_MAX_CHN_NUM] = {0};
    pthread_t tidSend[AF_MAX_CHN_NUM] = {0};
    pthread_t tidReceive[AF_MAX_CHN_NUM] = {0};
    pthread_t tidUpdate[AF_MAX_CHN_NUM] = {0};
    ctx->bBlock = RK_TRUE;
    if (ctx->s32ChnNum > AF_MAX_CHN_NUM) {
        RK_LOGE("af chn(%d) > max_chn(%d)", ctx->s32ChnNum, AF_MAX_CHN_NUM);
        goto __FAILED;
    }
    for (i = 0; i < ctx->s32ChnNum; i++) {
        memcpy(&afCtx[i], ctx, sizeof(TEST_AF_CTX_S));
        afCtx[i].s32ChnIndex = i;
        if (test_init_af(&afCtx[i]) == RK_FAILURE) {
            goto __FAILED;
        }

        pthread_create(&tidSend[i], RK_NULL, send_af_thread, reinterpret_cast<void *>(&afCtx[i]));
        pthread_create(&tidReceive[i], RK_NULL, receive_af_thread, reinterpret_cast<void *>(&afCtx[i]));

        if (ctx->s32Update) {
            pthread_create(&tidUpdate[i], RK_NULL, update_af_thread, reinterpret_cast<void *>(&afCtx[i]));
        }
    }

    for (i = 0; i < ctx->s32ChnNum; i++) {
        pthread_join(tidSend[i], RK_NULL);
        pthread_join(tidReceive[i], RK_NULL);
        if (ctx->s32Update) {
            pthread_join(tidUpdate[i], RK_NULL);
        }


        if (strstr(ctx->chAfType, "sed")) {
            RK_LOGI("cnt: aed(%d), lsd(%d), bcd(%d)", afCtx[i].s32AedCnt,
                    afCtx[i].s32LsdCnt, afCtx[i].s32BcdCnt);
        }

        test_deinit_af(&afCtx[i]);
    }

    return RK_SUCCESS;
__FAILED:

    return RK_FAILURE;
}

static const char *const usages[] = {
    "./rk_mpi_af_test [-i src_path] [-o dst_patch] [--af_type af_type] [--input_rate rate] [--input_ch ch]...",
    NULL,
};

void mpi_af_test_show_options(const TEST_AF_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("loop count            : %d\n", ctx->s32LoopCount);
    RK_PRINT("input  file name      : %s\n", ctx->srcFilePath);
    RK_PRINT("output file name      : %s\n", ctx->dstFilePath);
    RK_PRINT("af type               : %s\n", ctx->chAfType);
    RK_PRINT("af config patch       : %s\n", ctx->agCfgsPath);
    RK_PRINT("input rate            : %d\n", ctx->s32InputRate);
    RK_PRINT("output rate           : %d\n", ctx->s32OutRate);
    RK_PRINT("input channel         : %d\n", ctx->s32InputCh);
    RK_PRINT("output channel        : %d\n", ctx->s32OutCh);
    RK_PRINT("input bitwidth        : %d\n", ctx->s32InputBitWidth);
    RK_PRINT("output bitwidth       : %d\n", ctx->s32OutputBitWidth);
    RK_PRINT("input chnLayout       : %d\n", ctx->s32InputChnLayout);
    RK_PRINT("input refLayout       : %d\n", ctx->s32InputRefLayout);
    RK_PRINT("input recLayout       : %d\n", ctx->s32InputRecLayout);
    RK_PRINT("read size             : %d\n", ctx->s32ReadSize);
    RK_PRINT("cache size            : %d\n", ctx->s32CacheSize);
    RK_PRINT("volume                : %d\n", ctx->s32Volume);
    RK_PRINT("update                : %d\n", ctx->s32Update);
    RK_PRINT("vqe type              : %d\n", ctx->s32VqeType);
    RK_PRINT("reload config patch   : %s\n", ctx->reloadCfgPath);
    RK_PRINT("fd get enable         : %d\n", ctx->s32FdGetEnable);
}

int main(int argc, const char **argv) {
    RK_S32          i;
    RK_S32          s32Ret;
    TEST_AF_CTX_S  *ctx;

    ctx = reinterpret_cast<TEST_AF_CTX_S *>(malloc(sizeof(TEST_AF_CTX_S)));
    memset(ctx, 0, sizeof(TEST_AF_CTX_S));

    ctx->s32LoopCount       = 1;
    ctx->s32ChnNum          = 1;
    ctx->srcFilePath        = RK_NULL;
    ctx->dstFilePath        = RK_NULL;
    ctx->chAfType           = RK_NULL;
    ctx->agCfgsPath         = RK_NULL;
    ctx->s32InputRate       = 0;
    ctx->s32OutRate         = 0;
    ctx->s32InputCh         = 0;
    ctx->s32OutCh           = 0;
    ctx->s32InputBitWidth   = 0;
    ctx->s32OutputBitWidth  = 0;
    ctx->s32InputChnLayout  = 0;
    ctx->s32InputRefLayout  = 0;
    ctx->s32InputRecLayout  = 0;
    ctx->s32ReadSize        = 1024;
    ctx->s32Volume          = 100;
    ctx->s32FdGetEnable     = 0;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input",  &(ctx->srcFilePath),
                   "input file name , e.g.(./*.pcm). <required>", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx->dstFilePath),
                    "output file name, e.g.(./ao). default(required).", NULL, 0, 0),
        OPT_STRING('\0', "af_type", &(ctx->chAfType),
                    "set audio filter type, default(required)", NULL, 0, 0),
        OPT_STRING('\0', "3a_cfgs", &(ctx->agCfgsPath),
                    "3a configs patch, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "input_rate", &(ctx->s32InputRate),
                    "the sample rate of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_ch", &(ctx->s32InputCh),
                    "the channels of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_bit", &(ctx->s32InputBitWidth),
                    "the bit width of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "out_rate", &(ctx->s32OutRate),
                    "the sample rate of out data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "out_ch", &(ctx->s32OutCh),
                    "the channels of out data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "out_bit", &(ctx->s32OutputBitWidth),
                    "the bit width of out data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_ch_layout", &(ctx->s32InputChnLayout),
                    "the channels layout of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_rec_layout", &(ctx->s32InputRecLayout),
                    "the mic channels layout of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_ref_layout", &(ctx->s32InputRefLayout),
                    "the ref channels layout of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx->s32LoopCount),
                    "loop running count. can be any count. default(1)", NULL, 0, 0),
        OPT_INTEGER('r', "read_size", &(ctx->s32ReadSize),
                    "read size. default(1024)", NULL, 0, 0),
        OPT_INTEGER('\0', "cache_size", &(ctx->s32CacheSize),
                    "cache size. default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "volume", &(ctx->s32Volume),
                    "volume. default(100)", NULL, 0, 0),
        OPT_INTEGER('\0', "update", &(ctx->s32Update),
                    "enable update. default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "vqe_type", &(ctx->s32VqeType),
                    "vqe type. default(0)", NULL, 0, 0),
        OPT_STRING('\0', "reload_cfgs", &(ctx->reloadCfgPath),
                    "reload configs patch, default(NULL)", NULL, 0, 0),
        OPT_STRING('\0', "bcd_model", &(ctx->pBcdModelPath),
                    "the bcd config file, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "fd_get_enable", &(ctx->s32FdGetEnable),
                    "the fd get enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "chn_num", &(ctx->s32ChnNum),
                    "the chn num. default(1).", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_af_test_show_options(ctx);
    RK_LOGI("ctx->srcFilePath : %s, ctx->chAfType: %s, ctx->s32InputRate : %d, ctx->s32InputCh: %d",
            ctx->srcFilePath, ctx->chAfType, ctx->s32InputRate, ctx->s32InputCh);
    if (ctx->srcFilePath == RK_NULL
        || ctx->chAfType == RK_NULL
        || ctx->s32InputRate <= 0
        || ctx->s32InputCh <= 0) {
        argparse_usage(&argparse);
        goto __FAILED;
    }

    signal(SIGINT, sigterm_handler);
    RK_MPI_SYS_Init();

    for (i = 0; i < ctx->s32LoopCount; i++) {
        RK_LOGI("start running loop count  = %d", i);
        s32Ret = unit_test_mpi_af(ctx);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }
        RK_LOGI("end running loop count  = %d", i);
    }

__FAILED:
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }

    RK_MPI_SYS_Exit();
    return 0;
}
