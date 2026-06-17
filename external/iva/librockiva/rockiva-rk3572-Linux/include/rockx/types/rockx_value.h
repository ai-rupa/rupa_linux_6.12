#ifndef _ROCKX_VALUE_H_
#define _ROCKX_VALUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "rockx_data_type.h"

/**
 * @brief Value type
 *
 */
typedef enum RockXValueType {
    ROCKX_VALUE_TYPE_NONE,     ///< No type
    ROCKX_VALUE_TYPE_NUMBER,   ///< Number
    ROCKX_VALUE_TYPE_STRING,   ///< String
    ROCKX_VALUE_TYPE_ARRAY,    ///< Array
    ROCKX_VALUE_TYPE_POINTER,  ///< Pointer
} RockXValueType;

/**
 * @brief Value
 *
 */
typedef struct RockXValue
{
    RockXValueType type;  ///< Value type
    RockXDataType dtype;  ///< Data type
    union
    {
        void* p;   ///< Pointer
        char* s;   ///< String
        int i;     ///< Integer
        long l;    ///< Long integer
        float f;   ///< Float
        double d;  ///< Double
    };
    size_t count;         ///< Count
    size_t size;          ///< Memory size
    uint8_t needRelease;  ///< Whether to call free to release memory pointed by p or string pointed by s
} RockXValue;

/**
 * @brief Set an integer value
 *
 * @param value Value
 * @param v Value to set
 * @return int 0: success, -1: failed
 */
int RockXValueSetInt(RockXValue* value, int v);

/**
 * @brief Set a long integer value
 *
 * @param value Value
 * @param v Value to set
 * @return int 0: success, -1: failed
 */
int RockXValueSetLong(RockXValue* value, long v);

/**
 * @brief Set a double floating-point value
 *
 * @param value Value
 * @param v Value to set
 * @return int 0: success, -1: failed
 */
int RockXValueSetDouble(RockXValue* value, double v);

/**
 * @brief Set a floating-point value
 *
 * @param value Value
 * @param v Value to set
 * @return int 0: success, -1: failed
 */
int RockXValueSetFloat(RockXValue* value, float v);

/**
 * @brief Set a string value
 *
 * @param value Value
 * @param v Value to set
 * @param needRelease Whether needs to be released
 * @return int 0: success, -1: failed
 */
int RockXValueSetString(RockXValue* value, const char* v, int needRelease);

/**
 * @brief Set a pointer value
 *
 * @param value Value
 * @param v Value to set
 * @param needRelease Whether needs to be released
 * @return int 0: success, -1: failed
 */
int RockXValueSetPointer(RockXValue* value, void* v, int needRelease);

/**
 * @brief Set an array value
 *
 * @param value Value
 * @param v Value to set
 * @param count Count
 * @param needRelease Whether needs to be released
 * @return int 0: success, -1: failed
 */
int RockXValueSetArray(RockXValue* value, void* v, int count, int needRelease);

/**
 * @brief Get an integer value
 *
 * @param value Value
 * @param defValue Default value
 * @return int Integer value
 */
int RockXValueGetInt(const RockXValue* value, int defValue);

/**
 * @brief Get a long integer value
 *
 * @param value Value
 * @param defValue Default value
 * @return long Long integer value
 */
long RockXValueGetLong(const RockXValue* value, long defValue);

/**
 * @brief Get a floating-point value
 *
 * @param value Value
 * @param defValue Default value
 * @return float Floating-point value
 */
float RockXValueGetFloat(const RockXValue* value, float defValue);

/**
 * @brief Get a double floating-point value
 *
 * @param value Value
 * @param defValue Default value
 * @return double Double floating-point value
 */
double RockXValueGetDouble(const RockXValue* value, double defValue);

/**
 * @brief Get a string value
 *
 * @param value Value
 * @param defValue Default value
 * @return char* String value
 */
char* RockXValueGetString(const RockXValue* value, const char* defValue);

/**
 * @brief Get a pointer value
 *
 * @param value Value
 * @param defValue Default value
 * @return void* Pointer value
 */
void* RockXValueGetPointer(const RockXValue* value, const void* defValue);

/**
 * @brief Get an array value
 *
 * @param value Value
 * @param count Array element count
 * @return void* Array value
 */
void* RockXValueGetArray(const RockXValue* value, int* count);

/**
 * @brief Release a value
 *
 * @param value Value
 * @return int 0: success, -1: failed
 */
int RockXValueRelease(RockXValue* value);

#ifdef __cplusplus
}
#endif

#endif