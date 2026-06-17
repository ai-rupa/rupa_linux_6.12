#ifndef _ROCKX_MODULE_H_
#define _ROCKX_MODULE_H_

#include "rockx_builder.h"
#include "rockx_type.h"
#include "types/rockx_packet.h"
#include "types/rockx_param.h"

#ifdef __cplusplus
extern "C" {
#endif

struct RockXModule;

/**
 * @brief Module context
 *
 */
typedef void* RockXModuleContext;

/**
 * @brief Module type
 *
 */
typedef enum RockXModuleType {
    ROCKX_MODULE_TYPE_ATOMIC = 0,  ///< Atomic module, minimum unit
    ROCKX_MODULE_TYPE_SUBGRAPH,    ///< Subgraph, graph composed of multiple nodes (modules)
    ROCKX_MODULE_TYPE_LOOP,
    ROCKX_MODULE_TYPE_SWITCH,
} RockXModuleType;

/**
 * @brief Parameter scope
 */
typedef enum RockXParamScope {
    ROCKX_PARAM_SCOPE_GLOBAL = 0,  ///< Global parameters
    ROCKX_PARAM_SCOPE_GRAPH,       ///< Parameters of the graph where the node is located
    ROCKX_PARAM_SCOPE_NODE,        ///< Parameters of the node itself
} RockXParamScope;

/**
 * @brief Module registration callback function definition
 *
 */
typedef int (*OnModuleRegister)(struct RockXModule* module);

/**
 * @brief Module unregistration callback function definition
 *
 */
typedef int (*OnModuleUnRegister)(struct RockXModule* module);

/**
 * @brief Used for subgraph type modules to get subgraph building information
 *
 */
typedef int (*OnModuleBuilder)(RockXModuleContext ctx, RockXNodeInfo* nodeInfo);

/**
 * @brief Module pre-initialization callback function definition
 *
 */
typedef int (*OnModulePreInit)(RockXModuleContext ctx);

/**
 * @brief Module initialization callback function definition
 *
 */
typedef int (*OnModuleInit)(RockXModuleContext ctx);

/**
 * @brief Module post-initialization callback function definition
 *
 */
typedef int (*OnModulePostInit)(RockXModuleContext ctx);

/**
 * @brief Module deinitialization callback function definition
 *
 */
typedef int (*OnModuleRelease)(RockXModuleContext ctx);

/**
 * @brief Module parameter update callback function definition
 *
 */
typedef int (*OnModuleParamSet)(RockXModuleContext ctx, const RockXParam* param);

/**
 * @brief Module parameter get callback function definition
 *
 */
typedef int (*OnModuleParamGet)(RockXModuleContext ctx, RockXParam* param);

/**
 * @brief Module input get callback function definition, used to pre-allocate memory or set data dimension information
 * for previous nodes
 *
 */
typedef int (*OnModuleInputGet)(RockXModuleContext ctx, RockXPacket* input, int index);

/**
 * @brief Module output get callback function definition, used to pre-allocate memory or set data dimension information
 *
 */
typedef int (*OnModuleOutputGet)(RockXModuleContext ctx, RockXPacket* output, int index);

/**
 * @brief Module pre-processing callback function definition
 *
 */
typedef int (*OnModulePreProcess)(RockXModuleContext ctx, RockXInput* input, RockXOutput* output);

/**
 * @brief Module processing callback function definition
 *
 */
typedef int (*OnModuleProcess)(RockXModuleContext ctx, RockXInput* input, RockXOutput* output);

/**
 * @brief Module post-processing callback function definition
 *
 */
typedef int (*OnModulePostProcess)(RockXModuleContext ctx, RockXInput* input, RockXOutput* output);

/**
 * @brief Loop module get loop count
 */
typedef int (*OnModuleLoopGetCount)(RockXModuleContext ctx, RockXInput* input);

/**
 * @brief Loop module init output
 */
typedef int (*OnModuleLoopInitOutput)(RockXModuleContext ctx, RockXInput* input, RockXOutput* output);

/**
 * @brief Loop module input slice callback
 */
typedef int (*OnModuleLoopSlice)(RockXModuleContext ctx, int index, RockXInput* input, RockXInput* slicedInput);

/**
 * @brief Loop module output gather callback
 */
typedef int (*OnModuleLoopGather)(RockXModuleContext ctx, int index, RockXInput* input, RockXOutput* slicedOutput, RockXOutput* output);

/**
 * @brief Loop module callback
 * 
 */
typedef struct RockXLoop {
    OnModuleLoopGetCount onGetCount;
    OnModuleLoopInitOutput onInitOutput;
    OnModuleLoopSlice onSlice;
    OnModuleLoopGather onGather;
} RockXLoop;

/**
 * @brief Condition switch module get index
 * 
 */
typedef int (*OnModuleSwitchGetIndex)(RockXModuleContext ctx, RockXInput* input);

/**
 * @brief Condition switch module callback
 * 
 */
typedef struct RockXSwitch {
    OnModuleSwitchGetIndex onGetIndex;
} RockXSwitch;

/**
 * @brief Module
 *
 */
typedef struct RockXModule
{
    // Header info
    uint32_t magic;                   ///< Magic number
    uint32_t version;                 ///< Version number
    uint32_t size;                    ///< Size of the module
    uint32_t flags;                   ///< Flags
    // Basic info
    char* name;                       ///< Module name
    int inNum;                        ///< Number of module inputs
    int outNum;                       ///< Number of module outputs
    int shareNum;                     ///< Number of module shared data
    int paramNum;                     ///< Number of module parameters
    const char** inputs;              ///< Module input names
    const char** outputs;             ///< Module output names
    const char** shares;              ///< Module shared data names
    const char** params;              ///< Module parameter names
    RockXModuleType type;             ///< Module type
    // Callback functions
    OnModuleBuilder onBuilder;        ///< Subgraph type building callback function
    OnModuleRegister onRegister;      ///< Registration callback function
    OnModuleUnRegister onUnRegister;  ///< Unregistration callback function
    OnModulePreInit onPreInit;        ///< Pre-initialization callback function
    OnModuleInit onInit;              ///< Initialization callback function
    OnModulePostInit onPostInit;      ///< Post-initialization callback function
    OnModuleRelease onRelease;        ///< Deinitialization callback function
    OnModuleParamSet onParamSet;      ///< Parameter update callback function
    OnModuleParamGet onParamGet;      ///< Parameter get callback function
    OnModuleInputGet onInputsGet;     ///< Callback function to pre-get input Tensor information and memory
    OnModuleOutputGet onOutputsGet;   ///< Callback function to pre-get output Tensor information and memory
    OnModuleProcess onProcess;        ///< Processing callback function
    RockXLoop loopModule;
    RockXSwitch switchModule;
} RockXModule;

/**
 * @brief Register module
 *
 * @param handle Instance handle
 * @param module Module
 * @return RockXRetCode
 */
RockXRetCode RockXModuleRegister(RockXHandle handle, RockXModule* module);

/**
 * @brief Unregister module
 *
 * @param handle Instance handle
 * @param moduleName Module name
 * @return RockXRetCode
 */
RockXRetCode RockXModuleUnRegister(RockXHandle handle, const char* moduleName);

/**
 * @brief Global module registration
 *
 * @param module Module
 * @return RockXRetCode
 */
RockXRetCode RockXModuleGlobalRegister(RockXModule* module);

/**
 * @brief Global module unregistration
 *
 * @param moduleName Module name
 * @return RockXRetCode
 */
RockXRetCode RockXModuleGlobalUnRegister(const char* moduleName);

/**
 * @brief Set module context custom data
 *
 * @param context Module context
 * @param extData Custom data
 * @return int
 */
int RockXModuleSetExtData(RockXModuleContext context, void* extData);

/**
 * @brief Get module context custom data
 *
 * @param context Module context
 * @return void* Custom data
 */
void* RockXModuleGetExtData(RockXModuleContext context);

/**
 * @brief Get shared data packet
 *
 * @param context Module context
 * @param index Shared data index
 * @return RockXPacket* Shared data packet
 */
RockXPacket* RockXModuleGetSharedDataAt(RockXModuleContext context, int index);

/**
 * @brief Get module corresponding to module context
 *
 * @param context Module context
 * @return RockXModule* Module
 */
RockXModule* RockXModuleGetModule(RockXModuleContext context);

/**
 * @brief Get configuration parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @return RockXParam* Parameter
 */
RockXParam* RockXModuleGetParam(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                                const char* name);

/**
 * @brief Get integer value of configuration parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param defval Default value
 * @return int Integer value
 */
int RockXModuleGetParamInt(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                           int defval);

/**
 * @brief Get float value of configuration parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param defval Default value
 * @return float Float value
 */
float RockXModuleGetParamFloat(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                               const char* name, float defval);

/**
 * @brief Get string value of configuration parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param defval Default value
 * @return char* String value
 */
char* RockXModuleGetParamString(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                                const char* name, const char* defval);

/**
 * @brief Get pointer value of configuration parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param defval Default value
 * @return void* Pointer value
 */
void* RockXModuleGetParamPointer(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                                 const char* name, const void* defval);

/**
 * @brief Get module's own parameter
 *
 * @param context Module context
 * @param index Parameter index
 * @return RockXParam* Parameter
 */
RockXParam* RockXModuleGetParamAt(RockXModuleContext context, int index);

/**
 * @brief Get integer value of module's own parameter
 *
 * @param context Module context
 * @param index Parameter index
 * @param defval Default value
 * @return int Integer value
 */
int RockXModuleGetParamIntAt(RockXModuleContext context, int index, int defval);

/**
 * @brief Get float value of module's own parameter
 *
 * @param context Module context
 * @param index Parameter index
 * @param defval Default value
 * @return float Float value
 */
float RockXModuleGetParamFloatAt(RockXModuleContext context, int index, float defval);

/**
 * @brief Get string value of module's own parameter
 *
 * @param context Module context
 * @param index Parameter index
 * @param defval Default value
 * @return char* String value
 */
char* RockXModuleGetParamStringAt(RockXModuleContext context, int index, const char* defval);

/**
 * @brief Get pointer value of module's own parameter
 *
 * @param context Module context
 * @param index Parameter index
 * @param defval Default value
 * @return void*
 */
void* RockXModuleGetParamPointerAt(RockXModuleContext context, int index, const void* defval);

/**
 * @brief Set parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param param Parameter
 * @return int
 */
int RockXModuleSetParam(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                        const RockXParam* param);

/**
 * @brief Set integer value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param value Parameter value
 * @return int
 */
int RockXModuleSetParamInt(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                           int value);

/**
 * @brief Set long value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param value Parameter value
 * @return int
 */
int RockXModuleSetParamLong(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                            long value);

/**
 * @brief Set float value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param value Parameter value
 * @return int
 */
int RockXModuleSetParamFloat(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                             float value);

/**
 * @brief Set double value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param value Parameter value
 * @return int
 */
int RockXModuleSetParamDouble(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                              double value);

/**
 * @brief Set string value of parameter
 *
 * @param context
 * @param scope
 * @param nodeName
 * @param name
 * @param value
 * @return int
 */
int RockXModuleSetParamString(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                              const char* value, int needRelease);

/**
 * @brief Set pointer value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param value Parameter value
 * @return int
 */
int RockXModuleSetParamPointer(RockXModuleContext context, RockXParamScope scope, const char* nodeName,
                               const char* name, void* value, int needRelease, RockXParamCallback* callback);

/**
 * @brief Set array value of parameter
 *
 * @param context Module context
 * @param scope Parameter scope
 * @param nodeName Node name
 * @param name Parameter name
 * @param array array pointer
 * @param array_size array size
 * @param needRelease Whether to release the parameter value
 * @param callback Callback
 * @return int
 */
int RockXModuleSetParamArray(RockXModuleContext context, RockXParamScope scope, const char* nodeName, const char* name,
                             void* array, int array_size, int needRelease, RockXParamCallback* callback);

/**
 * @brief Module authorization check
 *
 * @param ctx Module context
 * @param authModule Authorization module
 * @return RockXRetCode Error code
 */
RockXRetCode RockXModuleAuthCheck(RockXModuleContext ctx, const char* authModule);

/**
 * @brief Module authorization check
 *
 * @param ctx Module context
 * @param authModule Authorization module
 * @param trialTime Trial time (seconds)
 * @return RockXRetCode Error code
 */
RockXRetCode RockXModuleAuthCheckWithTrial(RockXModuleContext ctx, const char* authModule, int trialTime);

/**
 * @brief Module registration macro function
 *
 * @param NAME Module name
 */
#define REGISTER_MODULE_GLOBAL(NAME)                                                                                   \
    extern RockXModule* GetRockXModule##NAME();                                                                        \
    RockXModuleGlobalRegister(GetRockXModule##NAME());

#define ROCKX_MODULE_MAGIC 0x524D4F44       // RMOD
#define ROCKX_MODULE_VERSION 0x00000100     // 1.0.0

#define ROCKX_MODULE_FULL_HEADER(mod) do { \
    (mod).magic = ROCKX_MODULE_MAGIC; \
    (mod).version = ROCKX_MODULE_VERSION; \
    (mod).size = sizeof(mod); \
    (mod).flags = 0; \
} while(0)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif