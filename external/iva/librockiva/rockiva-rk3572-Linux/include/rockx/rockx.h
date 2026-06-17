#ifndef _ROCKX_H_
#define _ROCKX_H_

#include "rockx_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create instance
 *
 * @return RockXHandle Created instance handle
 */
RockXHandle RockXCreate();

/**
 * @brief Destroy instance
 *
 * @param handle Instance handle
 * @return RockXRetCode Error code
 */
RockXRetCode RockXDestroy(RockXHandle handle);

/**
 * @brief Load algorithm SDK
 *
 * @param handle Instance handle
 * @param path Algorithm SDK path
 * @return RockXRetCode Error code
 */
RockXRetCode RockXLoadSDK(RockXHandle handle, const char* path);

/**
 * @brief Initialize instance
 *
 * @param handle Instance handle
 * @return RockXRetCode Error code
 */
RockXRetCode RockXInit(RockXHandle handle);

/**
 * @brief Set node parameters
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param param Node options
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParam(RockXHandle handle, const char* nodeName, const RockXParam* param);

/**
 * @brief Set node parameter as integer
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param paramName Parameter name
 * @param value Parameter value
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParamInt(RockXHandle handle, const char* nodeName, const char* paramName, int value);

/**
 * @brief Set node parameter as long integer
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param paramName Parameter name
 * @param value Parameter value
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParamLong(RockXHandle handle, const char* nodeName, const char* paramName, long value);

/**
 * @brief Set node parameter as float
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param paramName Parameter name
 * @param value Parameter value
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParamFloat(RockXHandle handle, const char* nodeName, const char* paramName, float value);

/**
 * @brief Set node parameter as string
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param paramName Parameter name
 * @param value Parameter value
 * @param needRelease Whether to release
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParamString(RockXHandle handle, const char* nodeName, const char* paramName, const char* value,
                                 int needRelease);

/**
 * @brief Set node parameter as pointer
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param paramName Parameter name
 * @param value Parameter value
 * @param needRelease Whether to release
 * @param cb Callback function
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetParamPointer(RockXHandle handle, const char* nodeName, const char* paramName, void* value,
                                  int needRelease, RockXParamCallback* cb);

/**
 * @brief Get parameter
 *
 * @param handle Instance handle
 * @param nodeName Node name (optional, NULL for global parameters)
 * @param paramName Parameter name
 * @return RockXParam* Parameter
 */
RockXParam* RockXGetParam(RockXHandle handle, const char* nodeName, const char* paramName);

/**
 * @brief Update node state
 *
 * @param handle Instance handle
 * @param nodeName Node name
 * @param action Action
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetNodeState(RockXHandle handle, const char* nodeName, RockXNodeAction action);

/**
 * @brief Configure required inputs, RockXInputCreate will create inputs based on configuration
 *
 * @param handle Instance handle
 * @param name Input name
 * @param inNames Input name list
 * @param inNum Number of inputs
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetInputInfo(RockXHandle handle, const char* name, const char* inNames[], int inNum);

/**
 * @brief Result callback function
 *
 * @param handle Instance handle
 * @param output Output
 * @param userdata User-defined data
 */
typedef void (*RockXResultCallback)(RockXHandle handle, RockXOutput* output, void* userdata);

/**
 * @brief Configure required outputs, two execution modes:
 *      In synchronous mode, callback can be NULL, RockXOutputCreate will create outputs based on configuration
 *      In asynchronous mode, callback function and callback mode can be set
 *
 * @param handle Instance handle
 * @param name Output name
 * @param outputName Output name list
 * @param callback Callback function
 * @param userdata User-defined data
 * @param mode Callback mode
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetOutputInfo(RockXHandle handle, const char* name, const char* outNames[], int outNum,
                                RockXResultCallback callback, RockXCallbackMode mode, void* userdata);

/**
 * @brief Configure default output callback function
 * 
 * @param handle Instance handle
 * @param name Output name
 * @param callback Output Result callback function
 * @param userdata User-defined data
 * @param mode Callback mode
 * @return RockXRetCode Error code
 */
RockXRetCode RockXSetOutputInfoDefault(RockXHandle handle, const char* name, RockXResultCallback callback, RockXCallbackMode mode, void* userdata);

/**
 * @brief Remove output configuration
 *
 * @param handle Instance handle
 * @param name Input name
 * @return RockXRetCode Error code
 */
RockXRetCode RockXRemoveOutputInfo(RockXHandle handle, const char* name);

/**
 * @brief Create input
 *
 * @param handle Instance handle
 * @return RockXInput* Input
 */
RockXInput* RockXInputCreate(RockXHandle handle);

/**
 * @brief Release input
 *
 * @param handle Instance handle
 * @param input Input
 * @return RockXRetCode Error code
 */
RockXRetCode RockXInputDestroy(RockXHandle handle, RockXInput* input);

/**
 * @brief Create output
 *
 * @param handle Instance handle
 * @param outNames Output name list
 * @param outNum Number of outputs
 * @return RockXOutput* Output
 */
RockXOutput* RockXOutputCreate(RockXHandle handle, const char* outNames[], int outNum);

/**
 * @brief Release output
 *
 * @param handle Instance handle
 * @param output Output
 * @return RockXRetCode Error code
 */
RockXRetCode RockXOutputDestroy(RockXHandle handle, RockXOutput* output);

/**
 * @brief Process execution
 *
 * @param handle Instance handle
 * @param input Input
 * @param output Output
 * @return RockXRetCode Error code
 */
RockXRetCode RockXProcess(RockXHandle handle, RockXInput* input, RockXOutput* output);

/**
 * @brief Asynchronous process execution
 *
 * @param handle Instance handle
 * @param input Input
 * @param wait Wait handle (can be NULL if no necessary wait)
 * @return RockXRetCode Error code
 */
RockXRetCode RockXProcessAsync(RockXHandle handle, RockXInput* input, RockXWait* wait);

/**
 * @brief Wait for asynchronous execution to complete
 *
 * @param handle Instance handle
 * @param wait Wait handle
 * @return RockXRetCode Error code
 */
RockXRetCode RockXWaitFinish(RockXHandle handle, RockXWait wait);

/**
 * @brief Get number of input packets
 *
 * @param input Input
 * @return int Number of packets
 */
int RockXInputPacketsNum(RockXInput* input);

/**
 * @brief Get number of output packets
 *
 * @param output Output
 * @return int Number of packets
 */
int RockXOutputPacketsNum(RockXOutput* output);

/**
 * @brief Get input ext data
 *
 * @param input Input
 * @param extdata Ext data
 * @return int Error code
 */
int RockXInputSetExtData(RockXInput* input, void* extdata);

/**
 * @brief Get output ext data
 *
 * @param output Output
 * @return void* Ext data set in RockXInput
 */
void* RockXOutputGetExtData(RockXOutput* output);

/**
 * @brief Get input packet
 *
 * @param input Input
 * @param index Index
 * @return RockXPacket* Packet
 */
RockXPacket* RockXInputGetPacketAt(RockXInput* input, int index);

/**
 * @brief Get output packet
 *
 * @param output Output
 * @param index Index
 * @return RockXPacket* Packet
 */
RockXPacket* RockXOutputGetPacketAt(RockXOutput* output, int index);

/**
 * @brief Get input packet
 *
 * @param input Input
 * @param name Packet name
 * @return RockXPacket* Packet
 */
RockXPacket* RockXInputGetPacketByName(RockXInput* input, const char* name);

/**
 * @brief Get output packet
 *
 * @param output Output
 * @param name Packet name
 * @return RockXPacket* Packet
 */
RockXPacket* RockXOutputGetPacketByName(RockXOutput* output, const char* name);

/**
 * @brief Get input packets
 *
 * @param input Input
 * @param count Number of packets
 * @return RockXPacket* Packet array
 */
RockXPacket* RockXInputGetPackets(RockXInput* input);

/**
 * @brief Get output packets
 *
 * @param output Output
 * @param count Number of packets
 * @return RockXPacket* Packet array
 */
RockXPacket* RockXOutputGetPackets(RockXOutput* output);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif