#ifndef CAPI_ROCKX_PACKET_H
#define CAPI_ROCKX_PACKET_H

#include "rockx_data_type.h"
#include "rockx_tensor.h"
#include "rockx_image.h"
#include "rockx_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ROCKX_PACKET_TYPE_NAME_INT "Int"
#define ROCKX_PACKET_TYPE_NAME_LONG "Long"
#define ROCKX_PACKET_TYPE_NAME_DOUBLE "Double"
#define ROCKX_PACKET_TYPE_NAME_FLOAT "Float"
#define ROCKX_PACKET_TYPE_NAME_STRING "String"
#define ROCKX_PACKET_TYPE_NAME_POINTER "Pointer"
#define ROCKX_PACKET_TYPE_NAME_ARRAY "Array"
#define ROCKX_PACKET_TYPE_NAME_TENSOR "Tensor"
#define ROCKX_PACKET_TYPE_NAME_IMAGE "Image"
#define ROCKX_PACKET_TYPE_NAME_AUDIO "Audio"

/**
 * @brief Packet
 *
 */
typedef struct RockXPacket RockXPacket;

/**
 * @brief Packet state
 *
 */
typedef enum RockXPacketState {
    ROCKX_PACKET_STATE_NONE = 0,  //< Initial state
    ROCKX_PACKET_STATE_DOING,     //< Packet is being processed
    ROCKX_PACKET_STATE_READY,     //< Packet processing completed
    ROCKX_PACKET_STATE_OUTPUT,    //< Packet has been output
    ROCKX_PACKET_STATE_RELEASE,   //< Packet has been released
    ROCKX_PACKET_STATE_ERROR,     //< Packet processing error
    ROCKX_PACKET_STATE_INVALID,   //< Packet is invalid
} RockXPacketState;

/**
 * @brief Packet type
 *
 */
typedef enum RockXPacketType {
    ROCKX_PACKET_TYPE_NONE,    ///< No type
    ROCKX_PACKET_TYPE_IMAGE,   ///< Image
    ROCKX_PACKET_TYPE_AUDIO,   ///< Audio
    ROCKX_PACKET_TYPE_TENSOR,  ///< Tensor
    ROCKX_PACKET_TYPE_INT,     ///< Int
    ROCKX_PACKET_TYPE_LONG,    ///< Long
    ROCKX_PACKET_TYPE_FLOAT,   ///< Float
    ROCKX_PACKET_TYPE_DOUBLE,  ///< Double
    ROCKX_PACKET_TYPE_STRING,  ///< String
    ROCKX_PACKET_TYPE_POINTER, ///< Pointer
    ROCKX_PACKET_TYPE_ARRAY,   ///< Array
} RockXPacketType;

struct RockXPacket;

/**
 * @brief Packet release function
 *
 * @param packet Packet
 * @param userdata User data
 */
typedef void (*RockXPacketReleaseCallback)(struct RockXPacket* packet, void* userdata);

/**
 * @brief Packet copy function
 *
 * @param src Source packet
 * @param dst Destination packet
 */
typedef void (*RockXPacketCopyCallback)(struct RockXPacket* src, struct RockXPacket* dst);

/**
 * @brief Packet dump function
 *
 * @param path Path
 * @param packet Packet
 * @param userdata User data
 */
typedef void (*RockXPacketDumpCallback)(const char* path, struct RockXPacket* packet, void* userdata);

/**
 * @brief Packet data management mode
 *
 */
typedef enum {
    ROCKX_PACKET_MODE_COPY,    //< Default
    ROCKX_PACKET_MODE_SHARED,  //< Support multiple Packets sharing data, use RockXPacketShareData for shared data
                               // management
    ROCKX_PACKET_MODE_UNIQUE,  //< Only one Packet can exclusively manage data, use RockXPacketMoveData for data
                               // management transfer
} RockXPacketMode;

/**
 * @brief Packet callback
 *
 */
typedef struct RockXPacketCallback
{
    RockXPacketReleaseCallback OnRelease;  ///< Release callback
    RockXPacketCopyCallback OnCopy;        ///< Copy callback
    RockXPacketDumpCallback OnDump;        ///< Dump callback
} RockXPacketCallback;

/**
 * @brief Create a data packet
 * 
 * @param name Data packet name
 * @return RockXPacket* Data packet instance
 */
RockXPacket* RockXPacketCreate(char* name);

/**
 * @brief Create a data packet array
 * 
 * @param num Data packet number
 * @return RockXPacket* Data packet instance
 */
RockXPacket* RockXPacketCreateArray(int num);

/**
 * @brief Initialize a data packet
 *
 * @param packet Data packet instance
 * @param name Data packet name
 * @param mode Data packet mode
 * @param state Data packet state
 * @return int
 */
int RockXPacketInit(RockXPacket* packet, const char* name, RockXPacketMode mode, RockXPacketState state);

/**
 * @brief Set packet name
 * 
 * @param packet packet instance
 * @param name packet name
 * @return int
 */
int RockXPacketSetName(RockXPacket* packet, const char* name);

/**
 * @brief Get packet name
 * 
 * @param packet packet instance
 * @return char* packet name
 */
char* RockXPacketGetName(RockXPacket* packet);

/**
 * @brief Set packet state
 * 
 * @param packet packet instance
 * @param state packet state
 * @return int
 */
int RockXPacketSetState(RockXPacket* packet, RockXPacketState state);

/**
 * @brief Get packet state
 * 
 * @param packet packet instance
 * @return RockXPacketState packet state
 */
RockXPacketState RockXPacketGetState(RockXPacket* packet);

/**
 * @brief Set packet type
 * 
 * @param packet packet instance
 * @param type packet type
 * @return int
 */
int RockXPacketSetType(RockXPacket* packet, RockXPacketType type);

/**
 * @brief Get packet type
 * 
 * @param packet packet instance
 * @return RockXPacketType packet type
 */
RockXPacketType RockXPacketGetType(RockXPacket* packet);

/**
 * @brief Set packet type name
 * 
 * @param packet packet instance
 * @param typeName packet type name
 * @return int
 */
int RockXPacketSetTypeName(RockXPacket* packet, const char* typeName);

/**
 * @brief Get packet type name
 * 
 * @param packet packet instance
 * @return char* packet type name
 */
char* RockXPacketGetTypeName(RockXPacket* packet);

/**
 * @brief Set packet frame index
 * 
 * @param packet packet instance
 * @param index packet frame index
 * @return int
 */
int RockXPacketSetFrameIndex(RockXPacket* packet, unsigned long index);

/**
 * @brief Get packet frame index
 * 
 * @param packet packet instance
 * @return unsigned long packet frame index
 */
unsigned long RockXPacketGetFrameIndex(RockXPacket* packet);

/**
 * @brief Set packet channel
 * 
 * @param packet packet instance
 * @param channel packet channel
 * @return int
 */
int RockXPacketSetChannel(RockXPacket* packet, int channel);

/**
 * @brief Get packet channel
 * 
 * @param packet packet instance
 * @return int packet channel
 */
int RockXPacketGetChannel(RockXPacket* packet);

/**
 * @brief Set packet ext data
 * 
 * @param packet packet instance
 * @param extData ext data
 * @return int
 */
int RockXPacketSetExtData(RockXPacket* packet, void* extData);

/**
 * @brief Get packet ext data
 * 
 * @param packet packet instance
 * @return void* ext data
 */
void* RockXPacketGetExtData(RockXPacket* packet);

/**
 * @brief Set packet userdata
 * 
 * @param packet packet instance
 * @param userdata userdata
 * @return int
 */
int RockXPacketSetUserdata(RockXPacket* packet, void* userdata);

/**
 * @brief Get packet userdata
 * 
 * @param packet packet instance
 * @return void* userdata
 */
void* RockXPacketGetUserdata(RockXPacket* packet);

/**
 * @brief Get packet at index
 * 
 * @param packets packet array
 * @param index packet index
 * @return RockXPacket* packet instance
 */
RockXPacket* RockXPacketsAt(RockXPacket* packets, int index);

/**
 * @brief Copy a data packet
 *
 * @param srcPacket Source data packet
 * @param dstPacket Destination data packet
 * @return int
 */
int RockXPacketCopyData(RockXPacket* srcPacket, RockXPacket* dstPacket);

/**
 * @brief Share a data packet
 *
 * @param srcPacket Source data packet
 * @param dstPacket Destination data packet
 * @return int
 */
int RockXPacketShareData(RockXPacket* srcPacket, RockXPacket* dstPacket);

/**
 * @brief Transfer ownership of data packet (for operations like data release) to another data packet
 *
 * @param srcPacket Source data packet
 * @param dstPacket Destination data packet
 * @return int
 */
int RockXPacketMoveData(RockXPacket* srcPacket, RockXPacket* dstPacket);

/**
 * @brief release packet
 *
 * @param packet Data packet instance
 * @param userdata User data
 * @return int
 */
int RockXPacketRelease(RockXPacket* packet);

/**
 * @brief Set data packet data to an integer value
 *
 * @param packet Data packet
 * @param v Value to set
 * @return int
 */
int RockXPacketSetInt(RockXPacket* packet, int v);

/**
 * @brief Set data packet data to a long integer value
 *
 * @param packet Data packet
 * @param v Value to set
 * @return int
 */
int RockXPacketSetLong(RockXPacket* packet, long v);

/**
 * @brief Set data packet data to a double floating-point value
 *
 * @param packet Data packet
 * @param v Value to set
 * @return int
 */
int RockXPacketSetDouble(RockXPacket* packet, double v);

/**
 * @brief Set data packet data to a floating-point value
 *
 * @param packet Data packet
 * @param v Value to set
 * @return int
 */
int RockXPacketSetFloat(RockXPacket* packet, float v);

/**
 * @brief Set data packet data to a string
 *
 * @param packet Data packet
 * @param v Value to set
 * @param needRelease Whether needs to be released
 * @return int
 */
int RockXPacketSetString(RockXPacket* packet, const char* v, int needRelease);

/**
 * @brief Set data packet data to a pointer
 *
 * @param packet Data packet
 * @param v Value to set
 * @param needRelease Whether needs to be released
 * @return int
 */
int RockXPacketSetPointer(RockXPacket* packet, void* v, int needRelease, const char* typeName);

/**
 * @brief Set data packet data to an array
 *
 * @param packet Data packet
 * @param v Value to set
 * @param count Count
 * @param needRelease Whether needs to be released
 */
int RockXPacketSetArray(RockXPacket* packet, void* v, int count, int needRelease);

/**
 * @brief Set data packet data to a tensor
 *
 * @param packet Data packet
 * @param tensor Tensor
 * @return int
 */
int RockXPacketSetTensor(RockXPacket* packet, RockXTensor* tensor);

/**
 * @brief Set data packet data to an image
 *
 * @param packet Data packet
 * @param image Image
 * @return int Error code
 */
int RockXPacketSetImage(RockXPacket* packet, RockXImage* image);

/**
 * @brief Set data packet data to an audio
 *
 * @param packet Data packet
 * @param audio Audio
 * @return int
 */
int RockXPacketSetAudio(RockXPacket* packet, RockXAudioWav* audio);

/**
 * @brief Set data packet data to an audio
 *
 * @param packet Data packet
 * @param state Stream state
 * @param sampleRate Sample rate
 * @param channels Number of channels
 * @param sampleNum Number of samples
 * @param dtype Data type
 * @param data Data (only assign the data pointer to the RockXAudioWav's data field, need to pay attention to the memory lifecycle)
 * @return int
 */
int RockXPacketSetAudio2(RockXPacket* packet, RockXStreamState state, int sampleRate, int channels, int sampleNum, RockXDataType dtype, void* data);

/**
 * @brief Get packet data as an integer value
 * 
 * @param packet packet instance
 * @param defValue default value
 * @return int
 */
int RockXPacketGetInt(RockXPacket* packet, int defValue);

/**
 * @brief Get packet data as a long integer value
 * 
 * @param packet packet instance
 * @param defValue default value
 * @return long
 */
long RockXPacketGetLong(RockXPacket* packet, long defValue);

/**
 * @brief Get packet data as a float value
 * 
 * @param packet packet instance
 * @param defValue default value
 * @return float
 */
float RockXPacketGetFloat(RockXPacket* packet, float defValue);

/**
 * @brief Get packet data as a double floating-point value
 * 
 * @param packet packet instance
 * @param defValue default value
 * @return double
 */
double RockXPacketGetDouble(RockXPacket* packet, double defValue);

/**
 * @brief Get packet data as a pointer
 * 
 * @param packet packet instance
 * @param defValue default value
 * @return void*
 */
void* RockXPacketGetPointer(RockXPacket* packet, void* defValue);

/**
 * @brief Get packet data as an array
 * 
 * @param packet packet instance
 * @param count packet count
 * @return void*
 */
void* RockXPacketGetArray(RockXPacket* packet, int* count);

/**
 * @brief Get packet data as an image
 * 
 * @param packet packet instance
 * @return RockXImage* image
 */
RockXImage* RockXPacketGetImage(RockXPacket* packet);

/**
 * @brief Get packet data as a tensor
 * 
 * @param packet packet instance
 * @return RockXTensor* tensor
 */
RockXTensor* RockXPacketGetTensor(RockXPacket* packet);

/**
 * @brief Get packet data as a string
 * 
 * @param packet packet instance
 * @return char* string
 */
char* RockXPacketGetString(RockXPacket* packet, char* defValue);

/**
 * @brief Get packet data as an audio
 * 
 * @param packet packet instance
 * @return RockXAudioWav* audio
 */
RockXAudioWav* RockXPacketGetAudio(RockXPacket* packet);

/**
 * @brief Set packet release callback
 * 
 * @param packet packet instance
 * @param releaseCallback release callback
 * @return int
 */
int RockXPacketSetReleaseCallback(RockXPacket* packet, RockXPacketReleaseCallback releaseCallback);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif