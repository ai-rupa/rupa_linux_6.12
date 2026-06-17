#ifndef _ROCKX_DATA_TYPE_H_
#define _ROCKX_DATA_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Data type
 *
 */
typedef enum {
    ROCKX_DATA_TYPE_UINT8,    ///< Unsigned 8-bit integer
    ROCKX_DATA_TYPE_INT8,     ///< Signed 8-bit integer
    ROCKX_DATA_TYPE_UINT16,   ///< Unsigned 16-bit integer
    ROCKX_DATA_TYPE_INT16,    ///< Signed 16-bit integer
    ROCKX_DATA_TYPE_UINT32,   ///< Unsigned 32-bit integer
    ROCKX_DATA_TYPE_INT32,    ///< Signed 32-bit integer
    ROCKX_DATA_TYPE_UINT64,   ///< Unsigned 64-bit integer
    ROCKX_DATA_TYPE_INT64,    ///< Signed 64-bit integer
    ROCKX_DATA_TYPE_FLOAT,    ///< 32-bit float
    ROCKX_DATA_TYPE_DOUBLE,   ///< 64-bit float
    ROCKX_DATA_TYPE_FLOAT16,  ///< 16-bit float
    ROCKX_DATA_TYPE_RAW,      ///< Raw data
    ROCKX_DATA_TYPE_STRUCT,   ///< Structure
    ROCKX_DATA_TYPE_MAX,      ///< Maximum value
} RockXDataType;

/**
 * @brief Get data type size
 *
 * @param dtype Data type
 * @return size
 */
static inline int RockXDataTypeSize(RockXDataType dtype)
{
    switch (dtype) {
        case ROCKX_DATA_TYPE_UINT8: return 1;
        case ROCKX_DATA_TYPE_INT8: return 1;
        case ROCKX_DATA_TYPE_UINT16: return 2;
        case ROCKX_DATA_TYPE_INT16: return 2;
        case ROCKX_DATA_TYPE_UINT32: return 4;
        case ROCKX_DATA_TYPE_INT32: return 4;
        case ROCKX_DATA_TYPE_UINT64: return 8;
        case ROCKX_DATA_TYPE_INT64: return 8;
        case ROCKX_DATA_TYPE_FLOAT: return 4;
        case ROCKX_DATA_TYPE_DOUBLE: return 8;
        case ROCKX_DATA_TYPE_FLOAT16: return 2;
        default: return 0;
    }
}

/**
 * @brief Data type enum to string
 *
 * @param dtype Data type enum
 * @return const char* Data type string
 */
static inline const char* RockXDataTypeToString(RockXDataType dtype)
{
    switch (dtype) {
        case ROCKX_DATA_TYPE_UINT8: return "UINT8";
        case ROCKX_DATA_TYPE_INT8: return "INT8";
        case ROCKX_DATA_TYPE_UINT16: return "UINT16";
        case ROCKX_DATA_TYPE_INT16: return "INT16";
        case ROCKX_DATA_TYPE_UINT32: return "UINT32";
        case ROCKX_DATA_TYPE_INT32: return "INT32";
        case ROCKX_DATA_TYPE_UINT64: return "UINT64";
        case ROCKX_DATA_TYPE_INT64: return "INT64";
        case ROCKX_DATA_TYPE_FLOAT: return "FLOAT";
        case ROCKX_DATA_TYPE_DOUBLE: return "DOUBLE";
        case ROCKX_DATA_TYPE_FLOAT16: return "FLOAT16";
        case ROCKX_DATA_TYPE_RAW: return "RAW";
        case ROCKX_DATA_TYPE_STRUCT: return "STRUCT";
        default: return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif  // _ROCKX_DATA_TYPE_H_