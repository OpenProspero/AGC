/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_DRIVER_H
#define OPENAGC_DRIVER_H

#include "openagc/openagc.h"
#include "openagc/pm4_cb_capture_fw940.h"
#include "openagc/pm4_ib_dump_fw940.h"
#include "openagc/presentation_refuse_fw940.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_GPU_API_VERSION 1u
#define OPENAGC_GPU_DEFAULT_MEMORY_BUDGET_BYTES 16777216u

typedef struct openagc_gpu_device openagc_gpu_device;
typedef struct openagc_gpu_memory openagc_gpu_memory;
typedef struct openagc_gpu_buffer openagc_gpu_buffer;
typedef struct openagc_gpu_command_buffer openagc_gpu_command_buffer;
typedef struct openagc_gpu_queue openagc_gpu_queue;
typedef struct openagc_gpu_fence openagc_gpu_fence;

typedef uint32_t openagc_gpu_memory_class;
enum {
    OPENAGC_GPU_MEMORY_HOST_REFERENCE = 1u,
    OPENAGC_GPU_MEMORY_DEVICE_LOCAL = 2u
};

typedef uint32_t openagc_gpu_buffer_usage;
enum {
    OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT = 1u,
    OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT = 2u,
    /* Host reflection metadata only; no shader execution is available. */
    OPENAGC_GPU_BUFFER_SHADER_READ_BIT = 16u,
    /* Binding record only. No vertex fetch runs. */
    OPENAGC_GPU_BUFFER_VERTEX_BIT = 32u,
    /* Binding record only. No index fetch runs. */
    OPENAGC_GPU_BUFFER_INDEX_BIT = 64u,
    OPENAGC_GPU_BUFFER_INDIRECT_BIT = 128u
};

typedef uint32_t openagc_gpu_queue_family;
enum {
    OPENAGC_GPU_QUEUE_COPY = 1u,
    OPENAGC_GPU_QUEUE_GRAPHICS = 2u,
    OPENAGC_GPU_QUEUE_COMPUTE = 3u
};

typedef struct openagc_gpu_device_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t memory_budget_bytes;
} openagc_gpu_device_desc;

typedef struct openagc_gpu_capabilities {
    uint32_t struct_size;
    openagc_backend backend;
    uint32_t gpu_execution;
    uint32_t video_output;
    uint32_t host_copy_simulation;
    uint32_t reference_pm4_encoding;
    uint64_t memory_budget_bytes;
    uint32_t max_allocation_bytes;
    uint32_t max_allocations;
    uint32_t max_buffers;
    uint32_t max_command_buffers;
    uint32_t max_commands_per_buffer;
    uint32_t max_command_words;
    uint32_t max_copy_bytes;
    uint32_t max_queues;
    uint32_t max_fences;
} openagc_gpu_capabilities;

typedef struct openagc_gpu_memory_desc {
    uint32_t struct_size;
    openagc_gpu_memory_class memory_class;
    uint64_t size_bytes;
} openagc_gpu_memory_desc;

typedef struct openagc_gpu_buffer_desc {
    uint32_t struct_size;
    openagc_gpu_buffer_usage usage;
    uint64_t size_bytes;
} openagc_gpu_buffer_desc;

typedef struct openagc_gpu_command_buffer_desc {
    uint32_t struct_size;
    uint32_t max_commands;
    uint32_t max_words;
} openagc_gpu_command_buffer_desc;

typedef struct openagc_gpu_queue_desc {
    uint32_t struct_size;
    openagc_gpu_queue_family family;
} openagc_gpu_queue_desc;

typedef struct openagc_gpu_recording_view {
    uint32_t struct_size;
    uint32_t command_count;
    uint32_t word_count;
    /* Synthetic host-address PM4; never submit this snapshot to a console. */
    const uint32_t *words;
} openagc_gpu_recording_view;

typedef struct openagc_gpu_submission_view {
    uint32_t struct_size;
    uint32_t word_count;
    uint64_t submission_id;
    uint32_t gpu_submitted;
    /* Host-derived packets, valid until the next successful submit/destroy. */
    const uint32_t *words;
} openagc_gpu_submission_view;

typedef struct openagc_gpu_fence_info {
    uint32_t struct_size;
    uint32_t signaled;
    uint32_t expected_value;
    uint32_t observed_value;
    uint64_t submission_id;
} openagc_gpu_fence_info;

#define OPENAGC_GPU_DEVICE_DESC_INIT \
    { (uint32_t)sizeof(openagc_gpu_device_desc), OPENAGC_GPU_API_VERSION, \
      OPENAGC_GPU_DEFAULT_MEMORY_BUDGET_BYTES }
#define OPENAGC_GPU_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_gpu_capabilities), 0u, 0u, 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_GPU_MEMORY_DESC_INIT(bytes_value) \
    { (uint32_t)sizeof(openagc_gpu_memory_desc), OPENAGC_GPU_MEMORY_HOST_REFERENCE, \
      (bytes_value) }
#define OPENAGC_GPU_BUFFER_DESC_INIT(bytes_value, usage_value) \
    { (uint32_t)sizeof(openagc_gpu_buffer_desc), (usage_value), (bytes_value) }
#define OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(commands_value, words_value) \
    { (uint32_t)sizeof(openagc_gpu_command_buffer_desc), (commands_value), (words_value) }
#define OPENAGC_GPU_QUEUE_DESC_INIT \
    { (uint32_t)sizeof(openagc_gpu_queue_desc), OPENAGC_GPU_QUEUE_COPY }
#define OPENAGC_GPU_RECORDING_VIEW_INIT \
    { (uint32_t)sizeof(openagc_gpu_recording_view), 0u, 0u, (const uint32_t *)0 }
#define OPENAGC_GPU_SUBMISSION_VIEW_INIT \
    { (uint32_t)sizeof(openagc_gpu_submission_view), 0u, 0u, 0u, (const uint32_t *)0 }
#define OPENAGC_GPU_FENCE_INFO_INIT \
    { (uint32_t)sizeof(openagc_gpu_fence_info), 0u, 0u, 0u, 0u }

/* The context must outlive its GPU device; PS5 contexts remain denied. */
openagc_result openagc_gpu_device_create(openagc_context *context,
                                         const openagc_gpu_device_desc *desc,
                                         openagc_gpu_device **out_device);
openagc_result openagc_gpu_device_destroy(openagc_gpu_device *device);
openagc_result openagc_gpu_device_get_capabilities(
    const openagc_gpu_device *device, openagc_gpu_capabilities *capabilities);

openagc_result openagc_gpu_memory_allocate(openagc_gpu_device *device,
                                           const openagc_gpu_memory_desc *desc,
                                           openagc_gpu_memory **out_memory);
openagc_result openagc_gpu_memory_destroy(openagc_gpu_memory *memory);
openagc_result openagc_gpu_memory_write(openagc_gpu_memory *memory,
                                       uint64_t offset, const void *data,
                                       uint64_t size_bytes);
openagc_result openagc_gpu_memory_read(const openagc_gpu_memory *memory,
                                      uint64_t offset, void *data,
                                      uint64_t size_bytes);
/* Host read of a bound buffer. Does not submit a copy. */
openagc_result openagc_gpu_buffer_read(const openagc_gpu_buffer *buffer,
                                       uint64_t offset, void *data,
                                       uint64_t size_bytes);
openagc_result openagc_gpu_buffer_write(openagc_gpu_buffer *buffer, uint64_t offset,
                                        const void *data, uint64_t size_bytes);

/*
 * Host-only store-const: encode the FW9.40 compute PM4 packet (51 dwords) and
 * write 0xA5A5A5A5 into destination. Does not open a compute queue or set
 * gpu_execution. Inspect with openagc_gpu_device_get_last_compute.
 */
openagc_result openagc_gpu_host_store_const(openagc_gpu_device *device,
                                            openagc_gpu_buffer *destination,
                                            uint64_t destination_offset);
/* Console Step I: 8-lane flat_store span (32 bytes) at destination. */
openagc_result openagc_gpu_host_store_span(openagc_gpu_device *device,
                                           openagc_gpu_buffer *destination,
                                           uint64_t destination_offset);
/* Console Step J: two store_span dispatches (64 bytes) in one IB. */
openagc_result openagc_gpu_host_store_span2(openagc_gpu_device *device,
                                            openagc_gpu_buffer *destination,
                                            uint64_t destination_offset);
/* Steps I–K: 1..8 store_span dispatches (32..256 bytes) in one IB. */
openagc_result openagc_gpu_host_store_span_n(openagc_gpu_device *device,
                                             openagc_gpu_buffer *destination,
                                             uint64_t destination_offset,
                                             uint32_t span_count);
openagc_result openagc_gpu_device_get_last_compute(const openagc_gpu_device *device,
                                                   openagc_gpu_submission_view *view);

/*
 * Host-only WRITE_DATA fill: encode the FW9.40 CP WRITE_DATA+EOP packet
 * (console-proven Step D for one dword; Step E extends to N dwords up to
 * a 4x4 RGBA8 tile) and write the pattern. Does not set gpu_execution.
 * Inspect with openagc_gpu_device_get_last_write.
 * Memory form is used by graphics color clears of contiguous tiles;
 * buffer form requires COPY_DESTINATION usage.
 */
openagc_result openagc_gpu_host_write_data_memory(openagc_gpu_device *device,
                                                  openagc_gpu_memory *memory,
                                                  uint64_t memory_offset,
                                                  uint32_t value,
                                                  uint32_t dword_count);
/* One WRITE_DATA per row (pitch stride) then one EOP; ≤ MAX_ROWS × ≤16 dwords. */
openagc_result openagc_gpu_host_write_data_rows(openagc_gpu_device *device,
                                                openagc_gpu_memory *memory,
                                                uint64_t memory_offset,
                                                uint32_t pitch_bytes,
                                                uint32_t value,
                                                uint32_t dwords_per_row,
                                                uint32_t row_count);
/* Buffer form of host_write_data_rows; requires COPY_DESTINATION usage. */
openagc_result openagc_gpu_host_write_data_buffer_rows(openagc_gpu_device *device,
                                                       openagc_gpu_buffer *destination,
                                                       uint64_t destination_offset,
                                                       uint32_t pitch_bytes,
                                                       uint32_t value,
                                                       uint32_t dwords_per_row,
                                                       uint32_t row_count);
/*
 * Host-only multi-column WRITE_DATA grid + one EOP (console Step N):
 * column_count packets per row, each ≤16 dwords. Requires memory span
 * covering the grid. Does not set gpu_execution.
 */
openagc_result openagc_gpu_host_write_data_grid(openagc_gpu_device *device,
                                                openagc_gpu_memory *memory,
                                                uint64_t memory_offset,
                                                uint32_t pitch_bytes,
                                                uint32_t value,
                                                uint32_t dwords_per_column,
                                                uint32_t column_count,
                                                uint32_t row_count);
openagc_result openagc_gpu_host_write_data(openagc_gpu_device *device,
                                           openagc_gpu_buffer *destination,
                                           uint64_t destination_offset,
                                           uint32_t value,
                                           uint32_t dword_count);
/*
 * Host-only DMA + WRITE_DATA + EOP (console Step H): copy then fill the
 * start of the destination in one PM4 snapshot. Requires copy-source and
 * copy-destination. Does not set gpu_execution.
 */
openagc_result openagc_gpu_host_dma_write_data(openagc_gpu_device *device,
                                               openagc_gpu_buffer *source,
                                               uint64_t source_offset,
                                               openagc_gpu_buffer *destination,
                                               uint64_t destination_offset,
                                               uint32_t dma_bytes,
                                               uint32_t value,
                                               uint32_t dword_count);
openagc_result openagc_gpu_device_get_last_write(const openagc_gpu_device *device,
                                                 openagc_gpu_submission_view *view);
/*
 * Host-only: copy a graphics SET_CONTEXT/SET_SH register program and append
 * the shared FW9.40 EOP+NOP trailer into the write-data snapshot slot.
 * Does not submit, does not emit DRAW, and does not set gpu_execution.
 * register_dword_count must leave room for OPENAGC_PM4_EOP_WITH_NOP_WORDS.
 */
openagc_result openagc_gpu_host_graphics_register_eop(openagc_gpu_device *device,
                                                      const uint32_t *register_words,
                                                      uint32_t register_dword_count);
/*
 * Structural CB/DB capture verify: kind/firmware/count bounds and
 * SHA-256(words) must match the manifest. Does not invent packets and does
 * not set evidence_qualified (pin table is empty).
 */
openagc_result openagc_cb_capture_verify(const openagc_cb_capture_manifest *manifest,
                                         const uint32_t *words);
/*
 * 1 only when manifest digest matches an independently owned evidence pin.
 * Always 0 until OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT is nonzero.
 */
uint32_t openagc_cb_capture_evidence_qualified(const openagc_cb_capture_manifest *manifest);
/*
 * Always UNSUPPORTED_OPERATION. Inventing CB/DB bind or DRAW dwords is
 * forbidden; supply a verified capture artifact instead.
 */
openagc_result openagc_cb_capture_encode_invent(openagc_cb_capture_kind kind,
                                                uint32_t *words, uint32_t max_words,
                                                uint32_t *out_count);
/*
 * Host-only: verify then copy a CB_BIND or DB_BIND capture into the write
 * snapshot. DRAW captures remain NOT_READY. Does not submit, does not set
 * gpu_execution, and leaves evidence_qualified=0 until a pin exists.
 */
openagc_result openagc_gpu_host_cb_bind_from_capture(
    openagc_gpu_device *device, const openagc_cb_capture_manifest *manifest,
    const uint32_t *words);
openagc_result openagc_gpu_device_get_cb_capture_info(const openagc_gpu_device *device,
                                                     openagc_cb_capture_info *info);
/*
 * Host-only: parse a console IB dump log (openagc-ib-dump: ...) into words.
 * Accepts tag=step-u, ctxreg-cb, ctxreg-abs, ctxreg-rt, ctxreg-cb-bind,
 * ctxreg-cb-bind-full, mmio-tilemode. Never sets evidence_qualified: the
 * owned matches are separate helpers
 * (openagc_ib_dump_cb_bind_legacy_base_match for the Step-Z probe layout,
 * openagc_ib_dump_cb_bind_full_match for the Step-AB nine-word set). Does
 * not invent CB/DB/DRAW packets and does not submit.
 */
openagc_result openagc_ib_dump_parse(const char *text, uint32_t *words,
                                     uint32_t max_words, openagc_ib_dump_info *info);
/* Reserve a synthetic host VA span (page-aligned). Never maps console memory. */
openagc_result openagc_gpu_device_reserve_synthetic_va(openagc_gpu_device *device,
                                                       uint64_t size_bytes,
                                                       uint64_t *out_va);
/*
 * Host-only synthetic device address for a bound memory span. offset must fit
 * in the allocation. Never maps console memory.
 */
openagc_result openagc_gpu_memory_get_device_address(const openagc_gpu_memory *memory,
                                                     uint64_t offset, uint64_t *out_va);

openagc_result openagc_gpu_buffer_create(openagc_gpu_device *device,
                                         const openagc_gpu_buffer_desc *desc,
                                         openagc_gpu_buffer **out_buffer);
openagc_result openagc_gpu_buffer_bind_memory(openagc_gpu_buffer *buffer,
                                              openagc_gpu_memory *memory,
                                              uint64_t memory_offset);
/* Drops a bind that no command still references. A second drop is BAD_STATE. */
openagc_result openagc_gpu_buffer_unbind_memory(openagc_gpu_buffer *buffer);
openagc_result openagc_gpu_buffer_destroy(openagc_gpu_buffer *buffer);

openagc_result openagc_gpu_command_buffer_create(
    openagc_gpu_device *device, const openagc_gpu_command_buffer_desc *desc,
    openagc_gpu_command_buffer **out_command_buffer);
openagc_result openagc_gpu_command_buffer_destroy(openagc_gpu_command_buffer *command_buffer);
openagc_result openagc_gpu_command_buffer_begin(openagc_gpu_command_buffer *command_buffer);
openagc_result openagc_gpu_command_copy_buffer(
    openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_buffer *source, uint64_t source_offset,
    openagc_gpu_buffer *destination, uint64_t destination_offset,
    uint64_t size_bytes);
openagc_result openagc_gpu_command_buffer_end(openagc_gpu_command_buffer *command_buffer);
openagc_result openagc_gpu_command_buffer_reset(openagc_gpu_command_buffer *command_buffer);
/* The view expires on a successful reset or command-buffer destruction. */
openagc_result openagc_gpu_command_buffer_get_recording(
    const openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_recording_view *view);

openagc_result openagc_gpu_queue_create(openagc_gpu_device *device,
                                        const openagc_gpu_queue_desc *desc,
                                        openagc_gpu_queue **out_queue);
openagc_result openagc_gpu_queue_destroy(openagc_gpu_queue *queue);
/* Synchronous CPU copy simulation; never GPU submission or VideoOut. */
openagc_result openagc_gpu_queue_submit(openagc_gpu_queue *queue,
                                       openagc_gpu_command_buffer *command_buffer,
                                       openagc_gpu_fence *fence);
openagc_result openagc_gpu_queue_get_last_submission(
    const openagc_gpu_queue *queue, openagc_gpu_submission_view *view);

openagc_result openagc_gpu_fence_create(openagc_gpu_device *device,
                                       openagc_gpu_fence **out_fence);
openagc_result openagc_gpu_fence_destroy(openagc_gpu_fence *fence);
/* Returns NOT_READY if unsignaled, otherwise OK; fills info in both cases. */
openagc_result openagc_gpu_fence_poll(const openagc_gpu_fence *fence,
                                     openagc_gpu_fence_info *info);
openagc_result openagc_gpu_fence_reset(openagc_gpu_fence *fence);

#ifdef __cplusplus
}
#endif

#endif
