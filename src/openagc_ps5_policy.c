/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"

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

openagc_result openagc_shader_pipeline_plan_destroy(openagc_shader_pipeline_plan *plan)
{
    return plan == 0 ? OPENAGC_ERROR_INVALID_ARGUMENT
                     : OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}
