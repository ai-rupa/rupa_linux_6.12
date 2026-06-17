#ifndef _ROCKX_AUDIO_H_
#define _ROCKX_AUDIO_H_

#include <stddef.h>
#include <stdint.h>

#include "rockx_data_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stream data packet state
 *
 */
typedef enum RockXStreamState {
    ROCKX_STREAM_STATE_FIRST = 0,  //< The first data packet of the stream
    ROCKX_STREAM_STATE_CONTINUE,   //< The middle continuous data packet of the stream
    ROCKX_STREAM_STATE_LAST,       //< The last data packet of the stream
} RockXStreamState;

/**
 * @brief Audio wav data (PCM data)
 */
typedef struct RockXAudioWav
{
    int sampleRate;           ///< Audio sample rate
    int channels;             ///< Audio channels
    RockXStreamState state;   ///< Audio stream state
    unsigned long timestamp;  ///< Audio timestamp
    int sampleNum;            ///< Audio sample number
    RockXDataType dtype;      ///< Audio data type
    void* data;               ///< Audio data
} RockXAudioWav;

/**
 * @brief stream state enum to string
 * 
 * @param state stream state enum
 * @return const char* stream state string
 */
static inline const char* RockXStreamStateToString(RockXStreamState state)
{
    switch (state) {
        case ROCKX_STREAM_STATE_FIRST: return "FIRST";
        case ROCKX_STREAM_STATE_CONTINUE: return "CONTINUE";
        case ROCKX_STREAM_STATE_LAST: return "LAST";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get float data from audio wav
 * 
 * @param audio_wav 
 * @param pcm_data_float 
 * @param data_size 
 * @return int 
 */
int RockXAudioGetFloatDataMono(RockXAudioWav* audio_wav, float* pcm_data_float, size_t data_size);

/**
 * @brief Get int16 data from audio wav
 * 
 * @param audio_wav 
 * @param pcm_data_int16 
 * @param data_size 
 * @return int 
 */
int RockXAudioGetS16DataMono(RockXAudioWav* audio_wav, int16_t* pcm_data_int16, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif