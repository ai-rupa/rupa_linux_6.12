#ifndef __ROCKX_FUNCTION_H__
#define __ROCKX_FUNCTION_H__

#include "rockx_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function call
 */
typedef RockXRetCode (*RockXFunctionOnCall)(RockXInput* input, RockXOutput* output);

/**
 * @brief Function definition
 */
typedef struct
{
    // Header info
    uint32_t magic;                   ///< Magic number
    uint32_t version;                 ///< Version number
    uint32_t size;                    ///< Size of the module
    uint32_t flags;                   ///< Flags
    // Basic info
    const char* name;          ///< Function name
    int inNum;                 ///< Input number
    int outNum;                ///< Output number
    const char** inputs;       ///< Inputs
    const char** outputs;      ///< Outputs
    // Callback functions
    RockXFunctionOnCall call;  ///< Function call
} RockXFunction;

/**
 * @brief Register global function
 *
 * @param function Function
 * @return RockXRetCode Error code
 */
RockXRetCode RockXFunctionGlobalRegister(RockXFunction* function);

/**
 * @brief Unregister global function
 *
 * @param name Function name
 * @return RockXRetCode Error code
 */
RockXRetCode RockXFunctionGlobalUnregister(const char* name);

/**
 * @brief Find function
 *
 * @param name Function name
 * @return RockXFunction* Function
 */
RockXFunction* RockXFunctionFind(const char* name);

/**
 * @brief Call function
 *
 * @param function Function
 * @param input Input
 * @param output Output
 * @return RockXRetCode Error code
 */
RockXRetCode RockXFunctionCall(RockXFunction* function, RockXInput* input, RockXOutput* output);

/**
 * @brief Create function input
 *
 * @param function Function
 * @return RockXInput* Input
 */
RockXInput* RockXFunctionInputCreate(RockXFunction* function);

/**
 * @brief Create function output
 *
 * @param function Function
 * @return RockXOutput* Output
 */
RockXOutput* RockXFunctionOutputCreate(RockXFunction* function);

/**
 * @brief Destroy function input
 *
 * @param input Input
 * @return RockXRetCode Error code
 */
RockXRetCode RockXFunctionInputDestroy(RockXInput* input);

/**
 * @brief Destroy function output
 *
 * @param output Output
 * @return RockXRetCode Error code
 */
RockXRetCode RockXFunctionOutputDestroy(RockXOutput* output);

#define ROCKX_FUNCTION_MAGIC 0x52464E43       // RFNC
#define ROCKX_FUNCTION_VERSION 0x00000100     // 1.0.0

#define ROCKX_FUNCTION_FULL_HEADER(func) do { \
    (func).magic = ROCKX_FUNCTION_MAGIC; \
    (func).version = ROCKX_FUNCTION_VERSION; \
    (func).size = sizeof(func); \
    (func).flags = 0; \
} while(0)

#ifdef __cplusplus
}
#endif

#endif
