#ifndef _ROCKX_PARAM_H_
#define _ROCKX_PARAM_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameter
 *
 */
typedef struct RockXParam RockXParam;

/**
 * @brief Parameter release function
 *
 * @param param Parameter
 */
typedef void (*RockXParamReleaseCallback)(struct RockXParam* param);

/**
 * @brief Parameter copy function
 *
 * @param src Source parameter
 * @param dst Destination parameter
 */
typedef void (*RockXParamCopyCallback)(const struct RockXParam* src, struct RockXParam* dst);

/**
 * @brief Parameter dump function
 *
 * @param path Path
 * @param param Parameter
 * @param userdata User data
 */
typedef void (*RockXParamDumpCallback)(const char* path, const struct RockXParam* param, void* userdata);

/**
 * @brief Parameter callback
 *
 */
typedef struct RockXParamCallback
{
    RockXParamReleaseCallback OnRelease;  ///< Release callback
    RockXParamCopyCallback OnCopy;        ///< Copy callback
    RockXParamDumpCallback OnDump;        ///< Dump callback
} RockXParamCallback;

/**
 * @brief Create a parameter
 *
 * @param name Parameter name
 * @return RockXParam* Parameter
 */
RockXParam* RockXParamCreate(const char* name);

/**
 * @brief Destroy a parameter
 *
 * @param param Parameter
 */
void RockXParamDestroy(RockXParam* param);

/**
 * @brief Copy a parameter
 *
 * @param param Parameter
 * @param need_copy_value Whether needs to copy value
 * @return RockXParam* Parameter
 */
RockXParam* RockXParamDup(const RockXParam* param, int need_copy_value);

/**
 * @brief Copy a configuration
 *
 * @param srcParam Source configuration
 * @param dstParam Destination configuration
 * @param need_copy_value Whether needs to copy value
 * @return int Error code
 */
int RockXParamDup2(const RockXParam* srcParam, RockXParam* dstParam, int need_copy_value);

/**
 * @brief Set parameter callback
 *
 * @param param parameter instance
 * @param callback callback
 * @return int
 */
int RockXParamSetCallback(RockXParam* param, RockXParamCallback* callback);

/**
 * @brief Get parameter name
 *
 * @param param parameter instance
 * @return const char* parameter name
 */
const char* RockXParamGetName(const RockXParam* param);

/**
 * @brief Get parameter value as an integer value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return int
 */
int RockXParamGetInt(const RockXParam* param, int defValue);

/**
 * @brief Get parameter value as a long integer value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return long
 */
long RockXParamGetLong(const RockXParam* param, long defValue);

/**
 * @brief Get parameter value as a floating-point value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return float
 */
float RockXParamGetFloat(const RockXParam* param, float defValue);

/**
 * @brief Get parameter value as a double floating-point value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return double
 */
double RockXParamGetDouble(const RockXParam* param, double defValue);

/**
 * @brief Get parameter value as a string value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return char*
 */
char* RockXParamGetString(const RockXParam* param, const char* defValue);

/**
 * @brief Get parameter value as a pointer value
 *
 * @param param parameter instance
 * @param defValue default value
 * @return void*
 */
void* RockXParamGetPointer(const RockXParam* param, void* defValue);

/**
 * @brief Set parameter value as an integer value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetInt(RockXParam* param, int value);

/**
 * @brief Set parameter value as a long integer value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetLong(RockXParam* param, long value);

/**
 * @brief Set parameter value as a floating-point value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetFloat(RockXParam* param, float value);

/**
 * @brief Set parameter value as a double floating-point value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetDouble(RockXParam* param, double value);

/**
 * @brief Set parameter value as a string value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetString(RockXParam* param, const char* value, int needRelease);

/**
 * @brief Set parameter value as a pointer value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetPointer(RockXParam* param, void* value, int needRelease);

/**
 * @brief Set parameter value as an array value
 *
 * @param param parameter instance
 * @param value value
 * @return int
 */
int RockXParamSetArray(RockXParam* param, void* v, int count, int needRelease);

#ifdef __cplusplus
}
#endif

#endif