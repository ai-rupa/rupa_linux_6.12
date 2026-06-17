/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "osd.h"
#include "osd_rockchip_logo.h"
#include "uvc_log.h"
#include "uvc_mpi_config.h"
#include "uvc_video.h"
#include "uvc_mpi_vpss.h"

#define UPALIGNTO(value, align) ((value + align - 1) & (~(align - 1)))
#define UPALIGNTO2(value) UPALIGNTO(value, 2)
#define RED_COLOR 0x0000FF
#define BLUE_COLOR 0xFF0000
#define WHILTE_COLOR 0xFFFFFF
#define BLACK_COLOR 0x000000

typedef enum rkCOLOR_INDEX_E {
	RGN_COLOR_LUT_INDEX_0 = 0,
	RGN_COLOR_LUT_INDEX_1 = 1,
} COLOR_INDEX_E;

static RK_U8 rgn_color_lut_0_left_value[4] = {0x03, 0xf, 0x3f, 0xff};
static RK_U8 rgn_color_lut_0_right_value[4] = {0xc0, 0xf0, 0xfc, 0xff};
static RK_U8 rgn_color_lut_1_left_value[4] = {0x02, 0xa, 0x2a, 0xaa};
static RK_U8 rgn_color_lut_1_right_value[4] = {0x80, 0xa0, 0xa8, 0xaa};
RK_S32 draw_rect_2bpp(RK_U8 *buffer, RK_U32 width, RK_U32 height, int rgn_x, int rgn_y, int rgn_w,
                      int rgn_h, int line_pixel, COLOR_INDEX_E color_index) {
	int i;
	RK_U8 *ptr = buffer;
	RK_U8 value = 0;
	if (color_index == RGN_COLOR_LUT_INDEX_0)
		value = 0xff;
	if (color_index == RGN_COLOR_LUT_INDEX_1)
		value = 0xaa;

	if (line_pixel > 4) {
		printf("line_pixel %d > 4, not support\n", line_pixel);
		return -1;
	}

	// printf("YUV %dx%d, rgn (%d,%d,%d,%d), line pixel %d\n", width, height, rgn_x, rgn_y, rgn_w,
	// rgn_h, line_pixel); draw top line
	ptr += (width * rgn_y + rgn_x) >> 2;
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, value, (rgn_w + 3) >> 2);
		ptr += width >> 2;
	}
	// draw letft/right line
	for (i = 0; i < (rgn_h - line_pixel * 2); i++) {
		if (color_index == RGN_COLOR_LUT_INDEX_1) {
			*ptr = rgn_color_lut_1_left_value[line_pixel - 1];
			*(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_1_right_value[line_pixel - 1];
		} else {
			*ptr = rgn_color_lut_0_left_value[line_pixel - 1];
			*(ptr + ((rgn_w + 3) >> 2)) = rgn_color_lut_0_right_value[line_pixel - 1];
		}
		ptr += width >> 2;
	}
	// draw bottom line
	for (i = 0; i < line_pixel; i++) {
		memset(ptr, value, (rgn_w + 3) >> 2);
		ptr += width >> 2;
	}
	return 0;
}

static RK_S32 osd_create_2bpp(RGN_HANDLE handle, MPP_CHN_S *mppChn, RK_U32 layer, int width, int height) {
	LOG_DEBUG("start\n");
	int ret = 0;
	RGN_HANDLE RgnHandle = handle;
	RGN_ATTR_S stRgnAttr;
	MPP_CHN_S stMppChn;
	RGN_CHN_ATTR_S stRgnChnAttr;
	BITMAP_S stBitmap;
	int rotation = rk_param_get_int("video.source:rotation", 0);

	// create overlay regions
	memset(&stRgnAttr, 0, sizeof(stRgnAttr));
	stRgnAttr.enType = OVERLAY_RGN;
	stRgnAttr.unAttr.stOverlay.enPixelFmt = RK_FMT_2BPP;
	stRgnAttr.unAttr.stOverlay.u32CanvasNum = 1;
	if (rotation == 90 || rotation == 270) {
		stRgnAttr.unAttr.stOverlay.stSize.u32Width = height;
		stRgnAttr.unAttr.stOverlay.stSize.u32Height = width;
	} else {
		stRgnAttr.unAttr.stOverlay.stSize.u32Width = width;
		stRgnAttr.unAttr.stOverlay.stSize.u32Height = height;
	}
	ret = RK_MPI_RGN_Create(RgnHandle, &stRgnAttr);
	if (RK_SUCCESS != ret) {
		LOG_ERROR("RK_MPI_RGN_Create (%d) failed with %#x\n", RgnHandle, ret);
		RK_MPI_RGN_Destroy(RgnHandle);
		return RK_FAILURE;
	}
	LOG_DEBUG("The handle: %d, create success\n", RgnHandle);
	// after malloc max size, it needs to be set to the actual size
	if (rotation == 90 || rotation == 270) {
		stRgnAttr.unAttr.stOverlay.stSize.u32Width = height;
		stRgnAttr.unAttr.stOverlay.stSize.u32Height = width;
	} else {
		stRgnAttr.unAttr.stOverlay.stSize.u32Width = width;
		stRgnAttr.unAttr.stOverlay.stSize.u32Height = height;
	}
	ret = RK_MPI_RGN_SetAttr(RgnHandle, &stRgnAttr);
	if (RK_SUCCESS != ret) {
		LOG_ERROR("RK_MPI_RGN_SetAttr (%d) failed with %#x!", RgnHandle, ret);
		return RK_FAILURE;
	}

	// display overlay regions to venc groups
	memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
	stRgnChnAttr.bShow = RK_TRUE;
	stRgnChnAttr.enType = OVERLAY_RGN;
	stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 0;
	stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 0;
	stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
	stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
	stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = layer;
	stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_0] = BLUE_COLOR;
	stRgnChnAttr.unChnAttr.stOverlayChn.u32ColorLUT[RGN_COLOR_LUT_INDEX_1] = RED_COLOR;

	ret = RK_MPI_RGN_AttachToChn(RgnHandle, mppChn, &stRgnChnAttr);
	if (RK_SUCCESS != ret) {
		LOG_ERROR("RK_MPI_RGN_AttachToChn (%d) failed with %#x\n", RgnHandle, ret);
		return RK_FAILURE;
	}
	LOG_DEBUG("end\n");

	return ret;

}
static RK_S32 osd_create(RGN_HANDLE handle, MPP_CHN_S *mppChn, RK_U32 layer,
                         RECT_S *rect) {
  RK_S32 s32Ret = RK_SUCCESS;
  RGN_ATTR_S stRgnAttr;
  RGN_CHN_ATTR_S stRgnChnAttr;
  if (mppChn->enModId == RK_ID_VPSS)
    stRgnAttr.enType = OVERLAY_EX_RGN;
  else
    stRgnAttr.enType = OVERLAY_RGN;
  stRgnAttr.unAttr.stOverlay.enPixelFmt = RK_FMT_RGBA8888;
  stRgnAttr.unAttr.stOverlay.stSize.u32Width = rect->u32Width;
  stRgnAttr.unAttr.stOverlay.stSize.u32Height = rect->u32Height;
  stRgnAttr.unAttr.stOverlay.u32ClutNum = 0;

  s32Ret = RK_MPI_RGN_Create(handle, &stRgnAttr);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_Create (%d) failed with %#x!\n", handle, s32Ret);
    RK_MPI_RGN_Destroy(handle);
    return s32Ret;
  }
  memset(&stRgnChnAttr, 0, sizeof(stRgnChnAttr));
  stRgnChnAttr.bShow = RK_TRUE;
  stRgnChnAttr.enType = stRgnAttr.enType;
  stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = rect->s32X;
  stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = rect->s32Y;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
  stRgnChnAttr.unChnAttr.stOverlayChn.u32Layer = layer;
  stRgnChnAttr.unChnAttr.stOverlayChn.stQpInfo.bEnable = RK_FALSE;

  s32Ret = RK_MPI_RGN_AttachToChn(handle, mppChn, &stRgnChnAttr);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_AttachToChn (%d) failed with %#x!\n", handle, s32Ret);
  }

  return s32Ret;
}

static RK_S32 osd_destroy(RGN_HANDLE handle, MPP_CHN_S *mppChn) {
  RK_S32 s32Ret = RK_SUCCESS;

  s32Ret = RK_MPI_RGN_DetachFromChn(handle, mppChn);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_DetachFrmChn (%d) failed with %#x!\n", handle,
              s32Ret);
  }
  s32Ret = RK_MPI_RGN_Destroy(handle);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_Destroy [%d] failed with %#x\n", handle, s32Ret);
  }

  return s32Ret;
}

static RK_S32 osd_show_or_hide(RGN_HANDLE handle, const MPP_CHN_S *mppChn,
                               RK_BOOL bShow) {
  RGN_CHN_ATTR_S stChnAttr;
  RK_S32 s32Ret = RK_SUCCESS;

  if (RK_NULL == mppChn) {
    LOG_ERROR("input parameter is null. it is invaild!\n");
    return RK_FAILURE;
  }

  s32Ret = RK_MPI_RGN_GetDisplayAttr(handle, mppChn, &stChnAttr);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_GetDisplayAttr (%d)) failed with %#x!\n", handle,
              s32Ret);
    return RK_FAILURE;
  }

  stChnAttr.bShow = bShow;

  s32Ret = RK_MPI_RGN_SetDisplayAttr(handle, mppChn, &stChnAttr);
  if (RK_SUCCESS != s32Ret) {
    LOG_ERROR("RK_MPI_RGN_SetDisplayAttr (%d)) failed with %#x!\n", handle,
              s32Ret);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

static RK_S32 osd_test_show_rockchip_logo(RGN_HANDLE handle) {
  RK_S32 s32Ret = RK_SUCCESS;
  RGN_CANVAS_INFO_S stCanvasInfo;
  RK_U32 *data;
  FILE *p;

  LOG_ERROR("test show rockchip logo\n");
  memset(&stCanvasInfo, 0, sizeof(RGN_CANVAS_INFO_S));

  s32Ret = RK_MPI_RGN_GetCanvasInfo(handle, &stCanvasInfo);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_RGN_GetCanvasInfo failed with %#x!\n", s32Ret);
  }

  data = (RK_U32 *)stCanvasInfo.u64VirAddr;
  p = fopen("/data/rockchip-logo-320-96.rgba", "r");
  if (p) {
    fread(data, 4, stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight, p);
    fclose(p);
  } else {
// no osd file, copy array to rgn
#if 1 // test rgba data to set 0
    memcpy(data, u32BGRA8888RKLOGO,
           stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight * 4);
#else // test rgba data
    for (RK_U32 i = 0; i < stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight;
         i++) {
      // RK_FMT_RGBA8888
      if (i < stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight / 4)
        data[i] = 0xff000000; // black
      else if (i < stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight / 2)
        data[i] = 0xffff0000; // blue
      else if (i < stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight / 4 * 3)
        data[i] = 0xff00ff00; // green
      else
        data[i] = 0xff0000ff; // red
    }
#endif
  }

  s32Ret = RK_MPI_RGN_UpdateCanvas(handle);
  if (s32Ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_RGN_UpdateCanvas failed with %#x!\n", s32Ret);
  }

  return s32Ret;
}

static RK_S32 osd_test_init(UVC_MPI_CFG *uvcCfg) {
  RGN_HANDLE handle = 3 *uvcCfg->vi_cfg->dev_id;
  RGN_HANDLE handle_2bpp = 1 + 3 * uvcCfg->vi_cfg->dev_id;
  RGN_HANDLE handle_asd = 2 + 3 * uvcCfg->vi_cfg->dev_id;
  MPP_CHN_S mppChn;
  RK_U32 layer = 0;
  RK_U32 layer_2bpp = 1;
  RK_S32 layer_asd = 2;
  RECT_S rect;
  RK_S32 s32Ret;

  if (uvcCfg->uvc_cfg.fcc == V4L2_PIX_FMT_YUYV ||
      uvcCfg->osd_cfg.force_use_vpss) {
    mppChn.enModId = RK_ID_VPSS;
    mppChn.s32DevId = uvcCfg->vpss_cfg[MPI_VPSS_CHANNEL_TYPE_UVC].group_id;
    mppChn.s32ChnId = uvcCfg->vpss_cfg[MPI_VPSS_CHANNEL_TYPE_UVC].channel_id;
  } else {
    mppChn.enModId = RK_ID_VENC;
    mppChn.s32DevId = 0;
    mppChn.s32ChnId = uvcCfg->venc_cfg.common_cfg.channel_id;
  }
  rect.s32X = 100;
  rect.s32Y = 200;
  rect.u32Width = 320; // need align 16
  rect.u32Height = 96; // need align 16

  s32Ret = osd_create(handle, &mppChn, layer, &rect);
  s32Ret = osd_test_show_rockchip_logo(handle);
  // defalut is show, if want hide this osd, refer to below setting
  // osd_show_or_hide(handle, &mppChn, RK_FALSE);
  s32Ret = osd_create_2bpp(handle_2bpp, &mppChn, layer_2bpp, uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height);
  s32Ret = osd_create_2bpp(handle_asd, &mppChn, layer_asd, uvcCfg->uvc_cfg.width, uvcCfg->uvc_cfg.height);

  return s32Ret;
}

static RK_S32 osd_test_deinit(UVC_MPI_CFG *uvcCfg) {
  RK_S32 s32Ret;
  MPP_CHN_S mppChn;
  RGN_HANDLE handle = 3 *uvcCfg->vi_cfg->dev_id;
  RGN_HANDLE handle_2bpp = 1 + 3 * uvcCfg->vi_cfg->dev_id;
  RGN_HANDLE handle_asd = 2 + 3 * uvcCfg->vi_cfg->dev_id;
  if (uvcCfg->uvc_cfg.fcc == V4L2_PIX_FMT_YUYV ||
      uvcCfg->osd_cfg.force_use_vpss) {
    mppChn.enModId = RK_ID_VPSS;
    mppChn.s32DevId = uvcCfg->vpss_cfg[MPI_VPSS_CHANNEL_TYPE_UVC].group_id;
    mppChn.s32ChnId = uvcCfg->vpss_cfg[MPI_VPSS_CHANNEL_TYPE_UVC].channel_id;
  } else {
    mppChn.enModId = RK_ID_VENC;
    mppChn.s32DevId = 0;
    mppChn.s32ChnId = uvcCfg->venc_cfg.common_cfg.channel_id;
  }

  s32Ret = osd_destroy(handle, &mppChn);
  s32Ret |= osd_destroy(handle_2bpp, &mppChn);
  s32Ret |= osd_destroy(handle_asd, &mppChn);
  return s32Ret;
}

static RK_S32 osd_draw_nn_osd_internal(UVC_MPI_CFG *uvcCfg, FaceInfo *faceinfo, 
                                       RK_U32 handle_offset, COLOR_INDEX_E color_index) {
  RGN_HANDLE RgnHandle = handle_offset + 3 * uvcCfg->vi_cfg->dev_id;
  RGN_CANVAS_INFO_S stCanvasInfo;
  RK_S32 ret = RK_SUCCESS;
  RK_U32 video_width = uvcCfg->uvc_cfg.width;
  RK_U32 video_height = uvcCfg->uvc_cfg.height;
  VPSS_CROP_INFO_S stCropInfo;
  RK_S32 crop_x = 0, crop_y = 0;
  RK_U32 crop_w = 0, crop_h = 0;

  memset(&stCanvasInfo, 0, sizeof(RGN_CANVAS_INFO_S));
  ret = RK_MPI_RGN_GetCanvasInfo(RgnHandle, &stCanvasInfo);
  if (ret != RK_SUCCESS) {
    LOG_DEBUG("RK_MPI_RGN_GetCanvasInfo failed with %#x!", ret);
    return ret;
  }
  if ((stCanvasInfo.stSize.u32Width != UPALIGNTO2(video_width)) ||
      (stCanvasInfo.stSize.u32Height != UPALIGNTO2(video_height))) {
    LOG_ERROR(
        "canvas is %d*%d, not equal %d*%d, maybe in the process of switching,"
        "skip this time\n",
        stCanvasInfo.stSize.u32Width, stCanvasInfo.stSize.u32Height,
        UPALIGNTO2(video_width), UPALIGNTO2(video_height));
    return RK_FAILURE;
  }

  memset((void *)stCanvasInfo.u64VirAddr, 0,
         stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight >> 2);

  if ((uvcCfg->common_cfg.uvc_enable_vpss) &&
      uvc_vpss_get_crop(*uvcCfg, &stCropInfo, MPI_VPSS_CHANNEL_TYPE_UVC) ==
          RK_SUCCESS) {
    crop_x = stCropInfo.stCropRect.s32X;
    crop_y = stCropInfo.stCropRect.s32Y;
    crop_w = stCropInfo.stCropRect.u32Width;
    crop_h = stCropInfo.stCropRect.u32Height;
  }

  for (int i = 0; i < faceinfo->count; i++) {
    int x = video_width * faceinfo->boxes[i].x / 10000;
    int y = video_height * faceinfo->boxes[i].y / 10000;
    int w = video_width * faceinfo->boxes[i].w / 10000;
    int h = video_height * faceinfo->boxes[i].h / 10000;

    if (crop_w > 0 && crop_h > 0) {
      x = (x - crop_x) * video_width / crop_w;
      y = (y - crop_y) * video_height / crop_h;
      w = w * video_width / crop_w;
      h = h * video_height / crop_h;
    }

    x = x / 16 * 16;
    y = y / 16 * 16;
    w = (w + 3) / 16 * 16;
    h = (h + 3) / 16 * 16;
    int line_pixel = 2;

    while (x + w + line_pixel >= video_width) {
      w -= 8;
    }
    while (y + h + line_pixel >= video_height) {
      h -= 8;
    }
    if (x < 0 || y < 0 || w <= 0 || h <= 0) {
      LOG_ERROR("x:%d, y:%d, w:%d, h:%d is invalid\n", x, y, w, h);
      LOG_ERROR("video_width:%d, video_height:%d\n", video_width, video_height);
      return RK_FAILURE;
    }

    draw_rect_2bpp((RK_U8 *)stCanvasInfo.u64VirAddr, stCanvasInfo.u32VirWidth,
                   stCanvasInfo.u32VirHeight, x, y, w, h, line_pixel,
                   color_index);
  }

  ret = RK_MPI_RGN_UpdateCanvas(RgnHandle);
  if (ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_RGN_UpdateCanvas failed with %#x!", ret);
    return ret;
  }
  return RK_SUCCESS;
}

RK_S32 osd_draw_nn_osd(UVC_MPI_CFG *uvcCfg, FaceInfo *faceinfo) {
  return osd_draw_nn_osd_internal(uvcCfg, faceinfo, 1, RGN_COLOR_LUT_INDEX_0);
}

RK_S32 osd_draw_nn_osd_asd(UVC_MPI_CFG *uvcCfg, FaceInfo *faceinfo) {
  return osd_draw_nn_osd_internal(uvcCfg, faceinfo, 2, RGN_COLOR_LUT_INDEX_1);
}

static RK_S32 osd_clean_nn_osd_internal(UVC_MPI_CFG *uvcCfg, RK_U32 handle_offset) {
  RGN_HANDLE RgnHandle = handle_offset + 3 * uvcCfg->vi_cfg->dev_id;
  RGN_CANVAS_INFO_S stCanvasInfo;
  RK_S32 ret = RK_SUCCESS;
  RK_U32 video_width = uvcCfg->uvc_cfg.width;
  RK_U32 video_height = uvcCfg->uvc_cfg.height;
  memset(&stCanvasInfo, 0, sizeof(RGN_CANVAS_INFO_S));
  ret = RK_MPI_RGN_GetCanvasInfo(RgnHandle, &stCanvasInfo);
  if (ret != RK_SUCCESS) {
    LOG_ERROR("RK_MPI_RGN_GetCanvasInfo failed with %#x!", ret);
    return ret;
  }

  if ((stCanvasInfo.stSize.u32Width != UPALIGNTO2(video_width)) ||
      (stCanvasInfo.stSize.u32Height != UPALIGNTO2(video_height))) {
    LOG_ERROR("canvas is %d*%d, not equal %d*%d, maybe in the process of switching,"
             "skip this time\n",
             stCanvasInfo.stSize.u32Width, stCanvasInfo.stSize.u32Height,
             UPALIGNTO2(video_width), UPALIGNTO2(video_height));
    return RK_FAILURE;
  }

  memset((void *)stCanvasInfo.u64VirAddr, 0,
         stCanvasInfo.u32VirWidth * stCanvasInfo.u32VirHeight >> 2);

  ret = RK_MPI_RGN_UpdateCanvas(RgnHandle);
  if (ret != RK_SUCCESS)
  {
    LOG_ERROR("RK_MPI_RGN_UpdateCanvas failed with %#x!", ret);
    return ret;
  }
  return RK_SUCCESS;
}

RK_S32 osd_clean_nn_osd(UVC_MPI_CFG *uvcCfg) {
  return osd_clean_nn_osd_internal(uvcCfg, 1);
}

RK_S32 osd_clean_nn_osd_asd(UVC_MPI_CFG *uvcCfg) {
  return osd_clean_nn_osd_internal(uvcCfg, 2);
}
RK_S32 osd_start(UVC_MPI_CFG *uvcCfg) {
  RK_S32 s32Ret;

  // now just for test run
  s32Ret = osd_test_init(uvcCfg);

  // TODO(user) you can comment the code above, and implement it yourself
  return s32Ret;
}

RK_S32 osd_stop(UVC_MPI_CFG *uvcCfg) {
  RK_S32 s32Ret;

  // now just for test run
  s32Ret = osd_test_deinit(uvcCfg);

  // TODO(user) you can comment the code above, and implement it yourself
  return s32Ret;
}