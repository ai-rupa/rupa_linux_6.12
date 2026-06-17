/****************************************************************************
 *
 *    Copyright (c) 2025 by Rockchip Corp.  All rights reserved.
 *    Authors: ISP Algo-Team
 *
 *    The material in this file is confidential and contains trade secrets
 *    of Rockchip Corporation. This is proprietary information owned by
 *    Rockchip Corporation. No part of this work may be disclosed,
 *    reproduced, copied, transmitted, or used in any way for any purpose,
 *    without the express written permission of Rockchip Corporation.
 *
 *****************************************************************************/
#ifndef __LIB_FACE_API_H__
#define __LIB_FACE_API_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FACE_NN_VERSION "version: V2.2"

/*
 * usage:

	//init
	float threshold = 0.6;
	void *ap_ctx = face_net_init(model_name, threshold);

	//run
	FaceResult result;
	FaceTrackerResult track_result;
	FaceImageFomat fmt = ISP_FACE_FORMAT_BGR888;

	face_net_run(ap_ctx, buff_fd, vir_addr, w, h, fmt, &result, &track_result);

	//test
	face_net_draw_box(vir_addr, 3, w, h, &result);

	//deinit
	face_net_deinit(ap_ctx);
*/

#define MAX_FACE (5)

typedef enum {
	ISP_FACE_FORMAT_RGB888 = 0,      /* RGB888 */
	ISP_FACE_FORMAT_BGR888,          /* BGR888 */
	ISP_FACE_FORMAT_RGBA8888,        /* RGBA8888 */
	ISP_FACE_FORMAT_BGRA8888,        /* BGRA8888 */
	ISP_FACE_FORMAT_YUV420P_YU12,    /* YUV420P YU12 */
	ISP_FACE_FORMAT_YUV420P_YV12,    /* YUV420P YV12 */
	ISP_FACE_FORMAT_YUV420SP_NV12,   /* YUV420SP NV12 */
	ISP_FACE_FORMAT_YUV420SP_NV21,   /* YUV420SP NV21 */
	ISP_FACE_FORMAT_MAX,
} FaceImageFomat;

/* 点坐标 */
typedef struct {
	int16_t x; /* 横坐标，万分比表示，数值范围0~9999 */
	int16_t y; /* 纵坐标，万分比表示，数值范围0~9999 */
} FacePoint;

/* 人脸角度 */
typedef struct {
	float yaw;   /* 左右摇头, +-90 */
	float pitch; /* 上下点头, +-90 */
	float roll;  /* 倾斜侧头, +-90 */
} FacePose;

typedef struct {
	int       id;          /* 人脸ID, 仅tracker有效 */
	int32_t   score;       /* 人脸检测分数 [1-100] */
	FacePoint topLeft;     /* 左上角坐标 */
	FacePoint bottomRight; /* 右下角坐标 */
	FacePoint point[5];    /* 五个关键点坐标 */
	int16_t   point_score[5];    /* 五个关键点分数 */
	FacePose  pose;
} FaceBox_t;

typedef struct {
	int face_num;
	FaceBox_t rect[MAX_FACE];
} FaceResult;

//face det api
typedef FaceResult FaceTrackerResult;

/************ face pose api ***************/
void *facePose_init(const char *model_name);

int facePose_run(void *ap_ctx,
		int bufferFd, void *virAddr,
		int width, int height, FaceImageFomat fmt,
		int x1, int y1, int x2, int y2,
		float *yaw, float *pitch, float *roll);

void facePose_deinit(void *ap_ctx);
/************ face pose end ***************/


/************ face det api ***************/
//threshold; 0.5 - 0.9
void *face_net_init(const char *model_name, float threshold);

int face_net_run(void *ap_ctx,
		int bufferFd, void *virAddr,
		int w, int h, FaceImageFomat fmt,
		FaceResult *result, FaceTrackerResult *trackResult);

void face_net_deinit(void *ap_ctx);

void face_net_draw_box(void *data, int bpp,
		int w, int h, FaceResult *result);
/************ face det end ***************/


/************ face occ api ***************/
void *faceOcc_init(const char *model_name);

//occMask: uchar 256x256
int faceOcc_run(void *ap_ctx,
		int bufferFd, void *virAddr,
		int width, int height, FaceImageFomat fmt,
		int x1, int y1, int x2, int y2,
		unsigned char *occMask);

void faceOcc_deinit(void *ap_ctx);

void faceOcc_draw(void *data, int bpp,
		int x1, int y1, int x2, int y2,
		int w, int h, unsigned char *occMask);
/************ face occ end ***************/

#ifdef __cplusplus
}
#endif /* end of __cplusplus */

#endif
