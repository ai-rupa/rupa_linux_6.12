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

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#ifndef HAVE_C_COMPILER
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/types.h>
#else
#include "rk_osal.h"
#endif
#include "rk_debug.h"
#include "rk_mpi_vdec.h"
#include "rk_mpi_vpss.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_cal.h"

#include "test_comm_argparse.h"
#include "test_comm_utils.h"

#define MAX_STREAM_CNT               8
#define MAX_TIME_OUT_MS              20

#ifndef VDEC_INT64_MIN
#define VDEC_INT64_MIN               (-0x7fffffffffffffffLL-1)
#endif

#ifndef VDEC_INT64_MAX
#define VDEC_INT64_MAX               INT64_C(9223372036854775807)
#endif

typedef struct _rkMpiVDECCtx {
    const char *srcFileUri;
    const char *dstFilePath;
    RK_U32 u32SrcWidth;
    RK_U32 u32SrcHeight;
    RK_S32 s32LoopCount;
    RK_U32 u32ChnIndex;
    RK_U32 u32ChNum;
    RK_U32 u32InputMode;
    RK_CODEC_ID_E enCodecId;
    RK_U32 u32CompressMode;
    RK_U32 u32VideoFormat;
    RK_U32 u32FrameBufferCnt;
    pthread_t pVdecThread;
    pthread_t pGetPicThread;
    RK_U32 u32ReadSize;
    RK_BOOL threadExit;
    RK_S32 s32ChnFd;
    RK_BOOL bEnableMbPool;
    MB_POOL s32Pool;
    RK_S32 s32OutputPixFmt;
    RK_BOOL bEnableDei;
    RK_BOOL bEnableColmv;
    VDEC_LIB_E enLib;
    RK_S32 s32VProcDevType;
} TEST_VDEC_CTX_S;

static RK_CODEC_ID_E detect_codec_id_from_uri(const char *srcFileUri) {
    if (srcFileUri == RK_NULL) {
        return RK_VIDEO_ID_Unused;
    }

    if (strstr(srcFileUri, ".hevc") || strstr(srcFileUri, ".h265") || strstr(srcFileUri, ".265")) {
        return RK_VIDEO_ID_HEVC;
    } else if (strstr(srcFileUri, ".264") || strstr(srcFileUri, ".h264") || strstr(srcFileUri, ".avc")) {
        return RK_VIDEO_ID_AVC;
    } else if (strstr(srcFileUri, ".mjpeg") || strstr(srcFileUri, ".mjpg")) {
        return RK_VIDEO_ID_MJPEG;
    } else if (strstr(srcFileUri, ".jpeg") || strstr(srcFileUri, ".jpg")) {
        return RK_VIDEO_ID_JPEG;
    }

    return RK_VIDEO_ID_Unused;
}

static unsigned h264_get_next_frame_size(int fd, unsigned char nal[5], int *eos) {
    int frameType = 0;
    int isNalu = 0;
    int isFrame = 0;
    unsigned size = 0;
    off_t filesize;

    off_t pos = lseek(fd, 0, SEEK_CUR);
    filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, pos, SEEK_SET);

    while (1) {
        nal[0] = nal[1];
        nal[1] = nal[2];
        nal[2] = nal[3];
        nal[3] = nal[4];
        if (read(fd, &nal[4], 1) != 1) {
            *eos = (lseek(fd, 0, SEEK_CUR) == filesize) ? 1 : 0;
            lseek(fd, pos, SEEK_SET);
            return size;
        }
        size++;

        if (isNalu) {
            isNalu = 0;
            frameType = nal[4] & 0x1F;
            if (frameType == 5 || frameType == 1)
                isFrame = 1;
        }

        if ((nal[0] != 0x00 && nal[1] == 0x00 && nal[2] == 0x00 && nal[3] == 0x01) ||
            (nal[1] == 0x00 && nal[2] == 0x00 && nal[3] == 0x00 && nal[4] == 0x01)) {
            if (isFrame) {
                size = nal[3] == 0x01 ? size - 3 : size - 4;
                *eos = (lseek(fd, 0, SEEK_CUR) == filesize) ? 1 : 0;
                lseek(fd, pos, SEEK_SET);
                return size;
            } else {
                isNalu = 1;
            }
        }
    }
}

static unsigned h265_get_next_frame_size(int fd, unsigned char nal[5], int *eos) {
    int frameType = 0;
    int isNalu = 0;
    int isFrame = 0;
    unsigned size = 0;
    off_t filesize;

    off_t pos = lseek(fd, 0, SEEK_CUR);
    filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, pos, SEEK_SET);

    while (1) {
        nal[0] = nal[1];
        nal[1] = nal[2];
        nal[2] = nal[3];
        nal[3] = nal[4];
        if (read(fd, &nal[4], 1) != 1) {
            *eos = (lseek(fd, 0, SEEK_CUR) == filesize) ? 1 : 0;
            lseek(fd, pos, SEEK_SET);
            return size;
        }
        size++;

        if (isNalu) {
            if (!frameType) {
                frameType = nal[4] << 8;
                continue;
            } else {
                isNalu = 0;
                frameType |= nal[4];
            }
            if (frameType == 0x2601 || frameType == 0x0201)
                isFrame = 1;
        }

        if ((nal[0] != 0x00 && nal[1] == 0x00 && nal[2] == 0x00 && nal[3] == 0x01) ||
            (nal[1] == 0x00 && nal[2] == 0x00 && nal[3] == 0x00 && nal[4] == 0x01)) {
            if (isFrame) {
                size = nal[3] == 0x01 ? size - 3 : size - 4;
                *eos = (lseek(fd, 0, SEEK_CUR) == filesize) ? 1 : 0;
                lseek(fd, pos, SEEK_SET);
                return size;
            } else {
                frameType = 0;
                isNalu = 1;
            }
        }
    }
}

static int mjpeg_get_next_frame_size(int fd) {
    off_t start_pos = lseek(fd, 0, SEEK_CUR);
    if (start_pos < 0) {
        RK_LOGE("lseek failed: %s", strerror(errno));
        return -1;
    }

    off_t cur_pos = start_pos;
    off_t filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, cur_pos, SEEK_SET);

    // 1) find SOI (FFD8)
    RK_U8 prev = 0;
    RK_U8 cur = 0;
    ssize_t n = 0;
    off_t soi_pos = -1;
    while (1) {
        n = read(fd, &cur, 1);
        if (n != 1) {
            lseek(fd, start_pos, SEEK_SET);
            return -1;
        }
        if (prev == 0xFF && cur == 0xD8) {
            soi_pos = lseek(fd, -2, SEEK_CUR);
            break;
        }
        prev = cur;
    }

    if (soi_pos < 0) {
        lseek(fd, start_pos, SEEK_SET);
        return -1;
    }

    // 2) find EOI (FFD9), count size from SOI
    lseek(fd, soi_pos, SEEK_SET);
    int length = 0;
    prev = 0;
    while (1) {
        n = read(fd, &cur, 1);
        if (n != 1) {
            lseek(fd, soi_pos, SEEK_SET);
            return length > 0 ? length : -1;
        }
        length++;
        if (prev == 0xFF && cur == 0xD9) {
            lseek(fd, soi_pos, SEEK_SET);
            return length;
        }
        prev = cur;
    }
}

static RK_S32 check_options(const TEST_VDEC_CTX_S *ctx) {
    if (ctx->srcFileUri == RK_NULL) {
        goto __FAILED;
    }

    if (ctx->enCodecId <= RK_VIDEO_ID_Unused) {
        goto __FAILED;
    }

    if (ctx->u32SrcWidth <= 0 || ctx->u32SrcHeight <= 0) {
        goto __FAILED;
    }
    return RK_SUCCESS;

__FAILED:
    return RK_FAILURE;
}

RK_S32 mpi_vdec_poll_event(RK_S32 timeoutMsec, RK_S32 fd) {
    RK_S32 num_fds = 1;
    struct pollfd pollFds[num_fds];
    RK_S32 ret = 0;

    RK_ASSERT(fd > 0);
    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = fd;
    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

    ret = poll(pollFds, num_fds, timeoutMsec);
    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
        RK_LOGE("fd:%d polled error", fd);
        return -1;
    }

    return ret;
}

RK_S32 mpi_create_vdec(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch, VIDEO_MODE_E enMode) {
    RK_S32 s32Ret = RK_SUCCESS;
    VDEC_CHN_ATTR_S stAttr;
    VDEC_CHN_PARAM_S stVdecParam;
    MB_POOL_CONFIG_S stMbPoolCfg;
    VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
    MB_PIC_CAL_S stMbPicCalResult;
    VDEC_MOD_PARAM_S stModParam;

    memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
    memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
    memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));
    memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

    if (ctx->bEnableMbPool) {
        stModParam.enVdecMBSource = MB_SOURCE_USER;
        s32Ret = RK_MPI_VDEC_SetModParam(&stModParam);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("vdec %d SetModParam failed! errcode %x", ctx->u32ChnIndex, s32Ret);
            return s32Ret;
        }
    }

    stVdecPicBufAttr.enCodecType = ctx->enCodecId;
    stVdecPicBufAttr.stPicBufAttr.u32Width = ctx->u32SrcWidth;
    stVdecPicBufAttr.stPicBufAttr.u32Height = ctx->u32SrcHeight;
    stVdecPicBufAttr.stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32OutputPixFmt;
    stVdecPicBufAttr.stPicBufAttr.enCompMode = (COMPRESS_MODE_E)ctx->u32CompressMode;
    s32Ret = RK_MPI_CAL_VDEC_GetPicBufferSize(&stVdecPicBufAttr, &stMbPicCalResult);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
        return s32Ret;
    }

    stAttr.enLib = ctx->enLib;
    stAttr.enMode = enMode;
    stAttr.enType = ctx->enCodecId;
    stAttr.u32PicWidth = ctx->u32SrcWidth;
    stAttr.u32PicHeight = ctx->u32SrcHeight;
    if (ctx->bEnableDei) {
        // iep must remain 5 buffers for deinterlace
        stAttr.u32FrameBufCnt = ctx->u32FrameBufferCnt + 5;
    } else {
        stAttr.u32FrameBufCnt = ctx->u32FrameBufferCnt;
    }
    stAttr.u32StreamBufCnt = MAX_STREAM_CNT;
    /*
     * if decode 10bit stream, need specify the u32FrameBufSize,
     * other conditions can be set to 0, calculated internally.
     */
    stAttr.u32FrameBufSize = stMbPicCalResult.u32MBSize;
    stAttr.u32FrameBufDepth = 8;
    stAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32OutputPixFmt;
    stAttr.enCompressMode = (COMPRESS_MODE_E)ctx->u32CompressMode;

    if (!ctx->bEnableColmv) {
        stAttr.stVdecVideoAttr.bTemporalMvpEnable = RK_FALSE;
    }

    s32Ret = RK_MPI_VDEC_CreateChn(ctx->u32ChnIndex, &stAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("create %d vdec failed! ", ctx->u32ChnIndex);
        return s32Ret;
    }

    stVdecParam.enType = ctx->enCodecId;
    if (ctx->enCodecId == RK_VIDEO_ID_MJPEG || ctx->enCodecId == RK_VIDEO_ID_JPEG) {
        stVdecParam.stVdecPictureParam.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32OutputPixFmt;
    } else {
        stVdecParam.stVdecVideoParam.enCompressMode = (COMPRESS_MODE_E)ctx->u32CompressMode;
    }
    stVdecParam.stVdecVideoParam.enVideoFormat = (VIDEO_FORMAT_E)ctx->u32VideoFormat;
    if (ctx->bEnableDei) {
        stVdecParam.stVdecVideoParam.bDeiEn = ctx->bEnableDei;
    }
    // it is only effective to disable MV when decoding sequence output
    if (!ctx->bEnableColmv) {
        stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;
    }

    s32Ret = RK_MPI_VDEC_SetChnParam(ctx->u32ChnIndex, &stVdecParam);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("set chn %d param failed %x! ", ctx->u32ChnIndex, s32Ret);
        return s32Ret;
    }

    ctx->s32ChnFd = RK_MPI_VDEC_GetFd(ctx->u32ChnIndex);
    if (ctx->s32ChnFd <= 0) {
        RK_LOGE("get fd chn %d failed %d", ctx->u32ChnIndex, ctx->s32ChnFd);
        return s32Ret;
    }

    if (ctx->bEnableMbPool) {
        memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));

        stMbPoolCfg.u64MBSize = stMbPicCalResult.u32MBSize;
        stMbPoolCfg.u32MBCnt  = 10;
        stMbPoolCfg.enRemapMode = MB_REMAP_MODE_CACHED;
        stMbPoolCfg.bPreAlloc = RK_TRUE;
        ctx->s32Pool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
        if (ctx->s32Pool == MB_INVALID_POOLID) {
            RK_LOGE("create pool failed!");
            return s32Ret;
        }
        s32Ret = RK_MPI_VDEC_AttachMbPool(ctx->u32ChnIndex, ctx->s32Pool);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("attatc vdec mb pool %d failed! ", ctx->u32ChnIndex);
            return s32Ret;
        }
    }

    if (ctx->u32CompressMode == 2) {
        //create vpss
        VPSS_GRP_ATTR_S stGrpAttr;
        VPSS_CHN_ATTR_S stChnAttr;
        RK_S32 s32VpssGrp = 0;
        RK_S32 s32VpssChn = 0;

        memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
        memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));

        stGrpAttr.u32MaxW = ctx->u32SrcWidth;
        stGrpAttr.u32MaxH = ctx->u32SrcHeight;
        stGrpAttr.enPixelFormat = RK_FMT_YUV420SP;
        stGrpAttr.enCompressMode = (COMPRESS_MODE_E)ctx->u32CompressMode;
        stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
        stGrpAttr.stFrameRate.s32DstFrameRate = -1;
        stGrpAttr.enVProcDev = (VIDEO_PROC_DEV_TYPE_E)ctx->s32VProcDevType;
        stChnAttr.enChnMode = VPSS_CHN_MODE_USER;
        stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
        stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        stChnAttr.stFrameRate.s32DstFrameRate = -1;
        stChnAttr.u32Width = ctx->u32SrcWidth;
        stChnAttr.u32Height = ctx->u32SrcHeight;
        stChnAttr.u32FrameBufCnt = 3;
        stChnAttr.u32Depth = 1;

        s32Ret = RK_MPI_VPSS_CreateGrp(s32VpssGrp, &stGrpAttr);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_CreateGrp[%d] failed[%x]", s32VpssGrp, s32Ret);
          return s32Ret;
        }

        s32Ret = RK_MPI_VPSS_StartGrp(s32VpssGrp);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_StartGrp[%d] failed[%x]", s32VpssGrp, s32Ret);
          return s32Ret;
        }

        s32Ret = RK_MPI_VPSS_ResetGrp(s32VpssGrp);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_ResetGrp[%d] failed[%x]", s32VpssGrp, s32Ret);
        }

        s32Ret = RK_MPI_VPSS_SetVProcDev(s32VpssGrp, (VIDEO_PROC_DEV_TYPE_E)ctx->s32VProcDevType);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_SetVProcDev[%d] failed[%x]", s32VpssGrp, s32Ret);
          return s32Ret;
        }


        s32Ret = RK_MPI_VPSS_SetChnAttr(s32VpssGrp, s32VpssChn, &stChnAttr);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_SetChnAttr[%d, %d] failed with[%x]", s32VpssGrp, s32VpssChn, s32Ret);
          return s32Ret;
        }

        s32Ret = RK_MPI_VPSS_EnableChn(s32VpssGrp, s32VpssChn);
        if (s32Ret) {
          RK_LOGE("RK_MPI_VPSS_EnableChn[%d, %d] failed[%x]", s32VpssGrp, s32VpssChn, s32Ret);
          return s32Ret;
        }
    }

    s32Ret = RK_MPI_VDEC_StartRecvStream(ctx->u32ChnIndex);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("start recv chn %d failed %x! ", ctx->u32ChnIndex, s32Ret);
        return s32Ret;
    }

    return RK_SUCCESS;
}

RK_S32 mpi_destory_vdec(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch) {
    RK_MPI_VDEC_StopRecvStream(s32Ch);

    if (ctx->u32CompressMode == 2) {
        RK_S32 s32VpssGrp = 0;
        RK_S32 s32VpssChn = 0;

        RK_MPI_VPSS_DisableChn(s32VpssGrp, s32VpssChn);
        RK_MPI_VPSS_StopGrp(s32VpssGrp);
        RK_MPI_VPSS_DestroyGrp(s32VpssGrp);
    }

    if (ctx->s32ChnFd > 0) {
        RK_MPI_VDEC_CloseFd(s32Ch);
    }

    if (ctx->bEnableMbPool) {
        RK_MPI_VDEC_DetachMbPool(s32Ch);
        RK_MPI_VDEC_DestroyChn(s32Ch);
        RK_MPI_MB_DestroyPool(ctx->s32Pool);
    } else {
        RK_MPI_VDEC_DestroyChn(s32Ch);
    }

    return RK_SUCCESS;
}

RK_S32 mpi_create_stream_mode(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch) {
    VIDEO_MODE_E enMode;

    if (ctx->enCodecId == RK_VIDEO_ID_MJPEG || ctx->enCodecId == RK_VIDEO_ID_JPEG) {
        ctx->u32ReadSize = ctx->u32SrcWidth * ctx->u32SrcHeight;
        enMode = VIDEO_MODE_FRAME;
    } else {
        enMode = VIDEO_MODE_STREAM;
    }

    return mpi_create_vdec(ctx, s32Ch, enMode);
}

RK_S32 mpi_create_frame_mode(TEST_VDEC_CTX_S *ctx, RK_S32 s32Ch) {
    return mpi_create_vdec(ctx, s32Ch, (VIDEO_MODE_E)ctx->u32InputMode);
}

void* mpi_get_vpss(void *pArgs) {
    TEST_VDEC_CTX_S *ctx = reinterpret_cast<TEST_VDEC_CTX_S *>(pArgs);
    FILE *fp = RK_NULL;
    VIDEO_FRAME_INFO_S sFrame;
    RK_S32 s32Ret;
    char name[256] = {0};
    RK_S32 s32FrameCount = 0;

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    if (ctx->dstFilePath != RK_NULL) {
        mkdir(ctx->dstFilePath, 0777);
        snprintf(name, sizeof(name), "%svpss_%d.bin", ctx->dstFilePath, ctx->u32ChnIndex);

        fp = fopen(name, "wb");
        if (fp == RK_NULL) {
            RK_LOGE("can't open output file %s\n", name);
            return NULL;
        }
    }

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    while (!ctx->threadExit) {
        s32Ret = RK_MPI_VPSS_GetChnFrame(0, 0, &sFrame, 1000);
        if (s32Ret >= 0) {
            if (RK_MPI_MB_GetLength(sFrame.stVFrame.pMbBlk) > 0) {
                TEST_COMM_DumpFrame2File(&sFrame, fp);
                s32FrameCount++;
                RK_LOGE("get vpss frame %d", s32FrameCount);
            }

            RK_MPI_VPSS_ReleaseChnFrame(0, 0, &sFrame);
        } else {
            usleep(1000llu);
        }
    }

    if (fp)
        fclose(fp);
    RK_LOGI("%s out", __FUNCTION__);
    return RK_NULL;
}

void* mpi_get_pic(void *pArgs) {
    TEST_VDEC_CTX_S *ctx = reinterpret_cast<TEST_VDEC_CTX_S *>(pArgs);
    FILE *fp = RK_NULL;
    VIDEO_FRAME_INFO_S sFrame;
    RK_S32 s32Ret;
    char name[256] = {0};
    RK_S32 s32FrameCount = 0;

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    if (ctx->dstFilePath != RK_NULL) {
        mkdir(ctx->dstFilePath, 0777);
        snprintf(name, sizeof(name), "%stest_%d.bin", ctx->dstFilePath, ctx->u32ChnIndex);

        fp = fopen(name, "wb");
        if (fp == RK_NULL) {
            RK_LOGE("can't open output file %s\n", name);
            return NULL;
        }
    }

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    while (!ctx->threadExit) {
            if (ctx->s32ChnFd > 0) {
                s32Ret = mpi_vdec_poll_event(-1, ctx->s32ChnFd);
                if (s32Ret < 0) {
                    if (ctx->threadExit)
                        break;

                    usleep(1000llu);
                    continue;
                }
            }

            s32Ret = RK_MPI_VDEC_GetFrame(ctx->u32ChnIndex, &sFrame, MAX_TIME_OUT_MS);
            if (s32Ret >= 0) {
                if (RK_MPI_MB_GetLength(sFrame.stVFrame.pMbBlk) > 0) {
                    TEST_COMM_DumpFrame2File(&sFrame, fp);
                    s32FrameCount++;
                    RK_LOGI("get chn %d frame %d", ctx->u32ChnIndex, s32FrameCount);
                }

                if (ctx->u32CompressMode == 2) {
                    s32Ret = RK_MPI_VPSS_SendFrame(0, 0, &sFrame, -1);
                    if (s32Ret != RK_SUCCESS) {
                        RK_LOGE("Send vpssframe failed[%d]", s32Ret);
                    }
                }

                if ((sFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END) {
                    RK_MPI_VDEC_ReleaseFrame(ctx->u32ChnIndex, &sFrame);
                    RK_LOGI("chn %d reach eos frame.", ctx->u32ChnIndex);
                    break;
                }

                RK_MPI_VDEC_ReleaseFrame(ctx->u32ChnIndex, &sFrame);
            } else {
                if (ctx->threadExit)
                    break;

                usleep(1000llu);
            }
    }

    if (fp)
        fclose(fp);
    RK_LOGI("%s out", __FUNCTION__);
    return RK_NULL;
}


static RK_S32 mpi_vdec_free(void *opaque) {
    if (opaque)
        free(opaque);
    return 0;
}

static void* mpi_send_stream(void *pArgs) {
    TEST_VDEC_CTX_S *ctx = reinterpret_cast<TEST_VDEC_CTX_S *>(pArgs);
    RK_S32 s32Size = 0;
    RK_S32 s32Ret = 0;
    RK_U8 *data = RK_NULL;
    int fd = -1;
    MB_BLK buffer = RK_NULL;
    MB_EXT_CONFIG_S pstMbExtConfig;
    VDEC_STREAM_S stStream;
    RK_S32 s32PacketCount = 0;
    RK_S32 s32ReachEOS = 0;

    memset(&stStream, 0, sizeof(VDEC_STREAM_S));

    fd = open(ctx->srcFileUri, O_RDONLY);
    if (fd < 0) {
        RK_LOGE("open file %s failed", ctx->srcFileUri);
        return RK_NULL;
    }

    while (!ctx->threadExit) {
        if (ctx->enCodecId == RK_VIDEO_ID_MJPEG || ctx->enCodecId == RK_VIDEO_ID_JPEG) {
            int length = mjpeg_get_next_frame_size(fd);
            RK_LOGI("get mjpeg length %d", length);
            ctx->u32ReadSize = length > 0 ? length : 1024;
        }
        data = reinterpret_cast<RK_U8 *>(calloc(ctx->u32ReadSize, sizeof(RK_U8)));
        memset(data, 0, ctx->u32ReadSize);
        s32Size = read(fd, data, ctx->u32ReadSize);
        if (s32Size <= 0) {
            s32ReachEOS = 1;
        }

        memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
        pstMbExtConfig.pFreeCB = mpi_vdec_free;
        pstMbExtConfig.pOpaque = data;
        pstMbExtConfig.pu8VirAddr = data;
        pstMbExtConfig.u64Size = s32Size;

        RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

        stStream.u64PTS = 0;
        stStream.pMbBlk = buffer;
        stStream.u32Len = s32Size;
        stStream.bEndOfStream = s32ReachEOS ? RK_TRUE : RK_FALSE;
        stStream.bEndOfFrame = s32ReachEOS ? RK_TRUE : RK_FALSE;
        stStream.bBypassMbBlk = RK_TRUE;
__RETRY:
        s32Ret = RK_MPI_VDEC_SendStream(ctx->u32ChnIndex, &stStream, MAX_TIME_OUT_MS);
        if (s32Ret < 0) {
            if (ctx->threadExit) {
                mpi_vdec_free(data);
                RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
                break;
            }
            usleep(1000llu);
            goto  __RETRY;
        } else {
            s32PacketCount++;
            RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
            RK_LOGI("send chn %d packet %d", ctx->u32ChnIndex, s32PacketCount);
        }
        if (s32ReachEOS) {
            RK_LOGI("chn %d input reach EOS", ctx->u32ChnIndex);
            break;
        }
    }

    if (fd >= 0)
        close(fd);

    RK_LOGI("%s out\n", __FUNCTION__);
    return RK_NULL;
}

void* mpi_send_frame(void *pArgs) {
    TEST_VDEC_CTX_S *ctx = reinterpret_cast<TEST_VDEC_CTX_S *>(pArgs);
    RK_S32 s32Size = 0;
    RK_S32 s32Ret = 0;
    RK_U8 *data = RK_NULL;
    int fd = -1;
    MB_BLK buffer = RK_NULL;
    MB_EXT_CONFIG_S pstMbExtConfig;
    VDEC_STREAM_S stStream;
    RK_S32 s32PacketCount = 0;
    RK_S32 s32ReachEOS = 0;
    unsigned char nal[5] = {0, 0, 0, 0, 0};
    int eos = 0;
    int frameSize = 0;

    memset(&stStream, 0, sizeof(VDEC_STREAM_S));

    fd = open(ctx->srcFileUri, O_RDONLY);
    if (fd < 0) {
        RK_LOGE("open file %s failed", ctx->srcFileUri);
        return RK_NULL;
    }

    while (!ctx->threadExit) {
        if (ctx->enCodecId == RK_VIDEO_ID_AVC) {
            frameSize = h264_get_next_frame_size(fd, nal, &eos);
        } else if (ctx->enCodecId == RK_VIDEO_ID_HEVC) {
            frameSize = h265_get_next_frame_size(fd, nal, &eos);
        } else if (ctx->enCodecId == RK_VIDEO_ID_MJPEG || ctx->enCodecId == RK_VIDEO_ID_JPEG) {
            frameSize = mjpeg_get_next_frame_size(fd);
        } else {
            RK_LOGE("unsupported codec id %d", ctx->enCodecId);
            break;
        }

        if (frameSize <= 0) {
            if (ctx->enCodecId == RK_VIDEO_ID_MJPEG || ctx->enCodecId == RK_VIDEO_ID_JPEG)
                ctx->threadExit = RK_TRUE;
            else
                s32ReachEOS = eos ? 1 : 0;
            break;
        }

        data = reinterpret_cast<RK_U8 *>(calloc(frameSize, sizeof(RK_U8)));
        if (data == RK_NULL) {
            RK_LOGE("calloc failed, frameSize %d", frameSize);
            break;
        }

        s32Size = read(fd, data, frameSize);
        if (s32Size <= 0) {
            mpi_vdec_free(data);
            s32ReachEOS = 1;
            break;
        }

        memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
        pstMbExtConfig.pFreeCB = mpi_vdec_free;
        pstMbExtConfig.pOpaque = data;
        pstMbExtConfig.pu8VirAddr = data;
        pstMbExtConfig.u64Size = s32Size;
        RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

        stStream.u64PTS = 0;
        stStream.pMbBlk = buffer;
        stStream.u32Len = s32Size;
        stStream.bEndOfStream = RK_FALSE;
        stStream.bEndOfFrame = RK_FALSE;
        stStream.bBypassMbBlk = RK_TRUE;
__FRAME_RETRY:
        s32Ret = RK_MPI_VDEC_SendStream(ctx->u32ChnIndex, &stStream, MAX_TIME_OUT_MS);
        if (s32Ret < 0) {
            if (ctx->threadExit) {
                mpi_vdec_free(data);
                RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
                break;
            }
            usleep(1000llu);
            goto __FRAME_RETRY;
        } else {
            s32PacketCount++;
            RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
            RK_LOGI("send chn %d frame-packet %d", ctx->u32ChnIndex, s32PacketCount);
        }
    }

    if (s32ReachEOS) {
        data = reinterpret_cast<RK_U8 *>(calloc(1024, sizeof(RK_U8)));
        if (data != RK_NULL) {
            memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
            pstMbExtConfig.pFreeCB = mpi_vdec_free;
            pstMbExtConfig.pOpaque = data;
            pstMbExtConfig.pu8VirAddr = data;
            pstMbExtConfig.u64Size = 1024;
            RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

            stStream.u64PTS = 0;
            stStream.pMbBlk = buffer;
            stStream.u32Len = 1024;
            stStream.bEndOfStream = RK_TRUE;
            stStream.bEndOfFrame = RK_TRUE;
            stStream.bBypassMbBlk = RK_TRUE;
            RK_MPI_VDEC_SendStream(ctx->u32ChnIndex, &stStream, MAX_TIME_OUT_MS);
            RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
            RK_LOGI("chn %d input reach EOS", ctx->u32ChnIndex);
        }
    }

    if (fd >= 0)
        close(fd);

    RK_LOGI("%s out\n", __FUNCTION__);
    return RK_NULL;
}

RK_S32 unit_test_mpi_vdec(TEST_VDEC_CTX_S *ctx) {
    RK_U32 u32Ch = 0;
    TEST_VDEC_CTX_S vdecCtx[VDEC_MAX_CHN_NUM];
    pthread_t vdecThread[VDEC_MAX_CHN_NUM];
    pthread_t getPicThread[VDEC_MAX_CHN_NUM];
    pthread_t getVpssThread[VDEC_MAX_CHN_NUM];

    for (u32Ch = 0; u32Ch < ctx->u32ChNum; u32Ch++) {
        if (ctx->u32ChNum > 1) {
            ctx->u32ChnIndex = u32Ch;
        }
        memcpy(&(vdecCtx[u32Ch]), ctx, sizeof(TEST_VDEC_CTX_S));

        if (ctx->u32InputMode == VIDEO_MODE_STREAM) {
            mpi_create_stream_mode(&vdecCtx[u32Ch], u32Ch);
            pthread_create(&vdecThread[u32Ch], 0, mpi_send_stream, reinterpret_cast<void *>(&vdecCtx[u32Ch]));
        } else  {
            mpi_create_frame_mode(&vdecCtx[u32Ch], u32Ch);
            pthread_create(&vdecThread[u32Ch], 0, mpi_send_frame, reinterpret_cast<void *>(&vdecCtx[u32Ch]));
        }

        pthread_create(&getPicThread[u32Ch], 0, mpi_get_pic, reinterpret_cast<void *>(&vdecCtx[u32Ch]));

        if (ctx->u32CompressMode == 2)
            pthread_create(&getVpssThread[u32Ch], 0, mpi_get_vpss, reinterpret_cast<void *>(&vdecCtx[u32Ch]));
    }

    for (u32Ch = 0; u32Ch < ctx->u32ChNum; u32Ch++) {
        pthread_join(vdecThread[u32Ch], RK_NULL);
        pthread_join(getPicThread[u32Ch], RK_NULL);

        vdecCtx[u32Ch].threadExit = RK_TRUE;
        if (ctx->u32CompressMode == 2)
            pthread_join(getVpssThread[u32Ch], RK_NULL);

        if (ctx->u32ChNum > 1) {
            mpi_destory_vdec(&vdecCtx[u32Ch], u32Ch);
        } else {
            mpi_destory_vdec(&vdecCtx[u32Ch], ctx->u32ChnIndex);
        }
    }

    return RK_SUCCESS;
}

static void mpi_vdec_test_show_options(const TEST_VDEC_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input file name        : %s\n", ctx->srcFileUri);
    RK_PRINT("output path            : %s\n", ctx->dstFilePath);
    RK_PRINT("input width            : %d\n", ctx->u32SrcWidth);
    RK_PRINT("input height           : %d\n", ctx->u32SrcHeight);
    RK_PRINT("input codecId          : %d\n", ctx->enCodecId);
    RK_PRINT("loop count             : %d\n", ctx->s32LoopCount);
    RK_PRINT("channel index          : %d\n", ctx->u32ChnIndex);
    RK_PRINT("channel number         : %d\n", ctx->u32ChNum);
    RK_PRINT("output compress mode   : %d\n", ctx->u32CompressMode);
    RK_PRINT("output video format    : %d\n", ctx->u32VideoFormat);
    RK_PRINT("input mode             : %d\n", ctx->u32InputMode);
    RK_PRINT("enable mb pool         : %d\n", ctx->bEnableMbPool);
    RK_PRINT("output pix format      : %d\n", ctx->s32OutputPixFmt);
    RK_PRINT("enable deinterlace     : %d\n", ctx->bEnableDei);
    RK_PRINT("enable colmv           : %d\n", ctx->bEnableColmv);
    RK_PRINT("video process device   : %d\n", ctx->s32VProcDevType);
    return;
}

static const char *const usages[] = {
    "./rk_mpi_vdec_test [-i SRC_PATH] [-o OUTPUT_PATH]...",
    NULL,
};

int main(int argc, const char **argv) {
    TEST_VDEC_CTX_S ctx;
    memset(&ctx, 0, sizeof(TEST_VDEC_CTX_S));

    ctx.u32InputMode = VIDEO_MODE_FRAME;
    ctx.s32LoopCount = 1;
    ctx.u32CompressMode = COMPRESS_MODE_NONE;  // Suggest::COMPRESS_AFBC_16x16;
    ctx.u32FrameBufferCnt = 8;
    ctx.u32ReadSize = 1024;
    ctx.u32ChNum = 1;
    ctx.bEnableColmv = RK_TRUE;
    ctx.s32OutputPixFmt = (RK_S32)RK_FMT_YUV420SP;
    ctx.s32VProcDevType = VIDEO_PROC_DEV_RGA;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input", &(ctx.srcFileUri),
                   "input file name. <required>", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx.dstFilePath),
                   "the directory of decoder output", NULL, 0, 0),
        OPT_INTEGER('C', "codec", &(ctx.enCodecId),
                   "input stream codec(8:h264, 9:mjpeg, 12:h265,...) <required on StreamMode>", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx.s32LoopCount),
                    "loop running count. default(1)", NULL, 0, 0),
        OPT_INTEGER('w', "width", &(ctx.u32SrcWidth),
                    "input source width <required on StreamMode>", NULL, 0, 0),
        OPT_INTEGER('h', "height", &(ctx.u32SrcHeight),
                    "input source height <required on StreamMode>", NULL, 0, 0),
        OPT_INTEGER('\0', "channel_index", &(ctx.u32ChnIndex),
                    "vdec channel index. default(0).", NULL, 0, 0),
        OPT_INTEGER('c', "channel_count", &(ctx.u32ChNum),
                    "vdec channel count. default(1).", NULL, 0, 0),
        OPT_INTEGER('\0', "dec_mode", &(ctx.u32InputMode),
                    "vdec decode mode. range(0:StreamMode, 1:FrameMode, 2:FrameSliceMode, 3:SliceMode). default(1)", NULL, 0, 0),
        OPT_INTEGER('\0', "dec_buf_cnt", &(ctx.u32FrameBufferCnt),
                    "vdec decode output buffer count, default(8)", NULL, 0, 0),
        OPT_INTEGER('\0', "compress_mode", &(ctx.u32CompressMode),
                    "vdec compress mode, default(0); 0: NONE, 1: AFBC_16X16, 2: RFBC_64X4", NULL, 0, 0),
        OPT_INTEGER('\0', "video_format", &(ctx.u32VideoFormat),
                    "vdec video format, default(0); 0: linear, 1: tile 64x16, 2: tile 16x8, 3: tile 4x4, 4: linear discrete", NULL, 0, 0),
        OPT_INTEGER('\0', "en_mbpool", &(ctx.bEnableMbPool),
                    "enable mb pool, default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "pixfmt", &(ctx.s32OutputPixFmt),
                    "jpeg output pixel format, default(0); 0: YUV420SP", NULL, 0, 0),
        OPT_INTEGER('\0', "en_dei", &(ctx.bEnableDei),
                    "enable deinterlace, default(0);", NULL, 0, 0),
        OPT_INTEGER('\0', "en_colmv", &(ctx.bEnableColmv),
                    "enable colmv, default(1);", NULL, 0, 0),
        OPT_INTEGER('\0', "lib", &(ctx.enLib),
                    "using which decode lib, 0(default) 1(codec) 2(mpp), default(0);", NULL, 0, 0),
        OPT_INTEGER('\0', "video_proc_dev_type", &(ctx.s32VProcDevType),
                    "the device type of video process. default(1. 0 is GPU, 1 is RGA, 2 is ISP, 3 is VPSS)", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    if (ctx.enCodecId <= RK_VIDEO_ID_Unused) {
        ctx.enCodecId = detect_codec_id_from_uri(ctx.srcFileUri);
    }
    mpi_vdec_test_show_options(&ctx);

    if (check_options(&ctx)) {
        RK_LOGE("illegal input parameters");
        argparse_usage(&argparse);
        goto __FAILED;
    }

    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        goto __FAILED;
    }

    while (ctx.s32LoopCount > 0) {
        if (unit_test_mpi_vdec(&ctx) < 0) {
            goto __FAILED;
        }
        ctx.s32LoopCount--;
    }

    if (RK_MPI_SYS_Exit() != RK_SUCCESS) {
        goto __FAILED;
    }

    RK_LOGE("test running success!");
    return RK_SUCCESS;
__FAILED:
    RK_LOGE("test running failed! %d count running done not yet.", ctx.s32LoopCount);
    return RK_FAILURE;
}

