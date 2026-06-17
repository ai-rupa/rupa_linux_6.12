/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_ivs.h"
#include "rk_comm_ivs.h"

#include "test_comm_argparse.h"
#include "test_comm_imgproc.h"
#include "test_comm_utils.h"

typedef struct _rkMpiIVSCtx {
    const char *srcFileUri;
    const char *dstFilePath;
    RK_U32      u32SrcWidth;
    RK_U32      u32SrcHeight;
    RK_U32      u32srcVirWidth;
    RK_U32      u32srcVirHeight;
    RK_S32      s32LoopCount;
    RK_U32      u32SrcPixFormat;
    RK_S32      s32SnapPicCount;
    RK_BOOL     threadExit;
    MB_POOL     ivsPoolInput;
    FILE       *fp_ivs_result;
    RK_S32      s32IvsChn;
    RK_S32      s32ThreshSad;
    RK_S32      s32ThreshMove;
    RK_S32      s32SwitchSad;
    RK_S32      s32SquarePct;
    RK_U32      u32BufferSize;
    RK_S32      s32ThreshSadOD;
    RK_S32      s32ThreshComplexCntOD;
} TEST_IVS_CTX_S;

static RK_S32 create_ivs(TEST_IVS_CTX_S *ctx) {
    IVS_CHN_ATTR_S attr;
    memset(&attr, 0, sizeof(attr));
    attr.enMode = IVS_MODE_MD_OD;
    attr.u32PicWidth = ctx->u32SrcWidth;
    attr.u32PicHeight = ctx->u32SrcHeight;
    attr.u32MaxWidth = ctx->u32SrcWidth;
    attr.u32MaxHeight = ctx->u32SrcHeight;
    attr.enPixelFormat = (PIXEL_FORMAT_E)ctx->u32SrcPixFormat;
    attr.s32Gop = 30;
    attr.bSmearEnable = RK_FALSE;
    attr.bWeightpEnable = RK_TRUE;
    attr.bMDEnable = RK_TRUE;
    attr.s32MDInterval = 1;
    attr.bMDNightMode = RK_FALSE;
    attr.u32MDSensibility = 1;
    attr.bODEnable = RK_TRUE;
    attr.s32ODInterval = 1;
    attr.s32ODPercent = 7;
    return RK_MPI_IVS_CreateChn(ctx->s32IvsChn, &attr);
}

static RK_S32 destroy_ivs(TEST_IVS_CTX_S *ctx) {
    RK_MPI_IVS_DestroyChn(ctx->s32IvsChn);
    return RK_SUCCESS;
}

static RK_S32 read_with_pixel_width(RK_U8 *pBuf, RK_U32 u32Width, RK_U32 u32VirHeight,
                                    RK_U32 u32VirWidth, RK_U32 u32PixWidth, FILE *fp) {
    RK_U32 u32Row;
    RK_S32 s32ReadSize;

    for (u32Row = 0; u32Row < u32VirHeight; u32Row++) {
        s32ReadSize = fread(pBuf + u32Row * u32VirWidth * u32PixWidth, 1, u32Width * u32PixWidth, fp);
        if (s32ReadSize != u32Width * u32PixWidth) {
            RK_LOGE("read file failed expect %d vs %d\n",
                    u32Width * u32PixWidth, s32ReadSize);
            return RK_FAILURE;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 read_image(RK_U8 *pVirAddr, RK_U32 u32Width, RK_U32 u32Height,
                         RK_U32 u32VirWidth, RK_U32 u32VirHeight, RK_U32 u32PixFormat, FILE *fp) {
    RK_U32 u32Row = 0;
    RK_U32 u32ReadSize = 0;
    RK_S32 s32Ret = RK_SUCCESS;

    RK_U8 *pBufy = pVirAddr;
    RK_U8 *pBufu = pBufy + u32VirWidth * u32VirHeight;
    RK_U8 *pBufv = pBufu + u32VirWidth * u32VirHeight / 4;

    switch (u32PixFormat) {
    case RK_FMT_YUV420P:
    case RK_FMT_YUV420SP: {
        for (u32Row = 0; u32Row < u32VirHeight; u32Row++) {
            u32ReadSize = fread(pBufy + u32Row * u32VirWidth, 1, u32Width, fp);
            if (u32ReadSize != u32Width) {
                return RK_FAILURE;
            }
        }

        for (u32Row = 0; u32Row < u32VirHeight / 2; u32Row++) {
            u32ReadSize = fread(pBufu + u32Row * u32VirWidth, 1, u32Width, fp);
            if (u32ReadSize != u32Width) {
                return RK_FAILURE;
            }
        }
    } break;
    case RK_FMT_RGB888:
    case RK_FMT_BGR888: {
        s32Ret = read_with_pixel_width(pBufy, u32Width, u32VirHeight, u32VirWidth, 3, fp);
    } break;
    default : {
        RK_LOGE("read image do not support fmt %d\n", u32PixFormat);
        return RK_FAILURE;
    } break;
    }

    return s32Ret;
}

static RK_S32 check_options(const TEST_IVS_CTX_S *ctx) {
    if (ctx->u32SrcPixFormat == RK_FMT_BUTT ||
        ctx->u32SrcWidth <= 0 ||
        ctx->u32SrcHeight <= 0) {
        goto __FAILED;
    }

    return RK_SUCCESS;

__FAILED:
    return RK_ERR_IVS_ILLEGAL_PARAM;
}

void* ivs_send_frame(void *pArgs) {
    TEST_IVS_CTX_S     *pstCtx        = (TEST_IVS_CTX_S *)(pArgs);
    RK_S32               s32Ret         = RK_SUCCESS;
    RK_U8               *pVirAddr       = RK_NULL;
    FILE                *fp             = RK_NULL;
    MB_BLK               blk            = RK_NULL;
    RK_S32               s32LoopCount   = pstCtx->s32LoopCount;
    MB_POOL              pool           = pstCtx->ivsPoolInput;
    RK_U32               u32Ch          = pstCtx->s32IvsChn;
    RK_S32               s32FrameCount  = 0;
    RK_S32               s32ReachEOS    = 0;
    VIDEO_FRAME_INFO_S   stFrame;
    MB_BLK               qpmapBlk = RK_NULL;
    RK_S16              *qpmapData = RK_NULL;
    RK_S16              *qpmapTmp = RK_NULL;
    RK_U32               u32TimeRef = 1;
    long                 filesize = 0;

    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    while (!pstCtx->threadExit) {
        blk = RK_MPI_MB_GetMB(pool, pstCtx->u32BufferSize, RK_TRUE);

        if (RK_NULL == blk) {
            RK_LOGE("RK_MPI_MB_GetMB fail:%d", u32Ch);
            usleep(2000llu);
            continue;
        }
        pVirAddr = (RK_U8 *)(RK_MPI_MB_Handle2VirAddr(blk));

        if (pstCtx->srcFileUri) {
            if (fp == RK_NULL) {
                fp = fopen(pstCtx->srcFileUri, "r");
                if (fp == RK_NULL) {
                    RK_LOGE("chn %d can't open file %s!\n", u32Ch, pstCtx->srcFileUri);
                    return RK_NULL;
                }
                fseek(fp, 0, SEEK_END);
                filesize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
            }
            s32Ret = read_image(pVirAddr, pstCtx->u32SrcWidth, pstCtx->u32SrcHeight,
                                pstCtx->u32srcVirWidth, pstCtx->u32srcVirHeight, pstCtx->u32SrcPixFormat, fp);
            if (ftell(fp) >= filesize)
                s32ReachEOS = 1;
        } else {
            s32Ret = TEST_COMM_FillImage(pVirAddr, pstCtx->u32SrcWidth,
                                         pstCtx->u32SrcHeight,
                                         pstCtx->u32srcVirWidth,
                                         pstCtx->u32srcVirHeight,
                                         (PIXEL_FORMAT_E)pstCtx->u32SrcPixFormat, s32FrameCount);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_MB_ReleaseMB(blk);
                RK_LOGE("TEST_COMM_FillImage fail ch %d", u32Ch);
                return RK_NULL;
            }
        }

        if (s32Ret != RK_SUCCESS) {
            s32ReachEOS = 1;
            if (s32LoopCount > 0) {
                s32LoopCount--;
                RK_LOGI("finish ivs count %d\n", pstCtx->s32LoopCount - s32LoopCount);
                if (s32LoopCount > 0) {
                    s32ReachEOS = 0;
                    RK_MPI_MB_ReleaseMB(blk);

                    fseek(fp, 0L, SEEK_SET);
                    RK_LOGI("seek finish ch %d", u32Ch);
                    continue;
                }
            }
        }

        RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);

        stFrame.stVFrame.pMbBlk = blk;
        stFrame.stVFrame.u32Width = pstCtx->u32SrcWidth;
        stFrame.stVFrame.u32Height = pstCtx->u32SrcHeight;
        stFrame.stVFrame.u32VirWidth = pstCtx->u32srcVirWidth;
        stFrame.stVFrame.u32VirHeight = pstCtx->u32srcVirHeight;
        stFrame.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->u32SrcPixFormat;
        stFrame.stVFrame.u32FrameFlag = 0;
        stFrame.stVFrame.u32FrameFlag |= s32ReachEOS ? FRAME_FLAG_SNAP_END : 0;
        stFrame.stVFrame.u32TimeRef = u32TimeRef++;

__RETRY:
        s32Ret = RK_MPI_IVS_SendFrame(pstCtx->s32IvsChn, &stFrame, -1);
        IVS_RESULT_INFO_S stResults;
        memset(&stResults, 0, sizeof(IVS_RESULT_INFO_S));
        s32Ret = RK_MPI_IVS_GetResults(pstCtx->s32IvsChn, &stResults, -1);
        if (s32Ret == RK_SUCCESS) {
            if (stResults.s32ResultNum == 1) {
                int x = RK_ALIGN(pstCtx->u32SrcWidth / 8 / 8, 4) / 4;
                int y = RK_ALIGN(pstCtx->u32SrcHeight / 8, 4) / 4;
                int bit = pstCtx->u32SrcWidth / 8 / 4;
                fprintf(pstCtx->fp_ivs_result, "Frame %d, MD u32Square: %u\n", s32FrameCount, stResults.pstResults->stMdInfo.u32Square);
                printf("Frame %d, MD u32Square: %u\n", s32FrameCount, stResults.pstResults->stMdInfo.u32Square);
                if (stResults.pstResults->stMdInfo.pData) {
                    for (int n = 0; n < x * 8; n++)
                        fprintf(pstCtx->fp_ivs_result, "-");
                    fprintf(pstCtx->fp_ivs_result, "\n");
                    for (int j = 0; j < y; j++) {
                        int cnt = 0;
                        for (int i = 0; i < x; i++) {
                            for (int k = 0; k < 8 && cnt < bit; k++) {
                                if (stResults.pstResults->stMdInfo.pData[j * 64 + i] & (1 << k))
                                    fprintf(pstCtx->fp_ivs_result, "1");
                                else
                                    fprintf(pstCtx->fp_ivs_result, "0");
                                cnt++;
                                if (cnt >= bit)
                                    break;
                            }
                        }
                        fprintf(pstCtx->fp_ivs_result, "\n");
                    }
                    for (int n = 0; n < x * 8; n++)
                        fprintf(pstCtx->fp_ivs_result, "-");
                    fprintf(pstCtx->fp_ivs_result, "\n");
                    fprintf(pstCtx->fp_ivs_result, "MD u32RectNum: %u\n", stResults.pstResults->stMdInfo.u32RectNum);
                    printf("MD u32RectNum: %u\n", stResults.pstResults->stMdInfo.u32RectNum);
                    for (int i = 0; i < stResults.pstResults->stMdInfo.u32RectNum; i++) {
                        fprintf(pstCtx->fp_ivs_result, "%d: [%d, %d, %d, %d]\n", i,
                               stResults.pstResults->stMdInfo.stRect[i].s32X,
                               stResults.pstResults->stMdInfo.stRect[i].s32Y,
                               stResults.pstResults->stMdInfo.stRect[i].u32Width,
                               stResults.pstResults->stMdInfo.stRect[i].u32Height);
                        printf("%d: [%d, %d, %d, %d]\n", i,
                               stResults.pstResults->stMdInfo.stRect[i].s32X,
                               stResults.pstResults->stMdInfo.stRect[i].s32Y,
                               stResults.pstResults->stMdInfo.stRect[i].u32Width,
                               stResults.pstResults->stMdInfo.stRect[i].u32Height);
                    }
                }
                fprintf(pstCtx->fp_ivs_result, "od flag: %d\n", stResults.pstResults->stOdInfo.u32Flag);
                fsync(fileno(pstCtx->fp_ivs_result));
            }
            RK_MPI_IVS_ReleaseResults(pstCtx->s32IvsChn, &stResults);
        } else {
            RK_LOGE("RK_MPI_IVS_GetResults fail %x", s32Ret);
        }
        if (s32Ret < 0) {
            if (pstCtx->threadExit) {
                RK_MPI_MB_ReleaseMB(blk);
                break;
            }

            usleep(10000llu);
            goto  __RETRY;
        } else {
            RK_MPI_MB_ReleaseMB(blk);
            s32FrameCount++;
            RK_LOGI("chn %d frame %d", u32Ch, s32FrameCount);
            usleep(33000);
        }
        if (s32ReachEOS ||
            (pstCtx->s32SnapPicCount != -1 && s32FrameCount >= pstCtx->s32SnapPicCount)) {
            RK_LOGI("chn %d reach EOS.", u32Ch);
            break;
        }
    }

__EXIT:
    if (fp)
        fclose(fp);

    return RK_NULL;
}

RK_S32 unit_test_mpi_ivs(TEST_IVS_CTX_S *ctx) {
    RK_S32                  s32Ret = RK_SUCCESS;
    RK_U32                  u32Ch = 0;
    MB_POOL_CONFIG_S        stMbPoolCfg;
    TEST_IVS_CTX_S         stIvsCtx[IVS_MAX_CHN_NUM];
    pthread_t               ivsThread[IVS_MAX_CHN_NUM] = {0};

    if (ctx->u32srcVirWidth <= 0) {
        ctx->u32srcVirWidth = ctx->u32SrcWidth;
    }
    if (ctx->u32srcVirHeight <= 0) {
        ctx->u32srcVirHeight = ctx->u32SrcHeight;
    }

    s32Ret = create_ivs(ctx);
    if (s32Ret) {
        RK_LOGE("ivs init failed:%x", s32Ret);
        return RK_FAILURE;
    }
    IVS_MD_ATTR_S stMdAttr;
    memset(&stMdAttr, 0, sizeof(stMdAttr));
    s32Ret = RK_MPI_IVS_GetMdAttr(ctx->s32IvsChn, &stMdAttr);
    if (s32Ret) {
        RK_LOGE("ivs get mdattr failed:%x", s32Ret);
        return RK_FAILURE;
    }
    stMdAttr.s32ThreshSad = ctx->s32ThreshSad;
    stMdAttr.s32ThreshMove = ctx->s32ThreshMove;
    stMdAttr.s32SwitchSad = ctx->s32SwitchSad;
    stMdAttr.bFlycatkinFlt = RK_TRUE;
    stMdAttr.s32ThresDustMove = 3;
    stMdAttr.s32ThresDustBlk = 3;
    stMdAttr.s32ThresDustChng = 50;
    printf("MD s32ThreshSad %d, s32ThreshMove %d, s32SwitchSad %d\n",
           ctx->s32ThreshSad, ctx->s32ThreshMove, ctx->s32SwitchSad);
    s32Ret = RK_MPI_IVS_SetMdAttr(ctx->s32IvsChn, &stMdAttr);
    if (s32Ret) {
        RK_LOGE("ivs set mdattr failed:%x", s32Ret);
        return RK_FAILURE;
    }
    IVS_OD_ATTR_S stOdAttr;
    memset(&stOdAttr, 0, sizeof(stOdAttr));
    s32Ret = RK_MPI_IVS_GetOdAttr(ctx->s32IvsChn, &stOdAttr);
    if (s32Ret) {
        RK_LOGE("ivs get odattr failed:%x", s32Ret);
        return RK_FAILURE;
    }
    stOdAttr.s32ThreshSad = ctx->s32ThreshSadOD;
    stOdAttr.s32ThreshComplexCnt = ctx->s32ThreshComplexCntOD;
    printf("OD s32ThreshSad %d, s32ThreshComplexCnt %d\n",
           ctx->s32ThreshSadOD, ctx->s32ThreshComplexCntOD);
    s32Ret = RK_MPI_IVS_SetOdAttr(ctx->s32IvsChn, &stOdAttr);
    if (s32Ret) {
        RK_LOGE("ivs set odattr failed:%x", s32Ret);
        return RK_FAILURE;
    }
    char name[128];
    snprintf(name, sizeof(name), "/tmp/ivs%d_result.txt", ctx->s32IvsChn);
    ctx->fp_ivs_result = fopen(name, "wb");
    if (!ctx->fp_ivs_result) {
        RK_LOGE("open /data/ivs_result.txt failed");
        return RK_FAILURE;
    }

    if (ctx->u32BufferSize <= 0) {
        PIC_BUF_ATTR_S stPicBufAttr;
        MB_PIC_CAL_S stMbPicCalResult;

        memset(&stPicBufAttr, 0, sizeof(stPicBufAttr));
        stPicBufAttr.u32Width = ctx->u32SrcWidth;
        stPicBufAttr.u32Height = ctx->u32SrcHeight;
        stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->u32SrcPixFormat;
        s32Ret = RK_MPI_CAL_COMM_GetPicBufferSize(&stPicBufAttr, &stMbPicCalResult);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
            return s32Ret;
        }
        ctx->u32BufferSize = stMbPicCalResult.u32MBSize;
        RK_LOGD("calc picture size: %d", ctx->u32BufferSize);
    }
    if (ctx->u32BufferSize > 8192 * 8192 * 4) {
        RK_LOGE("too large picture size: %d", ctx->u32BufferSize);
        return RK_FAILURE;
    }

    // for input image pool
    memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    stMbPoolCfg.u64MBSize = ctx->u32BufferSize;
    stMbPoolCfg.u32MBCnt  = 1;
    stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
    stMbPoolCfg.bPreAlloc = RK_TRUE;
    ctx->ivsPoolInput = RK_MPI_MB_CreatePool(&stMbPoolCfg);
    if (ctx->ivsPoolInput == MB_INVALID_POOLID) {
        RK_LOGE("create ivsPoolInput failed!");
        return RK_FAILURE;
    }

    memcpy(&(stIvsCtx[u32Ch]), ctx, sizeof(TEST_IVS_CTX_S));
    pthread_create(&ivsThread[u32Ch], 0, ivs_send_frame, (void *)(&stIvsCtx[u32Ch]));

__FAILED:
    if (ivsThread[u32Ch])
        pthread_join(ivsThread[u32Ch], RK_NULL);

    stIvsCtx[u32Ch].threadExit = RK_TRUE;
    RK_MPI_MB_DestroyPool(ctx->ivsPoolInput);

    if (ctx->fp_ivs_result) {
        fsync(fileno(ctx->fp_ivs_result));
        fclose(ctx->fp_ivs_result);
        ctx->fp_ivs_result = NULL;
    }
    destroy_ivs(ctx);

    return RK_SUCCESS;
}

static const char *const usages[] = {
    "./rk_mpi_ivs_test [-i SRC_PATH] [-w SRC_WIDTH] [-h SRC_HEIGHT] [-f PIXEL_FORMAT] [-o DIR] [--ivs_chn CHN] [--threshsad THRESH_SAD] [--threshmove THRESH_MOVE] [--switchsad SWITCH_SAD] [--squarepct SQUARE_PCT]",
    NULL,
};

static void mpi_ivs_test_show_options(const TEST_IVS_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input  file name       : %s\n", ctx->srcFileUri);
    RK_PRINT("output file name       : %s\n", ctx->dstFilePath);
    RK_PRINT("src width              : %d\n", ctx->u32SrcWidth);
    RK_PRINT("src height             : %d\n", ctx->u32SrcHeight);
    RK_PRINT("src virWidth           : %d\n", ctx->u32srcVirWidth);
    RK_PRINT("src virHeight          : %d\n", ctx->u32srcVirHeight);
    RK_PRINT("src pixel format       : %d\n", ctx->u32SrcPixFormat);
    RK_PRINT("loop count             : %d\n", ctx->s32LoopCount);
    RK_PRINT("snap picture count     : %d\n", ctx->s32SnapPicCount);
    RK_PRINT("ivs chn                : %d\n", ctx->s32IvsChn);
    RK_PRINT("threshsad              : %d\n", ctx->s32ThreshSad);
    RK_PRINT("threshmove             : %d\n", ctx->s32ThreshMove);
    RK_PRINT("switchsad              : %d\n", ctx->s32SwitchSad);
    RK_PRINT("squarepct              : %d\n", ctx->s32SquarePct);

    return;
}

int main(int argc, const char **argv) {
    RK_S32 s32Ret = RK_SUCCESS;
    TEST_IVS_CTX_S ctx;
    memset(&ctx, 0, sizeof(TEST_IVS_CTX_S));

    ctx.s32LoopCount    = 1;
    ctx.u32SrcPixFormat = RK_FMT_YUV420SP;
    ctx.s32SnapPicCount = -1;
    ctx.s32ThreshSad    = 64;
    ctx.s32ThreshMove   = 2;
    ctx.s32SwitchSad    = 2;
    ctx.s32SquarePct    = 35;
    ctx.s32ThreshSadOD  = 64;
    ctx.s32ThreshComplexCntOD = 2;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input",  &(ctx.srcFileUri),
                   "input file name.", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx.dstFilePath),
                   "the directory of encoder output", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx.s32LoopCount),
                    "loop running count. default(1)", NULL, 0, 0),
        OPT_INTEGER('w', "width", &(ctx.u32SrcWidth),
                    "input source width. <required>", NULL, 0, 0),
        OPT_INTEGER('h', "height", &(ctx.u32SrcHeight),
                    "input source height. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "vir_width", &(ctx.u32srcVirWidth),
                    "input source virWidth.", NULL, 0, 0),
        OPT_INTEGER('\0', "vir_height", &(ctx.u32srcVirHeight),
                    "input source virHeight.", NULL, 0, 0),
        OPT_INTEGER('f', "pixel_format", &(ctx.u32SrcPixFormat),
                    "input source pixel format. default(0: NV12).", NULL, 0, 0),
        OPT_INTEGER('s', "snap_pic_cnt", &(ctx.s32SnapPicCount),
                    "snap picture count(effective range[-1,0)(0, 2147483647)"
                    "-1:not limit else:snap count) default(-1)", NULL, 0, 0),
        OPT_INTEGER('\0', "ivs_chn", &(ctx.s32IvsChn),
                    "ivs channel, default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "threshsad", &(ctx.s32ThreshSad),
                    "MD threshsad, default(64)", NULL, 0, 0),
        OPT_INTEGER('\0', "threshmove", &(ctx.s32ThreshMove),
                    "MD threshmove, default(2)", NULL, 0, 0),
        OPT_INTEGER('\0', "switchsad", &(ctx.s32SwitchSad),
                    "MD switchsad, default(2)", NULL, 0, 0),
        OPT_INTEGER('\0', "squarepct", &(ctx.s32SquarePct),
                    "MD square percent, default(35)", NULL, 0, 0),
        OPT_INTEGER('\0', "threshsadod", &(ctx.s32ThreshSadOD),
                    "OD threshsad, default(64)", NULL, 0, 0),
        OPT_INTEGER('\0', "threshcomplexcntod", &(ctx.s32ThreshComplexCntOD),
                    "OD thresh complex cnt, default(2)", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                      "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_ivs_test_show_options(&ctx);

    if (check_options(&ctx)) {
        argparse_usage(&argparse);
        return RK_FAILURE;
    }

    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    if (unit_test_mpi_ivs(&ctx) < 0) {
        goto __FAILED;
    }
    s32Ret = RK_MPI_SYS_Exit();
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }
    RK_LOGE("test running success!");
    return RK_SUCCESS;
__FAILED:
    RK_MPI_SYS_Exit();
    RK_LOGE("test running failed!");
    return s32Ret;
}
