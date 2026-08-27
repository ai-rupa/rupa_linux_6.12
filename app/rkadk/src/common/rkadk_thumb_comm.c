#include "rkadk_thumb_comm.h"
#include "rkadk_thumb.h"
#include "rkadk_log.h"
#include "rkadk_photo.h"
#include "rkadk_hal.h"
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#define THM_BOX_HEADER_LEN 8 /* size: 4byte, type: 4byte */
#define VPSS_ZOOM_MAX 16

typedef enum {
  RKADK_THUMB_REPAIR_FAIL = -1,
  RKADK_THUMB_REPAIR_SUCCESS,
  RKADK_THUMB_ALREADY_EXIST,
  RKADK_THUMB_BUILD_IN_FAIL
} RKADK_THUMB_REPAIR_RESULT_E;

typedef struct {
  uint16_t tag_no; // tag number
  uint16_t format; // data format
  uint32_t length; // data len
  uint32_t offset; // data or data
  union IFD_VALUE {
    uint8_t ubv;     // format = 1, 1 byte
    const char *asv; // format = 2,7 1 byte
    uint16_t uwv;    // format = 3, 2 bytes
    uint32_t udwv;   // format = 4, 4 bytes
    uint64_t uddwv;  // format = 5, 8 bytes
    int8_t bv;       // format = 6, 1 byte
    int16_t wv;      // format = 8, 2 bytes
    int32_t dwv;     // format = 9, 4 bytes
    int64_t ddwv;    // format = 10, 8 bytes
    float fv;        // format = 11, 4 bytes
    double dv;       // format = 12, 8 bytes
  } value[10];
} IFD;

static IFD stIfd0[] = {
    {0x010e, 2, 32, 0x00, {{.asv = "thumbnail_test"}}},      // picture info
    {0x010f, 2, 32, 0x00, {{.asv = "rockchip"}}},            // manufact info
    {0x0110, 2, 32, 0x00, {{.asv = "rockchip IP Camrea"}}},  // module info
    {0x0131, 2, 32, 0x00, {{.asv = "rkadk v1.3.2"}}},        // software version
    {0x0132, 2, 32, 0x00, {{.asv = "2022:06:24 17:10:50"}}}, // date
};

static IFD stIfd1[] = {
    {0x0100, 3, 1, 320, {{.uwv = 320}}}, // ImageWidth
    {0x0101, 3, 1, 180, {{.uwv = 180}}}, // ImageLength
    {0x0202, 4, 1, 0, {{.udwv = 0}}},    // JpegIFByteCount
    {0x0201, 4, 1, 0, {{.udwv = 0}}},    // JpegIFOffset
};

static char exif_header[8] = {0x00, 0x00, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
static char tiff_header[8] = {0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00};
static int format_2_len[13] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};

static int PackageApp1(IFD ifd0[], IFD ifd1[], int ifd0_len, int ifd1_len,
                char *app1_buf, char *thumbnail_buf, int thumbnail_len) {
  int dir_start = 0;
  int data_start = 0;
  int data_len = 0;
  int total_len = 0;
  app1_buf[0] = 0xff;
  app1_buf[1] = 0xe1;
  memcpy(app1_buf + 2, exif_header, 8);
  app1_buf += 10;
  memcpy(app1_buf + dir_start, tiff_header, 8);
  dir_start += 8;
  // package ifd0
  *(short *)(app1_buf + dir_start) = ifd0_len;
  dir_start += 2;
  data_start = dir_start + ifd0_len * 12 + 4;
  for (int i = 0; i < ifd0_len; i++, dir_start += 12) {
    int value_len = ifd0[i].length * format_2_len[ifd0[i].format];
    if (value_len > 4) {
      if (ifd0[i].format == 2) {
        memcpy(app1_buf + data_start, ifd0[i].value[0].asv,
               strlen(ifd0[i].value[0].asv));
      } else {
        memcpy(app1_buf + data_start, (char *)ifd0[i].value, value_len);
      }
      ifd0[i].offset = data_start;
      data_start += value_len;
      data_len += value_len;
    }
    memcpy(app1_buf + dir_start, (char *)&ifd0[i], 12);
  }
  *(int *)(app1_buf + dir_start) = data_start;
  dir_start = data_start;
  // package ifd1
  *(short *)(app1_buf + dir_start) = ifd1_len;
  dir_start += 2;
  data_len = 0;
  data_start = dir_start + ifd1_len * 12 + 4;
  for (int i = 0; i < ifd1_len; i++, dir_start += 12) {
    int value_len = ifd1[i].length * format_2_len[ifd1[i].format];
    if (ifd1[i].tag_no == 0x0201) {
      ifd1[i].offset = ((data_start + 0x200) / 0x200) * 0x200;
      memcpy(app1_buf + ifd1[i].offset, thumbnail_buf, thumbnail_len);
      total_len = ifd1[i].offset + thumbnail_len;
    } else if (ifd1[i].tag_no == 0x0202) {
      ifd1[i].offset = thumbnail_len;
    } else if (value_len > 4) {
      if (ifd1[i].format == 2) {
        memcpy(app1_buf + data_start, ifd1[i].value[0].asv,
               strlen(ifd1[i].value[0].asv));
      } else {
        memcpy(app1_buf + data_start, (char *)ifd1[i].value, value_len);
      }
      ifd1[i].offset = data_start;
      data_start += value_len;
      data_len += value_len;
    }
    memcpy(app1_buf + dir_start, (char *)&ifd1[i], 12);
  }
  *(int *)(app1_buf + dir_start) = 0;
  app1_buf -= 10;
  total_len += 10;
  app1_buf[2] = ((total_len - 2) >> 8) & 0xff;
  app1_buf[3] = (total_len - 2) & 0xff;
  return total_len;
}

static int RKADK_Thumbnail_Vi(RKADK_S32 u32CamId, RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  int ret = 0;

  ret = RKADK_MPI_VI_Init(u32CamId, pstThumbCfg->vi_attr.u32ViChn,
                          &pstThumbCfg->vi_attr.stChnAttr);
  if (ret != 0) {
    RKADK_LOGE("RKADK_MPI_VI_Init failed[%x]", ret);
    return ret;
  }

  return 0;
}

static int RKADK_Thumbnail_Vpss(RKADK_U32 u32CamId, RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  int ret = 0;
  VPSS_GRP_ATTR_S stGrpAttr;
  VPSS_CHN_ATTR_S stChnAttr;

  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));

  stGrpAttr.u32MaxW = pstThumbCfg->vi_attr.stChnAttr.stSize.u32Width;
  stGrpAttr.u32MaxH = pstThumbCfg->vi_attr.stChnAttr.stSize.u32Height;
  stGrpAttr.enPixelFormat = pstThumbCfg->vi_attr.stChnAttr.enPixelFormat;
  stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;
  stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
  stGrpAttr.stFrameRate.s32DstFrameRate = -1;
#ifdef RV1126B
  stGrpAttr.u32MaxQueue = 1;
#endif

  stChnAttr.enChnMode = VPSS_CHN_MODE_USER;
  stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
  stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
  stChnAttr.enPixelFormat = pstThumbCfg->vi_attr.stChnAttr.enPixelFormat;
  stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stChnAttr.stFrameRate.s32DstFrameRate = -1;
  stChnAttr.u32Width = pstThumbCfg->thumb_width;
  stChnAttr.u32Height = pstThumbCfg->thumb_height;
  stChnAttr.u32FrameBufCnt = 2;
  stChnAttr.u32Depth = 0;

  ret = RKADK_MPI_VPSS_Init(pstThumbCfg->vpss_grp, pstThumbCfg->vpss_chn,
                            &stGrpAttr, &stChnAttr);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VPSS_Init falied[%d]",ret);
    return ret;
  }

  return 0;
}

static int RKADK_Thumbnail_Venc(RKADK_U32 u32CamId, RKADK_S32 ChnId,
                                RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  int ret = 0;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
  VENC_CHN_ATTR_S stAttr;
  VENC_JPEG_PARAM_S stJpegParam;

  memset(&stAttr, 0, sizeof(stAttr));
  stAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
  stAttr.stVencAttr.enPixelFormat = pstThumbCfg->vi_attr.stChnAttr.enPixelFormat;
  stAttr.stVencAttr.u32MaxPicWidth = pstThumbCfg->thumb_width;
  stAttr.stVencAttr.u32MaxPicHeight = pstThumbCfg->thumb_height;
  stAttr.stVencAttr.u32PicWidth = pstThumbCfg->thumb_width;
  stAttr.stVencAttr.u32PicHeight = pstThumbCfg->thumb_height;
  stAttr.stVencAttr.u32VirWidth = pstThumbCfg->thumb_width;
  stAttr.stVencAttr.u32VirHeight = pstThumbCfg->thumb_height;
  stAttr.stVencAttr.u32StreamBufCnt = 2;
  stAttr.stVencAttr.u32BufSize = pstThumbCfg->thumb_width * pstThumbCfg->thumb_height;

  stIfd1[0] = (IFD){0x0100, 3, 1, pstThumbCfg->thumb_width,
              {{.uwv = (uint16_t)pstThumbCfg->thumb_width}}}; // ImageWidth
  stIfd1[1] = (IFD){0x0100, 3, 1, pstThumbCfg->thumb_height,
              {{.uwv = (uint16_t)pstThumbCfg->thumb_height}}}; // ImageLength

  ret = RKADK_MPI_VENC_Init(u32CamId, ChnId, &stAttr);
  if (ret != 0) {
    RKADK_LOGE("RKADK_MPI_VENC_Init failed, ret = %d", ret);
    return ret;
  }

  memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
  stVencChnRefBufShare.bEnable = RK_TRUE;
  RK_MPI_VENC_SetChnRefBufShareAttr(ChnId, &stVencChnRefBufShare);

  memset(&stJpegParam, 0, sizeof(VENC_JPEG_PARAM_S));
  stJpegParam.u32Qfactor = pstThumbCfg->qfactor;
  RK_MPI_VENC_SetJpegParam(ChnId, &stJpegParam);

#ifndef THUMB_NORMAL
  RK_MPI_VENC_EnableThumbnail(ChnId);
  stRecvParam.s32RecvPicNum = -1;
#else
  stRecvParam.s32RecvPicNum = 1;
#endif

  // must, for no streams callback running failed
  RK_MPI_VENC_StartRecvFrame(ChnId, &stRecvParam);

  // must be before bind, interrupt data flow
  RK_MPI_VENC_StopRecvFrame(ChnId);
  RK_MPI_VENC_ResetChn(ChnId);

  return 0;
}

static int RKADK_THUMB_SetVideoChn(RKADK_PARAM_THUMB_CFG_S *pstThumbCfg,
                              RKADK_THUMB_MODULE_E enThumbModule, RKADK_U32 u32CamId,
                              MPP_CHN_S *pstViChn, MPP_CHN_S *pstVencChn,
                              MPP_CHN_S *pstSrcVpssChn, MPP_CHN_S *pstDstVpssChn) {
  memset(pstViChn, 0, sizeof(MPP_CHN_S));
  memset(pstVencChn, 0, sizeof(MPP_CHN_S));
  memset(pstSrcVpssChn, 0, sizeof(MPP_CHN_S));
  memset(pstDstVpssChn, 0, sizeof(MPP_CHN_S));

  pstViChn->enModId = RK_ID_VI;
  pstViChn->s32DevId = u32CamId;
  pstViChn->s32ChnId = pstThumbCfg->vi_attr.u32ViChn;

  pstSrcVpssChn->enModId = RK_ID_VPSS;
  pstSrcVpssChn->s32DevId = pstThumbCfg->vpss_grp;
  pstSrcVpssChn->s32ChnId = pstThumbCfg->vpss_chn;

  pstDstVpssChn->enModId = RK_ID_VPSS;
  pstDstVpssChn->s32DevId = pstThumbCfg->vpss_grp;
  pstDstVpssChn->s32ChnId = 0; //When vpss is dst, chn is equal to 0

  pstVencChn->enModId = RK_ID_VENC;
  pstVencChn->s32DevId = 0;

  switch (enThumbModule)
  {
  case RKADK_THUMB_MODULE_PHOTO:
    pstVencChn->s32ChnId = pstThumbCfg->photo_venc_chn;
    break;
  case RKADK_THUMB_MODULE_MAIN_RECORD:
    pstVencChn->s32ChnId = pstThumbCfg->record_main_venc_chn;
    break;
  case RKADK_THUMB_MODULE_SUB_RECORD:
    pstVencChn->s32ChnId = pstThumbCfg->record_sub_venc_chn;
    break;
  default:
    RKADK_LOGE("Not support other module[%d] thumbnail!", enThumbModule);
    return -1;
  }

  return 0;
}

static bool RKADK_THUMB_IsUseVpss(RKADK_U32 u32CamId, RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  bool bUseVpss = false;
  RKADK_U32 u32SrcWidth, u32SrcHeight;
  RKADK_U32 u32DstWidth, u32DstHeight;

  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return false;
  }

  u32SrcWidth= pstThumbCfg->vi_attr.stChnAttr.stSize.u32Width;
  u32SrcHeight = pstThumbCfg->vi_attr.stChnAttr.stSize.u32Height;
  u32DstWidth = pstThumbCfg->thumb_width;
  u32DstHeight = pstThumbCfg->thumb_height;

  if (u32DstWidth != u32SrcWidth || u32DstHeight != u32SrcHeight) {
    RKADK_LOGD("In[%d, %d], Out[%d, %d]", u32SrcWidth, u32SrcHeight, u32DstWidth, u32DstHeight);
    bUseVpss = true;
  }

  return bUseVpss;
}

PIXEL_FORMAT_E ThumbToRKPixFmt(RKADK_THUMB_TYPE_E enType) {
  PIXEL_FORMAT_E enPixelFormat = RK_FMT_YUV420SP;

  switch (enType) {
  case RKADK_THUMB_TYPE_NV12:
    enPixelFormat = RK_FMT_YUV420SP;
    break;

  case RKADK_THUMB_TYPE_RGB565:
    enPixelFormat = RK_FMT_RGB565;
    break;

  case RKADK_THUMB_TYPE_RGBA8888:
    enPixelFormat = RK_FMT_RGBA8888;
    break;

  case RKADK_THUMB_TYPE_BGRA8888:
    enPixelFormat = RK_FMT_BGRA8888;
    break;

  default:
    RKADK_LOGE("Unsupport thumb format, default NV12");
    break;
  }

  return enPixelFormat;
}

RKADK_S32 ThumbnailInit(RKADK_U32 u32CamId, RKADK_THUMB_MODULE_E enThumbModule,
                        RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  int ret = 0;
  bool bUseVpss = false;
  MPP_CHN_S stViChn, stVencChn, stSrcVpssChn, stDstVpssChn;

  RKADK_LOGI("Thumbnail [%d, %d] init start!", u32CamId, enThumbModule);

  ret = RKADK_THUMB_SetVideoChn(pstThumbCfg, enThumbModule, u32CamId,
                           &stViChn, &stVencChn, &stSrcVpssChn, &stDstVpssChn);
  if (ret) {
    RKADK_LOGE("Set chn failed [%d]!", ret);
    return -1;
  }

  ret = RKADK_Thumbnail_Vi(u32CamId, pstThumbCfg);
  if (ret) {
    RKADK_LOGE("Thumbnail vi [%d, %d] init failed[%x]",u32CamId,
                stViChn.s32ChnId, ret);
    return -1;
  }
  RKADK_BUFINFO("create thumb vi[%d]", pstThumbCfg->vi_attr.u32ViChn);

  bUseVpss = RKADK_THUMB_IsUseVpss(u32CamId, pstThumbCfg);
  if (bUseVpss) {
    ret = RKADK_Thumbnail_Vpss(u32CamId, pstThumbCfg);
    if (ret) {
      RKADK_LOGE("Thumbnail vpss [%d, %d] init failed[%x]",pstThumbCfg->vpss_grp,
                  pstThumbCfg->vpss_chn, ret);
      RKADK_MPI_VI_DeInit(u32CamId, stViChn.s32ChnId);
      return ret;
    }
    RKADK_BUFINFO("create thumb vpss[%d, %d]", pstThumbCfg->vpss_grp, pstThumbCfg->vpss_chn);
  }

  ret = RKADK_Thumbnail_Venc(u32CamId, stVencChn.s32ChnId, pstThumbCfg);
  if (ret) {
    RKADK_LOGE("Thumbnail venc [%d, %d] init failed[%x]", u32CamId,
                stVencChn.s32ChnId, ret);
    RKADK_MPI_VI_DeInit(u32CamId, stViChn.s32ChnId);
    RKADK_MPI_VPSS_DeInit(pstThumbCfg->vpss_grp, pstThumbCfg->vpss_chn);
    return -1;
  }
  RKADK_BUFINFO("create thumb venc[%d]", stVencChn.s32ChnId);

  if (bUseVpss) {
    // VPSS Bind VENC
    ret = RKADK_MPI_SYS_Bind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("Bind VPSS[%d] to VENC[%d] failed[%x]", stSrcVpssChn.s32ChnId,
                 stVencChn.s32ChnId, ret);
      goto failed;
    }

    // VI Bind VPSS
    ret = RKADK_MPI_SYS_Bind(&stViChn, &stDstVpssChn);
    if (ret) {
      RKADK_LOGE("Bind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId,
                 stDstVpssChn.s32DevId, ret);
      RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
      goto failed;
    }
  } else {
    // VI Bind VENC
    ret = RKADK_MPI_SYS_Bind(&stViChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("Bind VI[%d] to VENC[%d] failed[%x]", stViChn.s32ChnId,
                  stVencChn.s32ChnId, ret);
      goto failed;
    }
  }
  RKADK_BUFINFO("thumb bind[%d, %d, %d]", pstThumbCfg->vi_attr.u32ViChn, pstThumbCfg->vpss_chn, stVencChn.s32ChnId);

  RKADK_LOGI("Thumbnail [%d, %d] init end!", u32CamId, enThumbModule);
  return 0;

failed:
  RKADK_LOGE("failed");
  RKADK_MPI_VENC_DeInit(stVencChn.s32ChnId);
  RKADK_MPI_VPSS_DeInit(pstThumbCfg->vpss_grp, pstThumbCfg->vpss_chn);
  RKADK_MPI_VI_DeInit(u32CamId, stViChn.s32ChnId);
  return -1;
}

RKADK_S32 ThumbnailDeInit(RKADK_U32 u32CamId, RKADK_THUMB_MODULE_E enThumbModule,
                          RKADK_PARAM_THUMB_CFG_S *pstThumbCfg) {
  int ret = 0;
  bool bUseVpss = false;
  MPP_CHN_S stViChn, stVencChn, stSrcVpssChn, stDstVpssChn;

  RKADK_LOGI("Thumbnail [%d, %d] DeInit start!", u32CamId, enThumbModule);

  ret = RKADK_THUMB_SetVideoChn(pstThumbCfg, enThumbModule, u32CamId,
                           &stViChn, &stVencChn, &stSrcVpssChn, &stDstVpssChn);
  if (ret) {
    RKADK_LOGE("Set chn failed [%d]!", ret);
    return -1;
  }

  bUseVpss = RKADK_THUMB_IsUseVpss(u32CamId, pstThumbCfg);
  if (bUseVpss){
    // VPSS UnBind VENC
    ret = RKADK_MPI_SYS_UnBind(&stSrcVpssChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("UnBind VPSS[%d] to VENC[%d] failed[%x]", stSrcVpssChn.s32ChnId,
                 stVencChn.s32ChnId, ret);
      return ret;
    }

    // VI UnBind VPSS
    ret = RKADK_MPI_SYS_UnBind(&stViChn, &stDstVpssChn);
    if (ret) {
      RKADK_LOGE("UnBind VI[%d] to VPSS[%d] failed[%x]", stViChn.s32ChnId,
                 stSrcVpssChn.s32ChnId, ret);
      return ret;
    }
  } else {
    // VI UnBind VENC
    ret = RKADK_MPI_SYS_UnBind(&stViChn, &stVencChn);
    if (ret) {
      RKADK_LOGE("UnBind VI[%d] to VENC[%d] failed[%x]", stViChn.s32ChnId,
                stVencChn.s32ChnId, ret);
      return ret;
    }
  }

  // Destory VENC
  ret = RKADK_MPI_VENC_DeInit(stVencChn.s32ChnId);
  if (ret) {
    RKADK_LOGE("DeInit VENC[%d] failed[%x]", stVencChn.s32ChnId, ret);
    return ret;
  }

  // Destory VPSS
  if (bUseVpss) {
    ret = RKADK_MPI_VPSS_DeInit(pstThumbCfg->vpss_grp, pstThumbCfg->vpss_chn);
    if (ret) {
      RKADK_LOGE("DeInit VPSS[%d] failed[%x]", pstThumbCfg->vpss_chn, ret);
      return ret;
    }
  }

  // Destory VI
  ret = RKADK_MPI_VI_DeInit(u32CamId, stViChn.s32ChnId);
  if (ret) {
    RKADK_LOGE("DeInit VI[%d] failed[%x]", stViChn.s32ChnId, ret);
    return ret;
  }

  RKADK_LOGI("Thumbnail [%d, %d] DeInit end!", u32CamId, enThumbModule);
  return 0;
}

RKADK_S32 ThumbnailPhotoData(RKADK_U8 *pu8JpegData, RKADK_U32 u32JpegLen,
                               VENC_STREAM_S stThuFrame,
                               RKADK_U8 *pu8Photo) {
  char *thumb_data;
  int thumb_len;
  int app0_len;
  int off_set;
  int userdata_len;
  time_t timep;
  struct tm *p;
  char local_time[100];

  time(&timep);
  p = gmtime(&timep);

  memset(local_time, 0, sizeof(local_time));
  snprintf(local_time, sizeof(local_time) - 1, "%04d-%02d-%02d %02d:%02d:%02d",
          (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour,
          p->tm_min, p->tm_sec);

  stIfd0[4].value[0].asv = local_time;

  //thumbnail
  thumb_data = (char *)RK_MPI_MB_Handle2VirAddr(stThuFrame.pstPack->pMbBlk);
  thumb_len = stThuFrame.pstPack->u32Len;
  RKADK_LOGD("Thumbnail seq = %d, data %p, size = %d", stThuFrame.u32Seq,
              thumb_data, thumb_len);

  app0_len = pu8JpegData[5];
  off_set = app0_len + 4;
  memcpy(pu8Photo, pu8JpegData, off_set);
  pu8Photo += off_set;
  userdata_len = PackageApp1(stIfd0, stIfd1, sizeof(stIfd0) / sizeof(IFD),
                sizeof(stIfd1) / sizeof(IFD), (char *)pu8Photo, thumb_data, thumb_len);
  pu8Photo += userdata_len;
  memcpy(pu8Photo, pu8JpegData + off_set, u32JpegLen - off_set);
  pu8Photo -= (off_set + userdata_len);

  return u32JpegLen + userdata_len;
}

RKADK_S32 ThumbnailChnBind(RKADK_S32 s32VencChn, RKADK_S32 s32ThumbVencChn) {
  int ret;

  if (s32VencChn < 0 || s32ThumbVencChn < 0)
    return -1;

  ret = RK_MPI_VENC_ThumbnailBind(s32VencChn, s32ThumbVencChn);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("thumbnail bind %d ch venc failed", s32VencChn);
    return ret;
  }

  return 0;
}

RKADK_S32 ThumbnailRequest(RKADK_S32 s32ThumbVencChn) {
  int ret;
  VENC_RECV_PIC_PARAM_S stRecvParam;

  if (s32ThumbVencChn < 0)
    return -1;

  memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
  stRecvParam.s32RecvPicNum = 1;
  ret = RK_MPI_VENC_StartRecvFrame(s32ThumbVencChn, &stRecvParam);
  if (ret) {
    RKADK_LOGE("Request thumbnail failed %x", ret);
    return -1;
  }
  return 0;
}

static RKADK_S32 VdecThmFree(void *opaque) {
  RKADK_LOGD("free buffer: %p", opaque);
  if (opaque) {
    free(opaque);
    opaque = NULL;
  }
  return 0;
}

static RKADK_S32 ThumbDataCopy(RKADK_THUMB_ATTR_S *pstDstThmAttr,
                                        RK_U8 *pVdecData, RK_U64 VdecDataLen) {
  if (!pVdecData) {
    RKADK_LOGE("pVdecData is null");
    return -1;
  }

  if (!pstDstThmAttr->pu8Buf) {
    pstDstThmAttr->pu8Buf = (RKADK_U8 *)malloc(VdecDataLen);
    if (!pstDstThmAttr->pu8Buf) {
      RKADK_LOGE("malloc thumb buffer failed, VdecDataLen: %lld", VdecDataLen);
      return -1;
    }
    RKADK_LOGD("malloc thumb buffer[%p, %lld]", pstDstThmAttr->pu8Buf, VdecDataLen);

    pstDstThmAttr->u32BufSize = (RKADK_U32)VdecDataLen;
  } else {
    if (pstDstThmAttr->u32BufSize < VdecDataLen)
      RKADK_LOGW("buffer size[%d] < thumb size[%lld]",
                 pstDstThmAttr->u32BufSize, VdecDataLen);
    else
      pstDstThmAttr->u32BufSize = VdecDataLen;
  }

  memcpy(pstDstThmAttr->pu8Buf, pVdecData, pstDstThmAttr->u32BufSize);
  return 0;
}

static RKADK_S32 ThumbVpssInit(MPP_CHN_S stVpssChn, RKADK_THUMB_ATTR_S *pstSrcThmAttr,
                                       RKADK_THUMB_ATTR_S *pstDstThmAttr) {
  RKADK_U32 u32MaxW, u32MaxH;
  VPSS_GRP_ATTR_S stGrpAttr;
  VPSS_CHN_ATTR_S stChnAttr;

  memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
  memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));

  u32MaxW = pstSrcThmAttr->u32Width > pstDstThmAttr->u32Width ?
    pstSrcThmAttr->u32Width : pstDstThmAttr->u32Width;
  u32MaxH = pstSrcThmAttr->u32Height > pstDstThmAttr->u32Height ?
    pstSrcThmAttr->u32Height : pstDstThmAttr->u32Height;

  stGrpAttr.u32MaxW = u32MaxW;
  stGrpAttr.u32MaxH = u32MaxH;
  stGrpAttr.enPixelFormat = RK_FMT_YUV420SP;
  stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;
  stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
  stGrpAttr.stFrameRate.s32DstFrameRate = -1;
#ifdef RV1126B
  stGrpAttr.u32MaxQueue = 1;
#endif

  stChnAttr.enChnMode = VPSS_CHN_MODE_USER;
  stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
  stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
  stChnAttr.enPixelFormat = ThumbToRKPixFmt(pstDstThmAttr->enType);
  stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stChnAttr.stFrameRate.s32DstFrameRate = -1;
  stChnAttr.u32Width = pstDstThmAttr->u32Width;
  stChnAttr.u32Height = pstDstThmAttr->u32Height;
  stChnAttr.u32Depth = 1;

  return RKADK_MPI_VPSS_Init(stVpssChn.s32DevId, stVpssChn.s32ChnId,
                            &stGrpAttr, &stChnAttr);
}

RKADK_S32 ThumbnailJpgDecode(RKADK_THUMB_ATTR_S *pstSrcThmAttr,
                            RKADK_THUMB_ATTR_S *pstDstThmAttr, bool *bFree) {
  int ret = 0, deinitRet = 0;
  MB_BLK jpgMbBlk = RK_NULL;
  MB_EXT_CONFIG_S stMbExtConfig;
  VDEC_STREAM_S stStream;
  VIDEO_FRAME_INFO_S sFrame = {0};
  RK_U8 *pVdecData = RK_NULL;
  RK_U64 VdecDataLen = 0;
  MPP_CHN_S stVdecChn, stVpssChn;
  VIDEO_FRAME_INFO_S sFrameIn = {0};
  RKADK_THUMB_CHN_ATTR_S stThumbChnAttr;
  int timeout = pstDstThmAttr->timeout > 0 ? pstDstThmAttr->timeout : -1;

  if (pstSrcThmAttr->pu8Buf[0] != 0xFF || pstSrcThmAttr->pu8Buf[1] != 0xD8) {
    RKADK_LOGD("Invalid jpeg data");
    *bFree = true;
    return -1;
  }

  memset(&stMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
  memset(&stStream, 0, sizeof(VDEC_STREAM_S));
  memset(&stThumbChnAttr, 0, sizeof(RKADK_THUMB_CHN_ATTR_S));

  stVdecChn.enModId = RK_ID_VDEC;
  stVdecChn.s32DevId = 0;
  stVdecChn.s32ChnId = pstDstThmAttr->s32VdecChn;

  stVpssChn.enModId = RK_ID_VPSS;
  stVpssChn.s32DevId = pstDstThmAttr->s32VpssGrp;
  stVpssChn.s32ChnId = pstDstThmAttr->s32VpssChn;

  if (!pstDstThmAttr->bChnExCreate) {
    stThumbChnAttr.u32SrcWidth = pstSrcThmAttr->u32Width;
    stThumbChnAttr.u32SrcHeight = pstSrcThmAttr->u32Height;
    stThumbChnAttr.enDstType = pstDstThmAttr->enType;
    stThumbChnAttr.u32DstWidth = pstDstThmAttr->u32Width;
    stThumbChnAttr.u32DstHeight = pstDstThmAttr->u32Height;
    stThumbChnAttr.s32VdecChn = pstDstThmAttr->s32VdecChn;
    stThumbChnAttr.s32VpssGrp = pstDstThmAttr->s32VpssGrp;
    stThumbChnAttr.s32VpssChn = pstDstThmAttr->s32VpssChn;

    ret = GetThmChnCreate(&stThumbChnAttr);
    if (ret) {
      RKADK_LOGE("GetThmChnCreate failed[%d]", ret);
      *bFree = true;
      return ret;
    }
  }

  stMbExtConfig.pFreeCB = VdecThmFree;
  stMbExtConfig.pOpaque = pstSrcThmAttr->pu8Buf;
  stMbExtConfig.pu8VirAddr = pstSrcThmAttr->pu8Buf;
  stMbExtConfig.u64Size = pstSrcThmAttr->u32BufSize;
  ret = RK_MPI_SYS_CreateMB(&jpgMbBlk, &stMbExtConfig);
  if (ret) {
    RKADK_LOGE("Create vdec[%d] MB failed[%x]", stVdecChn.s32ChnId, ret);
    *bFree = true;
    goto exit;
  }

  stStream.u64PTS = 0;
  stStream.pMbBlk = jpgMbBlk;
  stStream.u32Len = pstSrcThmAttr->u32BufSize;
  stStream.bEndOfStream = RK_FALSE;
  stStream.bEndOfFrame = RK_FALSE;
  stStream.bBypassMbBlk = RK_TRUE;
  ret = RK_MPI_VDEC_SendStream(stVdecChn.s32ChnId, &stStream, timeout);
  if(ret) {
    RKADK_LOGE("Send vdec[%d] stream failed[%x]", stVdecChn.s32ChnId, ret);
    goto exit;
  }

  //get decode frame
  memset(&sFrameIn, 0, sizeof(VIDEO_FRAME_INFO_S));
  ret = RK_MPI_VDEC_GetFrame(stVdecChn.s32ChnId, &sFrameIn, timeout);
  if(ret) {
    RKADK_LOGE("Get vdec[%d] frame failed[%d]", stVdecChn.s32ChnId, ret);
    goto exit;
  }

  if (sFrameIn.stVFrame.enPixelFormat == ThumbToRKPixFmt(pstDstThmAttr->enType)
        && sFrameIn.stVFrame.u32Width == pstDstThmAttr->u32Width
        && sFrameIn.stVFrame.u32Height == pstDstThmAttr->u32Height) {
    pVdecData = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(sFrameIn.stVFrame.pMbBlk);
    VdecDataLen = RK_MPI_MB_GetSize(sFrameIn.stVFrame.pMbBlk);
    RKADK_LOGD("vdec output[%p, %lld], w*h[%d, %d]", pVdecData, VdecDataLen,
                sFrameIn.stVFrame.u32Width, sFrameIn.stVFrame.u32Height);

    ret = ThumbDataCopy(pstDstThmAttr, pVdecData, VdecDataLen);
    if (ret)
      RKADK_LOGE("Data copy failed");

    RK_MPI_VDEC_ReleaseFrame(stVdecChn.s32ChnId, &sFrameIn);
    goto exit;
  }

  //send frame to vpss
  ret = RK_MPI_VPSS_SendFrame(stVpssChn.s32DevId, 0, &sFrameIn, -1);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("Send vpss[%d] frame failed[%d]", stVpssChn.s32DevId, ret);
    RK_MPI_VDEC_ReleaseFrame(stVdecChn.s32ChnId, &sFrameIn);
    goto exit;
  }

  RK_MPI_VDEC_ReleaseFrame(stVdecChn.s32ChnId, &sFrameIn);

  //get vpss frame
  memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  ret = RK_MPI_VPSS_GetChnFrame(stVpssChn.s32DevId, stVpssChn.s32ChnId, &sFrame, -1);
  if(ret) {
    RKADK_LOGE("Get vpss[%d] frame failed[%d]", stVdecChn.s32ChnId, ret);
    goto exit;
  }

  pVdecData = (RK_U8 *)RK_MPI_MB_Handle2VirAddr(sFrame.stVFrame.pMbBlk);
  VdecDataLen = RK_MPI_MB_GetSize(sFrame.stVFrame.pMbBlk);
  RKADK_LOGD("vpss output[%p, %lld], w*h[%d, %d]", pVdecData, VdecDataLen,
              sFrame.stVFrame.u32Width, sFrame.stVFrame.u32Height);
  RK_MPI_SYS_MmzFlushCache(sFrame.stVFrame.pMbBlk, RK_TRUE);

  ret = ThumbDataCopy(pstDstThmAttr, pVdecData, VdecDataLen);
  if (ret)
    RKADK_LOGE("Data copy failed");

  RK_MPI_VPSS_ReleaseChnFrame(stVpssChn.s32DevId, stVpssChn.s32ChnId, &sFrame);

exit:
  if (!pstDstThmAttr->bChnExCreate) {
    deinitRet = GetThmChnDestory(&stThumbChnAttr);
    if (deinitRet)
      RKADK_LOGE("GetThmChnDestory failed[%d]", deinitRet);
  }

  if(jpgMbBlk)
    RK_MPI_MB_ReleaseMB(jpgMbBlk);

  return ret;
}

RKADK_S32 BuildInThmToFile(FILE *fd, RKADK_THUMB_ATTR_S *pstThumbAttr) {
  RKADK_U8 header[4] = {0};
  RKADK_U32 u32TotalLen;
  RKADK_U32 u32Width, u32Height, u32VirWidth, u32VirHeight;

  RKADK_CHECK_POINTER(pstThumbAttr, -1);
  RKADK_CHECK_POINTER(fd, -1);

  if (!feof(fd)) {
    if (fseek(fd, 0, SEEK_END) != 0) {
      RKADK_LOGE("seek end failed, errno[%d, %s]", errno, strerror(errno));
      return -1;
    }
  }

  header[0] = 't';
  header[1] = 'h';
  header[2] = 'm';
  header[3] = pstThumbAttr->enType;
  u32Width = bswap_32(pstThumbAttr->u32Width);
  u32Height = bswap_32(pstThumbAttr->u32Height);
  u32VirWidth = bswap_32(pstThumbAttr->u32VirWidth);
  u32VirHeight = bswap_32(pstThumbAttr->u32VirHeight);
  u32TotalLen = bswap_32(pstThumbAttr->u32BufSize + 4 * 4); // 4(w) + 4(h) + 4(vir_w) + 4(vir_h)

  if (fwrite(pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize, 1, fd) != 1) {
    RKADK_LOGE("write thm data failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(&u32Width, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm width failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(&u32Height, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm height failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(&u32VirWidth, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual width failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(&u32VirHeight, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual height failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(&u32TotalLen, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm data size failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  if (fwrite(header, 4, 1, fd) != 1) {
    RKADK_LOGE("write thm header failed, errno[%d, %s]", errno, strerror(errno));
    return -1;
  }

  fflush(fd);
  fsync(fileno(fd));
  return 0;
}

static RKADK_S32 BuildInThmToMp4(FILE *fd, RKADK_S64 s64FileSize, RKADK_CHAR *pszFileName,
                            RKADK_THUMB_ATTR_S *pstThumbAttr) {
  RKADK_S64 cur = 0, s64BoxSize = 0;
  bool bBuildInThm = false;
  RKADK_S32 s32BoxSize;
  RKADK_U32 u32Width, u32Height, u32VirWidth, u32VirHeight;
  char boxHeader[THM_BOX_HEADER_LEN];
  char largeSize[THM_BOX_HEADER_LEN];

  if (fseek(fd, 0, SEEK_SET)) {
    RKADK_LOGE("seek to %s[%d] header failed, errno[%d, %s]", pszFileName, fd, errno, strerror(errno));
    return -1;
  }

  while (!feof(fd)) {
    if (fread(boxHeader, THM_BOX_HEADER_LEN, 1, fd) != 1) {
      if (feof(fd)) {
        RKADK_LOGD("EOF");
        bBuildInThm = true;
      } else {
        RKADK_LOGE("fd[%d] can't read box header, errno[%d, %s]", fd, errno, strerror(errno));
      }
      break;
    }

    s32BoxSize = boxHeader[0] << 24 | boxHeader[1] << 16 | boxHeader[2] << 8 |
                 boxHeader[3];

    if (!s32BoxSize) {
      if (!boxHeader[4] && !boxHeader[5] && !boxHeader[6] && !boxHeader[7]) {
        RKADK_LOGE("invalid data, cover!!!");

        if (fseek(fd, -THM_BOX_HEADER_LEN, SEEK_CUR))
          RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
        else
          bBuildInThm = true;
      } else {
        RKADK_LOGE("u32BoxSize = %d, invalid data", s32BoxSize);
      }

      break;
    } else if (s32BoxSize == 1) {
      if (fread(largeSize, THM_BOX_HEADER_LEN, 1, fd) != 1) {
        RKADK_LOGE("fd[%d] read largeSize failed, errno[%d, %s]", fd, errno, strerror(errno));
        break;
      }

      s64BoxSize = (RKADK_S64)largeSize[0] << 56 |
                   (RKADK_S64)largeSize[1] << 48 |
                   (RKADK_S64)largeSize[2] << 40 |
                   (RKADK_S64)largeSize[3] << 32 | largeSize[4] << 24 |
                   largeSize[5] << 16 | largeSize[6] << 8 | largeSize[7];

      cur = ftell(fd);
      if (cur == -1) {
        RKADK_LOGE("fd[%d] ftell failed, errno[%d, %s]", fd, errno, strerror(errno));
        break;
      }

      if (s64BoxSize - THM_BOX_HEADER_LEN * 2 + cur < 0 ||
          s64BoxSize - THM_BOX_HEADER_LEN * 2 + cur > s64FileSize) {
        RKADK_LOGE("bad largeSize value, this box is invalid. Insert thumb here!");
        if (fseek(fd, 0 - THM_BOX_HEADER_LEN * 2, SEEK_CUR)) {
          RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
        } else {
          bBuildInThm = true;
        }
        break;
      } else {
        if (fseek(fd, s64BoxSize - (THM_BOX_HEADER_LEN * 2), SEEK_CUR))
          RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
      }
    } else if (boxHeader[4] == 't' && boxHeader[5] == 'h'
          && boxHeader[6] == 'm' && boxHeader[7] != RKADK_THUMB_TYPE_JPEG) {
      if (fseek(fd, -THM_BOX_HEADER_LEN, SEEK_CUR)) {
        RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
      } else {
        RKADK_LOGD("already exist thm[%d] tag, cover", boxHeader[7]);
        bBuildInThm = true;
      }

      break;
    } else {
      cur = ftell(fd);
      if (cur == -1) {
        RKADK_LOGE("fd[%d] ftell failed, errno[%d, %s]", fd, errno, strerror(errno));
        break;
      }

      if (s32BoxSize - THM_BOX_HEADER_LEN + cur < 0 ||
          s32BoxSize - THM_BOX_HEADER_LEN + cur > s64FileSize) {
        RKADK_LOGE("bad BoxSize value, this box is invalid. Insert thumb here!");
        if (fseek(fd, 0 - THM_BOX_HEADER_LEN, SEEK_CUR))
          RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
        else
          bBuildInThm = true;
        break;
      } else {
          if (fseek(fd, s32BoxSize - THM_BOX_HEADER_LEN, SEEK_CUR))
            RKADK_LOGE("fd[%d] seek failed, errno[%d, %s]", fd, errno, strerror(errno));
      }
    }
  }

  if (!bBuildInThm)
    return -1;

  // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
  s32BoxSize = pstThumbAttr->u32BufSize + THM_BOX_HEADER_LEN + 16;
  boxHeader[0] = s32BoxSize >> 24;
  boxHeader[1] = (s32BoxSize & 0x00FF0000) >> 16;
  boxHeader[2] = (s32BoxSize & 0x0000FF00) >> 8;
  boxHeader[3] = s32BoxSize & 0x000000FF;
  boxHeader[4] = 't';
  boxHeader[5] = 'h';
  boxHeader[6] = 'm';
  boxHeader[7] = pstThumbAttr->enType;

  u32Width = bswap_32(pstThumbAttr->u32Width);
  u32Height = bswap_32(pstThumbAttr->u32Height);
  u32VirWidth = bswap_32(pstThumbAttr->u32VirWidth);
  u32VirHeight = bswap_32(pstThumbAttr->u32VirHeight);

  if (fwrite(boxHeader, THM_BOX_HEADER_LEN, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm box header failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  if (fwrite(&(u32Width), 4, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm width failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  if (fwrite(&(u32Height), 4, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm height failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  if (fwrite(&(u32VirWidth), 4, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm virtual width failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  if (fwrite(&(u32VirHeight), 4, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm virtual height failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  if (fwrite(pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize, 1, fd) != 1) {
    RKADK_LOGE("fd[%d] write thm box body failed, errno[%d, %s]", fd, errno, strerror(errno));
    return -1;
  }

  RKADK_LOGD("done!");
  return 0;
}

static RKADK_S64 SeekToThmInMp4(const RKADK_U8 *pFile, RKADK_S64 s64FileSize,
                                      RKADK_THUMB_TYPE_E enType,
                                      RKADK_U64 *u64JpgThmPos) {
  RKADK_S64 s64BoxSize = 0, cur = 0;

  /* at least 16 bytes remains if thumbnail exist */
  while (cur + 2 * THM_BOX_HEADER_LEN < s64FileSize) {
    if (pFile[cur + 4] == 't' && pFile[cur + 5] == 'h' && pFile[cur + 6] == 'm') {
      if (pFile[cur + 7] == enType)
        return cur;
      else if (pFile[cur + 7] == RKADK_THUMB_TYPE_JPEG)
        *u64JpgThmPos = cur;
    }

    s64BoxSize = bswap_32(*(RKADK_U32*) (pFile + cur));
    if (s64BoxSize == 1) {
      s64BoxSize = bswap_64(*(RKADK_U64*) (pFile + cur + THM_BOX_HEADER_LEN));
    } else if (s64BoxSize <= 0) {
      RKADK_LOGE("Last one box, not find thm box");
      break;
    }

    cur += s64BoxSize;
  }

  return -1;
}

static RKADK_S32 GetSpecificThmInMp4(RKADK_U8 *pFile, RKADK_S64 s64FileSize,
                                RKADK_THUMB_ATTR_S *pstThumbAttr, RKADK_U64 *u64JpgThmPos) {
  int boxSize = 0;
  RKADK_S64 cur = 0;

  if (*u64JpgThmPos > 0)
    cur = *u64JpgThmPos;
  else
    cur = SeekToThmInMp4(pFile, s64FileSize, pstThumbAttr->enType, u64JpgThmPos);

  if (cur > 0 && (boxSize = bswap_32(*(int*) (pFile + cur))) > 0 &&
      cur + boxSize <= s64FileSize) {
    // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
    RKADK_U32 u32DataSize = boxSize - THM_BOX_HEADER_LEN - 16;

    if (!pstThumbAttr->pu8Buf) {
      pstThumbAttr->pu8Buf = (RKADK_U8 *)malloc(u32DataSize);
      if (!pstThumbAttr->pu8Buf) {
        RKADK_LOGE("malloc thumb buffer failed, size: %d", u32DataSize);
        return -1;
      }
      pstThumbAttr->u32BufSize = u32DataSize;
      RKADK_LOGD("malloc thumb buffer[%p, %d]", pstThumbAttr->pu8Buf, u32DataSize);
    } else {
      if (pstThumbAttr->u32BufSize < u32DataSize)
        RKADK_LOGW("buffer size[%d] < thm data size[%d]",
                   pstThumbAttr->u32BufSize, u32DataSize);
      else
        pstThumbAttr->u32BufSize = u32DataSize;
    }

    memcpy(pstThumbAttr->pu8Buf, pFile + cur + THM_BOX_HEADER_LEN + 16,
           pstThumbAttr->u32BufSize);
    pstThumbAttr->u32Width = bswap_32(*(RKADK_U32*) (pFile + cur + THM_BOX_HEADER_LEN));
    pstThumbAttr->u32Height = bswap_32(*(RKADK_U32*) (pFile + cur + THM_BOX_HEADER_LEN + 4));
    pstThumbAttr->u32VirWidth = bswap_32(*(RKADK_U32*) (pFile + cur + THM_BOX_HEADER_LEN + 8));
    pstThumbAttr->u32VirHeight = bswap_32(*(RKADK_U32*) (pFile + cur + THM_BOX_HEADER_LEN + 12));
    return 0;
  }

  return -1;
}

static RKADK_S64 SeekToThmInFile(const RKADK_U8 *pFile, RKADK_S64 s64FileSize,
                                      RKADK_THUMB_TYPE_E enType,
                                      RKADK_U64 *u64JpgThmPos) {
  RKADK_U32 len = 0;
  RKADK_THUMB_TYPE_E enTmpType;
  RKADK_S64 cur = s64FileSize;

  RKADK_LOGD("pFile[%llx]: %x %x %x %x", cur - 4, pFile[cur - 4], pFile[cur - 3], pFile[cur - 2], pFile[cur - 1]);
  while (cur > 0) {
    if (pFile[cur - 4] == 't' && pFile[cur - 3] == 'h' && pFile[cur - 2] == 'm') {
      if (pFile[cur - 1] == enType) {
        RKADK_LOGD("find thm%d tag, cur[%llx]", pFile[cur - 1], cur);
        return cur;
      } else if (pFile[cur - 1] == RKADK_THUMB_TYPE_JPEG) {
        *u64JpgThmPos = cur;
        RKADK_LOGD("find jpg thm tag, cur[%llx]", cur);
        return -1;
      } else {
        // find jpg thm
        enTmpType = pFile[cur - 1];
        cur = cur - 8; // 4(thmx) + 4(tatalLen)
        if (cur <= 0) {
          RKADK_LOGE("Invalid cur[%lld, %llx]", cur, cur);
          break;
        }

        len = bswap_32(*(RKADK_U32*) (pFile + cur));
        RKADK_LOGD("thm%d len[%d] pFile[%llx]: %x %x %x %x", enTmpType, len, cur, pFile[cur], pFile[cur + 1], pFile[cur + 2], pFile[cur + 3]);

        cur -= len;
      }
    } else {
      RKADK_LOGI("not find thm%d tag, cur[%x]", enType, cur);
      break;
    }
  }

  return -1;
}

static RKADK_S32 GetSpecificThmInFile(RKADK_U8 *pFile, RKADK_S64 s64FileSize,
                                RKADK_THUMB_ATTR_S *pstThumbAttr, RKADK_U64 *u64JpgThmPos) {
  int offset = 8; // 4(tatalLen) + 4(thmx)
  RKADK_U32 u32TotalSize, u32DataSize;
  RKADK_S64 cur = 0;

  if (*u64JpgThmPos > 0)
    cur = *u64JpgThmPos;
  else
    cur = SeekToThmInFile(pFile, s64FileSize, pstThumbAttr->enType, u64JpgThmPos);

  if ((cur - offset) <= 0) {
    if (cur != -1)
      RKADK_LOGE("seek to invalid cur[%lld, %llx]", cur, cur);
    return -1;
  }

  u32TotalSize = bswap_32(*(RKADK_U32*) (pFile + cur - offset));
  if ((cur - offset - u32TotalSize) <= 0) {
    RKADK_LOGE("seek to invalid cur[%d] u32TotalSize[%lld]", cur, u32TotalSize);
    return -1;
  }

  // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
  u32DataSize = u32TotalSize - 16;
  if (!pstThumbAttr->pu8Buf) {
    pstThumbAttr->pu8Buf = (RKADK_U8 *)malloc(u32DataSize);
    if (!pstThumbAttr->pu8Buf) {
      RKADK_LOGE("malloc thumb buffer failed, size: %d", u32DataSize);
      return -1;
    }
    pstThumbAttr->u32BufSize = u32DataSize;
    RKADK_LOGD("malloc thumb buffer[%p, %d]", pstThumbAttr->pu8Buf, u32DataSize);
  } else {
    if (pstThumbAttr->u32BufSize < u32DataSize)
      RKADK_LOGW("buffer size[%d] < thm data size[%d]",
                  pstThumbAttr->u32BufSize, u32DataSize);
    else
      pstThumbAttr->u32BufSize = u32DataSize;
  }

  pstThumbAttr->u32Width = bswap_32(*(RKADK_U32*) (pFile + cur - offset - 16));
  pstThumbAttr->u32Height = bswap_32(*(RKADK_U32*) (pFile + cur - offset - 12));
  pstThumbAttr->u32VirWidth = bswap_32(*(RKADK_U32*) (pFile + cur - offset - 8));
  pstThumbAttr->u32VirHeight = bswap_32(*(RKADK_U32*) (pFile + cur - offset - 4));
  memcpy(pstThumbAttr->pu8Buf, pFile + cur - offset - 16 - u32DataSize, pstThumbAttr->u32BufSize);
  RKADK_LOGD("thumb enType[%d] resolution[%d, %d, %d, %d], pu8Buf[%x, %x]",pstThumbAttr->enType,
            pstThumbAttr->u32Width, pstThumbAttr->u32Height, pstThumbAttr->u32VirWidth,
            pstThumbAttr->u32VirHeight, pstThumbAttr->pu8Buf[0], pstThumbAttr->pu8Buf[1]);

  return 0;
}

static int ReadThumbJpgFile(RKADK_CHAR *pszFileName, char *thumbPath, int thumbPathLen, RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret = -1;
  FILE *fd = NULL;
  RKADK_S64 thumbLen = 0;
  size_t fileSize = 0;

  RKADK_CHECK_POINTER(pstThumbAttr->pstThumbParam, -1);
  RKADK_CHECK_POINTER(pstThumbAttr->pstThumbParam->pThumbDir, -1);

  ret = RKADK_THUMB_GetFilePath(pszFileName, pstThumbAttr->pstThumbParam->pThumbDir, thumbPath, thumbPathLen);
  if (ret) {
    RKADK_LOGE("file[%s] get thumb path faild", pszFileName);
    return -1;
  }

  fd = fopen(thumbPath, "rb");
  if (!fd) {
    RKADK_LOGE("open %s failed, error[%d, %s]", thumbPath, errno, strerror(errno));
    return -1;
  }

  if (fseek(fd, 0, SEEK_END) || (thumbLen = ftell(fd)) == -1 ||
    fseek(fd, 0, SEEK_SET)) {
    RKADK_LOGE("get file[%s] len failed, errno[%d, %s]", thumbPath, errno, strerror(errno));
    goto exit;
  }
  fileSize = (size_t)thumbLen;

  if (!pstThumbAttr->pu8Buf) {
    pstThumbAttr->pu8Buf = (RKADK_U8 *)malloc(fileSize);
    if (!pstThumbAttr->pu8Buf) {
      RKADK_LOGE("malloc thumbnail buffer[%d] failed", fileSize);
      goto exit;
    }
    RKADK_LOGD("malloc jpg thumb buffer[%p, %d]", pstThumbAttr->pu8Buf, fileSize);

    pstThumbAttr->u32BufSize = fileSize;
  } else {
    if (pstThumbAttr->u32BufSize < fileSize)
      RKADK_LOGW("buffer size[%d] < thm data size[%d]", pstThumbAttr->u32BufSize, fileSize);
    else
      pstThumbAttr->u32BufSize = fileSize;
  }

  if (fread(pstThumbAttr->pu8Buf, 1, pstThumbAttr->u32BufSize, fd) != pstThumbAttr->u32BufSize) {
    RKADK_LOGE("read thumb[%s] failed, errno[%d, %s]", thumbPath, errno, strerror(errno));
  } else {
    RKADK_LOGD("read thumb[%s] done!", thumbPath);
    ret = 0;
  }

exit:
  if (fd)
    fclose(fd);

  return ret;
}

static int ThumbnailRepair(FILE *fd, RKADK_CHAR *pszFileName, RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret;
  RKADK_THUMB_REPAIR_RESULT_E result = RKADK_THUMB_REPAIR_FAIL;
  RKADK_U8 header[4] = {0};
  char thumbPath[RKADK_MAX_FILE_PATH_LEN];
  RKADK_THUMB_ATTR_S stThumbAttr;
  RKADK_THUMB_ATTR_S *pstBuildThmAttr = NULL;

  memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));

  ret = fseek(fd, -4, SEEK_END);
  if (ret) {
    RKADK_LOGE("fseek SEEK_END failed, error[%d, %s]", errno, strerror(errno));
    return RKADK_THUMB_REPAIR_FAIL;
  }

  ret = fread(header, 1, 4, fd);
  if (ret != 4) {
    RKADK_LOGE("fread failed, error[%d, %s]", errno, strerror(errno));
    return RKADK_THUMB_REPAIR_FAIL;
  }

  if (header[0] == 't' && header[1] == 'h' && header[2] == 'm'
      && (header[3] >= RKADK_THUMB_TYPE_NV12 && header[3] < RKADK_THUMB_TYPE_BUTT)) {
    RKADK_LOGD("already built-in thumbnail, don't need to repair");
    return RKADK_THUMB_ALREADY_EXIST;
  }
  RKADK_LOGD("don't find built-in thumbnail, try repair");

  if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
    pstBuildThmAttr = pstThumbAttr;
  } else {
    stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
    stThumbAttr.u32Width = pstThumbAttr->u32Width;
    stThumbAttr.u32Height = pstThumbAttr->u32Height;
    stThumbAttr.u32VirWidth = pstThumbAttr->u32VirWidth;
    stThumbAttr.u32VirHeight = pstThumbAttr->u32VirHeight;
    stThumbAttr.pstThumbParam = pstThumbAttr->pstThumbParam;

    pstBuildThmAttr = &stThumbAttr;
  }

  ret = ReadThumbJpgFile(pszFileName, thumbPath, RKADK_MAX_FILE_PATH_LEN, pstBuildThmAttr);
  if (ret) {
    RKADK_LOGE("read file[%s] jpg thumb failed", pszFileName);
    goto exit;
  }

  ret = BuildInThmToFile(fd, pstBuildThmAttr);
  if (ret) {
    RKADK_LOGE("pszFileName[%s] build-in thumb failed", pszFileName);
    result = RKADK_THUMB_BUILD_IN_FAIL;
    goto exit;
  }

  result = RKADK_THUMB_REPAIR_SUCCESS;
  RKADK_LOGD("pszFileName[%s] repair thumb done!", pszFileName);

  if (remove(thumbPath) == 0)
    RKADK_LOGD("remove thumb file[%s] done", thumbPath);
  else
    RKADK_LOGE("remove thumb file[%s] fail, errno[%d, %s]", thumbPath, errno, strerror(errno));

exit:
  if (stThumbAttr.pu8Buf) {
    RKADK_LOGD("free buffer[%p]", stThumbAttr.pu8Buf);
    free(stThumbAttr.pu8Buf);
  }

  return result;
}

static RKADK_S32 GetThmInFile(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                             RKADK_THUMB_ATTR_S *pstThumbAttr) {
  FILE *fd = NULL;
  int ret = 0, result = 0;
  RKADK_THUMB_ATTR_S stTmpThmAttr;
  bool bFree = false;
  RKADK_U64 u64JpgThmPos = 0;
  RKADK_S64 s64FileSize = 0;
  RKADK_U8 *pFile = NULL;
  struct stat stStatBuf;
  struct utimbuf stTimebuf;
  bool bBuildInThm = true;    // defult build-in thumbnail

  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!pstThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  if (!pstThumbAttr->u32Width || !pstThumbAttr->u32Height
        || !pstThumbAttr->u32VirWidth || !pstThumbAttr->u32VirHeight) {
    if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
      pstThumbAttr->u32Width = pstThumbCfg->thumb_width;
      pstThumbAttr->u32Height = pstThumbCfg->thumb_height;
    } else {
      pstThumbAttr->u32Width = UPALIGNTO(pstThumbCfg->thumb_width, 4);
      pstThumbAttr->u32Height = UPALIGNTO(pstThumbCfg->thumb_height, 2);
    }

    pstThumbAttr->u32VirWidth = pstThumbAttr->u32Width;
    pstThumbAttr->u32VirHeight = pstThumbAttr->u32Height;
  }

  if (pstThumbAttr->pstThumbParam)
    bBuildInThm = pstThumbAttr->pstThumbParam->bBuildInThm;

  fd = fopen(pszFileName, "r+");
  if (!fd) {
    RKADK_LOGE("open %s failed, error[%d, %s]", pszFileName, errno, strerror(errno));
    return -1;
  }

  memset(&stTimebuf, 0, sizeof(struct utimbuf));
  result = stat(pszFileName, &stStatBuf);
  if (result) {
    RKADK_LOGW("stat[%s] failed[%d]", pszFileName, result);
  } else {
    stTimebuf.actime = stStatBuf.st_atime;
    stTimebuf.modtime = stStatBuf.st_mtime;
  }

  if (bBuildInThm && pstThumbAttr->pstThumbParam && pstThumbAttr->pstThumbParam->pThumbDir) {
    RKADK_THUMB_REPAIR_RESULT_E enResult = ThumbnailRepair(fd, pszFileName, pstThumbAttr);
    if (enResult == RKADK_THUMB_REPAIR_FAIL) {
      RKADK_LOGE("file[%s] thumb repair failed", pszFileName);
      ret = -1;
      goto exit;
    } else if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
      if (enResult == RKADK_THUMB_BUILD_IN_FAIL || enResult == RKADK_THUMB_REPAIR_SUCCESS) {
        RKADK_LOGD("file[%s] get jpg thumb done[%d]", pszFileName, enResult);
        ret = 0;
        goto exit;
      }
    } else {
      if (enResult == RKADK_THUMB_BUILD_IN_FAIL) {
        RKADK_LOGD("file[%s] repair thumb build-in failed", pszFileName);
        ret = -1;
        goto exit;
      }
    }
  }

  if (fseek(fd, 0, SEEK_END) || (s64FileSize = ftell(fd)) == -1) {
    RKADK_LOGE("get file[%s] size failed, error[%d, %s]", pszFileName, errno, strerror(errno));
    fclose(fd);
    return -1;
  }

  pFile = (RKADK_U8 *)mmap(NULL, s64FileSize, PROT_READ, MAP_SHARED, fileno(fd), 0);
  if (pFile == MAP_FAILED) {
    RKADK_LOGE("mmap %s failed, errno[%d, %s]", pszFileName, errno, strerror(errno));
    fclose(fd);
    return -1;
  }

  //get specified type thumb
  ret = GetSpecificThmInFile(pFile, s64FileSize, pstThumbAttr, &u64JpgThmPos);
  if (!ret)
    goto exit;

  if (ret && pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
    if (bBuildInThm) {
      RKADK_LOGE("Get jpg thumbnail in %s failed!", pszFileName);
      goto exit;
    }
  }
  RKADK_LOGD("File[%s] get thm%d failed!", pszFileName, pstThumbAttr->enType);

  //get jpg thumb from file
  if (pstThumbAttr->enType != RKADK_THUMB_TYPE_JPEG) {
    memset(&stTmpThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
    stTmpThmAttr.enType = RKADK_THUMB_TYPE_JPEG;
    ret = GetSpecificThmInFile(pFile, s64FileSize, &stTmpThmAttr, &u64JpgThmPos);
    if (ret && bBuildInThm) {
      RKADK_LOGE("Get jpg thumbnail in %s failed!", pszFileName);
      goto exit;
    }
  }

  //get jpg thumb form thumb dir
  if (ret && !bBuildInThm) {
    char thumbPath[RKADK_MAX_FILE_PATH_LEN];

    if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
      ret = ReadThumbJpgFile(pszFileName, thumbPath, RKADK_MAX_FILE_PATH_LEN, pstThumbAttr);
      if (ret)
        RKADK_LOGE("read file[%s] thumb jpg failed", pszFileName);

      goto exit;
    } else {
      memset(&stTmpThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
      stTmpThmAttr.enType = RKADK_THUMB_TYPE_JPEG;
      stTmpThmAttr.u32Width = pstThumbAttr->u32Width;
      stTmpThmAttr.u32Height = pstThumbAttr->u32Height;
      stTmpThmAttr.u32VirWidth = pstThumbAttr->u32VirWidth;
      stTmpThmAttr.u32VirHeight = pstThumbAttr->u32VirHeight;
      stTmpThmAttr.pstThumbParam = pstThumbAttr->pstThumbParam;

      ret = ReadThumbJpgFile(pszFileName, thumbPath, RKADK_MAX_FILE_PATH_LEN, &stTmpThmAttr);
      if (ret) {
        RKADK_LOGE("read file[%s] thumb jpg failed", pszFileName);
        RKADK_ThmBufFree(&stTmpThmAttr);
        goto exit;
      }
    }
  }

  // decode jpg
  ret = ThumbnailJpgDecode(&stTmpThmAttr, pstThumbAttr, &bFree);
  if (!ret) {
    if (bBuildInThm) {
      ret = BuildInThmToFile(fd, pstThumbAttr);
      if (ret)
        RKADK_LOGE("BuildInThm to %s failed", pszFileName);
    }
  }

  if (bFree)
    RKADK_ThmBufFree(&stTmpThmAttr);

exit:
  if (fd)
    fclose(fd);

  if (pFile)
    munmap(pFile, s64FileSize);

  if (stTimebuf.actime != 0 && stTimebuf.modtime != 0) {
    result = utime(pszFileName, &stTimebuf);
    if (result)
      RKADK_LOGW("utime[%s] failed[%d]", pszFileName, result);
  }

  return ret;
}

static RKADK_S32 GetThmInMp4(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                             RKADK_THUMB_ATTR_S *pstThumbAttr) {
  FILE *fd = NULL;
  int ret = 0, result = 0;
  RKADK_THUMB_ATTR_S stTmpThmAttr;
  bool bFree = false;
  RKADK_U64 u64JpgThmPos = 0;
  RKADK_S64 s64FileSize = 0;
  RKADK_U8 *pFile = NULL;
  struct stat stStatBuf;
  struct utimbuf stTimebuf;

  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!pstThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
    return -1;
  }

  if (!pstThumbAttr->u32Width || !pstThumbAttr->u32Height
        || !pstThumbAttr->u32VirWidth || !pstThumbAttr->u32VirHeight) {
    if (pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
      pstThumbAttr->u32Width = pstThumbCfg->thumb_width;
      pstThumbAttr->u32Height = pstThumbCfg->thumb_height;
    } else {
      pstThumbAttr->u32Width = UPALIGNTO(pstThumbCfg->thumb_width, 4);
      pstThumbAttr->u32Height = UPALIGNTO(pstThumbCfg->thumb_height, 2);
    }

    pstThumbAttr->u32VirWidth = pstThumbAttr->u32Width;
    pstThumbAttr->u32VirHeight = pstThumbAttr->u32Height;
  }

  fd = fopen(pszFileName, "r+");
  if (!fd) {
    RKADK_LOGE("open %s failed, error[%d, %s]", pszFileName, errno, strerror(errno));
    return -1;
  }

  if (fseek(fd, 0, SEEK_END) || (s64FileSize = ftell(fd)) == -1) {
    RKADK_LOGE("get file[%s] size failed, error[%d, %s]", pszFileName, errno, strerror(errno));
    fclose(fd);
    return -1;
  }

  pFile = (RKADK_U8 *)mmap(NULL, s64FileSize, PROT_READ, MAP_SHARED, fileno(fd), 0);
  if (pFile == MAP_FAILED) {
    RKADK_LOGE("mmap %s failed, errno[%d, %s]", pszFileName, errno, strerror(errno));
    fclose(fd);
    return -1;
  }

  memset(&stTimebuf, 0, sizeof(struct utimbuf));
  result = stat(pszFileName, &stStatBuf);
  if (result) {
    RKADK_LOGW("stat[%s] failed[%d]", pszFileName, result);
  } else {
    stTimebuf.actime = stStatBuf.st_atime;
    stTimebuf.modtime = stStatBuf.st_mtime;
  }

  //get specified type thumb
  ret = GetSpecificThmInMp4(pFile, s64FileSize, pstThumbAttr, &u64JpgThmPos);
  if (!ret)
    goto exit;

  if (ret && pstThumbAttr->enType == RKADK_THUMB_TYPE_JPEG) {
    RKADK_LOGE("don't find jpg thumb");
    goto exit;
  }

  //get jpg thumb, then decode
  memset(&stTmpThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stTmpThmAttr.enType = RKADK_THUMB_TYPE_JPEG;
  ret = GetSpecificThmInMp4(pFile, s64FileSize, &stTmpThmAttr, &u64JpgThmPos);
  if (ret) {
    RKADK_LOGE("Get jpg thumbnail in %s failed!", pszFileName);
    goto exit;
  }

  ret = ThumbnailJpgDecode(&stTmpThmAttr, pstThumbAttr, &bFree);
  if (!ret) {
    ret = BuildInThmToMp4(fd, s64FileSize, pszFileName, pstThumbAttr);
    if (ret)
      RKADK_LOGE("BuildInThm to %s failed", pszFileName);
  }

  if (bFree)
    RKADK_ThmBufFree(&stTmpThmAttr);

exit:
  if (fd)
    fclose(fd);

  if (pFile)
    munmap(pFile, s64FileSize);

  if (stTimebuf.actime != 0 && stTimebuf.modtime != 0) {
    result = utime(pszFileName, &stTimebuf);
    if (result)
      RKADK_LOGW("utime[%s] failed[%d]", pszFileName, result);
  }

  return ret;
}

RKADK_S32 RKADK_GetThmInMp4(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                            RKADK_U8 *pu8Buf, RKADK_U32 *pu32Size) {
  int ret;
  RKADK_THUMB_ATTR_S stThumbAttr;

  RKADK_CHECK_POINTER(pszFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pu8Buf, RKADK_FAILURE);

  memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));

  /* thm size is unchangable */
  stThumbAttr.u32Width = 0;
  stThumbAttr.u32Height = 0;
  stThumbAttr.u32VirWidth = 0;
  stThumbAttr.u32VirHeight = 0;
  stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stThumbAttr.pu8Buf = pu8Buf;
  stThumbAttr.u32BufSize = *pu32Size;
  stThumbAttr.s32VdecChn = THM_VDEC_CHN;
  if (stThumbAttr.s32VdecChn < 0)
    stThumbAttr.s32VdecChn = 0;
  stThumbAttr.s32VpssGrp = THM_VPSS_GRP;
  stThumbAttr.s32VpssChn = THM_VPSS_CHN;

  ret = GetThmInMp4(u32CamId, pszFileName, &stThumbAttr);
  if (ret) {
    RKADK_LOGE("Get thumbnail in %s failed!", pszFileName);
    return ret;
  }

  *pu32Size = stThumbAttr.u32BufSize;
  return ret;
}

RKADK_S32 RKADK_ThmBufFree(RKADK_THUMB_ATTR_S *pstThumbAttr) {
  return RKADK_MEDIA_FrameFree((RKADK_FRAME_ATTR_S *)pstThumbAttr);
}

RKADK_S32 RKADK_GetThmInMp4Ex(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                              RKADK_THUMB_ATTR_S *pstThumbAttr) {
  int ret;

  RKADK_CHECK_POINTER(pszFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstThumbAttr, RKADK_FAILURE);

  if (pstThumbAttr->s32VdecChn < 0)
    pstThumbAttr->s32VdecChn = THM_VDEC_CHN;

  if (pstThumbAttr->s32VpssGrp < 0)
    pstThumbAttr->s32VpssGrp = THM_VPSS_GRP;

  if (pstThumbAttr->s32VpssChn < 0)
    pstThumbAttr->s32VpssChn = THM_VPSS_CHN;

  ret = GetThmInMp4(u32CamId, pszFileName, pstThumbAttr);
  if (ret)
    RKADK_ThmBufFree(pstThumbAttr);

  return ret;
}


RKADK_S32 RKADK_GetThmInFile(RKADK_U32 u32CamId, RKADK_CHAR *pszFileName,
                              RKADK_THUMB_ATTR_S *pstThumbAttr) {

  int ret;
  const char *suffix = NULL;

  RKADK_CHECK_POINTER(pszFileName, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstThumbAttr, RKADK_FAILURE);

  suffix = strrchr(pszFileName, '.');
  if (!suffix) {
    RKADK_LOGE("pszfilePath[%s] missing a suffix", pszFileName);
    return -1;
  }

  if (strlen(suffix) == 4 && tolower(suffix[1]) == 'm' && tolower(suffix[2]) == 'p' && tolower(suffix[3]) == '4')
    return RKADK_GetThmInMp4Ex(u32CamId, pszFileName, pstThumbAttr);

  if (pstThumbAttr->s32VdecChn < 0)
    pstThumbAttr->s32VdecChn = THM_VDEC_CHN;

  if (pstThumbAttr->s32VpssGrp < 0)
    pstThumbAttr->s32VpssGrp = THM_VPSS_GRP;

  if (pstThumbAttr->s32VpssChn < 0)
    pstThumbAttr->s32VpssChn = THM_VPSS_CHN;

  ret = GetThmInFile(u32CamId, pszFileName, pstThumbAttr);
  if (ret)
    RKADK_ThmBufFree(pstThumbAttr);

  return ret;
}

RKADK_S32 RKADK_THUMB_GetFilePath(char *videoPath, char *thumbDir, char *thumbPath, int thumbPathLen) {
  int totalLen = 0;
  char baseName[RKADK_MAX_FILE_PATH_LEN];

  const char *lastSlash = strrchr(videoPath, '/');
  const char *filename = (lastSlash != NULL) ? (lastSlash + 1) : videoPath;
  const char *lastDot = strrchr(filename, '.');
  int len = (lastDot != NULL) ? (lastDot - filename) : strlen(filename);
  if (len > RKADK_MAX_FILE_PATH_LEN) {
    RKADK_LOGE("len[%d] > baseName buf len[%d]", len, RKADK_MAX_FILE_PATH_LEN);
    return -1;
  }

  memset(baseName, 0, RKADK_MAX_FILE_PATH_LEN);
  strncpy(baseName, filename, len);
  baseName[len] = '\0';

  totalLen = strlen(thumbDir) + strlen(baseName) + 1/* '/' */ + 1/* . */ + 4/* ".jpg" */ + 1/* '\0' */;
  if (totalLen > thumbPathLen) {
    RKADK_LOGE("Path[%s/%s.jpg] too long[%d], max[%d]", thumbDir, baseName, totalLen, thumbPathLen);
    return -1;
  }

  memset(thumbPath, 0, thumbPathLen);
  snprintf(thumbPath, totalLen, "%s/.%s.jpg", thumbDir, baseName);
  RKADK_LOGD("thumb file path: %s", thumbPath);
  return 0;
}

static int ThumbCheckChnAttr(RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr) {
  if (pstThmChnAttr->u32SrcWidth <= 0 || pstThmChnAttr->u32SrcHeight <= 0) {
    RKADK_LOGE("invalid src width[%d] or height[%d]", pstThmChnAttr->u32SrcWidth,
                pstThmChnAttr->u32SrcHeight);
    return -1;
  }

  if (pstThmChnAttr->u32DstWidth <= 0 || pstThmChnAttr->u32DstHeight <= 0) {
    RKADK_LOGE("invalid dst width[%d] or height[%d]", pstThmChnAttr->u32DstWidth,
                pstThmChnAttr->u32DstHeight);
    return -1;
  }

  if (pstThmChnAttr->s32VdecChn < 0 || pstThmChnAttr->s32VpssGrp < 0 || pstThmChnAttr->s32VpssChn < 0) {
    RKADK_LOGE("invalid vdec_chn[%d] or vpss_grp[%d] or vpss_chn[%d]", pstThmChnAttr->s32VdecChn, pstThmChnAttr->s32VpssGrp, pstThmChnAttr->s32VpssChn);
    return -1;
  }
  return 0;
}

RKADK_S32 GetThmChnCreate(RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr) {
  int ret = 0;
  MPP_CHN_S stVdecChn, stVpssChn;
  VDEC_CHN_ATTR_S stAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  RKADK_THUMB_ATTR_S stSrcThmAttr, stDstThmAttr;

  if (ThumbCheckChnAttr(pstThmChnAttr))
    return -1;

  stVdecChn.enModId = RK_ID_VDEC;
  stVdecChn.s32DevId = 0;
  stVdecChn.s32ChnId = pstThmChnAttr->s32VdecChn;

  stVpssChn.enModId = RK_ID_VPSS;
  stVpssChn.s32DevId = pstThmChnAttr->s32VpssGrp;
  stVpssChn.s32ChnId = pstThmChnAttr->s32VpssChn;

  memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));

  if (pstThmChnAttr->enDstType != RKADK_THUMB_TYPE_JPEG) {
    stAttr.enMode = VIDEO_MODE_FRAME;
    stAttr.enType = RK_VIDEO_ID_JPEG;
    stAttr.u32PicWidth = pstThmChnAttr->u32SrcWidth;
    stAttr.u32PicHeight = pstThmChnAttr->u32SrcHeight;
    stAttr.u32FrameBufCnt = 1;
    stAttr.u32StreamBufCnt = 1;
    stAttr.u32FrameBufDepth = 1;
    ret = RK_MPI_VDEC_CreateChn(stVdecChn.s32ChnId, &stAttr);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("create vdec[%d] failed[%x]", stVdecChn.s32ChnId, ret);
      return ret;
    }

    stVdecParam.enType = RK_VIDEO_ID_JPEG;
    stVdecParam.stVdecPictureParam.enPixelFormat = RK_FMT_YUV420SP;
    ret = RK_MPI_VDEC_SetChnParam(stVdecChn.s32ChnId, &stVdecParam);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("set vdec chn[%d] param failed[%x]", stVdecChn.s32ChnId, ret);
      RK_MPI_VDEC_DestroyChn(stVdecChn.s32ChnId);
      return ret;
    }

    ret = RK_MPI_VDEC_StartRecvStream(stVdecChn.s32ChnId);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("start recv vdec[%d] failed[%x]", stVdecChn.s32ChnId, ret);
      RK_MPI_VDEC_DestroyChn(stVdecChn.s32ChnId);
      return ret;
    }

    RKADK_LOGD("create vdec_chn[%d] success", stVdecChn.s32ChnId);
  } else {
    RKADK_LOGD("src isn't jpg, don't create vdec");
  }

  if (pstThmChnAttr->enDstType == RKADK_THUMB_TYPE_JPEG) {
    RKADK_LOGD("dts is jpg type, don't create vpss");
    return 0;
  }

  if (pstThmChnAttr->enDstType == RKADK_THUMB_TYPE_NV12
    && pstThmChnAttr->u32SrcWidth == pstThmChnAttr->u32DstWidth
    && pstThmChnAttr->u32SrcHeight == pstThmChnAttr->u32DstHeight) {
    RKADK_LOGD("src and dst attr is same, don't create vpss");
    return 0;
  }

  memset(&stSrcThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stSrcThmAttr.u32Width = pstThmChnAttr->u32SrcWidth;
  stSrcThmAttr.u32Height = pstThmChnAttr->u32SrcHeight;
  stSrcThmAttr.u32VirWidth = UPALIGNTO(pstThmChnAttr->u32SrcWidth, 4);
  stSrcThmAttr.u32VirHeight = UPALIGNTO(pstThmChnAttr->u32SrcHeight, 2);
  stSrcThmAttr.enType = RKADK_THUMB_TYPE_JPEG;
  stSrcThmAttr.s32VdecChn = stVdecChn.s32ChnId;
  stSrcThmAttr.s32VpssGrp = stVpssChn.s32DevId;
  stSrcThmAttr.s32VpssChn = stVpssChn.s32ChnId;

  memset(&stDstThmAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
  stDstThmAttr.u32Width = pstThmChnAttr->u32DstWidth;
  stDstThmAttr.u32Height = pstThmChnAttr->u32DstHeight;
  stDstThmAttr.u32VirWidth = UPALIGNTO(pstThmChnAttr->u32DstWidth, 4);
  stDstThmAttr.u32VirHeight = UPALIGNTO(pstThmChnAttr->u32DstHeight, 2);
  stDstThmAttr.enType = pstThmChnAttr->enDstType;
  stDstThmAttr.s32VdecChn = stVdecChn.s32ChnId;
  stDstThmAttr.s32VpssGrp = stVpssChn.s32DevId;
  stDstThmAttr.s32VpssChn = stVpssChn.s32ChnId;
  ret = ThumbVpssInit(stVpssChn, &stSrcThmAttr, &stDstThmAttr);
  if (ret) {
    RKADK_LOGE("ThumbVpssInit vpss_grp[%d] vpss_chn[%d] falied[%x]",
                stVpssChn.s32DevId, stVpssChn.s32ChnId, ret);
    RKADK_MPI_VPSS_DeInit(stVpssChn.s32DevId, stVpssChn.s32ChnId);
    RK_MPI_VDEC_StopRecvStream(stVdecChn.s32ChnId);
    RK_MPI_VDEC_DestroyChn(stVdecChn.s32ChnId);
    return ret;
  }

  RKADK_LOGD("create vpss_grp[%d] vpss_chn[%d] success", stVpssChn.s32DevId, stVpssChn.s32ChnId);
  return 0;
}

RKADK_S32 GetThmChnDestory(RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr) {
  int ret = 0;
  MPP_CHN_S stVdecChn, stVpssChn;

  if (ThumbCheckChnAttr(pstThmChnAttr))
    return -1;

  stVdecChn.enModId = RK_ID_VDEC;
  stVdecChn.s32DevId = 0;
  stVdecChn.s32ChnId = pstThmChnAttr->s32VdecChn;

  stVpssChn.enModId = RK_ID_VPSS;
  stVpssChn.s32DevId = pstThmChnAttr->s32VpssGrp;
  stVpssChn.s32ChnId = pstThmChnAttr->s32VpssChn;

  if ((pstThmChnAttr->enDstType == RKADK_THUMB_TYPE_NV12
    && pstThmChnAttr->u32SrcWidth == pstThmChnAttr->u32DstWidth
    && pstThmChnAttr->u32SrcHeight == pstThmChnAttr->u32DstHeight)
    || (pstThmChnAttr->enDstType == RKADK_THUMB_TYPE_JPEG)) {
    RKADK_LOGD("don't create vpss, don't need destory");
  } else {
    ret = RKADK_MPI_VPSS_DeInit(stVpssChn.s32DevId, stVpssChn.s32ChnId);
    if (ret)
      RKADK_LOGE("destroy vpss[%d] vpss_chn[%d] failed[%x]", stVpssChn.s32DevId, stVpssChn.s32ChnId, ret);
  }

  if (pstThmChnAttr->enDstType != RKADK_THUMB_TYPE_JPEG) {
    ret = RK_MPI_VDEC_StopRecvStream(stVdecChn.s32ChnId);
    if (ret)
      RKADK_LOGE("stop vdec[%d] failed[%x]", stVdecChn.s32ChnId, ret);

    ret = RK_MPI_VDEC_DestroyChn(stVdecChn.s32ChnId);
    if (ret)
      RKADK_LOGE("destroy vdec[%d] failed[%x]", stVdecChn.s32ChnId, ret);
    else
      RKADK_LOGD("destroy vdec[%d] success", stVdecChn.s32ChnId);
  } else {
    RKADK_LOGD("don't create vdec, don't need destory");
  }

  return ret;
}

RKADK_S32 RKADK_GetThmChnCreate(RKADK_U32 u32CamId, RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr) {
  RKADK_CHECK_POINTER(pstThmChnAttr, RKADK_FAILURE);

  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!pstThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg[%d] failed", u32CamId);
    return -1;
  }

  if (pstThmChnAttr->u32SrcWidth <= 0 || pstThmChnAttr->u32SrcHeight <= 0) {
    RKADK_LOGI("invalid src resolution[%d, %d], use default[%d, %d]", pstThmChnAttr->u32SrcWidth,
                pstThmChnAttr->u32SrcHeight, pstThumbCfg->thumb_width, pstThumbCfg->thumb_height);
    pstThmChnAttr->u32SrcWidth = pstThumbCfg->thumb_width;
    pstThmChnAttr->u32SrcHeight = pstThumbCfg->thumb_height;
  }

  if (pstThmChnAttr->u32DstWidth <= 0 || pstThmChnAttr->u32DstHeight <= 0) {
    RKADK_LOGI("invalid dst width[%d, %d], use default[%d, %d]", pstThmChnAttr->u32DstWidth,
                pstThmChnAttr->u32DstHeight, pstThumbCfg->thumb_width, pstThumbCfg->thumb_height);
    pstThmChnAttr->u32DstWidth = pstThumbCfg->thumb_width;
    pstThmChnAttr->u32DstHeight = pstThumbCfg->thumb_height;
  }

  if (pstThmChnAttr->s32VdecChn < 0)
    pstThmChnAttr->s32VdecChn = THM_VDEC_CHN;

  if (pstThmChnAttr->s32VpssGrp < 0)
    pstThmChnAttr->s32VpssGrp = THM_VPSS_GRP;

  if (pstThmChnAttr->s32VpssChn < 0)
    pstThmChnAttr->s32VpssChn = THM_VPSS_CHN;

  return GetThmChnCreate(pstThmChnAttr);
}

RKADK_S32 RKADK_GetThmChnDestory(RKADK_U32 u32CamId, RKADK_THUMB_CHN_ATTR_S *pstThmChnAttr) {
  RKADK_CHECK_POINTER(pstThmChnAttr, RKADK_FAILURE);

  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = RKADK_PARAM_GetThumbCfg(u32CamId);
  if (!pstThumbCfg) {
    RKADK_LOGE("RKADK_PARAM_GetThumbCfg[%d] failed", u32CamId);
    return -1;
  }

  if (pstThmChnAttr->u32SrcWidth <= 0 || pstThmChnAttr->u32SrcHeight <= 0) {
    RKADK_LOGI("invalid src resolution[%d, %d], use default[%d, %d]", pstThmChnAttr->u32SrcWidth,
                pstThmChnAttr->u32SrcHeight, pstThumbCfg->thumb_width, pstThumbCfg->thumb_height);
    pstThmChnAttr->u32SrcWidth = pstThumbCfg->thumb_width;
    pstThmChnAttr->u32SrcHeight = pstThumbCfg->thumb_height;
  }

  if (pstThmChnAttr->u32DstWidth <= 0 || pstThmChnAttr->u32DstHeight <= 0) {
    RKADK_LOGE("invalid dst width[%d, %d], use default[%d, %d]", pstThmChnAttr->u32DstWidth,
                pstThmChnAttr->u32DstHeight, pstThumbCfg->thumb_width, pstThumbCfg->thumb_height);
    pstThmChnAttr->u32DstWidth = pstThumbCfg->thumb_width;
    pstThmChnAttr->u32DstHeight = pstThumbCfg->thumb_height;
  }

  if (pstThmChnAttr->s32VdecChn < 0)
    pstThmChnAttr->s32VdecChn = THM_VDEC_CHN;

  if (pstThmChnAttr->s32VpssGrp < 0)
    pstThmChnAttr->s32VpssGrp = THM_VPSS_GRP;

  if (pstThmChnAttr->s32VpssChn < 0)
    pstThmChnAttr->s32VpssChn = THM_VPSS_CHN;

  return GetThmChnDestory(pstThmChnAttr);
}