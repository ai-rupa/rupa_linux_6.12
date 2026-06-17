/**
 * Copyright     : Copyright(c) 2024 Rockchip. All right reserved.
 * FileName      : rkalg_idc_comm.h
 * Creacted By   : cjk
 * Creacted Date : 2024/05/29
 * Update Date   : 2024/06/18
 * Description   : Common header file for IDC
 */

#ifndef _RKALG_IDC_COMM_H__
#define _RKALG_IDC_COMM_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _rkAlgIDC_IMG_FMT_E {
    RKALG_IDC_IMG_FMT_NV12 = 0,
    RKALG_IDC_IMG_FMT_GRAY = 1      /* Reserved */
} RKALG_IDC_IMG_FMT_E;

typedef enum _rkAlgIDC_MESH_TYPE_E {
    RKALG_IDC_MESH_TYPE_COMMON = 0, /* meshX and meshY, for GPU (Reserved) */
    RKALG_IDC_MESH_TYPE_MERGED = 1, /* meshXy, for GPU */
    RKALG_IDC_MESH_TYPE_FEC    = 2  /* xi, xf, yi, yf, for FEC (Reserved) */
} RKALG_IDC_MESH_TYPE_E;

typedef struct _rkAlgIDC_IMAGE_S {
    uint64_t id;
    RKALG_IDC_IMG_FMT_E eImgFmt;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t u32Stride[3];
    uint32_t u32HgtStride[3];
    int fd[3];
    void *virAddr[3];
    void *phyAddr[3];            /* Reserved */
} RKALG_IDC_IMAGE_S;

typedef struct _rkAlgIDC_MESH_S {
    uint64_t id;
    uint32_t u32StepX;
    uint32_t u32StepY;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t u32Stride;              /* Reserved */
    uint32_t u32HgtStride;           /* Reserved */
    RKALG_IDC_MESH_TYPE_E eMeshType;
    int fd[4];
    void *virAddr[4];
    void *phyAddr[4];                /* Reserved */
} RKALG_IDC_MESH_S;

#ifdef __cplusplus
}
#endif


#endif // _RKALG_IDC_COMM_H__