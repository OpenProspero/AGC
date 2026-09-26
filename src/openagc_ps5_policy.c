/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"
#include "openagc/vulkan.h"
#include "openagc/opengl.h"

/* This translation unit is freestanding: no allocator, libc, or device imports. */
uint32_t openagc_api_version(void)
{
    return OPENAGC_API_VERSION;
}

const char *openagc_result_string(openagc_result result)
{
    switch (result) {
    case OPENAGC_OK: return "ok";
    case OPENAGC_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case OPENAGC_ERROR_INCOMPATIBLE_VERSION: return "incompatible API version or struct size";
    case OPENAGC_ERROR_UNSUPPORTED_BACKEND: return "unsupported backend";
    case OPENAGC_ERROR_UNSUPPORTED_FIRMWARE: return "firmware not qualified for PS5 access";
    case OPENAGC_ERROR_OUT_OF_RANGE: return "value out of range";
    case OPENAGC_ERROR_BAD_STATE: return "invalid lifecycle state";
    case OPENAGC_ERROR_CAPACITY: return "command capacity exhausted";
    case OPENAGC_ERROR_OUT_OF_MEMORY: return "host allocation failed";
    case OPENAGC_ERROR_OVERFLOW: return "size or counter overflow";
    case OPENAGC_ERROR_BUSY: return "resource still in use";
    case OPENAGC_ERROR_OWNERSHIP: return "objects belong to different devices";
    case OPENAGC_ERROR_NOT_READY: return "fence or compiler not ready";
    case OPENAGC_ERROR_UNSUPPORTED_OPERATION: return "operation is not supported";
    case OPENAGC_ERROR_INTEGRITY: return "artifact integrity check failed";
    default: return "unknown OpenAGC result code";
    }
}

openagc_result openagc_context_create(const openagc_context_desc *desc,
                                      openagc_context **out_context)
{
    if (out_context == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_context = 0;
    if (desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->backend == OPENAGC_BACKEND_PS5) {
        return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
    }
    return OPENAGC_ERROR_UNSUPPORTED_BACKEND;
}

openagc_result openagc_context_destroy(openagc_context *context)
{
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                        : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_device_create(openagc_context *context,
                                     const openagc_device_desc *desc,
                                     openagc_device **out_device)
{
    if (out_device == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = 0;
    if (context == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_device_destroy(openagc_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_device_get_capabilities(const openagc_device *device,
                                               openagc_capabilities *capabilities)
{
    return device == 0 || capabilities == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frame_begin(openagc_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frame_clear(openagc_device *device, openagc_color color)
{
    (void)color;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frame_rect(openagc_device *device, const openagc_rect *rect)
{
    return device == 0 || rect == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frame_present(openagc_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_device_get_last_frame(const openagc_device *device,
                                             openagc_frame_view *view)
{
    return device == 0 || view == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_create(openagc_context *context,
                                         const openagc_gpu_device_desc *desc,
                                         openagc_gpu_device **out_device)
{
    if (out_device == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = 0;
    if (context == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GPU_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_destroy(openagc_gpu_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_get_capabilities(
    const openagc_gpu_device *device, openagc_gpu_capabilities *capabilities)
{
    return device == 0 || capabilities == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_memory_allocate(openagc_gpu_device *device,
                                           const openagc_gpu_memory_desc *desc,
                                           openagc_gpu_memory **out_memory)
{
    if (out_memory == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_memory_destroy(openagc_gpu_memory *memory)
{
    return memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_memory_write(openagc_gpu_memory *memory,
                                       uint64_t offset, const void *data,
                                       uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || data == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_memory_read(const openagc_gpu_memory *memory,
                                      uint64_t offset, void *data,
                                      uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || data == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_read(const openagc_gpu_buffer *buffer,
                                       uint64_t offset, void *data,
                                       uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || data == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_write(openagc_gpu_buffer *buffer, uint64_t offset,
                                        const void *data, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || data == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_store_const(openagc_gpu_device *device,
                                            openagc_gpu_buffer *destination,
                                            uint64_t destination_offset)
{
    (void)destination_offset;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_store_span(openagc_gpu_device *device,
                                           openagc_gpu_buffer *destination,
                                           uint64_t destination_offset)
{
    (void)destination_offset;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_store_span2(openagc_gpu_device *device,
                                            openagc_gpu_buffer *destination,
                                            uint64_t destination_offset)
{
    (void)destination_offset;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_store_span_n(openagc_gpu_device *device,
                                             openagc_gpu_buffer *destination,
                                             uint64_t destination_offset,
                                             uint32_t span_count)
{
    (void)destination_offset;
    (void)span_count;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_get_last_compute(const openagc_gpu_device *device,
                                                   openagc_gpu_submission_view *view)
{
    return device == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_write_data_memory(openagc_gpu_device *device,
                                                  openagc_gpu_memory *memory,
                                                  uint64_t memory_offset,
                                                  uint32_t value,
                                                  uint32_t dword_count)
{
    (void)memory_offset;
    (void)value;
    (void)dword_count;
    return device == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_write_data_rows(openagc_gpu_device *device,
                                                openagc_gpu_memory *memory,
                                                uint64_t memory_offset,
                                                uint32_t pitch_bytes,
                                                uint32_t value,
                                                uint32_t dwords_per_row,
                                                uint32_t row_count)
{
    (void)memory_offset;
    (void)pitch_bytes;
    (void)value;
    (void)dwords_per_row;
    (void)row_count;
    return device == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_write_data_grid(openagc_gpu_device *device,
                                                openagc_gpu_memory *memory,
                                                uint64_t memory_offset,
                                                uint32_t pitch_bytes,
                                                uint32_t value,
                                                uint32_t dwords_per_column,
                                                uint32_t column_count,
                                                uint32_t row_count)
{
    (void)memory_offset;
    (void)pitch_bytes;
    (void)value;
    (void)dwords_per_column;
    (void)column_count;
    (void)row_count;
    return device == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_write_data(openagc_gpu_device *device,
                                           openagc_gpu_buffer *destination,
                                           uint64_t destination_offset,
                                           uint32_t value,
                                           uint32_t dword_count)
{
    (void)destination_offset;
    (void)value;
    (void)dword_count;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_write_data_buffer_rows(openagc_gpu_device *device,
                                                       openagc_gpu_buffer *destination,
                                                       uint64_t destination_offset,
                                                       uint32_t pitch_bytes,
                                                       uint32_t value,
                                                       uint32_t dwords_per_row,
                                                       uint32_t row_count)
{
    (void)destination_offset;
    (void)pitch_bytes;
    (void)value;
    (void)dwords_per_row;
    (void)row_count;
    return device == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_dma_write_data(openagc_gpu_device *device,
                                               openagc_gpu_buffer *source,
                                               uint64_t source_offset,
                                               openagc_gpu_buffer *destination,
                                               uint64_t destination_offset,
                                               uint32_t dma_bytes,
                                               uint32_t value,
                                               uint32_t dword_count)
{
    (void)source_offset;
    (void)destination_offset;
    (void)dma_bytes;
    (void)value;
    (void)dword_count;
    return device == 0 || source == 0 || destination == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_get_last_write(const openagc_gpu_device *device,
                                                 openagc_gpu_submission_view *view)
{
    return device == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_host_graphics_register_eop(openagc_gpu_device *device,
                                                      const uint32_t *register_words,
                                                      uint32_t register_dword_count)
{
    (void)register_words;
    (void)register_dword_count;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_cb_capture_verify(const openagc_cb_capture_manifest *manifest,
                                         const uint32_t *words)
{
    (void)words;
    return manifest == 0 || words == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

uint32_t openagc_cb_capture_evidence_qualified(const openagc_cb_capture_manifest *manifest)
{
    (void)manifest;
    return 0u;
}

openagc_result openagc_cb_capture_encode_invent(openagc_cb_capture_kind kind,
                                                uint32_t *words, uint32_t max_words,
                                                uint32_t *out_count)
{
    (void)kind;
    (void)words;
    (void)max_words;
    if (out_count != 0) {
        *out_count = 0u;
    }
    return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}

openagc_result openagc_gpu_host_cb_bind_from_capture(
    openagc_gpu_device *device, const openagc_cb_capture_manifest *manifest,
    const uint32_t *words)
{
    (void)manifest;
    (void)words;
    return device == 0 || manifest == 0 || words == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_get_cb_capture_info(const openagc_gpu_device *device,
                                                     openagc_cb_capture_info *info)
{
    return device == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_ib_dump_parse(const char *text, uint32_t *words,
                                     uint32_t max_words, openagc_ib_dump_info *info)
{
    (void)text;
    (void)words;
    (void)max_words;
    return text == 0 || words == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_device_reserve_synthetic_va(openagc_gpu_device *device,
                                                       uint64_t size_bytes,
                                                       uint64_t *out_va)
{
    (void)size_bytes;
    if (out_va != 0) {
        *out_va = 0u;
    }
    return device == 0 || out_va == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_memory_get_device_address(const openagc_gpu_memory *memory,
                                                     uint64_t offset, uint64_t *out_va)
{
    (void)offset;
    if (out_va != 0) {
        *out_va = 0u;
    }
    return memory == 0 || out_va == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_create(openagc_gpu_device *device,
                                         const openagc_gpu_buffer_desc *desc,
                                         openagc_gpu_buffer **out_buffer)
{
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_bind_memory(openagc_gpu_buffer *buffer,
                                              openagc_gpu_memory *memory,
                                              uint64_t memory_offset)
{
    (void)memory_offset;
    return buffer == 0 || memory == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_unbind_memory(openagc_gpu_buffer *buffer)
{
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_buffer_destroy(openagc_gpu_buffer *buffer)
{
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_create(
    openagc_gpu_device *device, const openagc_gpu_command_buffer_desc *desc,
    openagc_gpu_command_buffer **out_command_buffer)
{
    if (out_command_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_destroy(openagc_gpu_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_begin(openagc_gpu_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_copy_buffer(
    openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_buffer *source, uint64_t source_offset,
    openagc_gpu_buffer *destination, uint64_t destination_offset,
    uint64_t size_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    return command_buffer == 0 || source == 0 || destination == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_end(openagc_gpu_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_reset(openagc_gpu_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_command_buffer_get_recording(
    const openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_recording_view *view)
{
    return command_buffer == 0 || view == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_queue_create(openagc_gpu_device *device,
                                        const openagc_gpu_queue_desc *desc,
                                        openagc_gpu_queue **out_queue)
{
    if (out_queue == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_queue = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_queue_destroy(openagc_gpu_queue *queue)
{
    return queue == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_queue_submit(openagc_gpu_queue *queue,
                                       openagc_gpu_command_buffer *command_buffer,
                                       openagc_gpu_fence *fence)
{
    return queue == 0 || command_buffer == 0 || fence == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_queue_get_last_submission(
    const openagc_gpu_queue *queue, openagc_gpu_submission_view *view)
{
    return queue == 0 || view == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_fence_create(openagc_gpu_device *device,
                                       openagc_gpu_fence **out_fence)
{
    if (out_fence == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_fence = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_fence_destroy(openagc_gpu_fence *fence)
{
    return fence == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_fence_poll(const openagc_gpu_fence *fence,
                                     openagc_gpu_fence_info *info)
{
    return fence == 0 || info == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gpu_fence_reset(openagc_gpu_fence *fence)
{
    return fence == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_get_capabilities(
    const openagc_gpu_device *device, openagc_graphics_capabilities *capabilities)
{
    return device == 0 || capabilities == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_image_create(openagc_gpu_device *device,
                                              const openagc_graphics_image_desc *desc,
                                              openagc_graphics_image **out_image)
{
    if (out_image == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GRAPHICS_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_image_bind_memory(openagc_graphics_image *image,
                                                   openagc_gpu_memory *memory,
                                                   uint64_t memory_offset)
{
    (void)memory_offset;
    return image == 0 || memory == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_image_get_info(const openagc_graphics_image *image,
                                                openagc_graphics_image_info *info)
{
    return image == 0 || info == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_image_destroy(openagc_graphics_image *image)
{
    return image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_create(
    openagc_gpu_device *device, const openagc_graphics_command_buffer_desc *desc,
    openagc_graphics_command_buffer **out_command_buffer)
{
    if (out_command_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GRAPHICS_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_begin(
    openagc_graphics_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_transition(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image,
    const openagc_graphics_transition_desc *desc)
{
    return command_buffer == 0 || image == 0 || desc == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_bind_color_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image)
{
    return command_buffer == 0 || image == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_bind_depth_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image)
{
    return command_buffer == 0 || image == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_set_scissor(
    openagc_graphics_command_buffer *command_buffer, const openagc_graphics_scissor *scissor)
{
    return command_buffer == 0 || scissor == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_clear_color(
    openagc_graphics_command_buffer *command_buffer, openagc_color color)
{
    (void)color;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_clear_depth(
    openagc_graphics_command_buffer *command_buffer, uint32_t depth24, uint32_t stencil)
{
    (void)depth24;
    (void)stencil;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_end(
    openagc_graphics_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_apply_host_state(
    openagc_graphics_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_execute_host(
    openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_execution_info *info)
{
    return command_buffer == 0 || info == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_get_recording(
    const openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_recording_view *view)
{
    return command_buffer == 0 || view == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_reset(
    openagc_graphics_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_graphics_command_buffer_destroy(
    openagc_graphics_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_get_capabilities(
    const openagc_gpu_device *device, openagc_shader_capabilities *capabilities)
{
    return device == 0 || capabilities == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_intake_host(
    openagc_gpu_device *device, const openagc_shader_artifact_desc *desc,
    openagc_shader_artifact **out_artifact)
{
    if (out_artifact == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_artifact = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_get_info(
    const openagc_shader_artifact *artifact, openagc_shader_artifact_info *info)
{
    return artifact == 0 || info == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_get_binding(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_binding_decl *out_binding)
{
    (void)index;
    return artifact == 0 || out_binding == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_get_texture(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_texture_decl *out_texture)
{
    (void)index;
    return artifact == 0 || out_texture == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_get_compiler_metadata(
    const openagc_shader_artifact *artifact, const uint8_t **out_metadata,
    uint32_t *out_size)
{
    if (out_metadata != 0) {
        *out_metadata = 0;
    }
    if (out_size != 0) {
        *out_size = 0u;
    }
    return artifact == 0 || out_metadata == 0 || out_size == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_get_code(const openagc_shader_artifact *artifact,
                                                const uint8_t **out_code,
                                                uint32_t *out_size)
{
    if (out_code != 0) {
        *out_code = 0;
    }
    if (out_size != 0) {
        *out_size = 0u;
    }
    return artifact == 0 || out_code == 0 || out_size == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_destroy(openagc_shader_artifact *artifact)
{
    return artifact == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_artifact_require_compiler(
    const openagc_shader_artifact *artifact)
{
    return artifact == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_pipeline_plan_create_host(
    openagc_gpu_device *device, const openagc_shader_pipeline_desc *desc,
    openagc_shader_pipeline_plan **out_plan)
{
    if (out_plan == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_plan = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_pipeline_plan_get_info(
    const openagc_shader_pipeline_plan *plan, openagc_shader_pipeline_info *info)
{
    return plan == 0 || info == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_pipeline_plan_slot(
    const openagc_shader_pipeline_plan *plan, uint32_t slot, openagc_gpu_buffer **buffer,
    uint64_t *offset, uint64_t *size_bytes, openagc_graphics_image **image)
{
    (void)slot;
    if (buffer != 0) {
        *buffer = 0;
    }
    if (offset != 0) {
        *offset = 0u;
    }
    if (size_bytes != 0) {
        *size_bytes = 0u;
    }
    if (image != 0) {
        *image = 0;
    }
    return plan == 0 || buffer == 0 || offset == 0 || size_bytes == 0 || image == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_pipeline_plan_vertex_input(
    const openagc_shader_pipeline_plan *plan, uint32_t *input_mask)
{
    if (input_mask != 0) {
        *input_mask = 0u;
    }
    return plan == 0 || input_mask == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                        : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_shader_pipeline_plan_destroy(openagc_shader_pipeline_plan *plan)
{
    return plan == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_native_format_at(openagc_frontend_kind kind,
                                                 uint32_t index,
                                                 uint32_t *out_native_format)
{
    (void)kind;
    (void)index;
    return out_native_format == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                  : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_vertex_format(
    openagc_frontend_kind kind, uint32_t native, uint32_t components,
    openagc_frontend_vertex_format *out_format, uint32_t *out_bytes)
{
    (void)kind;
    (void)native;
    (void)components;
    if (out_format != 0) {
        *out_format = 0u;
    }
    if (out_bytes != 0) {
        *out_bytes = 0u;
    }
    return out_format == 0 || out_bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_format(openagc_frontend_kind kind,
                                                 uint32_t native_format,
                                                 openagc_graphics_format *out_format)
{
    (void)kind;
    (void)native_format;
    return out_format == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_image_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_graphics_usage *out_usage)
{
    (void)kind;
    (void)native_usage;
    return out_usage == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_image_layout(
    openagc_frontend_kind kind, uint32_t native_layout,
    openagc_graphics_image_state *out_state, openagc_graphics_owner *out_owner)
{
    (void)kind;
    (void)native_layout;
    return out_state == 0 || out_owner == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_buffer_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_gpu_buffer_usage *out_usage)
{
    (void)kind;
    (void)native_usage;
    return out_usage == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_device_create(
    openagc_gpu_device *device, const openagc_frontend_device_desc *desc,
    openagc_frontend_device **out_frontend)
{
    if (out_frontend == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_frontend = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_device_get_capabilities(
    const openagc_frontend_device *frontend,
    openagc_frontend_capabilities *capabilities)
{
    return frontend == 0 || capabilities == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_device_get_last_write(
    const openagc_frontend_device *frontend, openagc_gpu_submission_view *view)
{
    return frontend == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_device_destroy(openagc_frontend_device *frontend)
{
    return frontend == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_create(openagc_frontend_device *frontend,
                                             const openagc_frontend_image_desc *desc,
                                             openagc_frontend_image **out_image)
{
    if (out_image == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = 0;
    if (frontend == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_get_info(const openagc_frontend_image *image,
                                               openagc_frontend_image_info *info)
{
    return image == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                   : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_transition(
    openagc_frontend_image *image, openagc_graphics_image_state state,
    openagc_graphics_owner owner)
{
    (void)state;
    (void)owner;
    return image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_upload(openagc_frontend_image *image,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return image == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_readback(openagc_frontend_image *image,
                                               uint64_t offset, void *bytes,
                                               uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return image == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_clear(openagc_frontend_image *image,
                                            openagc_color color)
{
    (void)color;
    return image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_destroy(openagc_frontend_image *image)
{
    return image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_image_desc *desc,
    openagc_frontend_image **out_image)
{
    if (out_image == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = 0;
    return frontend == 0 || desc == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_memory_allocate(openagc_frontend_device *frontend,
                                                uint64_t size_bytes,
                                                openagc_frontend_memory **out_memory)
{
    (void)size_bytes;
    if (out_memory == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = 0;
    return frontend == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_memory_write(openagc_frontend_memory *memory,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_memory_read(const openagc_frontend_memory *memory,
                                            uint64_t offset, void *bytes,
                                            uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_memory_destroy(openagc_frontend_memory *memory)
{
    return memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_bind_memory(openagc_frontend_buffer *buffer,
                                                   openagc_frontend_memory *memory,
                                                   uint64_t offset)
{
    (void)offset;
    return buffer == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_bind_memory(openagc_frontend_image *image,
                                                  openagc_frontend_memory *memory,
                                                  uint64_t offset)
{
    (void)offset;
    return image == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_buffer_desc *desc,
    openagc_frontend_buffer **out_buffer)
{
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    return frontend == 0 || desc == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_create(openagc_frontend_device *frontend,
                                              const openagc_frontend_buffer_desc *desc,
                                              openagc_frontend_buffer **out_buffer)
{
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    if (frontend == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_get_info(const openagc_frontend_buffer *buffer,
                                                openagc_frontend_buffer_info *info)
{
    return buffer == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_upload(openagc_frontend_buffer *buffer,
                                              uint64_t offset, const void *bytes,
                                              uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_readback(openagc_frontend_buffer *buffer,
                                                uint64_t offset, void *bytes,
                                                uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_fill(openagc_frontend_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes, uint32_t value)
{
    (void)offset;
    (void)size_bytes;
    (void)value;
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_image_copy_rect(openagc_frontend_image *source, uint32_t source_x,
                                                uint32_t source_y,
                                                openagc_frontend_image *destination,
                                                uint32_t destination_x, uint32_t destination_y,
                                                uint32_t width, uint32_t height)
{
    (void)source_x;
    (void)source_y;
    (void)destination_x;
    (void)destination_y;
    (void)width;
    (void)height;
    return source == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_copy_image_to_buffer(openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     openagc_frontend_buffer *destination,
                                                     uint64_t destination_offset,
                                                     uint64_t size_bytes)
{
    (void)image_offset;
    (void)destination_offset;
    (void)size_bytes;
    return image == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_copy_buffer_to_image(openagc_frontend_buffer *source,
                                                     uint64_t source_offset,
                                                     openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     uint64_t size_bytes)
{
    (void)source_offset;
    (void)image_offset;
    (void)size_bytes;
    return source == 0 || image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_copy(openagc_frontend_buffer *source,
                                            uint64_t source_offset,
                                            openagc_frontend_buffer *destination,
                                            uint64_t destination_offset,
                                            uint64_t size_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    return source == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_copy_then_fill(openagc_frontend_buffer *source,
                                                      uint64_t source_offset,
                                                      openagc_frontend_buffer *destination,
                                                      uint64_t destination_offset,
                                                      uint64_t size_bytes, uint32_t value,
                                                      uint64_t fill_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    (void)value;
    (void)fill_bytes;
    return source == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_buffer_destroy(openagc_frontend_buffer *buffer)
{
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_sampler_create(openagc_frontend_device *frontend,
                                               const openagc_frontend_sampler_desc *desc,
                                               openagc_frontend_sampler **out_sampler)
{
    if (out_sampler == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = 0;
    return frontend == 0 || desc == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_sampler_get_info(const openagc_frontend_sampler *sampler,
                                                 openagc_frontend_sampler_info *info)
{
    return sampler == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_sampler_destroy(openagc_frontend_sampler *sampler)
{
    return sampler == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_timeline_create(
    openagc_frontend_device *frontend, openagc_frontend_timeline **out_timeline)
{
    if (out_timeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_timeline = 0;
    return frontend == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_timeline_signal(openagc_frontend_timeline *timeline)
{
    return timeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_timeline_poll(const openagc_frontend_timeline *timeline,
                                              uint64_t value,
                                              openagc_frontend_timeline_info *info)
{
    (void)value;
    return timeline == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_timeline_destroy(openagc_frontend_timeline *timeline)
{
    return timeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_graphics_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_pipeline **out_pipeline)
{
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return frontend == 0 || vertex == 0 || pixel == 0 || color_target == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_apply_reflection(
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_shader_resource_binding *resource, openagc_shader_texture_binding *texture,
    openagc_shader_pipeline_desc *plan)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    return resource == 0 || texture == 0 || plan == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_apply_reflection_set(
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_shader_resource_binding *resources,
    openagc_shader_texture_binding *textures, openagc_shader_pipeline_desc *plan)
{
    (void)uniforms;
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)resource_count;
    (void)sampled;
    (void)texture_bindings;
    (void)texture_count;
    return plan == 0 || (resource_count != 0u && resources == 0) ||
                   (texture_count != 0u && textures == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_graphics_pipeline_create_with_bindings(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_frontend_pipeline **out_pipeline)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return frontend == 0 || vertex == 0 || pixel == 0 || color_target == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_graphics_pipeline_create_with_resources(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_frontend_pipeline **out_pipeline)
{
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)texture_bindings;
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return frontend == 0 || vertex == 0 || pixel == 0 || color_target == 0 ||
                   (resource_count != 0u && uniforms == 0) ||
                   (texture_count != 0u && sampled == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_pipeline_desc *desc,
    openagc_frontend_pipeline **out_pipeline)
{
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    if (frontend == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_layout(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout)
{
    if (out_layout != 0) {
        *out_layout = 0;
    }
    return pipeline == 0 || out_layout == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_layout_matches(const openagc_frontend_pipeline *pipeline,
                                               const openagc_frontend_pipeline_layout *layout)
{
    (void)layout;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_layout_retain(openagc_frontend_pipeline_layout *layout)
{
    return layout == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_layout_release(openagc_frontend_pipeline_layout *layout)
{
    return layout == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_info(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_info *info)
{
    return pipeline == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_psbc_register_snapshot(
    openagc_frontend_pipeline *pipeline, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size)
{
    (void)vertex_metadata;
    (void)vertex_metadata_size;
    (void)pixel_metadata;
    (void)pixel_metadata_size;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_agc_linked_registers(
    openagc_frontend_pipeline *pipeline,
    const openagc_frontend_agc_register *context_records, uint32_t context_count,
    const openagc_frontend_agc_register *uconfig_records, uint32_t uconfig_count)
{
    (void)context_records;
    (void)context_count;
    (void)uconfig_records;
    (void)uconfig_count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_agc_target_registers(
    openagc_frontend_pipeline *pipeline,
    const openagc_frontend_agc_register *target_records, uint32_t target_count)
{
    (void)target_records;
    (void)target_count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_agc_build_linear_target(
    openagc_frontend_kind kind, uint32_t native_format,
    const openagc_frontend_agc_register *defaults, uint32_t default_count,
    uint64_t target_va, uint32_t width, uint32_t height,
    openagc_frontend_agc_register *out_records, uint32_t out_count)
{
    (void)kind;
    (void)native_format;
    (void)default_count;
    (void)target_va;
    (void)width;
    (void)height;
    (void)out_count;
    return defaults == 0 || out_records == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_host_register_program(
    const openagc_frontend_pipeline *pipeline, uint32_t *words, uint32_t max_words,
    uint32_t *out_count)
{
    (void)words;
    (void)max_words;
    if (out_count != 0) {
        *out_count = 0u;
    }
    return pipeline == 0 || out_count == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_patch_psbc_pgm_vas(
    openagc_frontend_pipeline *pipeline, uint64_t vertex_code_va, uint64_t pixel_code_va)
{
    (void)vertex_code_va;
    (void)pixel_code_va;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_record_psbc_register_eop(
    openagc_frontend_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_record_psbc_register_eop_if_bound(
    openagc_frontend_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_psbc_code_vas(
    const openagc_frontend_pipeline *pipeline, uint64_t *vertex_code_va,
    uint64_t *pixel_code_va)
{
    if (vertex_code_va != 0) {
        *vertex_code_va = 0u;
    }
    if (pixel_code_va != 0) {
        *pixel_code_va = 0u;
    }
    return pipeline == 0 || vertex_code_va == 0 || pixel_code_va == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_bind_psbc_code(openagc_frontend_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_resources_present(uint32_t has_buffer, uint32_t has_image,
                                                  uint32_t has_sampler)
{
    (void)has_buffer;
    (void)has_image;
    (void)has_sampler;
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_recording_covers(const openagc_frontend_pipeline *pipeline,
                                                 uint32_t resource_mask, uint32_t texture_mask)
{
    (void)resource_mask;
    (void)texture_mask;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_recording_matches(
    const openagc_frontend_pipeline *pipeline, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler)
{
    (void)buffers;
    (void)offsets;
    (void)sizes;
    (void)images;
    (void)sampler;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_layout_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_pipeline_layout *layout)
{
    (void)layout;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler)
{
    (void)buffers;
    (void)offsets;
    (void)sizes;
    (void)images;
    (void)sampler;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_covers(const openagc_frontend_render_pass *pass,
                                                   uint32_t resource_mask, uint32_t texture_mask)
{
    (void)resource_mask;
    (void)texture_mask;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_dispatch(const openagc_frontend_pipeline *pipeline,
                                         uint32_t groups_x, uint32_t groups_y,
                                         uint32_t groups_z)
{
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_dispatch_validate(const openagc_frontend_pipeline *pipeline,
                                                  uint32_t groups_x, uint32_t groups_y,
                                                  uint32_t groups_z)
{
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_vertex_stride(openagc_frontend_pipeline *pipeline,
                                                          uint32_t stride)
{
    (void)stride;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_sampler(openagc_frontend_pipeline *pipeline,
                                                   openagc_frontend_sampler *sampler)
{
    (void)sampler;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_vertex_input(
    openagc_frontend_pipeline *pipeline, uint32_t stride, uint32_t attribute_offset,
    uint32_t attribute_bytes)
{
    (void)stride;
    (void)attribute_offset;
    (void)attribute_bytes;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_vertex_attribute(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset, uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return pipeline == 0 || offset == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_primitive(openagc_frontend_kind kind, uint32_t native,
                                                    openagc_frontend_primitive *out_primitive)
{
    (void)kind;
    (void)native;
    if (out_primitive != 0) {
        *out_primitive = 0u;
    }
    return out_primitive == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_blend_factor(openagc_frontend_kind kind, uint32_t native,
                                                       openagc_frontend_blend_factor *out_factor)
{
    (void)kind;
    (void)native;
    if (out_factor != 0) {
        *out_factor = 0u;
    }
    return out_factor == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_blend(openagc_frontend_pipeline *pipeline,
                                                   uint32_t enable, openagc_frontend_blend_factor src,
                                                   openagc_frontend_blend_factor dst)
{
    (void)enable;
    (void)src;
    (void)dst;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_blend(const openagc_frontend_pipeline *pipeline,
                                                   uint32_t *enable,
                                                   openagc_frontend_blend_factor *src,
                                                   openagc_frontend_blend_factor *dst)
{
    if (enable != 0) {
        *enable = 0u;
    }
    if (src != 0) {
        *src = 0u;
    }
    if (dst != 0) {
        *dst = 0u;
    }
    return pipeline == 0 || enable == 0 || src == 0 || dst == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_write_push_constants(
    openagc_frontend_pipeline *pipeline, uint32_t offset, const void *bytes, uint32_t size)
{
    (void)offset;
    (void)bytes;
    (void)size;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_read_push_constants(
    const openagc_frontend_pipeline *pipeline, uint32_t offset, void *bytes, uint32_t size)
{
    (void)offset;
    (void)size;
    return pipeline == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_primitive(openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive primitive)
{
    (void)primitive;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_primitive(const openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive *out_primitive)
{
    if (out_primitive != 0) {
        *out_primitive = 0u;
    }
    return pipeline == 0 || out_primitive == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_vertex_rates(
    openagc_frontend_pipeline *pipeline, const uint32_t *rates, uint32_t count)
{
    (void)rates;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_vertex_formats(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const openagc_frontend_vertex_format *formats, uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)formats;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_get_vertex_format(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset,
    openagc_frontend_vertex_format *format, uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (format != 0) {
        *format = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return pipeline == 0 || offset == 0 || format == 0 || bytes == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_set_vertex_attributes(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *bytes, uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)bytes;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_pipeline_destroy(openagc_frontend_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_instance_create(const openagc_vk_instance_desc *desc,
                                          openagc_vk_instance **out_instance)
{
    if (out_instance == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_instance = 0;
    if (desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_instance_destroy(openagc_vk_instance *instance)
{
    return instance == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_get_capabilities(const openagc_vk_instance *instance,
                                           openagc_vk_capabilities *capabilities)
{
    return instance == 0 || capabilities == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_get_format_properties(const openagc_vk_instance *instance,
                                                uint32_t index,
                                                openagc_vk_format_properties *properties)
{
    (void)index;
    return instance == 0 || properties == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_get_queue_family(const openagc_vk_instance *instance,
                                           uint32_t index,
                                           openagc_vk_queue_family *family)
{
    (void)index;
    return instance == 0 || family == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                        : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_device_create(openagc_vk_instance *instance,
                                        const openagc_vk_device_desc *desc,
                                        openagc_vk_device **out_device)
{
    if (out_device == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = 0;
    if (instance == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_device_destroy(openagc_vk_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_device_get_last_write(const openagc_vk_device *device,
                                                 openagc_gpu_submission_view *view)
{
    return device == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_allocate_memory(openagc_vk_device *device, uint64_t size_bytes,
                                          openagc_vk_memory **out_memory)
{
    (void)size_bytes;
    if (out_memory == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_memory_write(openagc_vk_memory *memory, uint64_t offset,
                                       const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_memory_read(const openagc_vk_memory *memory, uint64_t offset,
                                      void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return memory == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_free_memory(openagc_vk_memory *memory)
{
    return memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_unbound_buffer(openagc_vk_device *device,
                                                const openagc_vk_buffer_desc *desc,
                                                openagc_vk_buffer **out_buffer)
{
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    return device == 0 || desc == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_bind_buffer_memory(openagc_vk_buffer *buffer,
                                             openagc_vk_memory *memory, uint64_t offset)
{
    (void)offset;
    return buffer == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_unbound_image(openagc_vk_device *device,
                                               const openagc_vk_image_desc *desc,
                                               openagc_vk_image **out_image)
{
    if (out_image == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = 0;
    return device == 0 || desc == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_bind_image_memory(openagc_vk_image *image,
                                            openagc_vk_memory *memory, uint64_t offset)
{
    (void)offset;
    return image == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_buffer(openagc_vk_device *device,
                                        const openagc_vk_buffer_desc *desc,
                                        openagc_vk_buffer **out_buffer)
{
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_buffer_upload(openagc_vk_buffer *buffer, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_buffer_readback(openagc_vk_buffer *buffer, uint64_t offset,
                                          void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_buffer_get_info(const openagc_vk_buffer *buffer,
                                          openagc_frontend_buffer_info *info)
{
    return buffer == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_descriptor_set_for_pipeline(
    openagc_vk_device *device, openagc_vk_pipeline *pipeline, openagc_vk_descriptor_set **out_set)
{
    (void)pipeline;
    if (out_set != 0) {
        *out_set = 0;
    }
    return device == 0 || pipeline == 0 || out_set == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_descriptor_set(openagc_vk_device *device,
                                                openagc_vk_descriptor_set **out_set)
{
    if (out_set == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_set = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_update_descriptor_set(openagc_vk_descriptor_set *set,
                                                openagc_vk_buffer *buffer)
{
    return openagc_vk_update_descriptor_set_at(set, buffer, 0u);
}

openagc_result openagc_vk_update_descriptor_buffer_range(openagc_vk_descriptor_set *set,
                                                         openagc_vk_buffer *buffer,
                                                         uint32_t binding, uint64_t offset,
                                                         uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return openagc_vk_update_descriptor_set_at(set, buffer, binding);
}

openagc_result openagc_vk_update_descriptor_set_at(openagc_vk_descriptor_set *set,
                                                   openagc_vk_buffer *buffer, uint32_t binding)
{
    (void)binding;
    return set == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                   : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_sampler(openagc_vk_device *device, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_vk_sampler **out_sampler)
{
    (void)mag_filter;
    (void)min_filter;
    (void)address_mode;
    if (out_sampler == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_update_descriptor_sampler(openagc_vk_descriptor_set *set,
                                                    openagc_vk_sampler *sampler)
{
    return set == 0 || sampler == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_sampler(openagc_vk_sampler *sampler)
{
    return sampler == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_update_descriptor_image(openagc_vk_descriptor_set *set,
                                                  openagc_vk_image_view *view)
{
    return openagc_vk_update_descriptor_image_at(set, view, 0u);
}

openagc_result openagc_vk_update_descriptor_image_at(openagc_vk_descriptor_set *set,
                                                     openagc_vk_image_view *view,
                                                     uint32_t binding)
{
    (void)binding;
    return set == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                 : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_descriptor_set(openagc_vk_descriptor_set *set)
{
    return set == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_buffer(openagc_vk_buffer *buffer)
{
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_image(openagc_vk_device *device,
                                       const openagc_vk_image_desc *desc,
                                       openagc_vk_image **out_image)
{
    if (out_image == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_image_get_info(const openagc_vk_image *image,
                                         openagc_frontend_image_info *info)
{
    return image == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                   : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_image_readback(openagc_vk_image *image, uint64_t offset,
                                         void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return image == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_image(openagc_vk_image *image)
{
    return image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_image_view(openagc_vk_device *device,
                                            openagc_vk_image *image,
                                            const openagc_vk_image_view_desc *desc,
                                            openagc_vk_image_view **out_view)
{
    if (out_view == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_view = 0;
    if (device == 0 || image == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_image_view_get_info(const openagc_vk_image_view *view,
                                              openagc_vk_image_view_info *info)
{
    return view == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                  : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_image_view(openagc_vk_image_view *view)
{
    return view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_command_pool(openagc_vk_device *device,
                                              const openagc_vk_command_pool_desc *desc,
                                              openagc_vk_command_pool **out_pool)
{
    if (out_pool == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pool = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_command_pool_reset(openagc_vk_command_pool *pool)
{
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_command_pool(openagc_vk_command_pool *pool)
{
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_allocate_command_buffer(openagc_vk_command_pool *pool,
                                                  openagc_vk_command_buffer **out_command_buffer)
{
    if (out_command_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = 0;
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_command_buffer_begin(openagc_vk_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_copy_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *source, uint64_t source_offset,
                                          openagc_vk_buffer *destination,
                                          uint64_t destination_offset, uint64_t size_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    return command_buffer == 0 || source == 0 || destination == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_image_barrier(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_image *image, uint32_t layout)
{
    (void)layout;
    return command_buffer == 0 || image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_copy_image_to_buffer(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset,
                                                   openagc_vk_buffer *destination,
                                                   uint64_t destination_offset,
                                                   uint64_t size_bytes)
{
    (void)image_offset;
    (void)destination_offset;
    (void)size_bytes;
    return command_buffer == 0 || image == 0 || destination == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_copy_buffer_to_image(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_buffer *source,
                                                   uint64_t source_offset,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset, uint64_t size_bytes)
{
    (void)source_offset;
    (void)image_offset;
    (void)size_bytes;
    return command_buffer == 0 || source == 0 || image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_copy_image(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_image *source, uint32_t source_x,
                                         uint32_t source_y, openagc_vk_image *destination,
                                         uint32_t destination_x, uint32_t destination_y,
                                         uint32_t width, uint32_t height)
{
    (void)source_x;
    (void)source_y;
    (void)destination_x;
    (void)destination_y;
    (void)width;
    (void)height;
    return command_buffer == 0 || source == 0 || destination == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_fill_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset,
                                          uint64_t size_bytes, uint32_t value)
{
    (void)offset;
    (void)size_bytes;
    (void)value;
    return command_buffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_update_buffer(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_buffer *buffer, uint64_t offset,
                                            const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return command_buffer == 0 || buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_clear_attachments(openagc_vk_command_buffer *command_buffer,
                                               openagc_color color)
{
    (void)color;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_clear_depth(openagc_vk_command_buffer *command_buffer, float depth,
                                         uint32_t stencil)
{
    (void)depth;
    (void)stencil;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_clear_color(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_image *image, openagc_color color)
{
    (void)color;
    return command_buffer == 0 || image == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_command_buffer_end(openagc_vk_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_command_buffer(openagc_vk_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_queue_submit_commands(openagc_vk_device *device,
                                                openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_fence *fence)
{
    (void)fence;
    return device == 0 || command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_fence(openagc_vk_device *device,
                                       openagc_vk_fence **out_fence)
{
    if (out_fence == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_fence = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_queue_submit(openagc_vk_device *device, openagc_vk_fence *fence)
{
    return device == 0 || fence == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_fence_poll(const openagc_vk_fence *fence, uint64_t value,
                                     openagc_frontend_timeline_info *info)
{
    (void)value;
    return fence == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                   : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_fence(openagc_vk_fence *fence)
{
    return fence == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_semaphore(openagc_vk_device *device,
                                           openagc_vk_semaphore **out_semaphore)
{
    if (out_semaphore == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_semaphore = 0;
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_semaphore_poll(const openagc_vk_semaphore *semaphore, uint64_t value,
                                         openagc_frontend_timeline_info *info)
{
    (void)value;
    return semaphore == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_semaphore(openagc_vk_semaphore *semaphore)
{
    return semaphore == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_queue_submit_wait(openagc_vk_device *device,
                                            openagc_vk_command_buffer *command_buffer,
                                            const openagc_vk_semaphore *wait, uint64_t wait_value,
                                            openagc_vk_semaphore *signal)
{
    (void)wait;
    (void)wait_value;
    return device == 0 || command_buffer == 0 || signal == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_graphics_pipeline(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_pipeline **out_pipeline)
{
    (void)vertex;
    (void)pixel;
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return device == 0 || color_target == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_compute_pipeline(openagc_vk_device *device,
                                                  const openagc_shader_artifact_desc *desc,
                                                  openagc_vk_pipeline **out_pipeline)
{
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    if (device == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_compute_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    return openagc_vk_create_compute_pipeline(device, desc, out_pipeline);
}

openagc_result openagc_vk_create_compute_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline)
{
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)texture_bindings;
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return device == 0 || desc == 0 || (resource_count != 0u && uniforms == 0) ||
                   (texture_count != 0u && sampled == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_graphics_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    return openagc_vk_create_graphics_pipeline(device, vertex, pixel, color_target,
                                               out_pipeline);
}

openagc_result openagc_vk_create_graphics_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline)
{
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)texture_bindings;
    if (out_pipeline == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = 0;
    return device == 0 || vertex == 0 || pixel == 0 || color_target == 0 ||
                   (resource_count != 0u && uniforms == 0) ||
                   (texture_count != 0u && sampled == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_sampler(openagc_vk_pipeline *pipeline,
                                             openagc_vk_sampler *sampler)
{
    (void)sampler;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_vertex_stride(openagc_vk_pipeline *pipeline, uint32_t stride)
{
    (void)stride;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_vertex_input(openagc_vk_pipeline *pipeline, uint32_t stride,
                                                    uint32_t attribute_offset,
                                                    uint32_t attribute_bytes)
{
    (void)stride;
    (void)attribute_offset;
    (void)attribute_bytes;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_vertex_attribute(const openagc_vk_pipeline *pipeline,
                                                      uint32_t index, uint32_t *offset,
                                                      uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return pipeline == 0 || offset == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_vertex_attributes(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)bytes;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_push_constants(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline, uint32_t offset,
                                             const void *bytes, uint32_t size)
{
    (void)offset;
    (void)bytes;
    (void)size;
    return command_buffer == 0 || pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_read_push_constants(const openagc_vk_pipeline *pipeline,
                                                       uint32_t offset, void *bytes,
                                                       uint32_t size)
{
    (void)offset;
    (void)size;
    return pipeline == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_blend(openagc_vk_pipeline *pipeline, uint32_t enable,
                                             uint32_t src_factor, uint32_t dst_factor)
{
    (void)enable;
    (void)src_factor;
    (void)dst_factor;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_blend(const openagc_vk_pipeline *pipeline, uint32_t *enable,
                                             openagc_frontend_blend_factor *src,
                                             openagc_frontend_blend_factor *dst)
{
    if (enable != 0) {
        *enable = 0u;
    }
    if (src != 0) {
        *src = 0u;
    }
    if (dst != 0) {
        *dst = 0u;
    }
    return pipeline == 0 || enable == 0 || src == 0 || dst == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_topology(openagc_vk_pipeline *pipeline, uint32_t topology)
{
    (void)topology;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_primitive(const openagc_vk_pipeline *pipeline,
                                                 openagc_frontend_primitive *out_primitive)
{
    if (out_primitive != 0) {
        *out_primitive = 0u;
    }
    return pipeline == 0 || out_primitive == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_vertex_rates(openagc_vk_pipeline *pipeline,
                                                 const uint32_t *rates, uint32_t count)
{
    (void)rates;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_vertex_formats(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *formats, uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)formats;
    (void)count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_vertex_format(const openagc_vk_pipeline *pipeline,
                                                    uint32_t index, uint32_t *offset,
                                                    openagc_frontend_vertex_format *format,
                                                    uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (format != 0) {
        *format = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return pipeline == 0 || offset == 0 || format == 0 || bytes == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_layout(
    const openagc_vk_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout)
{
    if (out_layout != 0) {
        *out_layout = 0;
    }
    return pipeline == 0 || out_layout == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_info(const openagc_vk_pipeline *pipeline,
                                            openagc_frontend_pipeline_info *info)
{
    return pipeline == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_psbc_register_snapshot(
    openagc_vk_pipeline *pipeline, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size)
{
    (void)vertex_metadata;
    (void)vertex_metadata_size;
    (void)pixel_metadata;
    (void)pixel_metadata_size;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_agc_linked_registers(
    openagc_vk_pipeline *pipeline,
    const openagc_frontend_agc_register *context_records, uint32_t context_count,
    const openagc_frontend_agc_register *uconfig_records, uint32_t uconfig_count)
{
    (void)context_records;
    (void)context_count;
    (void)uconfig_records;
    (void)uconfig_count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_set_agc_target_registers(
    openagc_vk_pipeline *pipeline,
    const openagc_frontend_agc_register *target_records, uint32_t target_count)
{
    (void)target_records;
    (void)target_count;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_host_register_program(
    const openagc_vk_pipeline *pipeline, uint32_t *words, uint32_t max_words,
    uint32_t *out_count)
{
    (void)words;
    (void)max_words;
    if (out_count != 0) {
        *out_count = 0u;
    }
    return pipeline == 0 || out_count == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_patch_psbc_pgm_vas(openagc_vk_pipeline *pipeline,
                                                      uint64_t vertex_code_va,
                                                      uint64_t pixel_code_va)
{
    (void)vertex_code_va;
    (void)pixel_code_va;
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_record_psbc_register_eop(openagc_vk_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_get_psbc_code_vas(const openagc_vk_pipeline *pipeline,
                                                     uint64_t *vertex_code_va,
                                                     uint64_t *pixel_code_va)
{
    if (vertex_code_va != 0) {
        *vertex_code_va = 0u;
    }
    if (pixel_code_va != 0) {
        *pixel_code_va = 0u;
    }
    return pipeline == 0 || vertex_code_va == 0 || pixel_code_va == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_pipeline_bind_psbc_code(openagc_vk_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_pipeline(openagc_vk_pipeline *pipeline)
{
    return pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_draw_indirect(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return command_buffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_draw(openagc_vk_command_buffer *command_buffer,
                                    uint32_t vertex_count, uint32_t instance_count,
                                    uint32_t first_vertex, uint32_t first_instance)
{
    (void)vertex_count;
    (void)instance_count;
    (void)first_vertex;
    (void)first_instance;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_draw_indexed(openagc_vk_command_buffer *command_buffer,
                                           uint32_t index_count, uint32_t instance_count,
                                           uint32_t first_index, int32_t vertex_offset,
                                           uint32_t first_instance)
{
    (void)index_count;
    (void)instance_count;
    (void)first_index;
    (void)vertex_offset;
    (void)first_instance;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_bind_descriptor_set(openagc_vk_command_buffer *command_buffer,
                                               openagc_vk_descriptor_set *set)
{
    return command_buffer == 0 || set == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_bind_compute_pipeline(openagc_vk_command_buffer *command_buffer,
                                                     openagc_vk_pipeline *pipeline)
{
    return command_buffer == 0 || pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_dispatch(openagc_vk_command_buffer *command_buffer,
                                       uint32_t groups_x, uint32_t groups_y,
                                       uint32_t groups_z)
{
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_create(
    openagc_frontend_device *frontend, openagc_frontend_image *color,
    openagc_frontend_render_pass **out_pass)
{
    if (out_pass == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pass = 0;
    return frontend == 0 || color == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_attach_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_image *depth)
{
    (void)depth;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_begin_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    (void)color_op;
    (void)color;
    (void)depth_op;
    (void)depth;
    (void)stencil;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_begin_validate_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    (void)color_op;
    (void)depth_op;
    (void)depth;
    (void)stencil;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_apply_loads(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    (void)color_op;
    (void)color;
    (void)depth_op;
    (void)depth;
    (void)stencil;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_clear_rect(openagc_frontend_render_pass *pass,
                                                       openagc_color color, uint32_t x, uint32_t y,
                                                       uint32_t width, uint32_t height)
{
    (void)color;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_clear_depth_rect(
    openagc_frontend_render_pass *pass, float depth, uint32_t stencil, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    (void)depth;
    (void)stencil;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_clear_bounds(
    const openagc_frontend_render_pass *pass, uint32_t require_depth, uint32_t *out_x,
    uint32_t *out_y, uint32_t *out_width, uint32_t *out_height)
{
    (void)require_depth;
    if (out_x != 0) {
        *out_x = 0u;
    }
    if (out_y != 0) {
        *out_y = 0u;
    }
    if (out_width != 0) {
        *out_width = 0u;
    }
    if (out_height != 0) {
        *out_height = 0u;
    }
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_begin_with_load(openagc_frontend_render_pass *pass,
                                                           openagc_frontend_load_op load_op,
                                                           openagc_color color)
{
    (void)load_op;
    (void)color;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_begin(openagc_frontend_render_pass *pass)
{
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_set_viewport(openagc_frontend_render_pass *pass,
                                                         uint32_t x, uint32_t y,
                                                         uint32_t width, uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_bind_pipeline(
    openagc_frontend_render_pass *pass, openagc_frontend_pipeline *pipeline)
{
    return pass == 0 || pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_bind_index(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_buffer *buffer,
                                                        uint64_t offset)
{
    (void)offset;
    return pass == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_translate_index_width(openagc_frontend_kind kind, uint32_t native,
                                                      uint32_t *out_width)
{
    (void)kind;
    (void)native;
    if (out_width != 0) {
        *out_width = 0u;
    }
    return out_width == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_set_index_width(openagc_frontend_render_pass *pass,
                                                            uint32_t width)
{
    (void)width;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_bind_vertex(openagc_frontend_render_pass *pass,
                                                         openagc_frontend_buffer *buffer,
                                                         uint64_t offset)
{
    (void)offset;
    return pass == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_bind_cb_capture(
    openagc_frontend_render_pass *pass, const openagc_cb_capture_manifest *manifest,
    const uint32_t *words)
{
    (void)manifest;
    (void)words;
    return pass == 0 || manifest == 0 || words == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_device_get_cb_capture_info(
    const openagc_frontend_device *frontend, openagc_cb_capture_info *info)
{
    return frontend == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_set_scissor(openagc_frontend_render_pass *pass,
                                                        uint32_t x, uint32_t y,
                                                        uint32_t width, uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_draw_indirect(
    const openagc_frontend_render_pass *pass, openagc_frontend_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return pass == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_draw(const openagc_frontend_render_pass *pass,
                                                 uint32_t vertex_count, uint32_t instance_count,
                                                 uint32_t first_vertex, uint32_t first_instance)
{
    (void)vertex_count;
    (void)instance_count;
    (void)first_vertex;
    (void)first_instance;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_draw_indexed(
    const openagc_frontend_render_pass *pass, uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
    (void)index_count;
    (void)instance_count;
    (void)first_index;
    (void)vertex_offset;
    (void)first_instance;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_clear(openagc_frontend_render_pass *pass,
                                                  openagc_color color)
{
    (void)color;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_clear_depth(openagc_frontend_render_pass *pass,
                                                       float depth, uint32_t stencil)
{
    (void)depth;
    (void)stencil;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_end(openagc_frontend_render_pass *pass)
{
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_query_pool_create(openagc_frontend_device *frontend,
                                                  openagc_frontend_query_kind kind, uint32_t count,
                                                  openagc_frontend_query_pool **out_pool)
{
    (void)kind;
    (void)count;
    if (out_pool != 0) {
        *out_pool = 0;
    }
    return frontend == 0 || out_pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_begin_query(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_query_pool *pool,
                                                        uint32_t index)
{
    (void)pool;
    (void)index;
    return pass == 0 || pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                 : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_end_query(openagc_frontend_render_pass *pass)
{
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_query_write_timestamp(openagc_frontend_query_pool *pool,
                                                     uint32_t index)
{
    (void)index;
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_query_get(const openagc_frontend_query_pool *pool, uint32_t index,
                                          uint32_t *available)
{
    (void)index;
    if (available != 0) {
        *available = 0u;
    }
    return pool == 0 || available == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_query_pool_destroy(openagc_frontend_query_pool *pool)
{
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_frontend_render_pass_destroy(openagc_frontend_render_pass *pass)
{
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_render_pass(openagc_vk_device *device,
                                             openagc_vk_image *color,
                                             openagc_vk_render_pass **out_pass)
{
    if (out_pass == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pass = 0;
    return device == 0 || color == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_render_pass_attach_depth(openagc_vk_render_pass *pass,
                                                   openagc_vk_image *depth)
{
    (void)depth;
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_render_pass(openagc_vk_render_pass *pass)
{
    return pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_begin_render_pass_with_load(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op load_op, openagc_color color)
{
    (void)pass;
    (void)load_op;
    (void)color;
    return command_buffer == 0 || pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_begin_render_pass_with_depth(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op color_op, openagc_color color, openagc_frontend_load_op depth_op,
    float depth, uint32_t stencil)
{
    (void)color_op;
    (void)color;
    (void)depth_op;
    (void)depth;
    (void)stencil;
    return command_buffer == 0 || pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_begin_render_pass(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_render_pass *pass)
{
    return command_buffer == 0 || pass == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_set_viewport(openagc_vk_command_buffer *command_buffer,
                                           uint32_t x, uint32_t y, uint32_t width,
                                           uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_bind_pipeline(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline)
{
    return command_buffer == 0 || pipeline == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_bind_index_buffer(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_buffer *buffer, uint64_t offset,
                                                uint32_t index_type)
{
    (void)offset;
    (void)index_type;
    return command_buffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_bind_vertex_buffer(openagc_vk_command_buffer *command_buffer,
                                                 openagc_vk_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return command_buffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_set_scissor(openagc_vk_command_buffer *command_buffer,
                                          uint32_t x, uint32_t y, uint32_t width,
                                          uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_query_pool(openagc_vk_device *device,
                                            openagc_frontend_query_kind kind, uint32_t count,
                                            openagc_vk_query_pool **out_pool)
{
    (void)kind;
    (void)count;
    if (out_pool != 0) {
        *out_pool = 0;
    }
    return device == 0 || out_pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_begin_query(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_query_pool *pool, uint32_t index)
{
    (void)pool;
    (void)index;
    return command_buffer == 0 || pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_end_query(openagc_vk_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_write_timestamp(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_query_pool *pool, uint32_t index)
{
    (void)pool;
    (void)index;
    return command_buffer == 0 || pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_get_query(const openagc_vk_query_pool *pool, uint32_t index,
                                   uint32_t *available)
{
    (void)index;
    if (available != 0) {
        *available = 0u;
    }
    return pool == 0 || available == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_destroy_query_pool(openagc_vk_query_pool *pool)
{
    return pool == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_cmd_end_render_pass(openagc_vk_command_buffer *command_buffer)
{
    return command_buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_vk_create_swapchain(openagc_vk_device *device)
{
    return device == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_context_create(const openagc_gl_context_desc *desc,
                                         openagc_gl_context **out_context)
{
    if (out_context == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_context = 0;
    if (desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_GL_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_context_destroy(openagc_gl_context *context)
{
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_context_get_last_write(const openagc_gl_context *context,
                                                  openagc_gpu_submission_view *view)
{
    return context == 0 || view == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_capabilities(const openagc_gl_context *context,
                                           openagc_gl_capabilities *capabilities)
{
    return context == 0 || capabilities == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_format(const openagc_gl_context *context, uint32_t index,
                                     uint32_t *native_format,
                                     openagc_graphics_format *backend_format)
{
    (void)index;
    return context == 0 || native_format == 0 || backend_format == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_texture(openagc_gl_context *context,
                                         const openagc_gl_image_desc *desc,
                                         openagc_gl_texture **out_texture)
{
    if (out_texture == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_texture = 0;
    if (context == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_GL_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_tex_sub_image(openagc_gl_texture *texture, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return texture == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_tex_image_to_buffer(openagc_gl_texture *texture,
                                                  uint64_t image_offset,
                                                  openagc_gl_buffer *buffer,
                                                  uint64_t buffer_offset, uint64_t size_bytes)
{
    (void)image_offset;
    (void)buffer_offset;
    (void)size_bytes;
    return texture == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_copy_tex_sub_image(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_texture *texture, uint32_t xoffset,
                                             uint32_t yoffset, uint32_t x, uint32_t y,
                                             uint32_t width, uint32_t height)
{
    (void)xoffset;
    (void)yoffset;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return framebuffer == 0 || texture == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_tex_sub_image_from_buffer(openagc_gl_texture *texture,
                                                    openagc_gl_buffer *buffer,
                                                    uint64_t buffer_offset,
                                                    uint64_t image_offset, uint64_t size_bytes)
{
    (void)buffer_offset;
    (void)image_offset;
    (void)size_bytes;
    return texture == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_tex_image(openagc_gl_texture *texture, uint64_t offset,
                                        void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return texture == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_sampler(openagc_gl_context *context, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_gl_sampler **out_sampler)
{
    (void)mag_filter;
    (void)min_filter;
    (void)address_mode;
    if (out_sampler == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = 0;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_sampler(openagc_gl_context *context, openagc_gl_sampler *sampler)
{
    (void)sampler;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_sampler(openagc_gl_sampler *sampler)
{
    return sampler == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_texture_for_sampling(openagc_gl_context *context,
                                                    openagc_gl_texture *texture)
{
    return context == 0 || texture == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                        : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_texture_unit(openagc_gl_context *context, uint32_t unit,
                                          openagc_gl_texture *texture)
{
    (void)unit;
    (void)texture;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_texture_get_info(const openagc_gl_texture *texture,
                                           openagc_frontend_image_info *info)
{
    return texture == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_texture(openagc_gl_texture *texture)
{
    return texture == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_renderbuffer(openagc_gl_context *context,
                                              const openagc_gl_image_desc *desc,
                                              openagc_gl_renderbuffer **out_renderbuffer)
{
    if (out_renderbuffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_renderbuffer = 0;
    if (context == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_GL_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_renderbuffer_get_info(const openagc_gl_renderbuffer *renderbuffer,
                                                openagc_frontend_image_info *info)
{
    return renderbuffer == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_renderbuffer_read(openagc_gl_renderbuffer *renderbuffer,
                                            uint64_t offset, void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return renderbuffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_renderbuffer(openagc_gl_renderbuffer *renderbuffer)
{
    return renderbuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_framebuffer(openagc_gl_context *context,
                                             openagc_gl_framebuffer **out_framebuffer)
{
    if (out_framebuffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_framebuffer = 0;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_framebuffer_renderbuffer(openagc_gl_framebuffer *framebuffer,
                                                   openagc_gl_renderbuffer *renderbuffer)
{
    return framebuffer == 0 || renderbuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                 : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_framebuffer_depth_renderbuffer(
    openagc_gl_framebuffer *framebuffer, openagc_gl_renderbuffer *renderbuffer)
{
    (void)renderbuffer;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_framebuffer_texture(openagc_gl_framebuffer *framebuffer,
                                              openagc_gl_texture *texture)
{
    return framebuffer == 0 || texture == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_viewport(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_program(openagc_gl_framebuffer *framebuffer,
                                        openagc_gl_program *program)
{
    return framebuffer == 0 || program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_index_buffer(openagc_gl_framebuffer *framebuffer,
                                            openagc_gl_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return framebuffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_vertex_buffer(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return framebuffer == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_scissor(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_framebuffer_begin(openagc_gl_framebuffer *framebuffer,
                                            openagc_frontend_load_op load_op, openagc_color color)
{
    (void)load_op;
    (void)color;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_framebuffer_begin_with_depth(
    openagc_gl_framebuffer *framebuffer, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    (void)color_op;
    (void)color;
    (void)depth_op;
    (void)depth;
    (void)stencil;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_clear_scissor(openagc_gl_framebuffer *framebuffer, openagc_color color)
{
    (void)color;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_clear_depth(openagc_gl_framebuffer *framebuffer, float depth,
                                     uint32_t stencil)
{
    (void)depth;
    (void)stencil;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_clear(openagc_gl_framebuffer *framebuffer, openagc_color color)
{
    (void)color;
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_read_pixels(openagc_gl_framebuffer *framebuffer, uint64_t offset,
                                      void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return framebuffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_framebuffer(openagc_gl_framebuffer *framebuffer)
{
    return framebuffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                            : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_memory_barrier(openagc_gl_context *context, uint32_t barriers)
{
    (void)barriers;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_finish(openagc_gl_context *context)
{
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_client_wait(const openagc_gl_context *context, uint64_t value,
                                      openagc_frontend_timeline_info *info)
{
    (void)value;
    return context == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_fence_poll(const openagc_gl_context *context, uint64_t value,
                                     openagc_frontend_timeline_info *info)
{
    (void)value;
    return context == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_allocate_memory(openagc_gl_context *context, uint64_t size_bytes,
                                          openagc_gl_memory **out_memory)
{
    (void)size_bytes;
    if (out_memory == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = 0;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_free_memory(openagc_gl_memory *memory)
{
    return memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_unbound_buffer(openagc_gl_context *context, uint32_t target,
                                                uint64_t size_bytes,
                                                openagc_gl_buffer **out_buffer)
{
    (void)target;
    (void)size_bytes;
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_buffer_memory(openagc_gl_buffer *buffer,
                                             openagc_gl_memory *memory, uint64_t offset)
{
    (void)offset;
    return buffer == 0 || memory == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_buffer(openagc_gl_context *context, uint32_t target,
                                        uint64_t size_bytes, openagc_gl_buffer **out_buffer)
{
    (void)target;
    (void)size_bytes;
    if (out_buffer == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = 0;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_uniform_range(openagc_gl_context *context, uint32_t index,
                                            openagc_gl_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return openagc_gl_bind_uniform_base(context, index, buffer);
}

openagc_result openagc_gl_bind_uniform_base(openagc_gl_context *context, uint32_t index,
                                          openagc_gl_buffer *buffer)
{
    (void)index;
    (void)buffer;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_buffer(openagc_gl_context *context, uint32_t target,
                                      openagc_gl_buffer *buffer)
{
    (void)target;
    (void)buffer;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_buffer_data(openagc_gl_buffer *buffer, uint64_t offset,
                                      const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                          const void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                              void *bytes, uint64_t size_bytes)
{
    (void)offset;
    (void)size_bytes;
    return buffer == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_clear_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                                uint64_t size_bytes, uint32_t value)
{
    (void)offset;
    (void)size_bytes;
    (void)value;
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_copy_buffer_sub_data(openagc_gl_buffer *source, uint64_t source_offset,
                                               openagc_gl_buffer *destination,
                                               uint64_t destination_offset, uint64_t size_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    return source == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_copy_buffer_then_clear_sub_data(
    openagc_gl_buffer *source, uint64_t source_offset, openagc_gl_buffer *destination,
    uint64_t destination_offset, uint64_t size_bytes, uint32_t value, uint64_t fill_bytes)
{
    (void)source_offset;
    (void)destination_offset;
    (void)size_bytes;
    (void)value;
    (void)fill_bytes;
    return source == 0 || destination == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                           : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_buffer_get_info(const openagc_gl_buffer *buffer,
                                          openagc_frontend_buffer_info *info)
{
    return buffer == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_buffer(openagc_gl_buffer *buffer)
{
    return buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_graphics_program(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_program **out_program)
{
    (void)vertex;
    (void)pixel;
    if (out_program == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = 0;
    return context == 0 || color_target == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                             : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_program(openagc_gl_context *context,
                                         const openagc_shader_artifact_desc *desc,
                                         openagc_gl_program **out_program)
{
    if (out_program == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = 0;
    if (context == 0 || desc == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    return openagc_gl_create_program(context, desc, out_program);
}

openagc_result openagc_gl_create_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program)
{
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)texture_bindings;
    if (out_program == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = 0;
    return context == 0 || desc == 0 || (resource_count != 0u && uniforms == 0) ||
                   (texture_count != 0u && sampled == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_graphics_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program)
{
    (void)uniform;
    (void)offset;
    (void)size_bytes;
    (void)resource_binding;
    (void)sampled;
    (void)texture_binding;
    return openagc_gl_create_graphics_program(context, vertex, pixel, color_target,
                                              out_program);
}

openagc_result openagc_gl_create_graphics_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program)
{
    (void)offsets;
    (void)sizes;
    (void)resource_bindings;
    (void)texture_bindings;
    if (out_program == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = 0;
    return context == 0 || vertex == 0 || pixel == 0 || color_target == 0 ||
                   (resource_count != 0u && uniforms == 0) ||
                   (texture_count != 0u && sampled == 0)
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_sampler(openagc_gl_program *program,
                                              openagc_gl_sampler *sampler)
{
    (void)sampler;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_vertex_stride(openagc_gl_program *program, uint32_t stride)
{
    (void)stride;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_vertex_input(openagc_gl_program *program, uint32_t stride,
                                                  uint32_t attribute_offset,
                                                  uint32_t attribute_bytes)
{
    (void)stride;
    (void)attribute_offset;
    (void)attribute_bytes;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_vertex_attribute(const openagc_gl_program *program,
                                                       uint32_t index, uint32_t *offset,
                                                       uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return program == 0 || offset == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                    : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_vertex_attributes(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)bytes;
    (void)count;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_uniform(openagc_gl_program *program, uint32_t offset,
                                          const void *bytes, uint32_t size)
{
    (void)offset;
    (void)bytes;
    (void)size;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_read_uniform(const openagc_gl_program *program, uint32_t offset,
                                               void *bytes, uint32_t size)
{
    (void)offset;
    (void)size;
    return program == 0 || bytes == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_blend(openagc_gl_program *program, uint32_t enable,
                                            uint32_t src_factor, uint32_t dst_factor)
{
    (void)enable;
    (void)src_factor;
    (void)dst_factor;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_blend(const openagc_gl_program *program, uint32_t *enable,
                                            openagc_frontend_blend_factor *src,
                                            openagc_frontend_blend_factor *dst)
{
    if (enable != 0) {
        *enable = 0u;
    }
    if (src != 0) {
        *src = 0u;
    }
    if (dst != 0) {
        *dst = 0u;
    }
    return program == 0 || enable == 0 || src == 0 || dst == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_primitive(openagc_gl_program *program, uint32_t mode)
{
    (void)mode;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_primitive(const openagc_gl_program *program,
                                                openagc_frontend_primitive *out_primitive)
{
    if (out_primitive != 0) {
        *out_primitive = 0u;
    }
    return program == 0 || out_primitive == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                              : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_vertex_divisors(openagc_gl_program *program,
                                                     const uint32_t *divisors, uint32_t count)
{
    (void)divisors;
    (void)count;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_vertex_formats(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets,
    const uint32_t *components, uint32_t type, uint32_t count)
{
    (void)stride;
    (void)offsets;
    (void)components;
    (void)type;
    (void)count;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_vertex_format(const openagc_gl_program *program,
                                                   uint32_t index, uint32_t *offset,
                                                   openagc_frontend_vertex_format *format,
                                                   uint32_t *bytes)
{
    (void)index;
    if (offset != 0) {
        *offset = 0u;
    }
    if (format != 0) {
        *format = 0u;
    }
    if (bytes != 0) {
        *bytes = 0u;
    }
    return program == 0 || offset == 0 || format == 0 || bytes == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_accepts_layout(
    const openagc_gl_program *program, const openagc_frontend_pipeline_layout *layout)
{
    (void)layout;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_info(const openagc_gl_program *program,
                                           openagc_frontend_pipeline_info *info)
{
    return program == 0 || info == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_psbc_register_snapshot(
    openagc_gl_program *program, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size)
{
    (void)vertex_metadata;
    (void)vertex_metadata_size;
    (void)pixel_metadata;
    (void)pixel_metadata_size;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_agc_linked_registers(
    openagc_gl_program *program,
    const openagc_frontend_agc_register *context_records, uint32_t context_count,
    const openagc_frontend_agc_register *uconfig_records, uint32_t uconfig_count)
{
    (void)context_records;
    (void)context_count;
    (void)uconfig_records;
    (void)uconfig_count;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_set_agc_target_registers(
    openagc_gl_program *program,
    const openagc_frontend_agc_register *target_records, uint32_t target_count)
{
    (void)target_records;
    (void)target_count;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_host_register_program(
    const openagc_gl_program *program, uint32_t *words, uint32_t max_words,
    uint32_t *out_count)
{
    (void)words;
    (void)max_words;
    if (out_count != 0) {
        *out_count = 0u;
    }
    return program == 0 || out_count == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                          : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_patch_psbc_pgm_vas(openagc_gl_program *program,
                                                     uint64_t vertex_code_va,
                                                     uint64_t pixel_code_va)
{
    (void)vertex_code_va;
    (void)pixel_code_va;
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_record_psbc_register_eop(openagc_gl_program *program)
{
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_get_psbc_code_vas(const openagc_gl_program *program,
                                                    uint64_t *vertex_code_va,
                                                    uint64_t *pixel_code_va)
{
    if (vertex_code_va != 0) {
        *vertex_code_va = 0u;
    }
    if (pixel_code_va != 0) {
        *pixel_code_va = 0u;
    }
    return program == 0 || vertex_code_va == 0 || pixel_code_va == 0
               ? OPENAGC_ERROR_INVALID_ARGUMENT
               : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_program_bind_psbc_code(openagc_gl_program *program)
{
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_use_program(openagc_gl_context *context, openagc_gl_program *program)
{
    (void)program;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_program(openagc_gl_program *program)
{
    return program == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_framebuffer(openagc_gl_context *context,
                                           openagc_gl_framebuffer *framebuffer)
{
    (void)framebuffer;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_create_query(openagc_gl_context *context, openagc_frontend_query_kind kind,
                                       uint32_t count, openagc_gl_query **out_query)
{
    (void)kind;
    (void)count;
    if (out_query != 0) {
        *out_query = 0;
    }
    return context == 0 || out_query == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                         : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_begin_query(openagc_gl_context *context, openagc_gl_query *query,
                                     uint32_t index)
{
    (void)query;
    (void)index;
    return context == 0 || query == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_end_query(openagc_gl_context *context)
{
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_get_query(const openagc_gl_query *query, uint32_t index,
                                   uint32_t *available)
{
    (void)index;
    if (available != 0) {
        *available = 0u;
    }
    return query == 0 || available == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_destroy_query(openagc_gl_query *query)
{
    return query == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_dispatch_compute(openagc_gl_context *context, uint32_t groups_x,
                                           uint32_t groups_y, uint32_t groups_z)
{
    (void)groups_x;
    (void)groups_y;
    (void)groups_z;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_draw_arrays_indirect(openagc_gl_context *context,
                                              openagc_gl_buffer *buffer, uint64_t offset)
{
    (void)offset;
    return context == 0 || buffer == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                                       : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_draw_arrays(openagc_gl_context *context, uint32_t first,
                                      uint32_t count)
{
    (void)first;
    (void)count;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_draw_elements(openagc_gl_context *context, uint32_t count,
                                        uint32_t type, uint64_t byte_offset)
{
    (void)count;
    (void)type;
    (void)byte_offset;
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

openagc_result openagc_gl_bind_default_framebuffer(openagc_gl_context *context)
{
    return context == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}
