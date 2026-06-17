/*
 * Copyright (c) 2025 Rockchip, Inc. All Rights Reserved.
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

#ifndef __RKADK_TYPE_H__
#define __RKADK_TYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdbool.h>

typedef unsigned char RKADK_U8;
typedef unsigned short RKADK_U16;
typedef unsigned int RKADK_U32;

typedef char RKADK_S8;
typedef short RKADK_S16;
typedef int RKADK_S32;

typedef unsigned long RKADK_UL;
typedef signed long RKADK_SL;

typedef float RKADK_FLOAT;
typedef double RKADK_DOUBLE;

#ifndef _M_IX86
typedef unsigned long long RKADK_U64;
typedef long long RKADK_S64;
#else
typedef unsigned __int64 RKADK_U64;
typedef __int64 RKADK_S64;
#endif

typedef char RKADK_CHAR;
#define RKADK_VOID void

typedef unsigned int RKADK_HANDLE;

typedef RKADK_VOID *RKADK_MW_PTR;

typedef enum {
  RKADK_FALSE = 0,
  RKADK_TRUE = 1,
} RKADK_BOOL;

typedef enum {
  RKADK_FMT_ARGB1555,                                   /* 16-bit RGB               */
  RKADK_FMT_ABGR1555,                                   /* 16-bit RGB               */
  RKADK_FMT_RGBA5551,                                   /* 16-bit RGB               */
  RKADK_FMT_BGRA5551,                                   /* 16-bit RGB               */
  RKADK_FMT_ARGB4444,                                   /* 16-bit RGB               */
  RKADK_FMT_ABGR4444,                                   /* 16-bit RGB               */
  RKADK_FMT_RGBA4444,                                   /* 16-bit RGB               */
  RKADK_FMT_BGRA4444,                                   /* 16-bit RGB               */
  RKADK_FMT_ARGB8888,                                   /* 32-bit RGB               */
  RKADK_FMT_ABGR8888,                                   /* 32-bit RGB               */
  RKADK_FMT_RGBA8888,                                   /* 32-bit RGB               */
  RKADK_FMT_BGRA8888,                                   /* 32-bit RGB               */
  RKADK_FMT_RGB565,                                     /* 16-bit RGB               */
  RKADK_FMT_BGR565,                                     /* 16-bit RGB               */
  RKADK_FMT_2BPP,
  RKADK_FMT_YUV420SP,
  RKADK_FMT_YUV420SP_10BIT,
  RKADK_FMT_YUV422SP,
  RKADK_FMT_YUV420SP_VU,
  RKADK_FMT_YUV422_UYVY,
  RKADK_FMT_YUV422_VYUY,
  RKADK_FMT_BUTT,
} RKADK_FORMAT_E;

#ifndef NULL
#define NULL 0L
#endif

#define RKADK_NULL 0L
#define RKADK_SUCCESS 0
#define RKADK_FAILURE (-1)
#define RKADK_PARAM_NOT_EXIST (-2)
#define RKADK_STATE_ERR (-3)

#define RKADK_ABS(x)              ((x) < (0) ? -(x) : (x))

#ifdef __cplusplus
}
#endif

#endif // __RKADK_TYPE_H__
