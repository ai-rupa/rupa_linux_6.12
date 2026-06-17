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

#include "camera_control.h"
#include "isp.h"
#include "param.h"
#include "uvc_log.h"
#include "face_detector.h"

extern char *rkuvc_iq_file_path_;

static void *uvc_camera(void *arg) {}

void camera_control_init() {
  LOG_INFO("%s \n", __func__);
#ifdef RK_UVC_ENABLE_FASTBOOT
  if (rk_param_get_int("video.source:enable_aiq", 1)) {
    rk_isp_fastboot_init(0);
    rk_isp_set_frame_rate(0, rk_param_get_int("isp.0.adjustment:fps", 30));

    if (rk_param_get_int("video.source:enable_2uvc", 0)) {
      rk_isp_fastboot_init(1);
    }
  }
#endif
}

void camera_control_deinit() {
  LOG_INFO("%s \n", __func__);
#ifdef RK_UVC_ENABLE_FASTBOOT
  if (rk_param_get_int("video.source:enable_aiq", 1)) {
    rk_isp_fastboot_deinit(0);
    if (rk_param_get_int("video.source:enable_2uvc", 0)) {
      rk_isp_fastboot_deinit(1);
    }
  }
#endif
}

int camera_control_start(int id, int width, int height, int fps, int format,
                         int eptz) {

   LOG_INFO("%s: id:%d\n", __func__, id);
#ifndef RK_UVC_ENABLE_FASTBOOT
  if (rk_param_get_int("video.source:enable_aiq", 1)) {
    rk_isp_init(id, rkuvc_iq_file_path_);
    rk_isp_set_frame_rate(id, rk_param_get_int("isp.0.adjustment:fps", 30));
  }
#endif
  return 0;
}

void camera_control_stop(int id) {
   LOG_INFO("%s: id:%d\n", __func__, id);
#ifndef RK_UVC_ENABLE_FASTBOOT
   if (rk_param_get_int("video.source:enable_aiq", 1))
     rk_isp_deinit(id);
#endif
}

void camera_control_set_zoom(int val) { LOG_INFO("set_zoom:%d\n", val); }

void camera_control_set_pan(int val) { LOG_INFO("set_pan:%d\n", val); }

void camera_control_set_tilt(int val) { LOG_INFO("set_tilt:%d\n", val); }

void camera_control_set_roll(int val) { LOG_INFO("set_roll:%d\n", val); }

void camera_control_set_asd_win(int cam_id, FaceInfo *face_info) {
  rk_isp_asd_set_face_win(cam_id, face_info);
}