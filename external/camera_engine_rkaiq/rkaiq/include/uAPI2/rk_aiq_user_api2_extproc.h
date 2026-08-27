#ifndef _RK_AIQ_USER_API2_EXTPROC_H_
#define _RK_AIQ_USER_API2_EXTPROC_H_

#include "common/rk_aiq.h"

#ifndef RK_AIQ_SYS_CTX_T
#define RK_AIQ_SYS_CTX_T
typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;
#endif

RKAIQ_BEGIN_DECLARE

/**
 * enum rk_aiq_extproc_type_e - External processing data types
 * @RK_AIQ_EXTPROC_TYPE_BAYER_RAW: Bayer RAW frame (not currently supported)
 * @RK_AIQ_EXTPROC_TYPE_BTNR_IIR: Bay3D IIR buffer for external NR processing
 * @RK_AIQ_EXTPROC_TYPE_MAX:       Sentinel value, do not use
 */
typedef enum rk_aiq_extproc_type_e {
    RK_AIQ_EXTPROC_TYPE_BAYER_RAW = 0,
    RK_AIQ_EXTPROC_TYPE_BTNR_IIR,
    RK_AIQ_EXTPROC_TYPE_MAX,
} rk_aiq_extproc_type_t;

#define RK_AIQ_EXTPROC_TYPE_BAYER_RAW_MASK (1U << RK_AIQ_EXTPROC_TYPE_BAYER_RAW)
#define RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK  (1U << RK_AIQ_EXTPROC_TYPE_BTNR_IIR)

/**
 * struct rk_aiq_extproc_raw_cfg_s - Configuration for Bayer RAW external processing
 * @width:        RAW frame width in pixels
 * @height:       RAW frame height in pixels
 * @format:       FourCC pixel format identifier
 * @bit_depth:    Bits per pixel (e.g. 10, 12)
 * @export_point: ISP pipeline stage at which RAW data is exported
 * @reserved:     Reserved for future use, must be zero
 */
typedef struct rk_aiq_extproc_raw_cfg_s {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t bit_depth;
    uint32_t export_point;

    uint32_t reserved[16];
} rk_aiq_extproc_raw_cfg_t;

/**
 * struct rk_aiq_extproc_btnr_iir_cfg_s - Configuration for Bay3D IIR external processing
 * @mode:       ISP working mode:
 *                0 - ISP works as a whole, without FE/BE split
 *                1 - ISP is split into isp_fe and isp_be
 *                2 - ISP is split into isp_fe and (isp_fe + isp_be)
 * @wr_linecnt: Line count that triggers the BTNR IIR write interrupt
 * @rd_linecnt: Line count that triggers the isp_be read interrupt
 * @wr_mode:    Write interrupt generation mode:
 *                0 - one event per frame
 *                1 - one event for every @wr_linecnt lines
 * @reserved:   Reserved for future use, must be zero
 *
 * Corresponds to struct rkisp_aiisp_cfg in the ISP driver.
 */
typedef struct rk_aiq_extproc_btnr_iir_cfg_s {
    int mode;

    int wr_linecnt;
    int rd_linecnt;
    int wr_mode;

    uint32_t reserved[16];
} rk_aiq_extproc_btnr_iir_cfg_t;

/**
 * struct rk_aiq_extproc_mem_s - Generic DMA buffer descriptor
 * @dma_fd:   DMA-BUF file descriptor
 * @vir_addr: CPU-mapped virtual address (NULL if not mapped)
 * @width:    Buffer width in pixels (0 if not applicable)
 * @height:   Buffer height in pixels (0 if not applicable)
 * @stride:   Row stride in bytes
 * @format:   FourCC pixel format identifier (0 if not applicable)
 */
typedef struct rk_aiq_extproc_mem_s {
    int dma_fd;
    void* vir_addr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
} rk_aiq_extproc_mem_t;

/**
 * struct rk_aiq_extproc_btnr_iir_buf_s - BTNR IIR buffers delivered to the application
 * @iir:      Input IIR DMA buffer (read-only for the application)
 * @gain:     Input gain DMA buffer (read-only for the application)
 * @iir_out:  Output IIR DMA buffer the application must write its result into.
 *            Points to a slot in the dedicated AIISP output pool.
 */
typedef struct rk_aiq_extproc_btnr_iir_buf_s {
    rk_aiq_extproc_mem_t iir;
    rk_aiq_extproc_mem_t gain;
    rk_aiq_extproc_mem_t iir_out;
} rk_aiq_extproc_btnr_iir_buf_t;

/**
 * struct rk_aiq_extproc_raw_buf_s - Bayer RAW buffer delivered to the application
 * @mem:         DMA buffer descriptor for the RAW frame data
 * @bit_depth:   Bits per pixel of the RAW data
 * @bayer_order: Bayer pattern order (driver-defined value)
 * @black_level: Sensor black level
 */
typedef struct rk_aiq_extproc_raw_buf_s {
    rk_aiq_extproc_mem_t mem;

    uint32_t bit_depth;
    uint32_t bayer_order;
    uint32_t black_level;
} rk_aiq_extproc_raw_buf_t;

/**
 * struct rk_aiq_extproc_buffer_s - Buffer passed from AIQ to the application callback
 * @type:         Buffer type, determines which union member is valid
 * @frame_id:     Frame sequence number
 * @timestamp:    Frame capture timestamp in nanoseconds
 * @priv:         Opaque handle; must be passed back unchanged in
 *                rk_aiq_extproc_result_s::priv when calling
 *                rk_aiq_uapi2_extProc_queueResult()
 * @u.btnr_iir:  Valid when @type == %RK_AIQ_EXTPROC_TYPE_BTNR_IIR
 * @u.raw:        Valid when @type == %RK_AIQ_EXTPROC_TYPE_BAYER_RAW
 * @reserved:     Reserved for future use
 */
typedef struct rk_aiq_extproc_buffer_s {
    rk_aiq_extproc_type_t type;

    uint32_t frame_id;
    uint64_t timestamp;

    void* priv;

    union {
        rk_aiq_extproc_btnr_iir_buf_t btnr_iir;
        rk_aiq_extproc_raw_buf_t raw;
    } u;

    uint32_t reserved[16];
} rk_aiq_extproc_buffer_t;

/**
 * struct rk_aiq_extproc_result_s - Processing result returned from the application to AIQ
 * @type:     Must match the type of the corresponding input buffer
 * @frame_id: Must match rk_aiq_extproc_buffer_s::frame_id of the input buffer
 * @priv:     Must be set to rk_aiq_extproc_buffer_s::priv of the input buffer
 * @reserved: Reserved for future use, must be zero
 */
typedef struct rk_aiq_extproc_result_s {
    rk_aiq_extproc_type_t type;

    uint32_t frame_id;
    void* priv;

    uint32_t reserved[16];
} rk_aiq_extproc_result_t;

/**
 * typedef rk_aiq_extproc_cb - Application callback invoked on each new buffer
 * @buf:       Buffer delivered by AIQ; valid only for the duration of the callback
 * @user_data: Opaque pointer supplied via rk_aiq_extproc_init_params_s::user_data
 *
 * The callback must call rk_aiq_uapi2_extProc_queueResult() before returning.
 * The buffer and inflight slot stay valid until the matching
 * RKISP_AIISP_RD_LINECNT_ID event; AIQ then recycles the request.  If the
 * buffer cannot be processed, call rk_aiq_uapi2_extProc_releaseBuffer() instead.
 *
 * Return: %XCAM_RETURN_NO_ERROR on success, error code otherwise
 */
typedef XCamReturn (*rk_aiq_extproc_cb)(const rk_aiq_extproc_buffer_t* buf, void* user_data);

/**
 * struct rk_aiq_extproc_init_params_s - Initialisation parameters for external processing
 * @enable_mask:   Bitmask of enabled types (e.g. %RK_AIQ_EXTPROC_TYPE_BTNR_IIR_MASK)
 * @cb:            Mandatory callback invoked for each new buffer
 * @user_data:     Opaque pointer forwarded to every @cb invocation; may be NULL
 * @raw_cfg:       RAW configuration, required when @enable_mask includes
 *                 %RK_AIQ_EXTPROC_TYPE_BAYER_RAW_MASK; may be NULL otherwise
 * @btnr_iir_cfg: Bay3D IIR configuration; if NULL, sensor dimensions are used
 *                 as defaults
 * @reserved:      Reserved for future use, must be zero
 */
typedef struct rk_aiq_extproc_init_params_s {
    uint32_t enable_mask;

    rk_aiq_extproc_cb cb;
    void* user_data;

    const rk_aiq_extproc_raw_cfg_t* raw_cfg;
    const rk_aiq_extproc_btnr_iir_cfg_t* btnr_iir_cfg;

    uint32_t reserved[16];
} rk_aiq_extproc_init_params_t;

/**
 * rk_aiq_uapi2_extProc_init() - Initialise external processing
 * @ctx:    AIQ system context, must be non-NULL and have a valid manager
 * @params: Initialisation parameters; @params->cb and @params->enable_mask
 *          must be set
 *
 * Must be called after rk_aiq_uapi2_sysctl_init().  Can be called before or
 * after rk_aiq_uapi2_sysctl_prepare().  Calling this function again without
 * a preceding rk_aiq_uapi2_extProc_deinit() replaces the current context.
 *
 * Return: %XCAM_RETURN_NO_ERROR on success, error code otherwise
 */
XCamReturn rk_aiq_uapi2_extProc_init(rk_aiq_sys_ctx_t* ctx,
                                     const rk_aiq_extproc_init_params_t* params);

/**
 * rk_aiq_uapi2_extProc_queueResult() - Return a processed buffer result to AIQ
 * @result: Processing result; @result->priv must match the corresponding
 *          rk_aiq_extproc_buffer_s::priv, and @result->type must match the
 *          buffer type
 *
 * Must be called from within the rk_aiq_extproc_cb callback.  Triggers ISP BE;
 * the request is not removed from the inflight list until the matching
 * RKISP_AIISP_RD_LINECNT_ID event (unless rk_aiq_uapi2_extProc_releaseBuffer()
 * is used).
 *
 * Return: %XCAM_RETURN_NO_ERROR on success, error code otherwise
 */
XCamReturn rk_aiq_uapi2_extProc_queueResult(const rk_aiq_extproc_result_t* result);

/**
 * rk_aiq_uapi2_extProc_releaseBuffer() - Discard a buffer without processing it
 * @buf: Buffer to release; @buf->priv must match the value received in the
 *       callback
 *
 * Call this instead of rk_aiq_uapi2_extProc_queueResult() when the application
 * decides to skip processing for a frame.  This returns the inflight slot so
 * that subsequent frames are not blocked.  It is safe (no-op) to call this
 * after rk_aiq_uapi2_extProc_queueResult() has already been called for the
 * same buffer.
 *
 * Return: %XCAM_RETURN_NO_ERROR on success, error code otherwise
 */
XCamReturn rk_aiq_uapi2_extProc_releaseBuffer(const rk_aiq_extproc_buffer_t* buf);

RKAIQ_END_DECLARE

#endif  // #ifndef _RK_AIQ_USER_API2_EXTPROC_H_
