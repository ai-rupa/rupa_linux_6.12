#ifndef __RK_ALGO_AVS_TOOL_API_V2_H__
#define __RK_ALGO_AVS_TOOL_API_V2_H__

#include "rk_algo_avs_tool_def.h"
#include "rk_algo_avs_tool_def_v2.h"
#include "rk_algo_avs_tool_comm.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
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


    /* init: calib file ---> middle lut ---> final lut */
    DLL_PUBLIC int32_t RKALGO_AVS_V2_Init(RKALGO_AVS_V2_CTX_S *pstAvsV2Ctx, RKALGO_AVS_V2_INIT_S *pstAvsV2Init);

    /* stitch: stitching images */
    DLL_PUBLIC int32_t RKALGO_AVS_V2_Process(RKALGO_AVS_V2_CTX_S *pstAvsV2Ctx, RKALGO_AVS_V2_PROCESS_S *pstAvsV2Process);

    /* deinit */
    DLL_PUBLIC int32_t RKALGO_AVS_V2_DeInit(RKALGO_AVS_V2_CTX_S *pstAvsV2Ctx);


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif // !__RK_ALGO_AVS_TOOL_API_V2_H__