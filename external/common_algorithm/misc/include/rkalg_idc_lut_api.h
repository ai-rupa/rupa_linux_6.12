/**
 * Copyright     : Copyright(c) 2024 Rockchip. All right reserved.
 * FileName      : rkalg_idc_lut_api.h
 * Creacted By   : cjk
 * Creacted Date : 2024/06/05
 * Updated Date  : 2024/06/12
 * Description   : header file for LUT API
 */

#ifndef _RKALG_IDC_LUT_API_H_
#define _RKALG_IDC_LUT_API_H_

#include "rkalg_idc_lut_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined _WIN32 || defined __CYGWIN__
    #ifdef BUILDING_DLL
        #ifdef __GNUC__
            #define DLL_PUBLIC __attribute__ ((dllexport))
        #else
            // Note: actually gcc seems to also supports this syntax.
            #define DLL_PUBLIC __declspec(dllexport)
        #endif
    #else
        #ifdef __GNUC__
            #define DLL_PUBLIC __attribute__ ((dllimport))
        #else
            // Note: actually gcc seems to also supports this syntax.
            #define DLL_PUBLIC
        #endif
        #define DLL_LOCAL
    #endif
#else
    #if __GNUC__ >= 4
        #define DLL_PUBLIC __attribute__ ((visibility ("default")))
        #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
    #else
        #define DLL_PUBLIC
        #define DLL_LOCAL
    #endif
#endif


/**
 * @brief Get LUT version info.
 * @param[out]  idcLutVersion  LUT version info.
 */
DLL_PUBLIC void RKALG_IDC_LUT_GetVersion(char idcLutVersion[128]);

/**
 * @brief Create LUT context
 * @param[in,out]  pLutCtx  LUT context
 * @param[in]  pInitParams  LUT init params
 * @return 0 success, other value fail
 */
DLL_PUBLIC int RKALG_IDC_LUT_Init(RKALG_LUT_CTX_S *pLutCtx, RKALG_LUT_INIT_PARAMS_S *pInitParams);

/**
 * @brief execute LUT task
 * @param[in,out]  pLutCtx  LUT context
 * @param[in]        pTask  LUT task
 * @return 0 success, other value fail
 */
DLL_PUBLIC int RKALG_IDC_LUT_DoLut(RKALG_LUT_CTX_S *pLutCtx, RKALG_LUT_TASK_S *pTask);

/**
 * @brief destroy LUT context
 * @param[in]  pLutCtx  LUT context
 * @return 0 success, other value fail
 */
DLL_PUBLIC int RKALG_IDC_LUT_Deinit(RKALG_LUT_CTX_S *pLutCtx);

#ifdef __cplusplus
}
#endif

#endif /*  _RKALG_IDC_LUT_API_H_ */