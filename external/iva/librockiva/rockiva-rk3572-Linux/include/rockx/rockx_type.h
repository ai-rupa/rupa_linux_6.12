#ifndef _ROCKX_TYPE_H_
#define _ROCKX_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "types/rockx_audio.h"
#include "types/rockx_data_type.h"
#include "types/rockx_image.h"
#include "types/rockx_packet.h"
#include "types/rockx_param.h"
#include "types/rockx_tensor.h"
#include "types/rockx_value.h"

#define ROCKX_KEY_ROOT_NODE_NAME "root_node_name"  //< Root node name (value type: const char*)
#define ROCKX_KEY_CONFIG_PATH "config_path"        //< Configuration graph config file path (value type: const char*)
#define ROCKX_KEY_CONFIG_STR "config_str"          //< Configuration graph config string (value type: const char*)
#define ROCKX_KEY_GRAPH_NAME "graph_name"          //< Configuration graph name (value type: const char*)
#define ROCKX_KEY_MODULE_NAME "module_name"        //< Configuration node name (value type: const char*)
#define ROCKX_KEY_GRAPH_INFO "graph_info"          //< Configuration graph info (value type: @ref RockXGraphInfo)

#define ROCKX_KEY_DATA_PATH "data_path"      //< Configuration data path (value type: const char*)
#define ROCKX_KEY_AUTH_CONFIG "auth_config"  //< Authorization configuration (value type: const char*)

#define ROCKX_KEY_STATE "state"                  //< Node state (value type: @ref RockXNodeState)
#define ROCKX_KEY_INSTANCE_MODE "instance_mode"  //< Node instance mode (value type: @ref RockXModuleInstanceMode)
#define ROCKX_KEY_INSTANCE_NAME "instance_name"  //< Node instance name (value type: const char*)
#define ROCKX_KEY_INSTANCE_NUM "instance_num"    //< Node instance number (value type: int)
#define ROCKX_KEY_THREAD_MODE "thread_mode"      //< Node thread mode (value type: @ref RockXThreadMode)

/**
 * @brief RockX instance
 *
 */
typedef void* RockXHandle;

/**
 * @brief Return value definition
 *
 */
typedef enum {
    ROCKX_RET_SUCCESS = 0,       ///< Success
    ROCKX_RET_FAIL = -1,         ///< Failure
    ROCKX_RET_PARAM_ERROR = -2,  ///< Parameter error
    ROCKX_RET_INIT_ERROR = -3,   ///< Initialization error
    ROCKX_RET_MEM_ERROR = -4,    ///< Memory error
    ROCKX_RET_NPU_CORE_ERROR = -5, ///< NPU Core error
    ROCKX_RET_AUTH_ERROR = -99,  ///< Authorization error
} RockXRetCode;

/**
 * @brief Update node state action
 *
 */
typedef enum RockXNodeAction {
    ROCKX_NODE_ACTION_START,   //< Start node
    ROCKX_NODE_ACTION_STOP,    //< Stop node
    ROCKX_NODE_ACTION_PAUSE,   //< Pause node
    ROCKX_NODE_ACTION_RESUME,  //< Resume node
    ROCKX_NODE_ACTION_RESET,   //< Reset node
    ROCKX_NODE_ACTION_PASS,    //< Skip node
} RockXNodeAction;

/**
 * @brief Module instance mode
 *
 */
typedef enum RockXModuleInstanceMode {
    ROCKX_MODULE_INSTANCE_MODE_DEFAULT = 0,  //< Default mode, create new module instance each time
    ROCKX_MODULE_INSTANCE_MODE_REUSE = 1,    //< Reuse mode, reuse existing module instance each time
} RockXModuleInstanceMode;

/**
 * @brief Callback result mode
 *
 */
typedef enum RockXCallbackMode {
    ROCKX_CALLBACK_NONE = 0,            //< No callback needed
    ROCKX_CALLBACK_ON_SCHED_THREAD,     //< Async result callback in scheduling thread, will block current frame
    ROCKX_CALLBACK_ON_SEPARATE_THREAD,  //< Async result callback in a separate thread
} RockXCallbackMode;

/**
 * @brief Execution thread mode
 *
 */
typedef enum RockXThreadMode {
    ROCKX_THREAD_MODE_COMMON = 0,    //< Use common thread
    ROCKX_THREAD_MODE_SEPARATE = 1,  //< Use separate thread
} RockXThreadMode;

/**
 * @brief Wait for execution result
 *
 */
typedef void* RockXWait;

/**
 * @brief Input data
 *
 */
typedef struct RockXInput RockXInput;

/**
 * @brief Output data
 *
 */
typedef struct RockXOutput RockXOutput;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif