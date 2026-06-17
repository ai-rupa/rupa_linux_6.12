#ifndef _ROCKX_MEM_H_
#define _ROCKX_MEM_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory type
 *
 */
typedef enum {
    ROCKX_MEM_TYPE_UNKNOW,   ///< Unknown
    ROCKX_MEM_TYPE_CPU,      ///< CPU
    ROCKX_MEM_TYPE_DMA_BUF,  ///< DMA buffer
    ROCKX_MEM_TYPE_DMA_HEAP  ///< RK DMA Heap
} RockXMemType;

/**
 * @brief Buffer memory
 *
 */
typedef struct RockXMemoryBuffer
{
    RockXMemType type;  ///< Memory type
    size_t size;        ///< Size
    void* virtAddr;     ///< Virtual address
    void* phyAddr;      ///< Physical address
    int fd;             ///< File descriptor
    int handle;         ///< Handle
    void* extdata;      ///< Extended data
    const char* tag;    ///< Tag
} RockXMemoryBuffer;

/**
 * @brief Allocate memory.
 *
 * @param tag Tag
 * @param size Allocation size
 * @param type Memory type
 * @param mem Memory buffer
 * @return int Error code
 */
int RockXMemAlloc(const char* tag, size_t size, RockXMemType type, RockXMemoryBuffer* mem);

/**
 * @brief Free memory.
 *
 * @param mem Memory buffer
 * @return int Error code
 */
int RockXMemFree(RockXMemoryBuffer* mem);

/**
 * @brief Sync memory.
 *
 * @param mem Memory buffer
 * @return int Error code
 */
int RockXMemSync(RockXMemoryBuffer* mem);

/**
 * @brief Free memory by virtual address.
 *
 * @param virtAddr Virtual address
 * @return int Error code
 */
int RockXMemFreeByVirtAddr(void* virtAddr);

/**
 * @brief Dump all memory.
 *
 * @return int Error code
 */
int RockXMemDumpAll();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif