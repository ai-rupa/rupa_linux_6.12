/*
 * Copyright 2022 Rockchip Electronics Co. LTD
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
#include <pthread.h>
#include <signal.h>
#include "rk_debug.h"
#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_amix.h"
#include "test_comm_argparse.h"

typedef struct _rkTEST_AMIX_CTX_S {
    RK_S32      s32DevId;
    RK_S32      s32PipeId;
    RK_S32      s32ChnId;
    const char *psControlName;
    char       *psControlValue;
    RK_BOOL     bListControls;
    RK_BOOL     bListContents;
    RK_S32      s32PipeNum;
    RK_S32      s32ChnNum;
    const char *srcFilePath[AMIX_MAX_PIPE_NUM];
    const char *dstFilePath[AMIX_MAX_CHN_NUM];
    RK_S32      s32InSampleRate;
    RK_U32      u32InChannel;
    RK_S32      s32OutSampleRate;
    RK_U32      u32OutChannel;
    RK_S32      s32PeriodSize;
    RK_BOOL     bUpdate;
    RK_FLOAT    fDB;
} TEST_AMIX_CTX_S;

static RK_BOOL gAmixExit = RK_FALSE;
static void sigterm_handler(int sig) {
    RK_PRINT("Catched SIGINT %d\n", sig);
    gAmixExit = RK_TRUE;
}

static RK_S32 unit_test_mpi_amix(TEST_AMIX_CTX_S *ctx) {
    RK_S32 i = 0, ret = 0;

    if (ctx == RK_NULL)
        return RK_FAILURE;

    if (ctx->psControlName && ctx->psControlValue) {
        ret = RK_MPI_AMIX_SetControl(ctx->s32DevId, ctx->psControlName, ctx->psControlValue);
        if (ret) {
            RK_LOGE("RK_MPI_AMIX_SetControl failed, ret = %X", ret);
            return RK_FAILURE;
        }
    } else if (ctx->psControlName && !ctx->psControlValue) {
        char value[64] = {0};
        // Usage 1: Print the current selected value of control
        ret = RK_MPI_AMIX_GetControl(ctx->s32DevId, ctx->psControlName, value);
        if (ret) {
            RK_LOGE("RK_MPI_AMIX_GetControl failed, ret = %X", ret);
            return RK_FAILURE;
        }

        RK_PRINT("Get current control value: %s\n", value);
        // Usage 2: List the values of control directly with the NULL value param
        RK_PRINT("List the control values:\n");
        ret = RK_MPI_AMIX_GetControl(ctx->s32DevId, ctx->psControlName, NULL);
        if (ret) {
            RK_LOGE("RK_MPI_AMIX_GetControl failed, ret = %X", ret);
            return RK_FAILURE;
        }
    }

    if (ctx->bListControls)
        RK_MPI_AMIX_ListControls(ctx->s32DevId);

    if (ctx->bListContents)
        RK_MPI_AMIX_ListContents(ctx->s32DevId);

    return RK_SUCCESS;
}

static AUDIO_SOUND_MODE_E amix_find_sound_mode(RK_S32 ch) {
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

static void* send_amix_thread(void *ptr) {
    TEST_AMIX_CTX_S *ctx = reinterpret_cast<TEST_AMIX_CTX_S *>(ptr);
    MB_POOL_CONFIG_S pool_config;
    // set default value for struct
    RK_U8 *srcData = RK_NULL;
    AUDIO_FRAME_S frame;
    RK_U64 timeStamp = 0;
    RK_S32 s32MilliSec = 1000;
    RK_S32 size = 0, readLen = 1024;
    RK_S32 result = 0;
    RK_U32 seq = 0;
    RK_S32 pipe = ctx->s32PipeId;
    RK_U32 timeout = ctx->s32PeriodSize * 1000 * 1000 / ctx->s32InSampleRate - 1000;
    const char *path = NULL;
    AMIX_UPDATE_PIPE_ATTR_S  stPipeUpdateAttr, stGetPipeUpdateAttr;
    memset(&stPipeUpdateAttr,  0, sizeof(stPipeUpdateAttr));
    memset(&stGetPipeUpdateAttr,  0, sizeof(stGetPipeUpdateAttr));

    AUDIO_FRAME_S stFrame;
    memset(&stFrame, 0, sizeof(AUDIO_FRAME_S));

    FILE *file = RK_NULL;
    if (pipe < AMIX_MAX_PIPE_NUM) {
        path = ctx->srcFilePath[pipe];
    } else {
        RK_LOGE("pipe(%d) is unsupport.", pipe);
        goto __EXIT;
    }

    if (path) {
        file = fopen(path, "rb");
    }

    if (file == RK_NULL) {
        RK_LOGE("open pipe(%d) input file %s failed because %s.", pipe, path, strerror(errno));
        goto __EXIT;
    }

    srcData = reinterpret_cast<RK_U8 *>(calloc(readLen, sizeof(RK_U8)));
    while (!gAmixExit) {
        memset(srcData, 0, readLen);
        size = fread(srcData, 1, readLen, file);

        stFrame.u32Len = size != 0 ? readLen : 0;
        stFrame.enBitWidth = AUDIO_BIT_WIDTH_16;
        stFrame.u64TimeStamp = timeStamp;
        stFrame.u32Seq = seq;
        stFrame.enSoundMode = amix_find_sound_mode(ctx->u32InChannel);
        stFrame.bBypassMbBlk = RK_FALSE;

        MB_EXT_CONFIG_S extConfig;
        memset(&extConfig, 0, sizeof(extConfig));
        extConfig.pOpaque = srcData;
        extConfig.pu8VirAddr = srcData;
        extConfig.u64Size = size != 0 ? readLen : 0;
        RK_MPI_SYS_CreateMB(&(stFrame.pMbBlk), &extConfig);

        result = RK_MPI_AMIX_SendPipeFrame(0, pipe, &stFrame, s32MilliSec);
        if (result != RK_SUCCESS) {
            RK_LOGE("chn(%d) send frame error, seq = %d", pipe, seq);
        }
        RK_MPI_MB_ReleaseMB(stFrame.pMbBlk);
        if (ctx->bUpdate && seq > 0 && (seq % 10 == 0)) {
            stPipeUpdateAttr.fDB = ((float)seq / 10) - 20;
            RK_MPI_AMIX_SetPipeUpdateAttr(0, pipe, &stPipeUpdateAttr);
            RK_MPI_AMIX_GetPipeUpdateAttr(0, pipe, &stGetPipeUpdateAttr);
            RK_LOGI("amix [%d, %d] set fDB = %f, get fDB = %f.",
                0, pipe, stPipeUpdateAttr.fDB, stGetPipeUpdateAttr.fDB);
        }

        usleep(timeout);
        timeStamp++;
        seq++;
        if (size <= 0) {
            RK_LOGI("chn(%d) send eof", pipe);
            break;
        }
    }

__EXIT:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }

    if (srcData)
        free(srcData);
    return RK_NULL;
}

static void* receive_amix_thread(void * ptr) {
    TEST_AMIX_CTX_S *ctx = reinterpret_cast<TEST_AMIX_CTX_S *>(ptr);
    MB_POOL_CONFIG_S pool_config;
    // set default value for struct
    AUDIO_FRAME_S frame;
    RK_U64 timeStamp = 0;
    RK_S32 s32MilliSec = 1000;
    RK_S32 size = 0;
    RK_S32 result = 0;
    RK_U32 seq = 0;
    RK_S32 eos = 0;
    RK_S32 chn = ctx->s32ChnId;
    RK_VOID *outdata = RK_NULL;

    AUDIO_FRAME_S stFrame;
    memset(&stFrame, 0, sizeof(AUDIO_FRAME_S));
    FILE *file = RK_NULL;
    const char *path = NULL;

    if (chn < AMIX_MAX_CHN_NUM) {
        path = ctx->dstFilePath[chn];
    } else {
        RK_LOGE("chn(%d) is unsupport.", chn);
        goto __EXIT;
    }

    if (path) {
        file = fopen(path, "wb+");
        if (file == RK_NULL) {
            RK_LOGE("failed to open output file %s, error: %s.", path, strerror(errno));
            goto __EXIT;
        }
    }

    if (file == RK_NULL) {
        RK_LOGE("open save file %s failed because %s.", path, strerror(errno));
        goto __EXIT;
    }

    while (!gAmixExit) {
        result = RK_MPI_AMIX_GetChnFrame(0, chn, &stFrame, s32MilliSec);
        if (result != RK_SUCCESS) {
            RK_LOGE("get frame error, seq = %d", seq);
        } else {
            outdata = RK_MPI_MB_Handle2VirAddr(stFrame.pMbBlk);
            size = stFrame.u32Len;
            eos = (size <= 0) ? 1 : 0;
            RK_LOGI("get chn(%d) frame (seq: %d, size: %d, outdata: %p)", chn, seq, size, outdata);
            if (file != RK_NULL && size >= 0) {
                fwrite(outdata, 1, size, file);
            }

            RK_MPI_AMIX_ReleaseChnFrame(0, chn, &stFrame);
            seq++;
        }

        if (eos) {
            RK_LOGI("get eos stream.");
            break;
        }

        if (gAmixExit)
            break;
    }

__EXIT:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }

    return RK_NULL;
}

RK_S32 test_init_amix(TEST_AMIX_CTX_S *ctx) {
    RK_S32    ret = 0;
    AMIX_GRP  AmixGrp = 0;
    AMIX_PIPE AmixPipe = 0;
    AMIX_CHN  AmixChn = 0;
    AMIX_GRP_ATTR_S   stGrpAttr;
    AMIX_PIPE_ATTR_S  stPipeAttr[AMIX_MAX_PIPE_NUM];
    AMIX_CHN_ATTR_S   stChnAttr[AMIX_MAX_CHN_NUM];
    memset(&stGrpAttr,  0, sizeof(stGrpAttr));
    memset(&stPipeAttr, 0, sizeof(stPipeAttr));
    memset(&stChnAttr,  0, sizeof(stChnAttr));

    stGrpAttr.enMode = AMIX_MODE_MIX;
    stGrpAttr.u32PeriodSize = ctx->s32PeriodSize;
    stGrpAttr.u32StartDelay = ctx->s32PeriodSize / ctx->s32InSampleRate;
    ret = RK_MPI_AMIX_CreateGrp(AmixGrp, &stGrpAttr);
    if (ret != RK_SUCCESS) {
        RK_LOGE("amix [%d, %d] RK_MPI_AMIX_CreateGrp failed: %#x!",
                AmixGrp, 0, ret);
        return ret;
    }

    RK_LOGD("amix [%d, %d] RK_MPI_AMIX_CreateGrp already.", AmixGrp, 0);
    for (AmixPipe = 0; AmixPipe < ctx->s32PipeNum; AmixPipe++) {
        stPipeAttr[AmixPipe].stAttr.s32SampleRate = ctx->s32InSampleRate;
        stPipeAttr[AmixPipe].stAttr.u32Channels = ctx->u32InChannel;
        stPipeAttr[AmixPipe].stAttr.enBitWidth = AUDIO_BIT_WIDTH_16;
        stPipeAttr[AmixPipe].u32InBufCnt = 4;
        ret = RK_MPI_AMIX_SetPipeAttr(AmixGrp, AmixPipe, &stPipeAttr[AmixPipe]);
        if (ret != RK_SUCCESS) {
            RK_LOGE("amix [%d, %d] RK_MPI_AMIX_SetPipeAttr failed: %#x!",
                    AmixGrp, AmixPipe, ret);
            return ret;
        }
        RK_LOGD("amix [%d, %d] RK_MPI_AMIX_SetPipeAttr already.", AmixGrp, AmixPipe);

        ret = RK_MPI_AMIX_EnablePipe(AmixGrp, AmixPipe);
        if (ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_AMIX_EnablePipe failed: %#x!", AmixPipe, ret);
            return ret;
        }
        RK_LOGD("amix [%d, %d] RK_MPI_AMIX_EnablePipe already.", AmixGrp, AmixPipe);
    }

    for (AmixChn = 0; AmixChn < ctx->s32ChnNum; AmixChn++) {
        stChnAttr[AmixChn].stAttr.s32SampleRate = ctx->s32InSampleRate;
        stChnAttr[AmixChn].stAttr.u32Channels = ctx->u32InChannel;
        stChnAttr[AmixChn].stAttr.enBitWidth = AUDIO_BIT_WIDTH_16;
        stChnAttr[AmixChn].u32OutBufCnt = 4;
        stChnAttr[AmixChn].u32Depth = 4;
        ret = RK_MPI_AMIX_SetChnAttr(AmixGrp, AmixChn, &stChnAttr[AmixChn]);
        if (ret != RK_SUCCESS) {
            RK_LOGE("amix [%d, %d]RK_MPI_AMIX_SetChnAttr failed: %#x!",
                    AmixGrp, AmixChn, ret);
            return ret;
        }
        RK_LOGD("amix [%d, %d] RK_MPI_AMIX_SetChnAttr already.", AmixGrp, AmixChn);

        ret = RK_MPI_AMIX_EnableChn(AmixGrp, AmixChn);
        if (ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_AMIX_EnableChn failed: %#x!", AmixChn, ret);
            return ret;
        }
        RK_LOGD("amix [%d, %d] RK_MPI_AMIX_EnableChn already.", AmixGrp, AmixChn);
    }

    ret = RK_MPI_AMIX_StartGrp(AmixGrp);
    if (ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_AMIX_StartGrp failed: %#x!", AmixGrp, ret);
        return ret;
    }
    RK_LOGD("amix [%d, %d] RK_MPI_AMIX_StartGrp already.", AmixGrp, 0);
    return ret;
}

RK_S32 test_deinit_amix(TEST_AMIX_CTX_S *ctx) {
    RK_S32    ret = 0;
    if (ctx != RK_NULL) {
        ret = RK_MPI_AMIX_StopGrp(0);
        if (ret != RK_SUCCESS)
            RK_LOGE("RK_MPI_AMIX_StopGrp failed: %#x!", 0, ret);

        ret = RK_MPI_AMIX_DestroyGrp(0);
        if (ret != RK_SUCCESS)
            RK_LOGE("RK_MPI_AMIX_DestroyGrp failed: %#x!", 0, ret);
    }

    return RK_SUCCESS;
}

static RK_S32 unit_test_mpi_amix_grp(TEST_AMIX_CTX_S *ctx) {
    RK_S32 ret = 0;
    AMIX_GRP AmixGrp = 0;
    AMIX_PIPE AmixPipe = 0;
    AMIX_CHN AmixChn = 0;
    pthread_t tidSend[AMIX_MAX_PIPE_NUM] = {0};
    pthread_t tidReceive[AMIX_MAX_CHN_NUM] = {0};
    TEST_AMIX_CTX_S params[AMIX_MAX_PIPE_NUM + AMIX_MAX_CHN_NUM] = {0};

    for (int i = 0; i < (AMIX_MAX_PIPE_NUM + AMIX_MAX_CHN_NUM); i++)
        memcpy(&(params[i]), ctx, sizeof(TEST_AMIX_CTX_S));

    if (test_init_amix(ctx))
        goto __FAILED;

    for (AmixChn = 0; AmixChn < ctx->s32ChnNum; AmixChn++) {
        if (!ctx->dstFilePath[AmixChn]) {
            RK_LOGE("out chn(%d) path is NULL", AmixChn);
            goto __FAILED;
        }

        params[AmixChn + AMIX_MAX_PIPE_NUM].s32ChnId = AmixChn;
        pthread_create(&tidReceive[AmixChn], RK_NULL, receive_amix_thread, reinterpret_cast<void *>(&params[AmixChn + AMIX_MAX_PIPE_NUM]));
    }

    for (AmixPipe = 0; AmixPipe < ctx->s32PipeNum; AmixPipe++) {
        params[AmixPipe].s32PipeId = AmixPipe;
        pthread_create(&tidSend[AmixPipe], RK_NULL, send_amix_thread, reinterpret_cast<void *>(&params[AmixPipe]));
    }

__FAILED:
    for (AmixChn = 0; AmixChn < ctx->s32ChnNum; AmixChn++) {
        if (tidReceive[AmixChn])
            pthread_join(tidReceive[AmixChn], RK_NULL);

        RK_MPI_AMIX_DisableChn(0, AmixChn);
    }

    for (AmixPipe = 0; AmixPipe < ctx->s32PipeNum; AmixPipe++) {
        if (tidSend[AmixPipe])
            pthread_join(tidSend[AmixPipe], RK_NULL);

        RK_MPI_AMIX_DisablePipe(0, AmixPipe);
    }

    test_deinit_amix(ctx);
    return ret;
}

static const char *const usages[] = {
    "./rk_mpi_amix_test [-C card] [--control ctl_name] [--value ctl_val] [--list_controls] [--list_contents]...",
    NULL,
};

static void mpi_amix_test_show_options(const TEST_AMIX_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("sound control id      : %d\n", ctx->s32DevId);
    RK_PRINT("control name          : %s\n", ctx->psControlName);
    RK_PRINT("control value         : %s\n", ctx->psControlValue);
    RK_PRINT("list controls         : %d\n", ctx->bListControls);
    RK_PRINT("list contents         : %d\n", ctx->bListContents);
    RK_PRINT("pipe num              : %d\n", ctx->s32PipeNum);
    RK_PRINT("chn  num              : %d\n", ctx->s32ChnNum);
    RK_PRINT("input  first file     : %s\n", ctx->srcFilePath[0]);
    RK_PRINT("input  second file    : %s\n", ctx->srcFilePath[1]);
    RK_PRINT("input  third file     : %s\n", ctx->srcFilePath[2]);
    RK_PRINT("output first file     : %s\n", ctx->dstFilePath[0]);
    RK_PRINT("output second file    : %s\n", ctx->dstFilePath[1]);
    RK_PRINT("input  sampleRate     : %d\n", ctx->u32InChannel);
    RK_PRINT("input  channel        : %d\n", ctx->s32InSampleRate);
    RK_PRINT("output sampleRate     : %d\n", ctx->u32InChannel);
    RK_PRINT("output channel        : %d\n", ctx->s32InSampleRate);
    RK_PRINT("group  periodSize     : %d\n", ctx->s32PeriodSize);
}

int main(int argc, const char **argv) {
    TEST_AMIX_CTX_S  *ctx = reinterpret_cast<TEST_AMIX_CTX_S *>(malloc(sizeof(TEST_AMIX_CTX_S)));

    memset(ctx, 0, sizeof(TEST_AMIX_CTX_S));
    ctx->s32DevId           = 0;
    ctx->psControlName      = RK_NULL;
    ctx->psControlValue     = RK_NULL;
    ctx->bListControls      = RK_FALSE;
    ctx->bListContents      = RK_FALSE;
    ctx->s32ChnNum          = 1;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_INTEGER('C', "card",  &(ctx->s32DevId),
                   "specifies the card number of the mixer. default(0)", NULL, 0, 0),
        OPT_STRING('\0', "control", &(ctx->psControlName),
                    "sets (or gets without control value) the name of a control. default(required)", NULL, 0, 0),
        OPT_STRING('\0', "value", &(ctx->psControlValue),
                    "sets the value of a control. default(required)", NULL, 0, 0),
        OPT_BOOLEAN('\0', "list_controls", &(ctx->bListControls),
                    "lists controls of the mixer. default(false).", NULL, 0, 0),
        OPT_BOOLEAN('\0', "list_contents", &(ctx->bListContents),
                    "lists controls of the mixer and their contents. default(false).", NULL, 0, 0),
        OPT_INTEGER('p', "pipe_num",  &(ctx->s32PipeNum),
                   "the number of pipe. default(0)", NULL, 0, 0),
        OPT_INTEGER('c', "chn_num",  &(ctx->s32ChnNum),
                   "the number of chn. default(1)", NULL, 0, 0),
        OPT_STRING('i', "input_first",  &(ctx->srcFilePath[0]),
                   "input file name , e.g.(./*.pcm). <required>", NULL, 0, 0),
        OPT_STRING('s', "input_second",  &(ctx->srcFilePath[1]),
                   "input second file name , e.g.(./*.pcm). <required>", NULL, 0, 0),
        OPT_STRING('t', "input_third",  &(ctx->srcFilePath[2]),
                   "input third file name , e.g.(./*.pcm). <required>", NULL, 0, 0),
        OPT_STRING('o', "output_first", &(ctx->dstFilePath[0]),
                    "output first file name, e.g.(./ao). default(required).", NULL, 0, 0),
        OPT_STRING('u', "output_second", &(ctx->dstFilePath[1]),
                    "output second file name, e.g.(./ao). default(required).", NULL, 0, 0),
        OPT_INTEGER('\0', "input_ch", &(ctx->u32InChannel),
                    "the in channel of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_rate", &(ctx->s32InSampleRate),
                    "the in sample rate of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "output_ch", &(ctx->u32OutChannel),
                    "the out channel of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "output_rate", &(ctx->s32OutSampleRate),
                    "the out sample rate of input data. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "period_size", &(ctx->s32PeriodSize),
                    "the period size, default(1024)", NULL, 0, 0),
        OPT_INTEGER('e', "update", &(ctx->bUpdate),
                    "update mode, default(0)", NULL, 0, 0),
        OPT_INTEGER('d', "update", &(ctx->fDB),
                    "update dB, default(0)", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_amix_test_show_options(ctx);

    if (!ctx->s32PipeNum) {
        if (ctx->psControlName == RK_NULL
            && ctx->psControlValue == RK_NULL
            && ctx->bListControls == RK_FALSE
            && ctx->bListContents == RK_FALSE) {
            argparse_usage(&argparse);
            goto __FAILED;
        }

        unit_test_mpi_amix(ctx);
    } else {
        if (ctx->s32PipeNum < 0) {
            RK_LOGE("pipe num(%d) < 0", ctx->s32PipeNum);
            goto __FAILED;
        } else {
            if (ctx->s32InSampleRate != ctx->s32OutSampleRate ||
                ctx->u32InChannel != ctx-> u32OutChannel) {
                RK_LOGE("input attr(%d, %d) != output attr(%d, %d)",
                ctx->s32InSampleRate, ctx->u32InChannel,
                ctx->s32OutSampleRate, ctx-> u32OutChannel);
                goto __FAILED;
            }

            if (ctx->srcFilePath[0] == RK_NULL) {
                argparse_usage(&argparse);
                goto __FAILED;
            }

            signal(SIGINT, sigterm_handler);
            RK_MPI_SYS_Init();
            unit_test_mpi_amix_grp(ctx);
            RK_MPI_SYS_Exit();
        }
    }

    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }

    return RK_SUCCESS;
__FAILED:
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }

    return RK_FAILURE;
}
