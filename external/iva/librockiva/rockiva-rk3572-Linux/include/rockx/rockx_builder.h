#ifndef ROCKX_GRAPH_BUILDER_H_
#define ROCKX_GRAPH_BUILDER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "types/rockx_value.h"

typedef struct RockXNodeInfo RockXNodeInfo;
typedef struct RockXNodeInfo RockXGraphInfo;

/**
 * @brief Create a graph
 *
 * @param name Graph name
 * @return RockXGraphInfo* Graph information
 */
RockXGraphInfo* RockXBuilderGraphCreate(const char* name);

/**
 * @brief Destroy a graph
 *
 * @param graph Graph
 * @return int
 */
int RockXBuilderGraphDestroy(RockXGraphInfo* graph);

/**
 * @brief Create a node
 *
 * @param name Node name
 * @param module Module name (if module is graph set Graph as module name)
 * @return RockXNodeInfo* Node information
 */
RockXNodeInfo* RockXBuilderNodeCreate(const char* name, const char* module);

/**
 * @brief Destroy a node
 *
 * @param node Node
 * @return int
 */
int RockXBuilderNodeDestroy(RockXNodeInfo* node);

/**
 * @brief Add a node to a graph
 *
 * @param graph Graph node
 * @param node Sub-node
 * @return int
 */
int RockXBuilderAddNode(RockXNodeInfo* graph, RockXNodeInfo* node);

/**
 * @brief Add an input to graph
 *
 * @param graph Graph
 * @param name Input name
 * @return int
 */
int RockXBuilderGraphAddInput(RockXGraphInfo* graph, const char* name);

/**
 * @brief Add an output to graph
 *
 * @param graph Graph
 * @param name Output name
 * @return int
 */
int RockXBuilderGraphAddOutput(RockXGraphInfo* graph, const char* name);

/**
 * @brief Add an input to node
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @param externalName External name (defined by user, used in computation graph)
 * @return int
 */
int RockXBuilderNodeAddInput(RockXNodeInfo* node, const char* internalName, const char* externalName);

/**
 * @brief Get node input
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @return const char* External name (defined by user, used in computation graph)
 */
const char* RockXBuilderNodeGetInput(const RockXNodeInfo* node, const char* internalName);

/**
 * @brief Add an output to node
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @param externalName External name (defined by user, used in computation graph)
 * @return int
 */
int RockXBuilderNodeAddOutput(RockXNodeInfo* node, const char* internalName, const char* externalName);

/**
 * @brief Get node output
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @return const char* External name
 */
const char* RockXBuilderNodeGetOutput(const RockXNodeInfo* node, const char* internalName);

/**
 * @brief Add shared data to node
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @param externalName External name (defined by user, used in computation graph)
 * @return int
 */
int RockXBuilderNodeAddShare(RockXNodeInfo* node, const char* internalName, const char* externalName);

/**
 * @brief Get node shared data
 *
 * @param node Node
 * @param internalName Internal name (defined in module)
 * @return const char* External name
 */
const char* RockXBuilderNodeGetShare(const RockXNodeInfo* node, const char* internalName);

/**
 * @brief Add a parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @return int
 */
int RockXBuilderAddParam(RockXNodeInfo* node, const char* key, const RockXValue* value, int needRelease);

/**
 * @brief Add an integer parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @return int
 */
int RockXBuilderAddParamInt(RockXNodeInfo* node, const char* key, int value);

/**
 * @brief Add a long parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @return int
 */
int RockXBuilderAddParamLong(RockXNodeInfo* node, const char* key, long value);

/**
 * @brief Add a float parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @return int
 */
int RockXBuilderAddParamFloat(RockXNodeInfo* node, const char* key, float value);

/**
 * @brief Add a double parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @return int
 */
int RockXBuilderAddParamDouble(RockXNodeInfo* node, const char* key, double value);

/**
 * @brief Add a string parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @param needRelease Whether needs to be released
 * @return int
 */
int RockXBuilderAddParamString(RockXNodeInfo* node, const char* key, const char* value, int needRelease);

/**
 * @brief Add a pointer parameter
 *
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @param needRelease Whether needs to be released
 * @return int
 */
int RockXBuilderAddParamPointer(RockXNodeInfo* node, const char* key, const void* value, int needRelease);

/**
 * @brief Add an array parameter
 * 
 * @param node Node
 * @param key Param name
 * @param value Param value
 * @param count Param count
 * @param needRelease Whether needs to be released
 * @return int
 */
int RockXBuilderAddParamArray(RockXNodeInfo* node, const char* key, const void* value, int count, int needRelease);

/**
 * @brief Check if parameter exists
 *
 * @param node Node
 * @param key Param name
 * @return int 1: exists; 0: not exists
 */
int RockXBuilderCheckParamExist(const RockXNodeInfo* node, const char* key);

/**
 * @brief Get an integer parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return int
 */
int RockXBuilderGetParamInt(const RockXNodeInfo* node, const char* key, int defval);

/**
 * @brief Get a long parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return long
 */
long RockXBuilderGetParamLong(const RockXNodeInfo* node, const char* key, long defval);

/**
 * @brief Get a float parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return float
 */
float RockXBuilderGetParamFloat(const RockXNodeInfo* node, const char* key, float defval);

/**
 * @brief Get a double parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return double
 */
double RockXBuilderGetParamDouble(const RockXNodeInfo* node, const char* key, double defval);

/**
 * @brief Get a string parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return const char*
 */
const char* RockXBuilderGetParamString(const RockXNodeInfo* node, const char* key, const char* defval);

/**
 * @brief Get a pointer parameter
 *
 * @param node Node
 * @param key Param name
 * @param defval Default value
 * @return void*
 */
void* RockXBuilderGetParamPointer(const RockXNodeInfo* node, const char* key, void* defval);

/**
 * @brief Get an array parameter
 *
 * @param node Node
 * @param key Param name
 * @param count Array count
 * @return void*
 */
void* RockXBuilderGetParamArray(const RockXNodeInfo* node, const char* key, int* count);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  //  ROCKX_GRAPH_BUILDER_H_