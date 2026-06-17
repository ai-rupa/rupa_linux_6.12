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

#undef DBG_MOD_ID
#define DBG_MOD_ID       RK_ID_GDC

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <cstdio>
#include <cerrno>
#include <cstring>

#include "rk_type.h"
#include "rk_debug.h"
#include "rk_mpi_gdc.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_vo.h"
#include "rk_comm_vo.h"
#include <sys/poll.h>
#include <signal.h>

#include "test_comm_argparse.h"
#include "test_comm_imgproc.h"

// for vo
#define RK35XX_VO_DEV_HD0 0
#define RK35XX_VO_DEV_HD1 1
#define RK35XX_VO_GDC_CHN 0
#define RK35XX_VOP_LAYER_CLUSTER_0 0
#define RK35XX_VOP_LAYER_CLUSTER_1 2
#define RK35XX_VOP_LAYER_ESMART_0 4
#define RK35XX_VOP_LAYER_ESMART_1 5
#define RK35XX_VOP_LAYER_SMART_0 6
#define RK35XX_VOP_LAYER_SMART_1 7

#define RK35XX_VO_LINE_CHN 127

#define TEST_GDC_WITH_FD 0

static RK_BOOL gGdcExit = RK_FALSE;
static RK_BOOL gOpenFail = RK_FALSE;
static void sigterm_handler(int sig) {
    RK_PRINT("Catched SIGINT %d\n", sig);
    gGdcExit = RK_TRUE;
}

RK_U16 gau16PointColor[9] = {
    0x8000 | 0x44c4,  // red
    0x8000 | 0x1234,  // blue
    0x8000 | 0x7fff,  // white
    0x8000 | 0x7322,
    0x8000 | 0x5433,
    0x8000 | 0x6544,
    0x8000 | 0x7644,
    0x8000 | 0x8744,
    0x8000 | 0x9844
};

typedef enum _TestGdcDeskCeilMode {
    TEST_GDC_DESK_CEIL_MODE_1P_ADD_1     = 0,
    TEST_GDC_DESK_CEIL_MODE_2P           = 1,
    TEST_GDC_DESK_CEIL_MODE_1_ADD_3      = 2,
    TEST_GDC_DESK_CEIL_MODE_1_ADD_4      = 3,
    TEST_GDC_DESK_CEIL_MODE_1P_ADD_6     = 4,
    TEST_GDC_DESK_CEIL_MODE_1_ADD_8      = 5,
    TEST_GDC_DESK_CEIL_MODE_BUTT
} TestGdcDeskCeilMode;

typedef enum _TestGdcWallMode {
    TEST_GDC_WALL_MODE_1P                = 0,
    TEST_GDC_WALL_MODE_1P_ADD_3          = 1,
    TEST_GDC_WALL_MODE_1P_ADD_4          = 2,
    TEST_GDC_WALL_MODE_1P_ADD_8          = 3,
    TEST_GDC_WALL_MODE_BUTT
} TestGdcWallMode;

typedef enum _TestGdcMode {
    TEST_GDC_BYPASS_VO                   = 0,
    TEST_GDC_GET_FRAME_SEND_VO           = 1,
    TEST_GDC_MODE_BUTT
} TestGdcMode;

typedef struct rkGDC_USR_ATTR_S {
    RK_U32              sensorSeq;
    RK_U32              vframeSeq;
    RK_U32              sendSeq;
    RK_U32              used[2];
    GDC_VFAME_INFO_S    vInfo[2];
    RK_U32              exit;
} GDC_USR_ATTR_S;

typedef struct _rkMpiGdcCtx {
    const char *srcFilePath;
    const char *dstFilePath;
    RK_S32  s32LoopCount;
    RK_S32  s32JobNum;
    RK_S32  s32TaskSum;
    RK_S32  s32TaskType;
    RK_S32  s32TaskMode;
    RK_S32  u32SrcWidth;
    RK_S32  u32SrcHeight;
    RK_S32  u32SrcVirWidth;
    RK_S32  u32SrcVirHeight;
    RK_S32  u32LineWidth;
    RK_S32  u32LineHeight;
    RK_S32  s32SrcCompressMode;
    RK_S32  s32SrcPixFormat;
    RK_S32  u32DstWidth;
    RK_S32  u32DstHeight;
    RK_S32  u32DstVirWidth;
    RK_S32  u32DstVirHeight;
    RK_S32  s32DstCompressMode;
    RK_S32  s32DstPixFormat;
    RK_S32  s32JobIdx;
    RK_U32  u32SrcSize;
    RK_U32  u32DstSize;
    RK_U32  u32ShowPoint;
    RK_U32  u32ChangeParam;
    RK_U32  u32PointLen;
    MB_POOL inPool;
    MB_POOL outPool;
    TestGdcDeskCeilMode enDeskCeilMode;
    TestGdcWallMode enWallMode;
    FISHEYE_JOB_CONFIG_S stFisheyeJobConfig;
    GDC_TASK_ATTR_S stTask;
    FISHEYE_ATTR_S stFisheyeAttr;
    POINT_S *pastDstPoint;
    POINT_S *pastSrcPoint;
    // for vo
    VO_CHN_ATTR_S stChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_LAYER s32VoLayer;
    VO_DEV s32VoDev;
    VO_CHN s32VoChn;
    VO_INTF_SYNC_E stVoOutputResolution;
    RK_S32 u32VoWidth;
    RK_S32 u32VoHeight;
    RECT_S stChnRect;
    TestGdcMode enTestMode;
    RK_S32 s32Type;
    const char *pCfgFile;
    RK_S32      s32ChnIndex;
    RK_S32      s32DevFd;
    RK_S32      s32FdGetEnable;
    RK_S32      s32ChnNum;
    MB_POOL     chnPool[GDC_MAX_CHN_NUM];
    RK_S32           s32EisType;
    GDC_USR_ATTR_S   stUsrAttr[GDC_MAX_CHN_NUM];
    GDC_INFO_CB_S    infoCB[GDC_MAX_CHN_NUM];
    GDC_CHN_ATTR_S   stGdcChnAttr;
    RK_S32           s32ChnMode;
    const char      *pMesh0;
    const char      *pMesh1;
    const char      *pMesh2;
    const char      *pMesh3;
    RK_S32           s32FrameCnt;
    RK_S32           s32FecMode;
    RK_S32           s32BorderMode;
    RK_S32           s32CrossBufMode;
    RK_S32           s32BgY;
    RK_S32           s32BgU;
    RK_S32           s32BgV;
    RK_S32           s32DstVideoFormat;
    const char      *pIniFile;
    RK_S32           s32WriteCnt;
    RK_BOOL          bUpdate;
    RK_BOOL          bFileMode;
    RK_S32           s32FecUpdateMode;
    RK_S32           s32SrcVideoFormat;
    RK_U32           u32DisBufCnt;
    RK_U32           u32CropRatio;
    MB_BLK           meshBlk[GDC_MAX_CHN_NUM];
    MB_BLK           updateMeshBlk[GDC_MAX_CHN_NUM];
} TEST_GDC_CTX_S;

typedef enum _rkGdcChangeType {
    TEST_GDC_CHANGE_TYPE_VIEW_MODE,
    TEST_GDC_CHANGE_TYPE_IN_RADIUS,
    TEST_GDC_CHANGE_TYPE_OUT_RADIUS,
    TEST_GDC_CHANGE_TYPE_PAN,
    TEST_GDC_CHANGE_TYPE_TILT,
    TEST_GDC_CHANGE_TYPE_HOR_ZOOM,
    TEST_GDC_CHANGE_TYPE_VER_ZOOM,
    TEST_GDC_CHANGE_TYPE_ALL
} TEST_GDC_CHANGE_TYPE;

static int cal_fec_mesh_size(int width, int height) {
    int block_w = 16, block_h = 8;
    int density = width >= 1920 ? 0 : width >= 512 ? 1 : 2;

    if (density == 0) {
        block_w = 32;
        block_h = 16;
    } else if (density == 1) {
        block_w = 16;
        block_h = 8;
    } else if (density == 2) {
        block_w = 4;
        block_h = 4;
    }

    width  = 32 * ((width + 31) / 32);
    height = 32 * ((height + 31) / 32);

    int lut_w = width / block_w + 1;
    int lut_h = height / block_h + 1;

    int lut_size = lut_w * lut_h;

    RK_LOGI("lut info: %dx%d, desity %d, size %d", lut_w, lut_h, density,
              lut_size);

    return lut_size;
}

static int load_mesh_file(const char* file, void* vir_addr, size_t size) {
    FILE* fp = fopen(file, "rb");
    int ret  = 0;

    if (fp) {
        ret = fseek(fp, 0L, SEEK_END);
        if (ret < 0) {
            RK_LOGE("%s fseek to end failed\n", file);
            fclose(fp);
            return ret;
        }

        size_t len = ftell(fp);
        rewind(fp);

        if (len > size) {
            RK_LOGE("mesh file len %zu > buf size %zu, limit read to %zu bytes only\n", len,
                        size, size);
            len = size;
        }

        size_t rd_size = fread(vir_addr, 1, len, fp);
        if (rd_size < len) RK_LOGE("file read size:%zu < %zu\n", rd_size, len);

        RK_LOGI("mesh file len %zu, read size %zu\n", len, rd_size);
        fclose(fp);
    } else {
        RK_LOGE("open file %s failed: %s\n", file, strerror(errno));
    }

    return ret;
}

static RK_S32 create_vo(TEST_GDC_CTX_S *ctx, RK_U32 u32Ch) {
    /* Enable VO */
    VO_PUB_ATTR_S VoPubAttr;
    RK_S32 s32Ret = RK_SUCCESS;
    VO_LAYER VoLayer = ctx->s32VoLayer;
    VO_DEV VoDev = ctx->s32VoDev;

    memset(&VoPubAttr, 0, sizeof(VO_PUB_ATTR_S));

    s32Ret = RK_MPI_VO_BindLayer(VoLayer, VoDev, VO_LAYER_MODE_VIDEO);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VO_BindLayer failed,s32Ret:%d\n", s32Ret);
        return RK_FAILURE;
    }

    s32Ret = RK_MPI_VO_GetPubAttr(VoDev, &VoPubAttr);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    VoPubAttr.enIntfType = VO_INTF_HDMI;
    VoPubAttr.enIntfSync = ctx->stVoOutputResolution;

    s32Ret = RK_MPI_VO_SetPubAttr(VoDev, &VoPubAttr);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_Enable(VoDev);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_SetLayerAttr(VoLayer, &ctx->stLayerAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VO_SetLayerAttr failed,s32Ret:%d\n", s32Ret);
        return RK_FAILURE;
    }

    s32Ret = RK_MPI_VO_EnableLayer(VoLayer);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VO_EnableLayer failed,s32Ret:%d\n", s32Ret);
        return RK_FAILURE;
    }

    s32Ret = RK_MPI_VO_SetChnAttr(VoLayer, u32Ch, &ctx->stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("set chn Attr failed,s32Ret:%d\n", s32Ret);
        return RK_FAILURE;
    }

    return s32Ret;
}

RK_S32 RKGdcSensorCB(RK_VOID *pUsr, GDC_SENSOR_INFO_S *pInfo) {
//    RK_LOGI("pUsr = %p, pInfo = %p", pUsr, pInfo);
    if (!pUsr || !pInfo) {
        RK_LOGE("pUsr(%p) or pInfo(%p) is NULL", pUsr, pInfo);
        return GDC_INFO_CB_ERROR;
    }

    GDC_USR_ATTR_S *pstUsr = (GDC_USR_ATTR_S *)pUsr;
    RK_LOGI("sensor seq = %d, time stamp = %lld",
            pstUsr->sensorSeq++, pInfo->s64Timestamp);
    return GDC_INFO_CB_SUCCESS;
}

RK_S32 RKGdcVframeCB(RK_VOID *pUsr, GDC_VFAME_INFO_S *pInfo) {
    RK_S32 ret = GDC_INFO_CB_ERROR;
    if (!pUsr || !pInfo) {
        RK_LOGE("pUsr(%p) or pInfo(%p) is NULL", pUsr, pInfo);
        return GDC_INFO_CB_ERROR;
    }

    GDC_USR_ATTR_S *pstUsr = (GDC_USR_ATTR_S *)pUsr;
    void *data = RK_MPI_MB_Handle2VirAddr(pInfo->pMbBlk);
    RK_U64 len = RK_MPI_MB_GetSize(pInfo->pMbBlk);
    RK_LOGI("vframe seq = %d, data = %p, len = %lld, time stamp = %llu",
            pInfo->u32Seq, data, len, pInfo->u64PTS);
    while (ret) {
        for (int i = 0; i < 2; i++) {
            if (!pstUsr->used[i]) {
                memcpy(&pstUsr->vInfo[i], pInfo, sizeof(GDC_VFAME_INFO_S));
                ret = GDC_INFO_CB_SUCCESS;
                pstUsr->used[i] = 1;
                break;
            }
        }

        usleep(1000);
    }

    pstUsr->vframeSeq++;
    return ret;
}

#if 0
RK_S32 unit_test_mpi_gdc(TEST_GDC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    pthread_t tids[GDC_MAX_JOB_NUM];
    TEST_GDC_CTX_S tmpCtx[GDC_MAX_JOB_NUM];

    for (RK_S32 jobIndex = 0; jobIndex < ctx->s32JobNum; jobIndex++) {
        memcpy(&(tmpCtx[jobIndex]), ctx, sizeof(TEST_GDC_CTX_S));
        pthread_create(&tids[jobIndex], 0, unit_test_gdc_counts, reinterpret_cast<void *>(&tmpCtx[jobIndex]));
        ctx->s32JobIdx++;
    }

    for (RK_S32 jobIndex = 0; jobIndex < ctx->s32JobNum; jobIndex++) {
        pthread_join(tids[jobIndex], RK_NULL);
    }

    return s32Ret;
}
#endif
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
        case RK_FMT_YUV420SP:
        case RK_FMT_YUV420SP_VU: {
            for (u32Row = 0; u32Row < u32Height; u32Row++) {
                u32ReadSize = fread(pBufy + u32Row * u32VirWidth, 1, u32Width, fp);
                if (u32ReadSize != u32Width) {
                     return RK_FAILURE;
                }
            }

            for (u32Row = 0; u32Row < u32Height / 2; u32Row++) {
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

RK_U32 unit_test_gdc_get_src_size(TEST_GDC_CTX_S *ctx, MB_PIC_CAL_S *pstMbPicCalResult) {
    RK_S32 s32Ret = RK_SUCCESS;
    PIC_BUF_ATTR_S stPicBufAttr = {0};

    if (ctx->u32SrcVirWidth == 0) {
        ctx->u32SrcVirWidth = ctx->u32SrcWidth;
    }
    if (ctx->u32SrcVirHeight == 0) {
        ctx->u32SrcVirHeight = ctx->u32SrcHeight;
    }

    stPicBufAttr.u32Width = ctx->u32SrcVirWidth;
    stPicBufAttr.u32Height = ctx->u32SrcVirHeight;
    stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32SrcPixFormat;
    stPicBufAttr.enCompMode = (COMPRESS_MODE_E)ctx->s32SrcCompressMode;
    s32Ret = RK_MPI_CAL_COMM_GetPicBufferSize(&stPicBufAttr, pstMbPicCalResult);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
        return s32Ret;
    }

    return RK_SUCCESS;
}

RK_U32 unit_test_gdc_get_dst_size(TEST_GDC_CTX_S *ctx, MB_PIC_CAL_S *pstMbPicCalResult) {
    RK_S32 s32Ret = RK_SUCCESS;
    PIC_BUF_ATTR_S stPicBufAttr;
    MB_PIC_CAL_S   stMbPicCalResult;

    stPicBufAttr.u32Width = ctx->u32DstWidth;
    stPicBufAttr.u32Height = ctx->u32DstHeight;
    stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32DstPixFormat;
    stPicBufAttr.enCompMode = (COMPRESS_MODE_E)ctx->s32DstCompressMode;
    s32Ret = RK_MPI_CAL_COMM_GetPicBufferSize(&stPicBufAttr, pstMbPicCalResult);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
        return s32Ret;
    }

    return 0;
}

RK_VOID test_mpi_gdc_change_parameter(TEST_GDC_CTX_S *ctx, RK_S32 index, TEST_GDC_CHANGE_TYPE type) {
    FISHEYE_REGION_ATTR_S regionAttr;
    memcpy(&regionAttr, &ctx->stFisheyeAttr.astFishEyeRegionAttr[index], sizeof(FISHEYE_REGION_ATTR_S));

    switch (type) {
        case TEST_GDC_CHANGE_TYPE_VIEW_MODE:
        break;
        case TEST_GDC_CHANGE_TYPE_IN_RADIUS:
        break;
        case TEST_GDC_CHANGE_TYPE_OUT_RADIUS:
        break;
        case TEST_GDC_CHANGE_TYPE_PAN:
            if (regionAttr.u32Pan < 360)
                regionAttr.u32Pan++;
            else
                regionAttr.u32Pan = 0;
        break;
        case TEST_GDC_CHANGE_TYPE_TILT:
            if (regionAttr.u32Tilt < 360)
                regionAttr.u32Tilt++;
            else
                regionAttr.u32Tilt = 0;
        break;
        case TEST_GDC_CHANGE_TYPE_HOR_ZOOM:
            if (regionAttr.u32HorZoom < 4095)
                regionAttr.u32HorZoom++;
            else
                regionAttr.u32HorZoom = 1;
        break;
        case TEST_GDC_CHANGE_TYPE_VER_ZOOM:
            if (regionAttr.u32VerZoom < 4095)
                regionAttr.u32VerZoom++;
            else
                regionAttr.u32VerZoom = 1;
        break;
        case TEST_GDC_CHANGE_TYPE_ALL:
        break;
    }
    memcpy(&ctx->stFisheyeAttr.astFishEyeRegionAttr[index], &regionAttr, sizeof(FISHEYE_REGION_ATTR_S));
}

RK_VOID test_mpi_gdc_get_src_point(TEST_GDC_CTX_S *ctx) {
    RK_S32 s32Ret;
    for (RK_U32 region = 1; region < ctx->stFisheyeAttr.u32RegionNum; region++) {
        GDC_FISHEYE_POINT_QUERY_ATTR_S stQuery;
        POINT_S stDstPoint, stSrcPoint;
        memset(&stQuery, 0, sizeof(sizeof(FISHEYE_ATTR_S)));
        stQuery.pstFishEyeAttr = &ctx->stFisheyeAttr;
        stQuery.u32RegionIndex = region;
        stDstPoint.s32X = 0;
        stDstPoint.s32Y = 540;
        s32Ret = RK_MPI_GDC_FisheyePosQueryDst2Src(&stQuery, &ctx->stTask.stImgIn, &stDstPoint, &stSrcPoint);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_GDC_FisheyePosQueryDst2Src err:0x%x!", s32Ret);
        }
        RK_LOGD("region:%d dst:%d,%d src:%d,%d", region, stDstPoint.s32X, stDstPoint.s32Y,
                                                 stSrcPoint.s32X, stSrcPoint.s32Y);
    }
}

RK_VOID test_mpi_gdc_get_pano_point(TEST_GDC_CTX_S *ctx) {
    RK_S32 s32Ret;
    for (RK_U32 region = 1; region < ctx->stFisheyeAttr.u32RegionNum; region++) {
        GDC_FISHEYE_POINT_QUERY_ATTR_S stQuery;
        POINT_S stDstPoint, stPanoPoint;
        memset(&stQuery, 0, sizeof(sizeof(FISHEYE_ATTR_S)));
        stQuery.pstFishEyeAttr = &ctx->stFisheyeAttr;
        stQuery.u32RegionIndex = region;
        stDstPoint.s32X = 0;
        stDstPoint.s32Y = 540;
        s32Ret = RK_MPI_GDC_FisheyePosQueryDst2Pano(&stQuery, &ctx->stTask.stImgIn, 0, &stDstPoint, &stPanoPoint);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_GDC_FisheyePosQueryDst2Pano err:0x%x!", s32Ret);
        }
        RK_LOGD("region:%d dst:%d,%d pano:%d,%d", region, stDstPoint.s32X, stDstPoint.s32Y,
                                                  stPanoPoint.s32X, stPanoPoint.s32Y);
    }
}

RK_S32 test_mpi_gdc_get_src_point_array(TEST_GDC_CTX_S *ctx, RK_U32 region) {
    RK_S32 s32Ret;
    RK_FLOAT step;
    RK_U32 w = ctx->stFisheyeAttr.astFishEyeRegionAttr[region].stOutRect.u32Width;
    RK_U32 h = ctx->stFisheyeAttr.astFishEyeRegionAttr[region].stOutRect.u32Height;

    ctx->u32PointLen = ctx->u32PointLen > ((w + h) * 2) ? (w + h) * 2 : ctx->u32PointLen;
    step = (RK_FLOAT)((w + h) * 2) / ctx->u32PointLen;

    for (RK_U32 i = 0; i < ctx->u32PointLen; i++) {
        if ((step * i) <= w) {
            ctx->pastDstPoint[i].s32X = step * i;
            ctx->pastDstPoint[i].s32Y = 0;
        } else if ((step * i) <= (w + h)) {
            ctx->pastDstPoint[i].s32X = (w - 1);
            ctx->pastDstPoint[i].s32Y = (step * i - w);
            // printf("22 region:%d i%d= %d,%d\n", region, i, ctx->pastDstPoint[i].s32X, ctx->pastDstPoint[i].s32Y);
        } else if ((step * i) <= (2 * w + h)) {
            ctx->pastDstPoint[i].s32X = 2 * w + h - step * i;
            ctx->pastDstPoint[i].s32Y = h - 1;
            // printf("33 region:%d i%d=%d\n", region, i, ctx->pastDstPoint[i].s32X);
        } else {
            ctx->pastDstPoint[i].s32X = 0;
            ctx->pastDstPoint[i].s32Y = (2 * (w + h)- step * i);
            // printf("44 region:%d i%d=%d\n", region, i, ctx->pastDstPoint[i].s32Y);
        }
    }

    // get the array
    GDC_FISHEYE_POINT_QUERY_ATTR_S stQuery;
    memset(&stQuery, 0, sizeof(sizeof(FISHEYE_ATTR_S)));
    stQuery.pstFishEyeAttr = &ctx->stFisheyeAttr;
    stQuery.u32RegionIndex = region;
    s32Ret = RK_MPI_GDC_FisheyePosQueryDst2SrcArray(&stQuery, &ctx->stTask.stImgIn, ctx->u32PointLen,
                                                    ctx->pastDstPoint, ctx->pastSrcPoint);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_GDC_FisheyePosQueryDst2SrcArray err:0x%x!", s32Ret);
    }

    return s32Ret;
}

RK_S32 test_mpi_gdc_get_pano_point_array(TEST_GDC_CTX_S *ctx, RK_U32 region, RK_U32 pano_region) {
    RK_S32 s32Ret;
    RK_FLOAT step;
    RK_U32 w = ctx->stFisheyeAttr.astFishEyeRegionAttr[region].stOutRect.u32Width;
    RK_U32 h = ctx->stFisheyeAttr.astFishEyeRegionAttr[region].stOutRect.u32Height;

    ctx->u32PointLen = ctx->u32PointLen > ((w + h) * 2) ? (w + h) * 2 : ctx->u32PointLen;
    step = (RK_FLOAT)((w + h) * 2) / ctx->u32PointLen;

    for (RK_U32 i = 0; i < ctx->u32PointLen; i++) {
        if ((step * i) <= w) {
            ctx->pastDstPoint[i].s32X = step * i;
            ctx->pastDstPoint[i].s32Y = 0;
        } else if ((step * i) <= (w + h)) {
            ctx->pastDstPoint[i].s32X = (w - 1);
            ctx->pastDstPoint[i].s32Y = (step * i - w);
            // printf("22 region:%d i%d= %d,%d\n", region, i, ctx->pastDstPoint[i].s32X, ctx->pastDstPoint[i].s32Y);
        } else if ((step * i) <= (2 * w + h)) {
            ctx->pastDstPoint[i].s32X = 2 * w + h - step * i;
            ctx->pastDstPoint[i].s32Y = h - 1;
            // printf("33 region:%d i%d=%d\n", region, i, ctx->pastDstPoint[i].s32X);
        } else {
            ctx->pastDstPoint[i].s32X = 0;
            ctx->pastDstPoint[i].s32Y = (2 * (w + h)- step * i);
            // printf("44 region:%d i%d=%d\n", region, i, ctx->pastDstPoint[i].s32Y);
        }
    }

    // get the array
    GDC_FISHEYE_POINT_QUERY_ATTR_S stQuery;
    memset(&stQuery, 0, sizeof(sizeof(FISHEYE_ATTR_S)));
    stQuery.pstFishEyeAttr = &ctx->stFisheyeAttr;
    stQuery.u32RegionIndex = region;
    s32Ret = RK_MPI_GDC_FisheyePosQueryDst2PanoArray(&stQuery, &ctx->stTask.stImgIn, pano_region, ctx->u32PointLen,
                                                     ctx->pastDstPoint, ctx->pastSrcPoint);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_GDC_FisheyePosQueryDst2PanoArray err:0x%x!", s32Ret);
    }

    return s32Ret;
}

RK_VOID test_mpi_gdc_desktop_ceil_mount_1p_add_1(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4132;
    fishOptParam_originY = 4169;
    RK_U32 outWidthBase = 1920;
    RK_U32 outHeightBase = 360;
    RK_U32 outYBase = 720;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 1920;
        outHeightBase = 360;
        outYBase = ctx->u32SrcHeight - outHeightBase;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 2560;
        outHeightBase = 480;
        outYBase = ctx->u32SrcHeight - outHeightBase;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 3840;
        outHeightBase = 720;
        outYBase = ctx->u32SrcHeight - outHeightBase;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 7680;
        outHeightBase = 1440;
        outYBase = ctx->u32SrcHeight - outHeightBase;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outYBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_360_PANORAMA;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outYBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_desktop_ceil_mount_1_add_3(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4132;
    fishOptParam_originY = 4169;
    RK_U32 outWidthBase = 640;
    RK_U32 outHeightBase = 360;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 960;
        outHeightBase = 540;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 1280;
        outHeightBase = 720;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 1920;
        outHeightBase = 1080;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 3840;
        outHeightBase = 2160;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;

    if (ctx->u32ShowPoint == 1)
        ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_NO_TRANSFORMATION;
    else
        ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_360_PANORAMA;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 120; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 240; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 0; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_desktop_ceil_mount_1_add_8(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4132;
    fishOptParam_originY = 4169;
    RK_U32 outWidthBase = 640;
    RK_U32 outHeightBase = 360;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 640;
        outHeightBase = 360;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 848;
        outHeightBase = 480;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 1280;
        outHeightBase = 720;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 2560;
        outHeightBase = 1440;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;

    // 0
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 0; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 1
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 45; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 2
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 90; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 3
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 135; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 4
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 5
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 6
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 225; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 7
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 270; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 8
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_NO_TRANSFORMATION;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 315; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 98; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_desktop_ceil_mount_2p(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4132;
    fishOptParam_originY = 4169;
    RK_U32 outWidthBase = 1920;
    RK_U32 outHeightBase = 540;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 1920;
        outHeightBase = 540;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 2560;
        outHeightBase = 720;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 3840;
        outHeightBase = 1080;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 7680;
        outHeightBase = 2160;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_360_PANORAMA;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 0; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 2047; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_360_PANORAMA;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 474; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 2047; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_desktop_mount(TEST_GDC_CTX_S *ctx) {
    switch (ctx->enDeskCeilMode) {
        case TEST_GDC_DESK_CEIL_MODE_1P_ADD_1:
            test_mpi_gdc_desktop_ceil_mount_1p_add_1(ctx);
        break;
        case TEST_GDC_DESK_CEIL_MODE_1_ADD_3:
            test_mpi_gdc_desktop_ceil_mount_1_add_3(ctx);
        break;
        case TEST_GDC_DESK_CEIL_MODE_1_ADD_8:
            test_mpi_gdc_desktop_ceil_mount_1_add_8(ctx);
        break;
         case TEST_GDC_DESK_CEIL_MODE_2P:
            test_mpi_gdc_desktop_ceil_mount_2p(ctx);
        break;
        default:
            RK_LOGE("not support this mode:%d", ctx->enDeskCeilMode);
        break;
    }
}

RK_VOID test_mpi_gdc_ceiling_mount(TEST_GDC_CTX_S *ctx) {
    test_mpi_gdc_desktop_mount(ctx);
}

RK_VOID test_mpi_gdc_wall_mount_1p_add_3(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4364;
    fishOptParam_originY = 4136;
    RK_U32 outWidthBase = 640;
    RK_U32 outHeightBase = 360;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 960;
        outHeightBase = 540;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 1280;
        outHeightBase = 720;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 1920;
        outHeightBase = 1080;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 3840;
        outHeightBase = 2160;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_180_PANORAMA;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_wall_mount_1p_add_8(TEST_GDC_CTX_S *ctx) {
    RK_S32 index = 0;
    RK_U32 fishOptParam_originX, fishOptParam_originY;
    fishOptParam_originX = 4364;
    fishOptParam_originY = 4136;
    RK_U32 outWidthBase = 640;
    RK_U32 outHeightBase = 360;

    if (ctx->u32SrcWidth == 1920 && ctx->u32SrcHeight == 1080) {
        outWidthBase = 640;
        outHeightBase = 360;
    } else if (ctx->u32SrcWidth == 2560 && ctx->u32SrcHeight == 1440) {
        outWidthBase = 848;
        outHeightBase = 480;
    } else if (ctx->u32SrcWidth == 3840 && ctx->u32SrcHeight == 2160) {
        outWidthBase = 1280;
        outHeightBase = 720;
    } else if (ctx->u32SrcWidth == 7680 && ctx->u32SrcHeight == 4320) {
        outWidthBase = 2560;
        outHeightBase = 1440;
    }

    ctx->stFisheyeAttr.bEnable = RK_TRUE;
    ctx->stFisheyeAttr.bLMF = RK_FALSE;
    ctx->stFisheyeAttr.bBgColor = RK_TRUE;
    ctx->stFisheyeAttr.u32BgColor = 0;
    ctx->stFisheyeAttr.s32HorOffset = ((fishOptParam_originX >> 3) - 512);
    ctx->stFisheyeAttr.s32VerOffset = ((fishOptParam_originY >> 3) - 512);
    ctx->stFisheyeAttr.u32TrapezoidCoef = 0;
    ctx->stFisheyeAttr.s32FanStrength = 0;
    // ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;

    // 0
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 1
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 2
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = 0;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 3
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 4 scale
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_180_PANORAMA;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 5
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 6
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 153; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = 0; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 7
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 180; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase; /* RW; out Imge rectangle attribute */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    // 8
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].enViewMode = FISHEYE_VIEW_NORMAL;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32InRadius = 0; /* RW; inner radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32OutRadius = 442; /* RW; out radius of gdc correction region */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Pan = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32Tilt = 207; /* RW; Range: [0, 360] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32HorZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].u32VerZoom = 4095; /* RW; Range: [1, 4095] */
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32X = outWidthBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.s32Y = outHeightBase * 2;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Width = outWidthBase;
    ctx->stFisheyeAttr.astFishEyeRegionAttr[index].stOutRect.u32Height = outHeightBase;
    index++;

    ctx->stFisheyeAttr.u32RegionNum = index;
}

RK_VOID test_mpi_gdc_wall_mount(TEST_GDC_CTX_S *ctx) {
    switch (ctx->enWallMode) {
        case TEST_GDC_WALL_MODE_1P_ADD_3:
            test_mpi_gdc_wall_mount_1p_add_3(ctx);
        break;
        case TEST_GDC_WALL_MODE_1P_ADD_8:
            test_mpi_gdc_wall_mount_1p_add_8(ctx);
        break;
        default:
            RK_LOGE("not support this mode:%d", ctx->enWallMode);
        break;
    }
}

static RK_BOOL test_mpi_gdc_xy_in_valid(RK_U32 xy, RK_U32 max, RK_S32 op) {
     RK_S32 result = xy + op;
     if (result < max && result >= 0)
         return RK_TRUE;
     else
         return RK_FALSE;
}

static int alignUp(int x, int y) {
    return ((x + y - 1) / y) * y;
}

static int readBinaryFile(const char* filename, void* pData, size_t targetSize) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Failed to open file '%s'\n", filename);
        return -1;
    }

    if (targetSize == 0) {
        fseek(fp, 0, SEEK_END);     /* move fp to end of file */
        targetSize = ftell(fp);
        fseek(fp, 0, SEEK_SET);     /* move fp back to beginning of file */
    }
    size_t readSize = fread(pData, sizeof(unsigned char), targetSize, fp);
    if (readSize != targetSize) {
        printf("read size (%zu) != target size (%zu). The file '%s' might be incomplete!\n", readSize, targetSize, filename);
    }
    fclose(fp);
    return 0;
}

static int idcMeshCreate(TEST_GDC_CTX_S *ctx, GDC_IDC_MESH_S *pstMesh, MB_BLK *pstBlk, const char *pMesh) {
    RK_S32 s32Ret = 0;
    RK_U32 u32DstStride =  alignUp(ctx->u32DstWidth, 16);
    pstMesh->u32StepX     = 16;
    pstMesh->u32StepY     = 8;
    pstMesh->u32Width     = (u32DstStride - 1) / pstMesh->u32StepX + 2;
    pstMesh->u32Height    = (ctx->u32DstHeight - 1) / pstMesh->u32StepY + 2;
    pstMesh->u32Stride    = pstMesh->u32Width;
    pstMesh->u32HgtStride = pstMesh->u32Height;
    pstMesh->enMeshType   = GDC_IDC_MESH_TYPE_MERGED;
    RK_U32 meshBpp = 8 * sizeof(float) * 2;
    RK_U32 stride = pstMesh->u32Width * meshBpp / 8;
    stride = alignUp(stride, 64);
    RK_U32 allocMeshSize = stride * pstMesh->u32Height;
    RK_LOGI("IDC mesh size = %u, stride = %u, height = %u, in = %p", allocMeshSize, stride, pstMesh->u32Height, *pstBlk);
    if (!(*pstBlk)) {
        s32Ret = RK_MPI_SYS_MmzAlloc(pstBlk, RK_NULL, RK_NULL, allocMeshSize);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("IDC mesh alloc failed, size = %u", allocMeshSize);
            return s32Ret;
        }
    }

    void  *pSrcData = RK_MPI_MB_Handle2VirAddr(*pstBlk);
    if (*pstBlk) {
        RK_U32 readMeshSize = pstMesh->u32Width * pstMesh->u32Height * sizeof(float) * 2;
        s32Ret = readBinaryFile(pMesh, pSrcData, readMeshSize);
        RK_MPI_SYS_MmzFlushCache(*pstBlk, RK_FALSE);
    }

    pstMesh->s32Fd[0]    = RK_MPI_MB_Handle2Fd(*pstBlk);
    pstMesh->pVirAddr[0] = pSrcData;
    RK_LOGI("IDC mesh blk = %p, vaddr = %p, fd = %d", *pstBlk, pSrcData, pstMesh->s32Fd[0]);
    return 0;
}

static void* sendDataThread(void * ptr) {
    TEST_GDC_CTX_S      *pstCtx         = (TEST_GDC_CTX_S *)(ptr);
    RK_S32               s32Ret         = RK_SUCCESS;
    RK_U8               *pVirAddr       = RK_NULL;
    MB_BLK               blk            = RK_NULL;
    RK_S32               s32ReachEOS    = 0;
    RK_S32               result         = 0;
    RK_S32               s32MilliSec    = 1000;
    FILE                *fp             = RK_NULL;
    RK_S32               s32FrameCount  = 0;
    RK_U32               u32TimeRef     = 0;
    VIDEO_FRAME_INFO_S   stFrame        = {0};
    MB_POOL_CONFIG_S     stMbPoolCfg    = {0};
    RK_S32               cnt            = 0;
    GDC_UPDATE_ATTR_S    attr;
    MB_PIC_CAL_S         stMbPicCalResult = {0};
    void* meshBuf                       = NULL;
    memset(&attr, 0, sizeof(attr));
    result = unit_test_gdc_get_src_size(pstCtx, &stMbPicCalResult);
    if (result) {
        return NULL;
    }

    pstCtx->u32SrcSize = stMbPicCalResult.u32MBSize;
    pstCtx->u32SrcVirWidth  = stMbPicCalResult.u32VirWidth;
    pstCtx->u32SrcVirHeight = stMbPicCalResult.u32VirHeight;
    stMbPoolCfg.u64MBSize = pstCtx->u32SrcSize;
    stMbPoolCfg.u32MBCnt = pstCtx->stGdcChnAttr.u32MaxInQueue;
    stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
    stMbPoolCfg.enRemapMode = MB_REMAP_MODE_CACHED;
    pstCtx->chnPool[pstCtx->s32ChnIndex] = RK_MPI_MB_CreatePool(&stMbPoolCfg);
    while (1) {
        blk = RK_MPI_MB_GetMB(pstCtx->chnPool[pstCtx->s32ChnIndex], pstCtx->u32SrcSize, RK_TRUE);
        if (RK_NULL == blk) {
            RK_LOGE("RK_MPI_MB_GetMB fail:%d", 0);
            usleep(2000llu);
            continue;
        }

        pVirAddr = (RK_U8 *)(RK_MPI_MB_Handle2VirAddr(blk));
        if (pstCtx->srcFilePath) {
            if (fp == RK_NULL) {
                fp = fopen(pstCtx->srcFilePath, "r");
                if (fp == RK_NULL) {
                    gOpenFail = RK_TRUE;
                    RK_LOGE("chn %d can't open file %s!\n", 0, pstCtx->srcFilePath);
                    goto __EXIT;
                }
            }

            if (pstCtx->s32SrcCompressMode == COMPRESS_AFBC_16x16) {
                if (fread(pVirAddr, 1, pstCtx->u32SrcSize, fp) != pstCtx->u32SrcSize)
                    s32Ret = RK_FAILURE;
                else
                    s32Ret = RK_SUCCESS;
            } else {
                s32Ret = read_image(pVirAddr, pstCtx->u32SrcWidth, pstCtx->u32SrcHeight,
                         pstCtx->u32SrcVirWidth, pstCtx->u32SrcVirHeight, pstCtx->s32SrcPixFormat, fp);
            }
        } else {
            s32Ret = TEST_COMM_FillImage(pVirAddr, pstCtx->u32SrcWidth,
                            pstCtx->u32SrcHeight,
                            pstCtx->u32SrcVirWidth,
                            pstCtx->u32SrcVirHeight,
                            (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat, s32FrameCount);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_MB_ReleaseMB(blk);
                RK_LOGE("TEST_COMM_FillImage fail ch %d", 0);
                goto __EXIT;
            }
        }

        RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);
        memset(&stFrame.stVFrame, 0, sizeof(stFrame.stVFrame));

        if (gGdcExit || pstCtx->s32LoopCount > 1)
            s32ReachEOS = 1;

        stFrame.stVFrame.pMbBlk = blk;
        stFrame.stVFrame.u32Width = pstCtx->u32SrcWidth;
        stFrame.stVFrame.u32Height = pstCtx->u32SrcHeight;
        stFrame.stVFrame.u32VirWidth = stMbPicCalResult.u32VirWidth;
        stFrame.stVFrame.u32VirHeight = stMbPicCalResult.u32VirHeight;
        stFrame.stVFrame.enVideoFormat = (VIDEO_FORMAT_E)pstCtx->s32SrcVideoFormat;
        stFrame.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->s32SrcPixFormat;
        stFrame.stVFrame.u32FrameFlag |= s32ReachEOS ? FRAME_FLAG_SNAP_END : 0;
        stFrame.stVFrame.enCompressMode = (COMPRESS_MODE_E)pstCtx->s32SrcCompressMode;
        stFrame.stVFrame.u32TimeRef = u32TimeRef++;
        stFrame.stVFrame.u64PTS = u32TimeRef * 10000;
        result = RK_MPI_GDC_SendFrame(pstCtx->s32ChnIndex, &stFrame, s32MilliSec);
        if (result < 0) {
            RK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                result, s32MilliSec);
        } else {
            if (pstCtx->s32EisType)
                pstCtx->stUsrAttr[pstCtx->s32ChnIndex].sendSeq++;
        }

        RK_MPI_MB_ReleaseMB(blk);
        if (s32ReachEOS) {
            RK_LOGI("send eos");
            break;
        }

        if ((pstCtx->s32ChnMode == GDC_CHN_MODE_FEC) && pstCtx->bUpdate) {
            RK_MPI_GDC_GetUpdateAttr(pstCtx->s32ChnIndex, &attr);
            if (!pstCtx->s32FecUpdateMode || pstCtx->s32FecUpdateMode == 3) {
                attr.stFecAttr.enFecMode = (pstCtx->s32FecUpdateMode == 0 ?
                                            GDC_FEC_UPDATE_MESH_ONLINE :
                                            GDC_FEC_UPDATE_MESH_ONLINE_FROM_INI);
                if (cnt % 2 == 0)
                    attr.stFecAttr.stOnlineCfg.s32CorrectLevel = 255;
                else
                    attr.stFecAttr.stOnlineCfg.s32CorrectLevel = 0;

                cnt++;
            } else if (pstCtx->s32FecUpdateMode == GDC_FEC_UPDATE_MESH_FROM_FILE) {
                if (!pstCtx->pMesh0) {
                    RK_LOGE("mesh file is NULL(%s)\n", pstCtx->pMesh0);
                    goto __EXIT;
                }

                RK_CHAR aMeshPath[256] = {0};
                sprintf(aMeshPath, "%s%s", pstCtx->pCfgFile, pstCtx->pMesh0);
                memcpy(attr.stFecAttr.stFileCfg.aMeshPath, aMeshPath, sizeof(attr.stFecAttr.stFileCfg.aMeshPath));
            } else if (pstCtx->s32FecUpdateMode == GDC_FEC_UPDATE_MESH_FROM_BUFFER) {
                attr.stFecAttr.enFecMode = GDC_FEC_UPDATE_MESH_FROM_BUFFER;
                const int meshSize = cal_fec_mesh_size(pstCtx->u32DstWidth, pstCtx->u32DstHeight) * 6;
                meshBuf            = malloc(meshSize);
                if (!meshBuf) {
                    RK_LOGE("Memory allocation failed for %d-byte mesh buffer\n", meshSize);
                    goto __EXIT;
                }

                attr.stFecAttr.stBufCfg.pVirAddr  = meshBuf;
                attr.stFecAttr.stBufCfg.u64Size   = meshSize;
                attr.stFecAttr.stBufCfg.bNeedCopy = RK_TRUE;

                if (load_mesh_file(pstCtx->pMesh0, meshBuf, meshSize)) {
                    RK_LOGE("Load %s failed: %s\n", pstCtx->pMesh0, strerror(errno));
                    break;
                }
            }

            result = RK_MPI_GDC_Update(pstCtx->s32ChnIndex, &attr);
            if (result) {
                RK_LOGE("update fail, result = %X", result);
            }

            if (meshBuf) {
                free(meshBuf);
            }
        } else if ((pstCtx->s32ChnMode == GDC_CHN_MODE_IDC) && pstCtx->bUpdate) {
            attr.enMode = GDC_CHN_MODE_IDC;
            if (cnt % 2 == 0)
                result = idcMeshCreate(pstCtx, &attr.stIdcAttr.stMesh, &(pstCtx->meshBlk[pstCtx->s32ChnIndex]), pstCtx->pMesh1);
            else
                result = idcMeshCreate(pstCtx, &attr.stIdcAttr.stMesh, &(pstCtx->updateMeshBlk[pstCtx->s32ChnIndex]), pstCtx->pMesh0);

            cnt++;
            if (!result) {
                result = RK_MPI_GDC_Update(pstCtx->s32ChnIndex, &attr);
                if (result) {
                    RK_LOGE("update fail, result = %X", result);
                }
            }
        }

        usleep(33000);

        if (fp) {
            fclose(fp);
            fp = RK_NULL;
        }
    }

__EXIT:
    pthread_exit(NULL);
    return RK_NULL;
}

static void* getDataThread(void * ptr) {
    TEST_GDC_CTX_S *ctx = (TEST_GDC_CTX_S *)(ptr);

    RK_S32 result = 0;
    RK_S32 s32MilliSec = 1000;
    VIDEO_FRAME_INFO_S getFrame = {0};
    FILE *fp = NULL;
    fd_set readfds, writefds;
    RK_S32 frameCnt = 0;
    struct timeval timeout = {0};
    char filename[128] = {0};
    MB_PIC_CAL_S stMbPicCalResult = {0};

    if (ctx->s32FdGetEnable) {
        RK_LOGI("%s - fd get enable", __func__);
        ctx->s32DevFd = RK_MPI_GDC_GetFd(ctx->s32ChnIndex);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }

    if (ctx->dstFilePath) {
        fp = fopen(ctx->dstFilePath, "wb+");
    }

    result = unit_test_gdc_get_dst_size(ctx, &stMbPicCalResult);
    if (result) {
        return NULL;
    }

    ctx->u32DstVirWidth  = stMbPicCalResult.u32VirWidth;
    ctx->u32DstVirHeight = stMbPicCalResult.u32VirHeight;

    while (1) {
        if (!ctx->s32EisType) {
            if (!ctx->s32FdGetEnable) {
                result = RK_MPI_GDC_GetFrame(ctx->s32ChnIndex, &getFrame, s32MilliSec);
                if (result == 0) {
                    void *data = RK_MPI_MB_Handle2VirAddr(getFrame.stVFrame.pMbBlk);
                    RK_U64 len = RK_MPI_MB_GetSize(getFrame.stVFrame.pMbBlk);
                    RK_LOGI("RK_MPI_GDC_GetFrame(chn: %d) ok: count:%d, data %p, seq:%d, pts:%lld ms, len=%llu",
                                            ctx->s32ChnIndex, ++frameCnt, data,
                                            getFrame.stVFrame.u32TimeRef,
                                            getFrame.stVFrame.u64PTS/1000,
                                            len);
                    if (fp && data && (ctx->s32WriteCnt-- > 0)) {
                        fwrite(data, 1, len, fp);
                        fflush(fp);
                    }

                    RK_MPI_GDC_ReleaseFrame(ctx->s32ChnIndex, &getFrame);
                    if (((ctx->s32FrameCnt > 0) && (frameCnt > ctx->s32FrameCnt)) || (getFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END) {
                        RK_LOGI("reach eos frame.");
                        gGdcExit = RK_TRUE;
                        break;
                    }
                } else {
                    break;
                }
            } else {
                FD_ZERO(&readfds);
                FD_SET(ctx->s32DevFd, &readfds);
                timeout.tv_sec = 1;
                timeout.tv_usec = 0;
                result = select(ctx->s32DevFd + 1, &readfds, NULL, NULL, &timeout);
                if (result == -1) {
                    perror("select");
                    exit(EXIT_FAILURE);
                } else if (result) {
                    if (FD_ISSET(ctx->s32DevFd, &readfds)) {
                        result = RK_MPI_GDC_GetFrame(ctx->s32ChnIndex, &getFrame, s32MilliSec);
                        if (result == 0) {
                            void *data = RK_MPI_MB_Handle2VirAddr(getFrame.stVFrame.pMbBlk);
                            RK_U64 len = RK_MPI_MB_GetSize(getFrame.stVFrame.pMbBlk);
                            RK_LOGI("RK_MPI_GDC_GetFrame(chn: %d) ok: count:%d, data %p, seq:%d, pts:%lld ms, len=%llu",
                                    ctx->s32ChnIndex, ++frameCnt, data,
                                    getFrame.stVFrame.u32TimeRef,
                                    getFrame.stVFrame.u64PTS/1000,
                                    len);

                            if (fp && data && (ctx->s32WriteCnt-- > 0)) {
                                fwrite(data, 1, len, fp);
                                fflush(fp);
                            }

                            RK_MPI_GDC_ReleaseFrame(ctx->s32ChnIndex, &getFrame);

                            if (((ctx->s32FrameCnt > 0) && (frameCnt > ctx->s32FrameCnt)) || (getFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END) {
                                gGdcExit = RK_TRUE;
                                RK_LOGI("reach eos frame.");
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                } else {
                    RK_LOGE("chn(%d) fd get timeout occurred %d\n", ctx->s32ChnIndex, result);
                    if (gGdcExit || gOpenFail)
                        break;
                }
            }
        } else {
            for (int i = 0; i < 2; i++) {
                if (ctx->stUsrAttr[ctx->s32ChnIndex].used[i]) {
                    RK_MPI_MB_ReleaseMB(ctx->stUsrAttr[ctx->s32ChnIndex].vInfo[i].pMbBlk);
                    memset(&ctx->stUsrAttr[ctx->s32ChnIndex].vInfo[i], 0, sizeof(GDC_VFAME_INFO_S));
                    ctx->stUsrAttr[ctx->s32ChnIndex].used[i] = 0;
                    break;
                }
            }

            if (ctx->stUsrAttr[ctx->s32ChnIndex].exit == 1) {
                if (ctx->stUsrAttr[ctx->s32ChnIndex].sendSeq == ctx->stUsrAttr[ctx->s32ChnIndex].vframeSeq) {
                    for (int i = 0; i < 2; i++) {
                        if (ctx->stUsrAttr[ctx->s32ChnIndex].used[i]) {
                            RK_MPI_MB_ReleaseMB(ctx->stUsrAttr[ctx->s32ChnIndex].vInfo[i].pMbBlk);
                            memset(&ctx->stUsrAttr[ctx->s32ChnIndex].vInfo[i], 0, sizeof(GDC_VFAME_INFO_S));
                            ctx->stUsrAttr[ctx->s32ChnIndex].used[i] = 0;
                            break;
                        }
                    }
                }

                break;
            }

            usleep(33000);
        }
    }

    if (fp) {
        fclose(fp);
        fp = NULL;
    }

    pthread_exit(NULL);
    return RK_NULL;
}

// tmp code
RK_S32 test_mpi_gdc_api(TEST_GDC_CTX_S *ctx) {
    RK_S32 s32Ret = 0;
    GDC_HANDLE hHandle = -1;
    void *pSrcData = RK_NULL;
    RK_S32 loopCount = 0;
    FILE *pSrcFile = NULL;
    FILE *pDstFile = NULL;
    RK_U32 u32ReadSize = 0;
    MB_POOL_CONFIG_S stMbPoolCfg;
    VIDEO_FRAME_INFO_S stFrame;
    VIDEO_FRAME_INFO_S stLineFrame;
    VO_FRAME_INFO_S voLineFrame;
    MB_BLK pLineMbBlk;
    TEST_GDC_CTX_S gdcCtx[GDC_MAX_CHN_NUM] = {0};
    MB_PIC_CAL_S stMbPicCalResult = {0};

    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));

    if (!ctx->s32Type) {
        s32Ret = unit_test_gdc_get_src_size(ctx, &stMbPicCalResult);
        if (s32Ret)
            return RK_FAILURE;

        ctx->u32SrcSize = stMbPicCalResult.u32MBSize;
        stMbPoolCfg.u64MBSize = ctx->u32SrcSize;
        stMbPoolCfg.u32MBCnt = 4;
        stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
        ctx->inPool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
        if (ctx->enTestMode == TEST_GDC_GET_FRAME_SEND_VO) {
            s32Ret = unit_test_gdc_get_dst_size(ctx, &stMbPicCalResult);
            if (s32Ret)
                return RK_FAILURE;

            ctx->u32DstSize = stMbPicCalResult.u32MBSize;
            stMbPoolCfg.u64MBSize = ctx->u32DstSize;
            stMbPoolCfg.u32MBCnt = 4;
            stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
            ctx->outPool = RK_MPI_MB_CreatePool(&stMbPoolCfg);
        }

        pSrcFile = fopen(ctx->srcFilePath, "rb+");
        if (pSrcFile == RK_NULL) {
            RK_LOGE("open path %s failed because %s", ctx->srcFilePath, strerror(errno));
            goto __FAILED;
        }

        // vo init and create
        ctx->s32VoLayer = RK35XX_VOP_LAYER_CLUSTER_0;
        ctx->s32VoDev = RK35XX_VO_DEV_HD0;
        ctx->s32VoChn = RK35XX_VO_GDC_CHN;
        ctx->stLayerAttr.enPixFormat = RK_FMT_BGR888;
        ctx->stLayerAttr.stDispRect.s32X = 0;
        ctx->stLayerAttr.stDispRect.s32Y = 0;
        ctx->stLayerAttr.u32DispFrmRt = 30;
        ctx->stLayerAttr.stDispRect.u32Width = ctx->u32VoWidth;
        ctx->stLayerAttr.stDispRect.u32Height = ctx->u32VoHeight;
        ctx->stLayerAttr.stImageSize.u32Width = ctx->u32VoWidth;
        ctx->stLayerAttr.stImageSize.u32Height = ctx->u32VoHeight;

        ctx->stChnAttr.stRect.s32X = ctx->stChnRect.s32X;
        ctx->stChnAttr.stRect.s32Y = ctx->stChnRect.s32Y;
        ctx->stChnAttr.stRect.u32Width = ctx->stChnRect.u32Width;
        ctx->stChnAttr.stRect.u32Height = ctx->stChnRect.u32Height;
        ctx->stChnAttr.u32Priority = 0;
        ctx->stChnAttr.u32FgAlpha = 128;
        ctx->stChnAttr.u32BgAlpha = 0;
        // ctx->stChnAttr.bGdc = RK_TRUE;  // reserve

        s32Ret = create_vo(ctx, ctx->s32VoChn);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("create vo dev:%d failed:0x%x", ctx->s32VoDev, s32Ret);
            goto __FAILED;
        }
        // enable vo
        s32Ret = RK_MPI_VO_EnableChn(ctx->s32VoLayer, ctx->s32VoChn);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("Enalbe vo chn failed, s32Ret = %d", s32Ret);
            goto __FAILED;
        }
        // test vo chn127 for show line
        if (ctx->u32ShowPoint) {
            /*ctx->u32LineWidth = 640;
            ctx->u32LineHeight = 480;*/
            RK_MPI_VO_CreateGraphicsFrameBuffer(ctx->u32LineWidth, ctx->u32LineHeight, RK_FMT_BGRA5551, &pLineMbBlk);
            stLineFrame.stVFrame.u32Width = ctx->u32LineWidth;
            stLineFrame.stVFrame.u32Height = ctx->u32LineHeight;
            stLineFrame.stVFrame.u32VirWidth = ctx->u32LineWidth;
            stLineFrame.stVFrame.u32VirHeight = ctx->u32LineHeight;
            stLineFrame.stVFrame.enPixelFormat = RK_FMT_BGRA5551;
            stLineFrame.stVFrame.enCompressMode = COMPRESS_MODE_NONE;
            stLineFrame.stVFrame.pMbBlk = pLineMbBlk;

            ctx->stChnAttr.stRect.s32X = 0;
            ctx->stChnAttr.stRect.s32Y = 0;
            ctx->stChnAttr.stRect.u32Width = 960;
            ctx->stChnAttr.stRect.u32Height = 540;
            ctx->stChnAttr.u32Priority = RK35XX_VO_LINE_CHN;
            ctx->stChnAttr.u32FgAlpha = 255;
            ctx->stChnAttr.u32BgAlpha = 0;
            // ctx->stChnAttr.bGdc = RK_FALSE;  // reserve
            s32Ret = RK_MPI_VO_SetChnAttr(ctx->s32VoLayer, RK35XX_VO_LINE_CHN, &ctx->stChnAttr);
            if (s32Ret != RK_SUCCESS) {
                RK_LOGE("set chn Attr failed,s32Ret:%d\n", s32Ret);
                return RK_FAILURE;
            }
            s32Ret = RK_MPI_VO_EnableChn(ctx->s32VoLayer, RK35XX_VO_LINE_CHN);
            if (s32Ret != RK_SUCCESS) {
                RK_LOGE("Enalbe vo chn failed, s32Ret = %d", s32Ret);
                goto __FAILED;
            }
        }

        // gdc test config
        if (ctx->stFisheyeAttr.enMountMode == FISHEYE_DESKTOP_MOUNT) {
            test_mpi_gdc_desktop_mount(ctx);
        } else if (ctx->stFisheyeAttr.enMountMode == FISHEYE_CEILING_MOUNT) {
            test_mpi_gdc_ceiling_mount(ctx);
        } else if (ctx->stFisheyeAttr.enMountMode == FISHEYE_WALL_MOUNT) {
            test_mpi_gdc_wall_mount(ctx);
        } else {
            RK_LOGE("not support this mount = %d", ctx->stFisheyeAttr.enMountMode);
            goto __FAILED;
        }

        while (1) {
            // img in
            ctx->stTask.stImgIn.stVFrame.pMbBlk = RK_MPI_MB_GetMB(ctx->inPool, ctx->u32SrcSize, RK_TRUE);
            if (RK_NULL == ctx->stTask.stImgIn.stVFrame.pMbBlk) {
                usleep(10000llu);
                continue;
            }
            ctx->stTask.stImgIn.stVFrame.u32Width = ctx->u32SrcWidth;
            ctx->stTask.stImgIn.stVFrame.u32Height = ctx->u32SrcHeight;
            ctx->stTask.stImgIn.stVFrame.u32VirWidth = ctx->u32SrcVirWidth;
            ctx->stTask.stImgIn.stVFrame.u32VirHeight = ctx->u32SrcVirHeight;
            ctx->stTask.stImgIn.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32SrcPixFormat;
            ctx->stTask.stImgIn.stVFrame.u32FrameFlag = 0;
            ctx->stTask.stImgIn.stVFrame.enCompressMode = (COMPRESS_MODE_E)ctx->s32SrcCompressMode;
            // img out
            ctx->stTask.stImgOut.stVFrame.u32Width = ctx->u32DstWidth;
            ctx->stTask.stImgOut.stVFrame.u32Height = ctx->u32DstHeight;
            ctx->stTask.stImgOut.stVFrame.u32VirWidth = ctx->u32DstWidth;
            ctx->stTask.stImgOut.stVFrame.u32VirHeight = ctx->u32DstHeight;
            ctx->stTask.stImgOut.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)ctx->s32SrcPixFormat;
            ctx->stTask.stImgOut.stVFrame.u32FrameFlag = 0;
            ctx->stTask.stImgOut.stVFrame.enCompressMode = (COMPRESS_MODE_E)ctx->s32SrcCompressMode;
            if (ctx->enTestMode == TEST_GDC_GET_FRAME_SEND_VO) {
                ctx->stTask.stImgOut.stVFrame.pMbBlk = RK_MPI_MB_GetMB(ctx->outPool, ctx->u32DstSize, RK_TRUE);
            } else {
                ctx->stTask.stImgOut.stVFrame.pMbBlk = ctx->stTask.stImgIn.stVFrame.pMbBlk;
            }
            // usleep(100*1000);

            // test_mpi_gdc_get_src_point(ctx);
            // test_mpi_gdc_get_pano_point(ctx);

            // read image
            pSrcData = RK_MPI_MB_Handle2VirAddr(ctx->stTask.stImgIn.stVFrame.pMbBlk);
            if (ctx->s32SrcCompressMode == COMPRESS_AFBC_16x16) {
                if (fread(pSrcData, 1, ctx->u32SrcSize, pSrcFile) != ctx->u32SrcSize)
                    s32Ret = RK_FAILURE;
                else
                    s32Ret = RK_SUCCESS;
            } else {
                s32Ret = read_image(reinterpret_cast<RK_U8 *>(pSrcData), ctx->u32SrcWidth, ctx->u32SrcHeight,
                                    ctx->u32SrcVirWidth, ctx->u32SrcVirHeight, ctx->s32SrcPixFormat, pSrcFile);
            }

            if (s32Ret != RK_SUCCESS) {
                if (loopCount < ctx->s32LoopCount) {
                    fseek(pSrcFile, 0L, SEEK_SET);
                    // RK_LOGI("seek:%d", loopCount);
                    RK_MPI_MB_ReleaseMB(ctx->stTask.stImgIn.stVFrame.pMbBlk);
                    if (ctx->enTestMode == TEST_GDC_GET_FRAME_SEND_VO) {
                        RK_MPI_MB_ReleaseMB(ctx->stTask.stImgOut.stVFrame.pMbBlk);
                    }
                    continue;
                } else {
                    RK_LOGD("loop:%d end!", loopCount);
                    break;
                }
            }
            RK_MPI_SYS_MmzFlushCache(ctx->stTask.stImgIn.stVFrame.pMbBlk, RK_FALSE);
            RK_LOGD("gdc loop:%d!", loopCount);
#if 1
            // test gdc
            s32Ret = RK_MPI_GDC_BeginJob(&hHandle);
            if (s32Ret != RK_SUCCESS) {
                RK_LOGE("RK_MPI_GDC_BeginJob err:0x%x!", s32Ret);
                goto __FAILED;
            }

            s32Ret = RK_MPI_GDC_SetConfig(hHandle, &ctx->stFisheyeJobConfig);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_GDC_CancelJob(hHandle);
                RK_LOGE("RK_MPI_GDC_SetConfig err:0x%x!", s32Ret);
                goto __FAILED;
            }
            for (RK_S32 multi = 0; multi < ctx->s32TaskSum; multi++) {
                s32Ret = RK_MPI_GDC_AddCorrectionTask(hHandle, &ctx->stTask, &ctx->stFisheyeAttr);
                if (s32Ret != RK_SUCCESS) {
                    RK_MPI_GDC_CancelJob(hHandle);
                    RK_LOGE("RK_MPI_GDC_AddCorrectionTask err:0x%x!", s32Ret);
                    goto __FAILED;
                }
                // ctx->stTask.u64TaskId = 0;
            }
#if 0  // test multi diff task
            ctx->stTask.u64TaskId = ctx->stTask.u64TaskId + 10;
            s32Ret = RK_MPI_GDC_AddCorrectionTask(hHandle, &ctx->stTask, &ctx->stFisheyeAttr);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_GDC_CancelJob(hHandle);
                RK_LOGE("RK_MPI_GDC_AddCorrectionTask err:0x%x!", s32Ret);
                goto __FAILED;
            }
#endif
            if (ctx->u32ChangeParam) {
                test_mpi_gdc_change_parameter(ctx, 1, TEST_GDC_CHANGE_TYPE_PAN);
                test_mpi_gdc_change_parameter(ctx, 2, TEST_GDC_CHANGE_TYPE_TILT);
                test_mpi_gdc_change_parameter(ctx, 3, TEST_GDC_CHANGE_TYPE_HOR_ZOOM);
                test_mpi_gdc_change_parameter(ctx, 3, TEST_GDC_CHANGE_TYPE_VER_ZOOM);
            }

            s32Ret = RK_MPI_GDC_EndJob(hHandle);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_GDC_CancelJob(hHandle);
                RK_LOGE("RK_MPI_GDC_EndJob err:0x%x!", s32Ret);
                goto __FAILED;
            }
#endif
            // gdc out show to vo
            stFrame.stVFrame = ctx->stTask.stImgOut.stVFrame;
            if (ctx->enTestMode == TEST_GDC_GET_FRAME_SEND_VO) {
                RK_MPI_MB_ReleaseMB(ctx->stTask.stImgIn.stVFrame.pMbBlk);
                // TEST_GDC_GET_FRAME_SEND_VO mode can get the gdc out file
                if (ctx->dstFilePath && !pDstFile) {
                    pDstFile = fopen(ctx->dstFilePath, "wb");
                    if (pDstFile == RK_NULL) {
                        RK_LOGE("chn %d can't open file %s in get picture thread!\n", ctx->dstFilePath);
                    }
                }
                if (pDstFile) {
                    RK_MPI_SYS_MmzFlushCache(ctx->stTask.stImgOut.stVFrame.pMbBlk, RK_TRUE);
                    char *pData = (char *)RK_MPI_MB_Handle2VirAddr(ctx->stTask.stImgOut.stVFrame.pMbBlk);
                    if (ctx->s32DstPixFormat == RK_FMT_YUV420SP || ctx->s32DstPixFormat == RK_FMT_YUV420SP_VU) {
                        fwrite(pData, 1, ctx->u32DstWidth * ctx->u32DstHeight * 3 / 2, pDstFile);
                    } else {
                        fwrite(pData, 1, ctx->u32DstSize, pDstFile);
                    }
                    fflush(pDstFile);
                }
            }

            s32Ret = RK_MPI_VO_SendFrame(ctx->s32VoLayer, ctx->s32VoChn, &stFrame, -1);
            if (s32Ret != RK_SUCCESS) {
                RK_LOGE("RK_MPI_VO_SendFrame failed ret:0x%x", s32Ret);
                RK_MPI_VO_DestroyGraphicsFrameBuffer(ctx->stTask.stImgOut.stVFrame.pMbBlk);
            }
            // test chn 127 show point chain
            if (ctx->u32ShowPoint && (loopCount == 0 || ctx->u32ChangeParam)) {
                if (ctx->u32ChangeParam) {
                    memset(RK_MPI_MB_Handle2VirAddr(pLineMbBlk), 0x0, ctx->u32LineWidth * ctx->u32LineHeight * 2);
                    RK_MPI_SYS_MmzFlushCache(pLineMbBlk, RK_FALSE);
                }
                RK_U16 *pLineData = (RK_U16 *)RK_MPI_MB_Handle2VirAddr(pLineMbBlk);
                if (ctx->u32ShowPoint == 1) {
                    RK_U32 x, y;
                    for (RK_U32 region = 1; region < ctx->stFisheyeAttr.u32RegionNum; region++) {
                        if (ctx->stFisheyeAttr.enMountMode == FISHEYE_WALL_MOUNT) {  // pano
                            test_mpi_gdc_get_pano_point_array(ctx, region, 0);
                            for (RK_U32 l = 0; l < ctx->u32PointLen; l++) {
                                x = ctx->pastSrcPoint[l].s32X *
                                    (ctx->u32LineWidth / ctx->stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.u32Width);
                                y = ctx->pastSrcPoint[l].s32Y *
                                    (ctx->u32LineHeight / ctx->stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.u32Height);
                                if (test_mpi_gdc_xy_in_valid(y * ctx->u32LineWidth + x,
                                                            ctx->u32LineWidth * ctx->u32LineHeight, 0))
                                    pLineData[y * ctx->u32LineWidth + x] = gau16PointColor[region - 1];
                            }
                        } else {  // src
                            test_mpi_gdc_get_src_point_array(ctx, region);
                            for (RK_U32 l = 0; l < ctx->u32PointLen; l++) {
                                x = ctx->pastSrcPoint[l].s32X / (ctx->u32SrcWidth / ctx->u32LineWidth);
                                y = ctx->pastSrcPoint[l].s32Y / (ctx->u32SrcHeight / ctx->u32LineHeight);
                                if (test_mpi_gdc_xy_in_valid(y * ctx->u32LineWidth + x,
                                                            ctx->u32LineWidth * ctx->u32LineHeight, 0))
                                    pLineData[y * ctx->u32LineWidth + x] = gau16PointColor[region - 1];
                            }
                        }
                    }
                }
                RK_MPI_SYS_MmzFlushCache(pLineMbBlk, RK_FALSE);
                s32Ret = RK_MPI_VO_SendFrame(ctx->s32VoLayer, RK35XX_VO_LINE_CHN, &stLineFrame, -1);
                if (s32Ret != RK_SUCCESS) {
                    RK_LOGE("RK_MPI_VO_SendFrame failed ret:0x%x", s32Ret);
                    RK_MPI_VO_DestroyGraphicsFrameBuffer(pLineMbBlk);
                }
            }
            RK_MPI_MB_ReleaseMB(ctx->stTask.stImgOut.stVFrame.pMbBlk);

            loopCount++;
            if (loopCount >= ctx->s32LoopCount) {
                RK_LOGD("gdc loop:%d end!", loopCount);
                break;
            }
            usleep(1*1000);
        }
    } else if (ctx->s32Type == 1) {
        pthread_t tidSend[GDC_MAX_CHN_NUM] = {0};
        pthread_t tidGet[GDC_MAX_CHN_NUM] = {0};
        GDC_CHN_ATTR_S stAttr = {0};
        GDC_UPDATE_ATTR_S stUpdateAttr;
        memset(&stUpdateAttr, 0, sizeof(stUpdateAttr));
        stAttr.u32MaxInQueue = 2;
        stAttr.u32MaxOutQueue = 2;
        stAttr.s32Depth = 2;
        stAttr.s32DstWidth = ctx->u32DstWidth;
        stAttr.s32DstHeight = ctx->u32DstHeight;
        stAttr.enDstPixelFormat = (PIXEL_FORMAT_E)ctx->s32DstPixFormat;
        stAttr.enDstCompMode = (COMPRESS_MODE_E)ctx->s32DstCompressMode;
        stAttr.enDstVideoFormat = (VIDEO_FORMAT_E)ctx->s32DstVideoFormat;
        memcpy(&ctx->stGdcChnAttr, &stAttr, sizeof(ctx->stGdcChnAttr));
        if (ctx->pCfgFile != RK_NULL) {
            memcpy(stAttr.cfgFile, ctx->pCfgFile, strlen(ctx->pCfgFile));
        }

        for (int j = 0; j < ctx->s32LoopCount; j++) {
            for (int i = 0; i < ctx->s32ChnNum; i++) {
                memcpy(&(gdcCtx[i]), ctx, sizeof(TEST_GDC_CTX_S));
                gdcCtx[i].s32ChnIndex = i;
                stAttr.stEisAttr.s32DevNo = 1;
                if (!ctx->s32ChnMode) {
                    if (ctx->s32EisType) {
                        ctx->infoCB[i].pfnSensorCB = RKGdcSensorCB;
                        ctx->infoCB[i].pfnVframeCB = RKGdcVframeCB;
                        if (RK_MPI_GDC_Register_InfoCB(gdcCtx[i].s32ChnIndex, &gdcCtx[i].stUsrAttr[i], &ctx->infoCB[i]))
                            goto __CHNFAILED;
                    }
                } else if (ctx->s32ChnMode == 1) {
                    stAttr.enMode = GDC_CHN_MODE_FEC;
                    stAttr.stFecAttr.s32InFourcc  = 0;
                    stAttr.stFecAttr.s32OutFourcc = 0;
                    stAttr.stFecAttr.s32BorderMode = ctx->s32BorderMode;
                    stAttr.stFecAttr.s32CrossBufMode = ctx->s32CrossBufMode;
                    stAttr.stFecAttr.stBgVal.s32BgY = ctx->s32BgY;
                    stAttr.stFecAttr.stBgVal.s32BgU = ctx->s32BgU;
                    stAttr.stFecAttr.stBgVal.s32BgV = ctx->s32BgV;
                    if (!ctx->s32FecMode) {
                        stUpdateAttr.enMode = GDC_CHN_MODE_FEC;
                        stAttr.stFecAttr.enFecMode = GDC_FEC_UPDATE_MESH_ONLINE;

                        if (ctx->pIniFile && ((strlen(ctx->pIniFile) + 1) <= sizeof(stAttr.stFecAttr.aIniFile))) {
                            if (!ctx->bFileMode) {
                                if (RK_MPI_GDC_GetAttrFromFile(&stUpdateAttr, ctx->pIniFile)) {
                                    RK_LOGE("get attr failed");
                                    goto __CHNFAILED;
                                }

                                stAttr.stFecAttr.dLightCenter[0]  = stUpdateAttr.stFecAttr.stOnlineCfg.dLightCenter[0];
                                stAttr.stFecAttr.dLightCenter[1]  = stUpdateAttr.stFecAttr.stOnlineCfg.dLightCenter[1];
                                for (int i = 0; i < 4; i++) {
                                    stAttr.stFecAttr.dCoeff[i] = stUpdateAttr.stFecAttr.stOnlineCfg.dCoeff[i];
                                }

                                stAttr.stFecAttr.s32CorrectLevel  = stUpdateAttr.stFecAttr.stOnlineCfg.s32CorrectLevel;
                                stAttr.stFecAttr.enDirection      = GDC_FEC_CORRECT_DIRECTION_XY;
                                stAttr.stFecAttr.enStyle          = stUpdateAttr.stFecAttr.stOnlineCfg.enStyle;
                            } else {
                                stAttr.stFecAttr.enFecMode = GDC_FEC_UPDATE_MESH_ONLINE_FROM_INI;
                                memcpy(stAttr.stFecAttr.aIniFile, ctx->pIniFile, strlen(ctx->pIniFile));
                            }
                        } else if (ctx->pIniFile) {
                            RK_LOGE("stAttr.stFecAttr.aIniFile len = %d", strlen(ctx->pIniFile) + 1);
                            goto __CHNFAILED;
                        } else {
                            stAttr.stFecAttr.dLightCenter[0]  = 1956.3909119999998438;
                            stAttr.stFecAttr.dLightCenter[1]  = 1140.6355200000000422;
                            stAttr.stFecAttr.dCoeff[0]        = -2819.4072493821618081;
                            stAttr.stFecAttr.dCoeff[1]        = 0.0000316126581792;
                            stAttr.stFecAttr.dCoeff[2]        = 0.0000000688410142;
                            stAttr.stFecAttr.dCoeff[3]        = -0.0000000000130686;
                            stAttr.stFecAttr.s32CorrectLevel  = 250;
                            stAttr.stFecAttr.enDirection      = GDC_FEC_CORRECT_DIRECTION_XY;
                            stAttr.stFecAttr.enStyle          = GDC_FEC_KEEP_ASPECT_RATIO_REDUCE_FOV;
                        }
                    } else if (ctx->s32FecMode == 1) {
                        stAttr.stFecAttr.enFecMode = GDC_FEC_UPDATE_MESH_FROM_FILE;
                        if (ctx->pMesh0 == RK_NULL) {
                            RK_LOGE("mesh file (%p) is NULL",
                                    ctx->pMesh0);
                            goto __CHNFAILED;
                        }

                        memcpy(stAttr.stFecAttr.mesh0, ctx->pMesh0, strlen(ctx->pMesh0));
#ifndef RKFEC_HW_V20
                        if (ctx->pMesh1 == RK_NULL ||
                            ctx->pMesh2 == RK_NULL ||
                            ctx->pMesh3 == RK_NULL) {
                            RK_LOGE("mesh file (%s %s %s) is NULL",
                                    ctx->pMesh1,
                                    ctx->pMesh2,
                                    ctx->pMesh3);
                            goto __CHNFAILED;
                        }
                        memcpy(stAttr.stFecAttr.mesh1, ctx->pMesh1, strlen(ctx->pMesh1));
                        memcpy(stAttr.stFecAttr.mesh2, ctx->pMesh2, strlen(ctx->pMesh2));
                        memcpy(stAttr.stFecAttr.mesh3, ctx->pMesh3, strlen(ctx->pMesh3));
#endif
                    } else if (ctx->s32FecMode == 2) {
                        stAttr.stFecAttr.enFecMode = GDC_FEC_UPDATE_MESH_FROM_BUFFER;
                        if (ctx->pMesh0 == RK_NULL) {
                            RK_LOGE("mesh file (%p) is NULL",
                                    ctx->pMesh0);
                            goto __CHNFAILED;
                        }
                    }
                } else if (ctx->s32ChnMode == 2) {
                    stAttr.enMode = GDC_CHN_MODE_DIS;
                    /* dis config init */
                    stAttr.stDisAttr.stCfg.enMode              = DIS_MODE_4_DOF_GME;
                    stAttr.stDisAttr.stCfg.enMotionLevel       = DIS_MOTION_LEVEL_NORMAL;
                    stAttr.stDisAttr.stCfg.u32CropRatio        = ctx->u32CropRatio;
                    stAttr.stDisAttr.stCfg.u32BufNum           = ctx->u32DisBufCnt;
                    stAttr.stDisAttr.stCfg.u32FrameRate        = 30;
                    stAttr.stDisAttr.stCfg.enPdtType           = DIS_PDT_TYPE_IPC;
                    stAttr.stDisAttr.stCfg.u32GyroOutputRange  = 0;
                    stAttr.stDisAttr.stCfg.bCameraSteady       = RK_FALSE;
                    stAttr.stDisAttr.stCfg.u32GyroDataBitWidth = 0;

                    stAttr.stDisAttr.stAttr.bEnable               = RK_TRUE;
                    stAttr.stDisAttr.stAttr.u32MovingSubjectLevel = 0;
                    stAttr.stDisAttr.stAttr.s32RollingShutterCoef = 0;
                    stAttr.stDisAttr.stAttr.u32Timelag            = 33333;
                    stAttr.stDisAttr.stAttr.u32ViewAngle          = 1000;
                    stAttr.stDisAttr.stAttr.bStillCrop            = RK_FALSE;
                    stAttr.stDisAttr.stAttr.u32HorizontalLimit    = 512;
                    stAttr.stDisAttr.stAttr.u32VerticalLimit      = 512;
                } else if (ctx->s32ChnMode == 3) {
                    stAttr.enMode = GDC_CHN_MODE_IDC;
                    if (ctx->pIniFile)
                        memcpy(stAttr.stIdcAttr.aOpenclLibPath, ctx->pIniFile, strlen(ctx->pIniFile));
                    else {
                        RK_LOGE("idc malic so path is NULL");
                        return -1;
                    }

                    stAttr.stIdcAttr.enMode = GDC_IDC_LUT_DEFAULT_MODE;
                    s32Ret = idcMeshCreate(ctx, &stAttr.stIdcAttr.stMesh, &ctx->meshBlk[i], ctx->pMesh0);
                    if (s32Ret) {
                        RK_LOGE("mesh create fail, result = %X", s32Ret);
                        goto __CHNFAILED;
                    }

                    if (ctx->bUpdate) {
                        GDC_IDC_MESH_S stMesh = {0};
                        s32Ret = idcMeshCreate(ctx, &stMesh, &(ctx->updateMeshBlk[i]), ctx->pMesh1);
                        if (s32Ret) {
                            RK_LOGE("update mesh create fail, result = %X", s32Ret);
                            goto __CHNFAILED;
                        }

                        gdcCtx[i].meshBlk[i] = ctx->meshBlk[i];
                        gdcCtx[i].updateMeshBlk[i] = ctx->updateMeshBlk[i];
                    }
                }

                if (RK_MPI_GDC_CreateChn(gdcCtx[i].s32ChnIndex, &stAttr))
                    goto __CHNFAILED;

                pthread_create(&tidSend[i], RK_NULL, sendDataThread, reinterpret_cast<void *>(&gdcCtx[i]));
                pthread_create(&tidGet[i], RK_NULL, getDataThread, reinterpret_cast<void *>(&gdcCtx[i]));
            }

            for (int i = 0; i < ctx->s32ChnNum; i++) {
                if (tidSend[i])
                    pthread_join(tidSend[i], RK_NULL);

                gdcCtx[i].stUsrAttr[gdcCtx[i].s32ChnIndex].exit = 1;
                if (tidGet[i])
                    pthread_join(tidGet[i], RK_NULL);
            }

__CHNFAILED:
            for (int i = 0; i < ctx->s32ChnNum; i++) {
                RK_MPI_GDC_DestroyChn(gdcCtx[i].s32ChnIndex);
                if (gdcCtx[i].chnPool[gdcCtx[i].s32ChnIndex])
                    RK_MPI_MB_DestroyPool(gdcCtx[i].chnPool[gdcCtx[i].s32ChnIndex]);

                if (ctx->meshBlk[i]) {
                    RK_MPI_SYS_Free(ctx->meshBlk[i]);
                }

                if (ctx->updateMeshBlk[i]) {
                    RK_MPI_SYS_Free(ctx->updateMeshBlk[i]);
                }
            }
        }

    }

__FAILED:
    if (!ctx->s32Type) {
        // disable vo
        RK_MPI_VO_ClearChnBuffer(ctx->s32VoLayer, ctx->s32VoChn, RK_TRUE);
        RK_MPI_VO_DisableChn(ctx->s32VoLayer, ctx->s32VoChn);
        if (ctx->u32ShowPoint) {
            RK_MPI_VO_ReleaseGfxFrameBuffer(ctx->s32VoLayer, RK35XX_VO_LINE_CHN);
            RK_MPI_VO_ClearChnBuffer(ctx->s32VoLayer, RK35XX_VO_LINE_CHN, RK_TRUE);
            RK_MPI_VO_DisableChn(ctx->s32VoLayer, RK35XX_VO_LINE_CHN);
        }
        RK_MPI_VO_DisableLayer(ctx->s32VoLayer);
        RK_MPI_VO_DisableLayer(RK35XX_VOP_LAYER_ESMART_0);
        RK_MPI_VO_DisableLayer(RK35XX_VOP_LAYER_ESMART_1);
        RK_MPI_VO_DisableLayer(RK35XX_VOP_LAYER_SMART_0);
        RK_MPI_VO_DisableLayer(RK35XX_VOP_LAYER_SMART_1);
        RK_MPI_VO_Disable(ctx->s32VoDev);
        RK_MPI_VO_UnBindLayer(ctx->s32VoLayer, ctx->s32VoDev);
        RK_MPI_VO_CloseFd();

        RK_LOGE("exit err:0x%x!", s32Ret);
        RK_MPI_GDC_StopJob(hHandle);
        RK_MPI_MB_DestroyPool(ctx->inPool);
        if (ctx->enTestMode == TEST_GDC_GET_FRAME_SEND_VO) {
            RK_MPI_MB_DestroyPool(ctx->outPool);
        }
        if (pSrcFile) {
            fclose(pSrcFile);
        }
        if (pDstFile) {
            fclose(pDstFile);
        }
    }

    return s32Ret;
}

static const char *const usages[] = {
    "./rk_mpi_gdc_test -i /data/fisheye_in_s1920_h1080_w1920_14.yuv -w 1920 -h 1080  -f 6 -l 1000"
    " --region_num 4 --show_point 1 --point_len 6000 --change_param 0",
    NULL,
};

static void mpi_gdc_test_show_options(const TEST_GDC_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input  file name       : %s\n", ctx->srcFilePath);
    RK_PRINT("output file name       : %s\n", ctx->dstFilePath);
    RK_PRINT("src width              : %d\n", ctx->u32SrcWidth);
    RK_PRINT("src height             : %d\n", ctx->u32SrcHeight);
    RK_PRINT("dst width              : %d\n", ctx->u32DstWidth);
    RK_PRINT("dst height             : %d\n", ctx->u32DstHeight);
    RK_PRINT("line width             : %d\n", ctx->u32LineWidth);
    RK_PRINT("line height            : %d\n", ctx->u32LineHeight);
    RK_PRINT("loop count             : %d\n", ctx->s32LoopCount);
    RK_PRINT("src pixel format       : %d\n", ctx->s32SrcPixFormat);
    RK_PRINT("dst pixel format       : %d\n", ctx->s32DstPixFormat);
    RK_PRINT("dst compress mode      : %d\n", ctx->s32DstCompressMode);
    RK_PRINT("dst video mode         : %d\n", ctx->s32DstVideoFormat);
    RK_PRINT("task_id                : %d\n", ctx->stTask.u64TaskId);
    RK_PRINT("task_sum               : %d\n", ctx->s32TaskSum);
    RK_PRINT("stepx                  : %d\n", ctx->stTask.au64privateData[0]);
    RK_PRINT("stepy                  : %d\n", ctx->stTask.au64privateData[1]);
    RK_PRINT("regionNum              : %d\n", ctx->stFisheyeAttr.u32RegionNum);
    RK_PRINT("mount mode             : %d\n", ctx->stFisheyeAttr.enMountMode);
    RK_PRINT("desk/ceil mode         : %d\n", ctx->enDeskCeilMode);
    RK_PRINT("wall mode              : %d\n", ctx->enWallMode);
    RK_PRINT("show_point             : %d\n", ctx->u32ShowPoint);
    RK_PRINT("point_len              : %d\n", ctx->u32PointLen);
    RK_PRINT("change param           : %d\n", ctx->u32ChangeParam);
    RK_PRINT("vo out resolution      : %d\n", ctx->stVoOutputResolution);
    RK_PRINT("vo chn rect x          : %d\n", ctx->stChnRect.s32X);
    RK_PRINT("vo chn rect y          : %d\n", ctx->stChnRect.s32Y);
    RK_PRINT("vo chn rect w          : %d\n", ctx->stChnRect.u32Width);
    RK_PRINT("vo chn rect h          : %d\n", ctx->stChnRect.u32Height);
    RK_PRINT("test mode              : %d\n", ctx->enTestMode);
    RK_PRINT("gdc type               : %d\n", ctx->s32Type);
    RK_PRINT("cfg file               : %s\n", ctx->pCfgFile);
    RK_PRINT("fd get enable          : %d\n", ctx->s32FdGetEnable);
    RK_PRINT("gdc chn mode           : %d\n", ctx->s32ChnMode);
    RK_PRINT("xint file              : %s\n", ctx->pMesh0);
    RK_PRINT("xfra file              : %s\n", ctx->pMesh1);
    RK_PRINT("yint file              : %s\n", ctx->pMesh2);
    RK_PRINT("xint file              : %s\n", ctx->pMesh3);
    RK_PRINT("get frame count        : %d\n", ctx->s32FrameCnt);
    RK_PRINT("gdc fec mode           : %d\n", ctx->s32FecMode);
    RK_PRINT("gdc fec border mode    : %d\n", ctx->s32BorderMode);
    RK_PRINT("gdc fec cross buf mode : %d\n", ctx->s32CrossBufMode);
    RK_PRINT("gdc fec bg y           : %d\n", ctx->s32BgY);
    RK_PRINT("gdc fec bg u           : %d\n", ctx->s32BgU);
    RK_PRINT("gdc fec bg v           : %d\n", ctx->s32BgV);
    RK_PRINT("gdc fec ini file       : %s\n", ctx->pIniFile);
    RK_PRINT("gdc write count        : %d\n", ctx->s32WriteCnt);
    RK_PRINT("gdc file mode          : %d\n", ctx->bFileMode);
    RK_PRINT("gdc fec update mode    : %d\n", ctx->s32FecUpdateMode);
    RK_PRINT("gdc dis out buf cnt       : %d\n", ctx->u32DisBufCnt);
    RK_PRINT("gdc dis crop ratio        : %d\n", ctx->u32CropRatio);
    return;
}

int main(int argc, const char **argv) {
    RK_S32 s32Ret = RK_SUCCESS;
    TEST_GDC_CTX_S *ctx;
    ctx = (TEST_GDC_CTX_S *)calloc(1, sizeof(TEST_GDC_CTX_S));
    if (!ctx) {
        RK_LOGE("calloc gdc ctx fail!");
        return 0;
    }

    ctx->s32FrameCnt = 0;

    //  set default params.
    ctx->s32LoopCount = 1;
    ctx->stFisheyeAttr.u32RegionNum = 4;
    ctx->s32TaskSum = 1;
    ctx->u32ChangeParam = 0;
    ctx->u32PointLen = 3000;
    ctx->stFisheyeAttr.enMountMode = FISHEYE_WALL_MOUNT;
    ctx->enWallMode = TEST_GDC_WALL_MODE_1P_ADD_3;
    ctx->enDeskCeilMode = TEST_GDC_DESK_CEIL_MODE_1_ADD_3;
    ctx->stVoOutputResolution = VO_OUTPUT_1080P30;
    ctx->u32VoWidth = 1920;
    ctx->u32VoHeight = 1080;
    ctx->stChnRect.s32X = 0;
    ctx->stChnRect.s32Y = 0;
    ctx->stChnRect.u32Width = 1920;
    ctx->stChnRect.u32Height = 1080;
    ctx->s32FdGetEnable = 1;
    ctx->s32ChnNum = 1;
    ctx->u32DisBufCnt  = 5;
    ctx->u32CropRatio  = 80;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input",  &(ctx->srcFilePath),
                   "input file name. <required>", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx->dstFilePath),
                   "output file name, when test mode 1 valid.default(NULL).", NULL, 0, 0),
        OPT_INTEGER('w', "src_width", &(ctx->u32SrcWidth),
                    "input source width. <required>", NULL, 0, 0),
        OPT_INTEGER('h', "src_height", &(ctx->u32SrcHeight),
                    "input source height. <required>", NULL, 0, 0),
        OPT_INTEGER('W', "dst_width", &(ctx->u32DstWidth),
                    "output img width.default(src_width)", NULL, 0, 0),
        OPT_INTEGER('H', "dst_height", &(ctx->u32DstHeight),
                    "output img height.default(src_height).", NULL, 0, 0),
        OPT_INTEGER('\0', "line_width", &(ctx->u32LineWidth),
                    "line img width.(default src_width;range(0, src_width))", NULL, 0, 0),
        OPT_INTEGER('\0', "line_height", &(ctx->u32LineHeight),
                    "line img height.(default src_height;range(0, src_height)).", NULL, 0, 0),
        OPT_INTEGER('f', "pixel_format", &(ctx->s32SrcPixFormat),
                    "input source pixel format. default 0; (0:NV12 6:NV21).", NULL, 0, 0),
        OPT_INTEGER('F', "dst_pixel_format", &(ctx->s32DstPixFormat),
                    "output source pixel format. default 0; (0:NV12 6:NV21).", NULL, 0, 0),
        OPT_INTEGER('l', "loop_count", &(ctx->s32LoopCount),
                    "loop running count. default(1)", NULL, 0, 0),
        OPT_INTEGER('\0', "src_compress_mode", &(ctx->s32SrcCompressMode),
                    "set input compressmode(default 0; 0:MODE_NONE 1:AFBC_16x16)", NULL, 0, 0),
        OPT_INTEGER('\0', "dst_compress_mode", &(ctx->s32DstCompressMode),
                    "set output compressmode(default 0; 0:MODE_NONE 1:AFBC_16x16)", NULL, 0, 0),
        OPT_INTEGER('\0', "task_id", &(ctx->stTask.u64TaskId),
                    "set task id(default 0; 0:auto alloc task id >0:specify the task id)", NULL, 0, 0),
        OPT_INTEGER('\0', "stepx", &(ctx->stTask.au64privateData[0]),
                     "set step x(default 0)", NULL, 0, 0),
        OPT_INTEGER('\0', "stepy", &(ctx->stTask.au64privateData[1]),
                     "set step y(default 0)", NULL, 0, 0),
        OPT_INTEGER('\0', "region_num", &(ctx->stFisheyeAttr.u32RegionNum),
                     "set region num(default 4(1+3))", NULL, 0, 0),
        OPT_INTEGER('\0', "mount_type", &(ctx->stFisheyeAttr.enMountMode),
                     "set mount type(default 2;(0:desktop mount 1:ceiling mount 2:wall mount))", NULL, 0, 0),
        OPT_INTEGER('\0', "desk_ceil_mode", &(ctx->enDeskCeilMode),
                      "set desk or ceil mode(default 2;(0:1p+1 1:2p 2:1+3 3:1+4(not implement)"
                      "4:1p+6(not implement) 5:1+8))", NULL, 0, 0),
        OPT_INTEGER('\0', "wall_mode", &(ctx->enWallMode),
                      "set wall moode(default 1;(0:1p(not implement) 1:1p+3 2:1p+4(not implement)"
                      "3:1p+8))", NULL, 0, 0),
        OPT_INTEGER('\0', "task_sum", &(ctx->s32TaskSum),
                     "set test task sum(default 1)", NULL, 0, 0),
        OPT_INTEGER('\0', "show_point", &(ctx->u32ShowPoint),
                     "set show point chain(default 0; 0:no show point chain 1:show with scale"
                     "2:show with pano(not unrealized))", NULL, 0, 0),
        OPT_INTEGER('\0', "change_param", &(ctx->u32ChangeParam),
                     "set change param test(default 0)", NULL, 0, 0),
        OPT_INTEGER('\0', "point_len", &(ctx->u32PointLen),
                     "set point len(default 3000, when ctx->u32ShowPoint > 0 valid;"
                     "range:[0, line image perimeter])", NULL, 0, 0),
        OPT_INTEGER('\0', "vo_resolution", &(ctx->stVoOutputResolution),
                     "set change param test(default 4;(4:1080p30 10:1080p60 31:3840*2160p30"
                     "41:7680*4320p30))", NULL, 0, 0),
        OPT_INTEGER('\0', "vo_chn_x", &(ctx->stChnRect.s32X),
                    "vo chn attr rect x.default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "vo_chn_y", &(ctx->stChnRect.s32Y),
                    "vo chn attr rect y.default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "vo_chn_w", &(ctx->stChnRect.u32Width),
                    "vo chn attr rect w.default(1920)", NULL, 0, 0),
        OPT_INTEGER('\0', "vo_chn_h", &(ctx->stChnRect.u32Height),
                    "vo chn attr rect h.default(1080).", NULL, 0, 0),
        OPT_INTEGER('m', "test_mode", &(ctx->enTestMode),
                    "test mode(0:gdc bypass to vo 1:gdc get frame to vo).default(0).", NULL, 0, 0),
        OPT_INTEGER('t', "type", &(ctx->s32Type),
                    "gdc type(0:FISHEYE; 1: CHANNEL).default(0).", NULL, 0, 0),
        OPT_STRING('\0', "cfg_file", &(ctx->pCfgFile),
                    "the cfg file, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "fd_get_enable", &(ctx->s32FdGetEnable),
                    "the fd get enable, 0:disable 1:enable. default(0).", NULL, 0, 0),
        OPT_INTEGER('c', "channel_count", &(ctx->s32ChnNum),
                    "the count of gdc channel. default(1).", NULL, 0, 0),
        OPT_INTEGER('u', "eis_type", &(ctx->s32EisType),
                    "eis type(0:rockchip; 1: usr).default(0).", NULL, 0, 0),
        OPT_INTEGER('d', "chn_mode", &(ctx->s32ChnMode),
                    "eis type(0:eis; 1: fec, 2:dis, 3:idc).default(0).", NULL, 0, 0),
        OPT_STRING('\0', "mesh0", &(ctx->pMesh0),
                    "the mesh xint file, default(NULL)", NULL, 0, 0),
        OPT_STRING('\0', "mesh1", &(ctx->pMesh1),
                    "the mesh xfra file, default(NULL)", NULL, 0, 0),
        OPT_STRING('\0', "mesh2", &(ctx->pMesh2),
                    "the mesh yint, default(NULL)", NULL, 0, 0),
        OPT_STRING('\0', "mesh3", &(ctx->pMesh3),
                    "the mesh yfra file, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('n', "frame_cnt", &(ctx->s32FrameCnt),
                    "get frame count.default(0).", NULL, 0, 0),
        OPT_INTEGER('e', "fec_mode", &(ctx->s32FecMode),
                    "fec mode(0:online; 1: file; 2: from buf).default(0).", NULL, 0, 0),
        OPT_INTEGER('b', "border_mode", &(ctx->s32BorderMode),
                    "fec border mode.default(0).", NULL, 0, 0),
        OPT_INTEGER('C', "cross_buf_mode", &(ctx->s32CrossBufMode),
                    "fec cross buf mode.default(0).", NULL, 0, 0),
        OPT_INTEGER('Y', "fec_bg_y", &(ctx->s32BgY),
                    "fec gb y.default(255).", NULL, 0, 0),
        OPT_INTEGER('U', "fec_bg_u", &(ctx->s32BgU),
                    "fec gb u.default(100).", NULL, 0, 0),
        OPT_INTEGER('V', "fec_bg_v", &(ctx->s32BgV),
                    "fec gb v.default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "src_video_format", &(ctx->s32SrcVideoFormat),
                    "set input video format(default 0; 0:VIDEO_FORMAT_LINEAR 3:VIDEO_FORMAT_TILE_4x4)", NULL, 0, 0),
        OPT_INTEGER('\0', "dst_video_format", &(ctx->s32DstVideoFormat),
                    "set output video format(default 0; 0:VIDEO_FORMAT_LINEAR 3:VIDEO_FORMAT_TILE_4x4)", NULL, 0, 0),
        OPT_STRING('\0', "ini_file", &(ctx->pIniFile),
                    "the fec ini file, default(NULL)", NULL, 0, 0),
        OPT_INTEGER('\0', "write_cnt", &(ctx->s32WriteCnt),
                    "the write count, default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "gdc_update", &(ctx->bUpdate),
                    "the update flag, default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "fec_file_mode", &(ctx->bFileMode),
                    "the fec file mode, default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "fec_update_mode", &(ctx->s32FecUpdateMode),
                    "the fec update mode, default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "dis_buf_count", &(ctx->u32DisBufCnt),
                    "dis out buf count, range[1, 8] (default 5).", NULL, 0, 0),
        OPT_INTEGER('\0', "crop_ratio", &(ctx->u32CropRatio),
                    "dis out buf count, range[50, 98] (default 80).", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);

    if (ctx->s32DstPixFormat == 0)
        ctx->s32DstPixFormat = ctx->s32SrcPixFormat;
    if (ctx->u32DstWidth == 0)
        ctx->u32DstWidth = ctx->u32SrcWidth;
    if (ctx->u32DstHeight == 0)
        ctx->u32DstHeight = ctx->u32SrcHeight;

    if (ctx->u32LineWidth == 0)
        ctx->u32LineWidth = ctx->u32SrcWidth;
    if (ctx->u32LineHeight == 0)
        ctx->u32LineHeight = ctx->u32SrcHeight;

    if (ctx->stVoOutputResolution == 31) {
        ctx->u32VoWidth = 3840;
        ctx->u32VoHeight = 2160;
    } else if (ctx->stVoOutputResolution >= VO_OUTPUT_7680x4320_24 &&
               ctx->stVoOutputResolution <= VO_OUTPUT_7680x4320_60) {
        ctx->u32VoWidth = 7680;
        ctx->u32VoHeight = 4320;
    }

    mpi_gdc_test_show_options(ctx);
    if (ctx->srcFilePath == RK_NULL ||
        ctx->u32SrcWidth <= 0 ||
        ctx->u32SrcHeight <= 0) {
        argparse_usage(&argparse);
        goto __FAILED;
    }

    if (ctx->u32ShowPoint && ctx->u32PointLen) {
        ctx->pastDstPoint = (POINT_S *)malloc(sizeof(POINT_S) * ctx->u32PointLen);
        ctx->pastSrcPoint = (POINT_S *)malloc(sizeof(POINT_S) * ctx->u32PointLen);
    }

    // setup the SIGINT to ctrl+c and handing de-init process
    signal(SIGINT, sigterm_handler);

    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

    test_mpi_gdc_api(ctx);
    s32Ret = RK_MPI_SYS_Exit();
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }
    RK_LOGD("gdc test done!");

__FAILED:
    if (ctx->pastDstPoint)
        free(ctx->pastDstPoint);
    if (ctx->pastSrcPoint)
        free(ctx->pastSrcPoint);
    if (ctx)
        free(ctx);

    return 0;
}
