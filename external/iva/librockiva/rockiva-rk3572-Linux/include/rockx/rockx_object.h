#ifndef _ROCKX_OBJECT_H_
#define _ROCKX_OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rockx_type.h"

typedef void (*RockXObjectSerialize)(void* object, void** buffer, int* size);

typedef void (*RockXObjectDeserialize)(void* buffer, int size, void* object);

typedef void (*RockXObjectRelease)(void* object);

typedef void (*RockXObjectCopy)(void* src, void* dst);

typedef struct
{
    // Header info
    uint32_t magic;    ///< Magic number
    uint32_t version;  ///< Version number
    uint32_t size;     ///< Size of the object
    uint32_t flags;    ///< Flags
    // Basic info
    char* typeName;                     ///< Object type name
    // Callback functions
    RockXObjectSerialize serialize;      ///< Serialize function
    RockXObjectDeserialize deserialize;  ///< Deserialize function
    RockXObjectRelease release;          ///< Release function
    RockXObjectCopy copy;                ///< Copy function
} RockXObject;

#ifdef __cplusplus
}
#endif

#endif