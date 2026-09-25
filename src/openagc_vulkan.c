/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/vulkan.h"

#include <stdlib.h>
#include <string.h>

struct openagc_vk_instance {
    openagc_context *context;
    openagc_gpu_device *gpu;
    uint32_t device_alive;
};

struct openagc_vk_device {
    openagc_vk_instance *instance;
    openagc_frontend_device *frontend;
    uint32_t child_count;
};

struct openagc_vk_memory {
    openagc_vk_device *device;
    openagc_frontend_memory *memory;
};

struct openagc_vk_buffer {
    openagc_vk_device *device;
    openagc_frontend_buffer *buffer;
    uint32_t descriptor_count;
};

struct openagc_vk_sampler {
    openagc_vk_device *device;
    openagc_frontend_sampler *sampler;
    uint32_t descriptor_count;
};

#define OPENAGC_VK_DESCRIPTOR_SLOTS 8u

struct openagc_vk_descriptor_set {
    openagc_vk_device *device;
    openagc_vk_buffer *buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
    uint64_t buffer_offsets[OPENAGC_VK_DESCRIPTOR_SLOTS];
    uint64_t buffer_sizes[OPENAGC_VK_DESCRIPTOR_SLOTS];
    openagc_vk_image_view *sampled[OPENAGC_VK_DESCRIPTOR_SLOTS];
    openagc_vk_sampler *sampler;
    openagc_frontend_pipeline_layout *layout;
};

struct openagc_vk_image {
    openagc_vk_device *device;
    openagc_frontend_image *image;
    uint32_t view_count;
};

struct openagc_vk_image_view {
    openagc_vk_device *device;
    openagc_vk_image *image;
    uint32_t view_type;
    uint32_t format;
    uint32_t aspect_mask;
    uint32_t descriptor_count;
};

#define OPENAGC_VK_MAX_COMMANDS 8u

typedef enum openagc_vk_command_kind {
    OPENAGC_VK_COMMAND_COPY = 1u,
    OPENAGC_VK_COMMAND_BARRIER = 2u,
    OPENAGC_VK_COMMAND_CLEAR = 3u,
    OPENAGC_VK_COMMAND_COPY_TO_IMAGE = 4u,
    OPENAGC_VK_COMMAND_COPY_FROM_IMAGE = 5u,
    OPENAGC_VK_COMMAND_COPY_IMAGE = 6u,
    OPENAGC_VK_COMMAND_FILL_BUFFER = 7u,
    OPENAGC_VK_COMMAND_UPDATE_BUFFER = 8u
} openagc_vk_command_kind;

typedef struct openagc_vk_command {
    openagc_vk_command_kind kind;
    openagc_vk_buffer *source;
    openagc_vk_buffer *destination;
    openagc_vk_image *image;
    openagc_vk_image *destination_image;
    uint64_t source_offset;
    uint64_t destination_offset;
    uint64_t size_bytes;
    uint32_t layout;
    openagc_color color;
    uint32_t source_x;
    uint32_t source_y;
    uint32_t destination_x;
    uint32_t destination_y;
    uint32_t width;
    uint32_t height;
    uint32_t fill_value;
    uint32_t inline_size;
    uint8_t inline_bytes[OPENAGC_VK_MAX_UPDATE_BYTES];
} openagc_vk_command;

#define OPENAGC_VK_MAX_POOL_BUFFERS 4u

struct openagc_vk_command_pool {
    openagc_vk_device *device;
    openagc_vk_command_buffer *buffers[OPENAGC_VK_MAX_POOL_BUFFERS];
    uint32_t buffer_count;
};

struct openagc_vk_query_pool {
    openagc_vk_device *device;
    openagc_frontend_query_pool *pool;
};

struct openagc_vk_render_pass {
    openagc_vk_device *device;
    openagc_frontend_render_pass *pass;
};

struct openagc_vk_command_buffer {
    openagc_vk_command_pool *pool;
    openagc_vk_command commands[OPENAGC_VK_MAX_COMMANDS];
    uint32_t command_count;
    uint32_t recording;
    uint32_t executable;
    openagc_vk_render_pass *pass;
    openagc_vk_pipeline *graphics;
    openagc_vk_pipeline *compute;
    openagc_vk_descriptor_set *descriptors;
};

struct openagc_vk_fence {
    openagc_vk_device *device;
    openagc_frontend_timeline *timeline;
};

struct openagc_vk_semaphore {
    openagc_vk_device *device;
    openagc_frontend_timeline *timeline;
};

struct openagc_vk_pipeline {
    openagc_vk_device *device;
    openagc_shader_artifact *artifact;
    openagc_frontend_pipeline *pipeline;
};

openagc_result openagc_vk_queue_submit_commands(openagc_vk_device *device,
                                                openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_fence *fence);

static void openagc_vk_release_device(openagc_vk_device *device)
{
    if (device->frontend != NULL) {
        (void)openagc_frontend_device_destroy(device->frontend);
    }
    if (device->instance != NULL) {
        device->instance->device_alive = 0u;
    }
    free(device);
}

openagc_result openagc_vk_instance_create(const openagc_vk_instance_desc *desc,
                                          openagc_vk_instance **out_instance)
{
    openagc_vk_instance *instance;
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc gpu_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_result result;

    if (out_instance == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_instance = NULL;
    if (desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    instance = (openagc_vk_instance *)calloc(1u, sizeof(*instance));
    if (instance == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    result = openagc_context_create(&context_desc, &instance->context);
    if (result != OPENAGC_OK) {
        free(instance);
        return result;
    }
    result = openagc_gpu_device_create(instance->context, &gpu_desc, &instance->gpu);
    if (result != OPENAGC_OK) {
        (void)openagc_context_destroy(instance->context);
        free(instance);
        return result;
    }
    *out_instance = instance;
    return OPENAGC_OK;
}

openagc_result openagc_vk_instance_destroy(openagc_vk_instance *instance)
{
    openagc_result result;

    if (instance == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (instance->device_alive != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_gpu_device_destroy(instance->gpu);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_context_destroy(instance->context);
    if (result != OPENAGC_OK) {
        return result;
    }
    free(instance);
    return OPENAGC_OK;
}

openagc_result openagc_vk_get_capabilities(const openagc_vk_instance *instance,
                                           openagc_vk_capabilities *capabilities)
{
    if (instance == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->physical_device_count = 1u;
    capabilities->copy_queue_count = 1u;
    capabilities->graphics_queue_count = 0u;
    capabilities->compute_queue_count = 0u;
    capabilities->gpu_execution = 0u;
    capabilities->presentation = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_vk_get_format_properties(const openagc_vk_instance *instance,
                                                uint32_t index,
                                                openagc_vk_format_properties *properties)
{
    uint32_t native = 0u;
    openagc_graphics_format format = 0u;
    openagc_result result;

    if (instance == NULL || properties == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (properties->struct_size != sizeof(*properties)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, index, &native);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, native, &format);
    if (result != OPENAGC_OK) {
        return result;
    }
    properties->format = native;
    properties->backend_format = format;
    return OPENAGC_OK;
}

openagc_result openagc_vk_get_queue_family(const openagc_vk_instance *instance,
                                           uint32_t index,
                                           openagc_vk_queue_family *family)
{
    if (instance == NULL || family == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (family->struct_size != sizeof(*family)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (index != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    family->queue_flags = OPENAGC_VK_QUEUE_TRANSFER_BIT;
    family->queue_count = 1u;
    return OPENAGC_OK;
}

openagc_result openagc_vk_device_create(openagc_vk_instance *instance,
                                        const openagc_vk_device_desc *desc,
                                        openagc_vk_device **out_device)
{
    openagc_vk_device *device;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_result result;

    if (out_device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = NULL;
    if (instance == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (instance->device_alive != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (desc->queue_family_index != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    device = (openagc_vk_device *)calloc(1u, sizeof(*device));
    if (device == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    device->instance = instance;
    result = openagc_frontend_device_create(instance->gpu, &frontend_desc, &device->frontend);
    if (result != OPENAGC_OK) {
        free(device);
        return result;
    }
    instance->device_alive = 1u;
    *out_device = device;
    return OPENAGC_OK;
}

openagc_result openagc_vk_device_destroy(openagc_vk_device *device)
{
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->child_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    openagc_vk_release_device(device);
    return OPENAGC_OK;
}

openagc_result openagc_vk_allocate_memory(openagc_vk_device *device, uint64_t size_bytes,
                                          openagc_vk_memory **out_memory)
{
    openagc_vk_memory *memory;
    openagc_result result;

    if (out_memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    memory = (openagc_vk_memory *)calloc(1u, sizeof(*memory));
    if (memory == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    memory->device = device;
    result = openagc_frontend_memory_allocate(device->frontend, size_bytes, &memory->memory);
    if (result != OPENAGC_OK) {
        free(memory);
        return result;
    }
    device->child_count++;
    *out_memory = memory;
    return OPENAGC_OK;
}

openagc_result openagc_vk_memory_write(openagc_vk_memory *memory, uint64_t offset,
                                       const void *bytes, uint64_t size_bytes)
{
    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_memory_write(memory->memory, offset, bytes, size_bytes);
}

openagc_result openagc_vk_memory_read(const openagc_vk_memory *memory, uint64_t offset,
                                      void *bytes, uint64_t size_bytes)
{
    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_memory_read(memory->memory, offset, bytes, size_bytes);
}

openagc_result openagc_vk_free_memory(openagc_vk_memory *memory)
{
    openagc_result result;

    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_memory_destroy(memory->memory);
    if (result != OPENAGC_OK) {
        return result;
    }
    memory->device->child_count--;
    free(memory);
    return OPENAGC_OK;
}

static openagc_result openagc_vk_create_buffer_common(openagc_vk_device *device,
                                                      const openagc_vk_buffer_desc *desc,
                                                      int dedicated,
                                                      openagc_vk_buffer **out_buffer)
{
    openagc_vk_buffer *buffer;
    openagc_frontend_buffer_desc frontend_desc;
    openagc_result result;

    if (out_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    buffer = (openagc_vk_buffer *)calloc(1u, sizeof(*buffer));
    if (buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    buffer->device = device;
    frontend_desc = (openagc_frontend_buffer_desc)OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, desc->usage, desc->size_bytes);
    result = dedicated != 0
                 ? openagc_frontend_buffer_create(device->frontend, &frontend_desc, &buffer->buffer)
                 : openagc_frontend_buffer_create_unbound(device->frontend, &frontend_desc,
                                                          &buffer->buffer);
    if (result != OPENAGC_OK) {
        free(buffer);
        return result;
    }
    device->child_count++;
    *out_buffer = buffer;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_buffer(openagc_vk_device *device,
                                        const openagc_vk_buffer_desc *desc,
                                        openagc_vk_buffer **out_buffer)
{
    return openagc_vk_create_buffer_common(device, desc, 1, out_buffer);
}

openagc_result openagc_vk_create_unbound_buffer(openagc_vk_device *device,
                                                const openagc_vk_buffer_desc *desc,
                                                openagc_vk_buffer **out_buffer)
{
    return openagc_vk_create_buffer_common(device, desc, 0, out_buffer);
}

openagc_result openagc_vk_bind_buffer_memory(openagc_vk_buffer *buffer,
                                             openagc_vk_memory *memory, uint64_t offset)
{
    if (buffer == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->device != memory->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_buffer_bind_memory(buffer->buffer, memory->memory, offset);
}

openagc_result openagc_vk_buffer_upload(openagc_vk_buffer *buffer, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_buffer_upload(buffer->buffer, offset, bytes, size_bytes);
}

openagc_result openagc_vk_buffer_readback(openagc_vk_buffer *buffer, uint64_t offset,
                                          void *bytes, uint64_t size_bytes)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_buffer_readback(buffer->buffer, offset, bytes, size_bytes);
}

openagc_result openagc_vk_buffer_get_info(const openagc_vk_buffer *buffer,
                                          openagc_frontend_buffer_info *info)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_buffer_get_info(buffer->buffer, info);
}

openagc_result openagc_vk_create_descriptor_set(openagc_vk_device *device,
                                                openagc_vk_descriptor_set **out_set)
{
    openagc_vk_descriptor_set *set;

    if (out_set == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_set = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    set = (openagc_vk_descriptor_set *)calloc(1u, sizeof(*set));
    if (set == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    set->device = device;
    device->child_count++;
    *out_set = set;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_descriptor_set_for_pipeline(
    openagc_vk_device *device, openagc_vk_pipeline *pipeline, openagc_vk_descriptor_set **out_set)
{
    openagc_frontend_pipeline_layout *layout = NULL;
    openagc_result result;

    if (pipeline == NULL || pipeline->device != device) {
        if (out_set != NULL) {
            *out_set = NULL;
        }
        return pipeline == NULL || device == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                                  : OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_pipeline_get_layout(pipeline->pipeline, &layout);
    if (result != OPENAGC_OK) {
        if (out_set != NULL) {
            *out_set = NULL;
        }
        return result;
    }
    result = openagc_vk_create_descriptor_set(device, out_set);
    if (result != OPENAGC_OK) {
        return result;
    }
    (*out_set)->layout = layout;
    return openagc_frontend_pipeline_layout_retain(layout);
}

openagc_result openagc_vk_update_descriptor_set(openagc_vk_descriptor_set *set,
                                                openagc_vk_buffer *buffer)
{
    return openagc_vk_update_descriptor_set_at(set, buffer, 0u);
}

openagc_result openagc_vk_update_descriptor_set_at(openagc_vk_descriptor_set *set,
                                                   openagc_vk_buffer *buffer, uint32_t binding)
{
    openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_result result;

    if (set == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (binding >= OPENAGC_VK_DESCRIPTOR_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (buffer->device != set->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_buffer_get_info(buffer->buffer, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if ((info.usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (set->buffers[binding] == buffer) {
        return OPENAGC_OK;
    }
    if (set->buffers[binding] != NULL) {
        set->buffers[binding]->descriptor_count--;
    }
    set->buffers[binding] = buffer;
    set->buffer_offsets[binding] = 0u;
    set->buffer_sizes[binding] = 0u;
    buffer->descriptor_count++;
    return OPENAGC_OK;
}

openagc_result openagc_vk_update_descriptor_buffer_range(openagc_vk_descriptor_set *set,
                                                         openagc_vk_buffer *buffer,
                                                         uint32_t binding, uint64_t offset,
                                                         uint64_t size_bytes)
{
    openagc_result result = openagc_vk_update_descriptor_set_at(set, buffer, binding);

    if (result != OPENAGC_OK) {
        return result;
    }
    if (size_bytes == 0u || (offset & 3u) != 0u || (size_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    {
        openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;

        result = openagc_frontend_buffer_get_info(buffer->buffer, &info);
        if (result != OPENAGC_OK) {
            return result;
        }
        if (offset > info.size_bytes || size_bytes > info.size_bytes - offset) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    set->buffer_offsets[binding] = offset;
    set->buffer_sizes[binding] = size_bytes;
    return OPENAGC_OK;
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
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_result result;

    if (set == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (binding >= OPENAGC_VK_DESCRIPTOR_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (view->device != set->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_image_get_info(view->image->image, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.state != OPENAGC_GRAPHICS_STATE_SHADER_READ ||
        info.owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (set->sampled[binding] == view) {
        return OPENAGC_OK;
    }
    if (set->sampled[binding] != NULL) {
        set->sampled[binding]->descriptor_count--;
    }
    set->sampled[binding] = view;
    view->descriptor_count++;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_sampler(openagc_vk_device *device, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_vk_sampler **out_sampler)
{
    openagc_vk_sampler *sampler;
    openagc_frontend_sampler_desc desc;
    openagc_result result;

    if (out_sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    sampler = (openagc_vk_sampler *)calloc(1u, sizeof(*sampler));
    if (sampler == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    sampler->device = device;
    desc = (openagc_frontend_sampler_desc)OPENAGC_FRONTEND_SAMPLER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, mag_filter, min_filter, address_mode);
    result = openagc_frontend_sampler_create(device->frontend, &desc, &sampler->sampler);
    if (result != OPENAGC_OK) {
        free(sampler);
        return result;
    }
    device->child_count++;
    *out_sampler = sampler;
    return OPENAGC_OK;
}

openagc_result openagc_vk_update_descriptor_sampler(openagc_vk_descriptor_set *set,
                                                    openagc_vk_sampler *sampler)
{
    if (set == NULL || sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler->device != set->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (set->sampler == sampler) {
        return OPENAGC_OK;
    }
    if (set->sampler != NULL) {
        set->sampler->descriptor_count--;
    }
    set->sampler = sampler;
    sampler->descriptor_count++;
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_sampler(openagc_vk_sampler *sampler)
{
    openagc_result result;

    if (sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler->descriptor_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_sampler_destroy(sampler->sampler);
    if (result != OPENAGC_OK) {
        return result;
    }
    sampler->device->child_count--;
    free(sampler);
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_descriptor_set(openagc_vk_descriptor_set *set)
{
    if (set == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    {
        uint32_t slot;

        for (slot = 0u; slot < OPENAGC_VK_DESCRIPTOR_SLOTS; ++slot) {
            if (set->buffers[slot] != NULL) {
                set->buffers[slot]->descriptor_count--;
                set->buffers[slot] = NULL;
            }
            if (set->sampled[slot] != NULL) {
                set->sampled[slot]->descriptor_count--;
                set->sampled[slot] = NULL;
            }
        }
    }
    if (set->sampler != NULL) {
        set->sampler->descriptor_count--;
        set->sampler = NULL;
    }
    if (set->layout != NULL) {
        (void)openagc_frontend_pipeline_layout_release(set->layout);
        set->layout = NULL;
    }
    set->device->child_count--;
    free(set);
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_buffer(openagc_vk_buffer *buffer)
{
    openagc_result result;

    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->descriptor_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_buffer_destroy(buffer->buffer);
    if (result != OPENAGC_OK) {
        return result;
    }
    buffer->device->child_count--;
    free(buffer);
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_fence(openagc_vk_device *device,
                                       openagc_vk_fence **out_fence)
{
    openagc_vk_fence *fence;
    openagc_result result;

    if (out_fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_fence = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    fence = (openagc_vk_fence *)calloc(1u, sizeof(*fence));
    if (fence == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    fence->device = device;
    result = openagc_frontend_timeline_create(device->frontend, &fence->timeline);
    if (result != OPENAGC_OK) {
        free(fence);
        return result;
    }
    device->child_count++;
    *out_fence = fence;
    return OPENAGC_OK;
}

openagc_result openagc_vk_queue_submit(openagc_vk_device *device, openagc_vk_fence *fence)
{
    if (device == NULL || fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (fence->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_timeline_signal(fence->timeline);
}

openagc_result openagc_vk_fence_poll(const openagc_vk_fence *fence, uint64_t value,
                                     openagc_frontend_timeline_info *info)
{
    if (fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_timeline_poll(fence->timeline, value, info);
}

openagc_result openagc_vk_create_semaphore(openagc_vk_device *device,
                                           openagc_vk_semaphore **out_semaphore)
{
    openagc_vk_semaphore *semaphore;
    openagc_result result;

    if (out_semaphore == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_semaphore = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    semaphore = (openagc_vk_semaphore *)calloc(1u, sizeof(*semaphore));
    if (semaphore == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    semaphore->device = device;
    result = openagc_frontend_timeline_create(device->frontend, &semaphore->timeline);
    if (result != OPENAGC_OK) {
        free(semaphore);
        return result;
    }
    device->child_count++;
    *out_semaphore = semaphore;
    return OPENAGC_OK;
}

openagc_result openagc_vk_semaphore_poll(const openagc_vk_semaphore *semaphore, uint64_t value,
                                         openagc_frontend_timeline_info *info)
{
    if (semaphore == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_timeline_poll(semaphore->timeline, value, info);
}

openagc_result openagc_vk_destroy_semaphore(openagc_vk_semaphore *semaphore)
{
    openagc_result result;

    if (semaphore == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_timeline_destroy(semaphore->timeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    semaphore->device->child_count--;
    free(semaphore);
    return OPENAGC_OK;
}

openagc_result openagc_vk_queue_submit_wait(openagc_vk_device *device,
                                            openagc_vk_command_buffer *command_buffer,
                                            const openagc_vk_semaphore *wait, uint64_t wait_value,
                                            openagc_vk_semaphore *signal)
{
    openagc_frontend_timeline_info info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_result result;

    if (device == NULL || command_buffer == NULL || signal == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pool->device != device || signal->device != device ||
        (wait != NULL && wait->device != device)) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (wait != NULL) {
        result = openagc_frontend_timeline_poll(wait->timeline, wait_value, &info);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    result = openagc_vk_queue_submit_commands(device, command_buffer, NULL);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_timeline_signal(signal->timeline);
}

openagc_result openagc_vk_destroy_fence(openagc_vk_fence *fence)
{
    openagc_result result;

    if (fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_timeline_destroy(fence->timeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    fence->device->child_count--;
    free(fence);
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_image(openagc_vk_device *device,
                                       const openagc_vk_image_desc *desc,
                                       openagc_vk_image **out_image)
{
    openagc_vk_image *image;
    openagc_frontend_image_desc frontend_desc;
    openagc_result result;

    if (out_image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    image = (openagc_vk_image *)calloc(1u, sizeof(*image));
    if (image == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    image->device = device;
    frontend_desc = (openagc_frontend_image_desc)OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, desc->format, desc->usage, desc->initial_layout,
        desc->width, desc->height, 0u);
    result = openagc_frontend_image_create(device->frontend, &frontend_desc, &image->image);
    if (result != OPENAGC_OK) {
        free(image);
        return result;
    }
    device->child_count++;
    *out_image = image;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_unbound_image(openagc_vk_device *device,
                                               const openagc_vk_image_desc *desc,
                                               openagc_vk_image **out_image)
{
    openagc_vk_image *image;
    openagc_frontend_image_desc frontend_desc;
    openagc_result result;

    if (out_image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    image = (openagc_vk_image *)calloc(1u, sizeof(*image));
    if (image == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    image->device = device;
    frontend_desc = (openagc_frontend_image_desc)OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, desc->format, desc->usage, desc->initial_layout,
        desc->width, desc->height, 0u);
    result = openagc_frontend_image_create_unbound(device->frontend, &frontend_desc, &image->image);
    if (result != OPENAGC_OK) {
        free(image);
        return result;
    }
    device->child_count++;
    *out_image = image;
    return OPENAGC_OK;
}

openagc_result openagc_vk_bind_image_memory(openagc_vk_image *image,
                                            openagc_vk_memory *memory, uint64_t offset)
{
    if (image == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->device != memory->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_image_bind_memory(image->image, memory->memory, offset);
}

openagc_result openagc_vk_image_get_info(const openagc_vk_image *image,
                                         openagc_frontend_image_info *info)
{
    if (image == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_get_info(image->image, info);
}

openagc_result openagc_vk_image_readback(openagc_vk_image *image, uint64_t offset,
                                         void *bytes, uint64_t size_bytes)
{
    if (image == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_readback(image->image, offset, bytes, size_bytes);
}

openagc_result openagc_vk_destroy_image(openagc_vk_image *image)
{
    openagc_result result;

    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->view_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_image_destroy(image->image);
    if (result != OPENAGC_OK) {
        return result;
    }
    image->device->child_count--;
    free(image);
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_image_view(openagc_vk_device *device,
                                            openagc_vk_image *image,
                                            const openagc_vk_image_view_desc *desc,
                                            openagc_vk_image_view **out_view)
{
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_vk_image_view *view;
    openagc_result result;

    if (out_view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_view = NULL;
    if (device == NULL || image == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (image->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (desc->view_type != OPENAGC_VK_IMAGE_VIEW_TYPE_2D ||
        desc->aspect_mask != OPENAGC_VK_IMAGE_ASPECT_COLOR_BIT ||
        desc->base_mip_level != 0u || desc->level_count != 1u ||
        desc->base_array_layer != 0u || desc->layer_count != 1u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    result = openagc_frontend_image_get_info(image->image, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (desc->format != info.native_format) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    view = (openagc_vk_image_view *)calloc(1u, sizeof(*view));
    if (view == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    view->device = device;
    view->image = image;
    view->view_type = desc->view_type;
    view->format = desc->format;
    view->aspect_mask = desc->aspect_mask;
    image->view_count++;
    device->child_count++;
    *out_view = view;
    return OPENAGC_OK;
}

openagc_result openagc_vk_image_view_get_info(const openagc_vk_image_view *view,
                                              openagc_vk_image_view_info *info)
{
    openagc_frontend_image_info image_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_result result;

    if (view == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_frontend_image_get_info(view->image->image, &image_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    info->view_type = view->view_type;
    info->format = view->format;
    info->aspect_mask = view->aspect_mask;
    info->width = image_info.width;
    info->height = image_info.height;
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_image_view(openagc_vk_image_view *view)
{
    if (view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (view->descriptor_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    view->image->view_count--;
    view->device->child_count--;
    free(view);
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_command_pool(openagc_vk_device *device,
                                              const openagc_vk_command_pool_desc *desc,
                                              openagc_vk_command_pool **out_pool)
{
    openagc_vk_command_pool *pool;

    if (out_pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pool = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_VK_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->queue_family_index != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    pool = (openagc_vk_command_pool *)calloc(1u, sizeof(*pool));
    if (pool == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pool->device = device;
    device->child_count++;
    *out_pool = pool;
    return OPENAGC_OK;
}

openagc_result openagc_vk_command_pool_reset(openagc_vk_command_pool *pool)
{
    uint32_t index;

    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < pool->buffer_count; ++index) {
        if (pool->buffers[index]->recording != 0u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    for (index = 0u; index < pool->buffer_count; ++index) {
        pool->buffers[index]->command_count = 0u;
        pool->buffers[index]->executable = 0u;
    }
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_command_pool(openagc_vk_command_pool *pool)
{
    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pool->buffer_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    pool->device->child_count--;
    free(pool);
    return OPENAGC_OK;
}

openagc_result openagc_vk_allocate_command_buffer(openagc_vk_command_pool *pool,
                                                  openagc_vk_command_buffer **out_command_buffer)
{
    openagc_vk_command_buffer *command_buffer;

    if (out_command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = NULL;
    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pool->buffer_count == OPENAGC_VK_MAX_POOL_BUFFERS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command_buffer = (openagc_vk_command_buffer *)calloc(1u, sizeof(*command_buffer));
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    command_buffer->pool = pool;
    pool->buffers[pool->buffer_count] = command_buffer;
    pool->buffer_count++;
    pool->device->child_count++;
    *out_command_buffer = command_buffer;
    return OPENAGC_OK;
}

openagc_result openagc_vk_command_buffer_begin(openagc_vk_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->command_count = 0u;
    command_buffer->executable = 0u;
    command_buffer->recording = 1u;
    return OPENAGC_OK;
}

static openagc_result openagc_vk_push(openagc_vk_command_buffer *command_buffer,
                                      const openagc_vk_command *command)
{
    if (command_buffer == NULL || command == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == OPENAGC_VK_MAX_COMMANDS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command_buffer->commands[command_buffer->command_count] = *command;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_copy_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *source, uint64_t source_offset,
                                          openagc_vk_buffer *destination,
                                          uint64_t destination_offset, uint64_t size_bytes)
{
    openagc_vk_command command;

    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || source->device != command_buffer->pool->device ||
        destination->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_COPY;
    command.source = source;
    command.destination = destination;
    command.source_offset = source_offset;
    command.destination_offset = destination_offset;
    command.size_bytes = size_bytes;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_image_barrier(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_image *image, uint32_t layout)
{
    openagc_vk_command command;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || image->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, layout, &state,
                                                     &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    (void)state;
    (void)owner;
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_BARRIER;
    command.image = image;
    command.layout = layout;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_clear_color(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_image *image, openagc_color color)
{
    openagc_vk_command command;

    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || image->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_CLEAR;
    command.image = image;
    command.color = color;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_copy_buffer_to_image(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_buffer *source,
                                                   uint64_t source_offset,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset, uint64_t size_bytes)
{
    openagc_vk_command command;

    if (source == NULL || image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || source->device != command_buffer->pool->device ||
        image->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_COPY_TO_IMAGE;
    command.source = source;
    command.image = image;
    command.source_offset = source_offset;
    command.destination_offset = image_offset;
    command.size_bytes = size_bytes;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_copy_image_to_buffer(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset,
                                                   openagc_vk_buffer *destination,
                                                   uint64_t destination_offset,
                                                   uint64_t size_bytes)
{
    openagc_vk_command command;

    if (image == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || image->device != command_buffer->pool->device ||
        destination->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_COPY_FROM_IMAGE;
    command.image = image;
    command.destination = destination;
    command.source_offset = image_offset;
    command.destination_offset = destination_offset;
    command.size_bytes = size_bytes;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_copy_image(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_image *source, uint32_t source_x,
                                         uint32_t source_y, openagc_vk_image *destination,
                                         uint32_t destination_x, uint32_t destination_y,
                                         uint32_t width, uint32_t height)
{
    openagc_vk_command command;

    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || source->device != command_buffer->pool->device ||
        destination->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_COPY_IMAGE;
    command.image = source;
    command.destination_image = destination;
    command.source_x = source_x;
    command.source_y = source_y;
    command.destination_x = destination_x;
    command.destination_y = destination_y;
    command.width = width;
    command.height = height;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_fill_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset,
                                          uint64_t size_bytes, uint32_t value)
{
    openagc_vk_command command;
    openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_result result;

    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || buffer->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_buffer_get_info(buffer->buffer, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if ((info.usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (size_bytes == 0u || (offset & 3u) != 0u || (size_bytes & 3u) != 0u ||
        offset > info.size_bytes || size_bytes > info.size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_FILL_BUFFER;
    command.destination = buffer;
    command.destination_offset = offset;
    command.size_bytes = size_bytes;
    command.fill_value = value;
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_cmd_update_buffer(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_buffer *buffer, uint64_t offset,
                                            const void *bytes, uint64_t size_bytes)
{
    openagc_vk_command command;
    openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_result result;

    if (buffer == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer == NULL || buffer->device != command_buffer->pool->device) {
        return command_buffer == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                                      : OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_buffer_get_info(buffer->buffer, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if ((info.usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (size_bytes == 0u || size_bytes > OPENAGC_VK_MAX_UPDATE_BYTES ||
        (offset & 3u) != 0u || (size_bytes & 3u) != 0u || offset > info.size_bytes ||
        size_bytes > info.size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    memset(&command, 0, sizeof(command));
    command.kind = OPENAGC_VK_COMMAND_UPDATE_BUFFER;
    command.destination = buffer;
    command.destination_offset = offset;
    command.size_bytes = size_bytes;
    command.inline_size = (uint32_t)size_bytes;
    memcpy(command.inline_bytes, bytes, (size_t)size_bytes);
    return openagc_vk_push(command_buffer, &command);
}

openagc_result openagc_vk_command_buffer_end(openagc_vk_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->recording = 0u;
    command_buffer->executable = 1u;
    command_buffer->compute = NULL;
    command_buffer->descriptors = NULL;
    return OPENAGC_OK;
}

openagc_result openagc_vk_queue_submit_commands(openagc_vk_device *device,
                                                openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_fence *fence)
{
    uint32_t index;
    openagc_result result;

    if (device == NULL || command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pool->device != device || (fence != NULL && fence->device != device)) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (command_buffer->executable == 0u || command_buffer->recording != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    for (index = 0u; index < command_buffer->command_count; ++index) {
        const openagc_vk_command *command = &command_buffer->commands[index];
        if (command->kind == OPENAGC_VK_COMMAND_COPY) {
            result = openagc_frontend_buffer_copy(command->source->buffer,
                                                  command->source_offset,
                                                  command->destination->buffer,
                                                  command->destination_offset,
                                                  command->size_bytes);
        } else if (command->kind == OPENAGC_VK_COMMAND_CLEAR) {
            result = openagc_frontend_image_clear(command->image->image, command->color);
        } else if (command->kind == OPENAGC_VK_COMMAND_COPY_TO_IMAGE) {
            result = openagc_frontend_copy_buffer_to_image(command->source->buffer,
                                                           command->source_offset,
                                                           command->image->image,
                                                           command->destination_offset,
                                                           command->size_bytes);
        } else if (command->kind == OPENAGC_VK_COMMAND_COPY_FROM_IMAGE) {
            result = openagc_frontend_copy_image_to_buffer(command->image->image,
                                                           command->source_offset,
                                                           command->destination->buffer,
                                                           command->destination_offset,
                                                           command->size_bytes);
        } else if (command->kind == OPENAGC_VK_COMMAND_COPY_IMAGE) {
            result = openagc_frontend_image_copy_rect(
                command->image->image, command->source_x, command->source_y,
                command->destination_image->image, command->destination_x,
                command->destination_y, command->width, command->height);
        } else if (command->kind == OPENAGC_VK_COMMAND_FILL_BUFFER) {
            result = openagc_frontend_buffer_fill(command->destination->buffer,
                                                  command->destination_offset,
                                                  command->size_bytes, command->fill_value);
        } else if (command->kind == OPENAGC_VK_COMMAND_UPDATE_BUFFER) {
            result = openagc_frontend_buffer_upload(command->destination->buffer,
                                                    command->destination_offset,
                                                    command->inline_bytes,
                                                    command->inline_size);
        } else {
            openagc_graphics_image_state state;
            openagc_graphics_owner owner;
            result = openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN,
                                                             command->layout, &state, &owner);
            if (result == OPENAGC_OK) {
                result = openagc_frontend_image_transition(command->image->image, state, owner);
            }
        }
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    command_buffer->executable = 0u;
    return fence == NULL ? OPENAGC_OK : openagc_frontend_timeline_signal(fence->timeline);
}

openagc_result openagc_vk_destroy_command_buffer(openagc_vk_command_buffer *command_buffer)
{
    openagc_vk_command_pool *pool;
    uint32_t index;

    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    pool = command_buffer->pool;
    for (index = 0u; index < pool->buffer_count; ++index) {
        if (pool->buffers[index] == command_buffer) {
            pool->buffers[index] = pool->buffers[pool->buffer_count - 1u];
            pool->buffer_count--;
            break;
        }
    }
    pool->device->child_count--;
    free(command_buffer);
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_graphics_pipeline(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_pipeline **out_pipeline)
{
    return openagc_vk_create_graphics_pipeline_with_bindings(
        device, vertex, pixel, color_target, NULL, 0u, 0u, 0u, NULL, 0u, out_pipeline);
}

openagc_result openagc_vk_create_graphics_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline)
{
    const openagc_vk_buffer *uniforms[1];
    const openagc_vk_image *images[1];
    uint64_t offsets[1];
    uint64_t sizes[1];
    uint32_t resource_bindings[1];
    uint32_t texture_bindings[1];

    uniforms[0] = uniform;
    images[0] = sampled;
    offsets[0] = offset;
    sizes[0] = size_bytes;
    resource_bindings[0] = resource_binding;
    texture_bindings[0] = texture_binding;
    return openagc_vk_create_graphics_pipeline_with_resources(
        device, vertex, pixel, color_target, uniform == NULL ? NULL : uniforms, offsets, sizes,
        resource_bindings, uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images,
        texture_bindings, sampled == NULL ? 0u : 1u, out_pipeline);
}

openagc_result openagc_vk_create_graphics_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline)
{
    const openagc_frontend_buffer *frontend_buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
    const openagc_frontend_image *frontend_images[OPENAGC_VK_DESCRIPTOR_SLOTS];
    openagc_vk_pipeline *pipeline;
    uint32_t index;
    openagc_result result;

    if (out_pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = NULL;
    if (device == NULL || color_target == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (resource_count > OPENAGC_VK_DESCRIPTOR_SLOTS ||
        texture_count > OPENAGC_VK_DESCRIPTOR_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (color_target->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    for (index = 0u; index < resource_count; ++index) {
        if (uniforms == NULL || uniforms[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (uniforms[index]->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_buffers[index] = uniforms[index]->buffer;
    }
    for (index = 0u; index < texture_count; ++index) {
        if (sampled == NULL || sampled[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (sampled[index]->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_images[index] = sampled[index]->image;
    }
    pipeline = (openagc_vk_pipeline *)calloc(1u, sizeof(*pipeline));
    if (pipeline == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pipeline->device = device;
    result = openagc_frontend_graphics_pipeline_create_with_resources(
        device->frontend, vertex, pixel, color_target->image,
        resource_count == 0u ? NULL : frontend_buffers, offsets, sizes, resource_bindings,
        resource_count, texture_count == 0u ? NULL : frontend_images, texture_bindings,
        texture_count, &pipeline->pipeline);
    if (result != OPENAGC_OK) {
        free(pipeline);
        return result;
    }
    device->child_count++;
    *out_pipeline = pipeline;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_compute_pipeline(openagc_vk_device *device,
                                                  const openagc_shader_artifact_desc *desc,
                                                  openagc_vk_pipeline **out_pipeline)
{
    return openagc_vk_create_compute_pipeline_with_bindings(device, desc, NULL, 0u, 0u, 0u, NULL,
                                                           0u, out_pipeline);
}

openagc_result openagc_vk_create_compute_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline)
{
    const openagc_vk_buffer *uniforms[1];
    const openagc_vk_image *images[1];
    uint64_t offsets[1];
    uint64_t sizes[1];
    uint32_t resource_bindings[1];
    uint32_t texture_bindings[1];

    uniforms[0] = uniform;
    images[0] = sampled;
    offsets[0] = offset;
    sizes[0] = size_bytes;
    resource_bindings[0] = resource_binding;
    texture_bindings[0] = texture_binding;
    return openagc_vk_create_compute_pipeline_with_resources(
        device, desc, uniform == NULL ? NULL : uniforms, offsets, sizes, resource_bindings,
        uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images, texture_bindings,
        sampled == NULL ? 0u : 1u, out_pipeline);
}

openagc_result openagc_vk_create_compute_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline)
{
    openagc_vk_pipeline *pipeline;
    openagc_shader_pipeline_desc plan = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_resource_binding resources[OPENAGC_VK_DESCRIPTOR_SLOTS];
    openagc_shader_texture_binding textures[OPENAGC_VK_DESCRIPTOR_SLOTS];
    const openagc_frontend_buffer *frontend_buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
    const openagc_frontend_image *frontend_images[OPENAGC_VK_DESCRIPTOR_SLOTS];
    uint32_t index;
    openagc_result result;

    if (out_pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->stage != OPENAGC_SHADER_STAGE_COMPUTE) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (resource_count > OPENAGC_VK_DESCRIPTOR_SLOTS ||
        texture_count > OPENAGC_VK_DESCRIPTOR_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < resource_count; ++index) {
        if (uniforms == NULL || uniforms[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (uniforms[index]->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_buffers[index] = uniforms[index]->buffer;
    }
    for (index = 0u; index < texture_count; ++index) {
        if (sampled == NULL || sampled[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (sampled[index]->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_images[index] = sampled[index]->image;
    }
    pipeline = (openagc_vk_pipeline *)calloc(1u, sizeof(*pipeline));
    if (pipeline == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pipeline->device = device;
    result = openagc_shader_artifact_intake_host(device->instance->gpu, desc, &pipeline->artifact);
    if (result != OPENAGC_OK) {
        free(pipeline);
        return result;
    }
    plan.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    plan.compute = pipeline->artifact;
    memset(resources, 0, sizeof(resources));
    memset(textures, 0, sizeof(textures));
    result = openagc_frontend_apply_reflection_set(
        resource_count == 0u ? NULL : frontend_buffers, offsets, sizes, resource_bindings,
        resource_count, texture_count == 0u ? NULL : frontend_images, texture_bindings,
        texture_count, resources, textures, &plan);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(pipeline->artifact);
        free(pipeline);
        return result;
    }
    result = openagc_frontend_pipeline_create(device->frontend, &plan, &pipeline->pipeline);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(pipeline->artifact);
        free(pipeline);
        return result;
    }
    device->child_count++;
    *out_pipeline = pipeline;
    return OPENAGC_OK;
}

openagc_result openagc_vk_pipeline_set_vertex_stride(openagc_vk_pipeline *pipeline, uint32_t stride)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_stride(pipeline->pipeline, stride);
}

openagc_result openagc_vk_pipeline_set_sampler(openagc_vk_pipeline *pipeline,
                                             openagc_vk_sampler *sampler)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler != NULL && sampler->device != pipeline->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_pipeline_set_sampler(
        pipeline->pipeline, sampler == NULL ? NULL : sampler->sampler);
}

openagc_result openagc_vk_pipeline_set_vertex_input(openagc_vk_pipeline *pipeline, uint32_t stride,
                                                    uint32_t attribute_offset,
                                                    uint32_t attribute_bytes)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_input(pipeline->pipeline, stride, attribute_offset,
                                                     attribute_bytes);
}

openagc_result openagc_vk_pipeline_set_vertex_attributes(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_attributes(pipeline->pipeline, stride, offsets,
                                                          bytes, count);
}

openagc_result openagc_vk_cmd_push_constants(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline, uint32_t offset,
                                             const void *bytes, uint32_t size)
{
    if (command_buffer == NULL || pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL || command_buffer->graphics != pipeline) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_pipeline_write_push_constants(pipeline->pipeline, offset, bytes, size);
}

openagc_result openagc_vk_pipeline_read_push_constants(const openagc_vk_pipeline *pipeline,
                                                       uint32_t offset, void *bytes,
                                                       uint32_t size)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_read_push_constants(pipeline->pipeline, offset, bytes, size);
}

openagc_result openagc_vk_pipeline_set_blend(openagc_vk_pipeline *pipeline, uint32_t enable,
                                             uint32_t src_factor, uint32_t dst_factor)
{
    openagc_frontend_blend_factor src = 0u;
    openagc_frontend_blend_factor dst = 0u;
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_translate_blend_factor(OPENAGC_FRONTEND_VULKAN, src_factor, &src);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_translate_blend_factor(OPENAGC_FRONTEND_VULKAN, dst_factor, &dst);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_pipeline_set_blend(pipeline->pipeline, enable, src, dst);
}

openagc_result openagc_vk_pipeline_get_blend(const openagc_vk_pipeline *pipeline, uint32_t *enable,
                                             openagc_frontend_blend_factor *src,
                                             openagc_frontend_blend_factor *dst)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_blend(pipeline->pipeline, enable, src, dst);
}

openagc_result openagc_vk_pipeline_set_topology(openagc_vk_pipeline *pipeline, uint32_t topology)
{
    openagc_frontend_primitive primitive = 0u;
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_translate_primitive(OPENAGC_FRONTEND_VULKAN, topology, &primitive);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_pipeline_set_primitive(pipeline->pipeline, primitive);
}

openagc_result openagc_vk_pipeline_get_primitive(const openagc_vk_pipeline *pipeline,
                                                 openagc_frontend_primitive *out_primitive)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_primitive(pipeline->pipeline, out_primitive);
}

openagc_result openagc_vk_pipeline_set_vertex_rates(openagc_vk_pipeline *pipeline,
                                                 const uint32_t *rates, uint32_t count)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_rates(pipeline->pipeline, rates, count);
}

openagc_result openagc_vk_pipeline_set_vertex_formats(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *formats, uint32_t count)
{
    openagc_frontend_vertex_format shared[8];
    uint32_t bytes[8];
    uint32_t index;
    openagc_result result;

    if (pipeline == NULL || offsets == NULL || formats == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u || count > 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < count; ++index) {
        result = openagc_frontend_translate_vertex_format(OPENAGC_FRONTEND_VULKAN, formats[index],
                                                          0u, &shared[index], &bytes[index]);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    return openagc_frontend_pipeline_set_vertex_formats(pipeline->pipeline, stride, offsets, shared,
                                                       count);
}

openagc_result openagc_vk_pipeline_get_vertex_format(const openagc_vk_pipeline *pipeline,
                                                    uint32_t index, uint32_t *offset,
                                                    openagc_frontend_vertex_format *format,
                                                    uint32_t *bytes)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_vertex_format(pipeline->pipeline, index, offset, format,
                                                      bytes);
}

openagc_result openagc_vk_pipeline_get_vertex_attribute(const openagc_vk_pipeline *pipeline,
                                                      uint32_t index, uint32_t *offset,
                                                      uint32_t *bytes)
{
    if (pipeline == NULL) {
        if (offset != NULL) {
            *offset = 0u;
        }
        if (bytes != NULL) {
            *bytes = 0u;
        }
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_vertex_attribute(pipeline->pipeline, index, offset, bytes);
}

openagc_result openagc_vk_pipeline_get_layout(
    const openagc_vk_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout)
{
    if (pipeline == NULL) {
        if (out_layout != NULL) {
            *out_layout = NULL;
        }
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_layout(pipeline->pipeline, out_layout);
}

openagc_result openagc_vk_pipeline_get_info(const openagc_vk_pipeline *pipeline,
                                            openagc_frontend_pipeline_info *info)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_info(pipeline->pipeline, info);
}

openagc_result openagc_vk_destroy_pipeline(openagc_vk_pipeline *pipeline)
{
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_pipeline_destroy(pipeline->pipeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (pipeline->artifact == NULL) {
        pipeline->device->child_count--;
        free(pipeline);
        return OPENAGC_OK;
    }
    result = openagc_shader_artifact_destroy(pipeline->artifact);
    if (result != OPENAGC_OK) {
        return result;
    }
    pipeline->device->child_count--;
    free(pipeline);
    return OPENAGC_OK;
}

static uint32_t openagc_vk_descriptor_mask(const openagc_vk_descriptor_set *set, uint32_t images)
{
    uint32_t mask = 0u;
    uint32_t slot;

    if (set == NULL) {
        return 0u;
    }
    for (slot = 0u; slot < OPENAGC_VK_DESCRIPTOR_SLOTS; ++slot) {
        if ((images == 0u && set->buffers[slot] != NULL) ||
            (images != 0u && set->sampled[slot] != NULL)) {
            mask |= 1u << slot;
        }
    }
    return mask;
}

static uint32_t openagc_vk_descriptor_present(const openagc_vk_descriptor_set *set, uint32_t images)
{
    return openagc_vk_descriptor_mask(set, images) != 0u ? 1u : 0u;
}

static openagc_result openagc_vk_layout_ok(const openagc_vk_descriptor_set *set,
                                           const openagc_frontend_pipeline *pipeline)
{
    return openagc_frontend_layout_matches(pipeline, set == NULL ? NULL : set->layout);
}

static void openagc_vk_descriptor_objects(const openagc_vk_descriptor_set *set,
                                          const openagc_frontend_buffer **buffers,
                                          uint64_t *offsets, uint64_t *sizes,
                                          const openagc_frontend_image **images)
{
    uint32_t slot;

    for (slot = 0u; slot < OPENAGC_VK_DESCRIPTOR_SLOTS; ++slot) {
        buffers[slot] = set != NULL && set->buffers[slot] != NULL ? set->buffers[slot]->buffer
                                                                  : NULL;
        offsets[slot] = set != NULL ? set->buffer_offsets[slot] : 0u;
        sizes[slot] = set != NULL ? set->buffer_sizes[slot] : 0u;
        images[slot] = set != NULL && set->sampled[slot] != NULL ? set->sampled[slot]->image->image
                                                                 : NULL;
    }
}

openagc_result openagc_vk_cmd_draw_indirect(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset)
{
    if (command_buffer == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    {
        const openagc_frontend_buffer *buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
        const openagc_frontend_image *images[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t offsets[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t sizes[OPENAGC_VK_DESCRIPTOR_SLOTS];

        openagc_vk_descriptor_objects(command_buffer->descriptors, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_layout_matches(
                command_buffer->pass->pass,
                command_buffer->descriptors == NULL ? NULL
                                                    : command_buffer->descriptors->layout) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (openagc_frontend_render_pass_matches(
                command_buffer->pass->pass, buffers, offsets, sizes, images,
                command_buffer->descriptors != NULL && command_buffer->descriptors->sampler != NULL
                    ? command_buffer->descriptors->sampler->sampler
                    : NULL) != OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_render_pass_draw_indirect(command_buffer->pass->pass, buffer->buffer,
                                                     offset);
}

openagc_result openagc_vk_cmd_draw(openagc_vk_command_buffer *command_buffer,
                                    uint32_t vertex_count, uint32_t instance_count,
                                    uint32_t first_vertex, uint32_t first_instance)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
        const openagc_frontend_image *images[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t offsets[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t sizes[OPENAGC_VK_DESCRIPTOR_SLOTS];

        openagc_vk_descriptor_objects(command_buffer->descriptors, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_layout_matches(
                command_buffer->pass->pass,
                command_buffer->descriptors == NULL ? NULL
                                                    : command_buffer->descriptors->layout) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (openagc_frontend_render_pass_matches(
                command_buffer->pass->pass, buffers, offsets, sizes, images,
                command_buffer->descriptors != NULL && command_buffer->descriptors->sampler != NULL
                    ? command_buffer->descriptors->sampler->sampler
                    : NULL) != OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_render_pass_draw(command_buffer->pass->pass, vertex_count,
                                            instance_count, first_vertex, first_instance);
}

openagc_result openagc_vk_cmd_draw_indexed(openagc_vk_command_buffer *command_buffer,
                                           uint32_t index_count, uint32_t instance_count,
                                           uint32_t first_index, int32_t vertex_offset,
                                           uint32_t first_instance)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
        const openagc_frontend_image *images[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t offsets[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t sizes[OPENAGC_VK_DESCRIPTOR_SLOTS];

        openagc_vk_descriptor_objects(command_buffer->descriptors, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_layout_matches(
                command_buffer->pass->pass,
                command_buffer->descriptors == NULL ? NULL
                                                    : command_buffer->descriptors->layout) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (openagc_frontend_render_pass_matches(
                command_buffer->pass->pass, buffers, offsets, sizes, images,
                command_buffer->descriptors != NULL && command_buffer->descriptors->sampler != NULL
                    ? command_buffer->descriptors->sampler->sampler
                    : NULL) != OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_render_pass_draw_indexed(command_buffer->pass->pass, index_count,
                                                     instance_count, first_index, vertex_offset,
                                                     first_instance);
}

openagc_result openagc_vk_cmd_bind_descriptor_set(openagc_vk_command_buffer *command_buffer,
                                               openagc_vk_descriptor_set *set)
{
    if (command_buffer == NULL || set == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (set->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (openagc_frontend_resources_present(openagc_vk_descriptor_present(set, 0u),
                                           openagc_vk_descriptor_present(set, 1u),
                                           set->sampler != NULL) != OPENAGC_OK) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->descriptors = set;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_bind_compute_pipeline(openagc_vk_command_buffer *command_buffer,
                                                     openagc_vk_pipeline *pipeline)
{
    if (command_buffer == NULL || pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pipeline->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    command_buffer->compute = pipeline;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_dispatch(openagc_vk_command_buffer *command_buffer,
                                       uint32_t groups_x, uint32_t groups_y,
                                       uint32_t groups_z)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->compute == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[OPENAGC_VK_DESCRIPTOR_SLOTS];
        const openagc_frontend_image *images[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t offsets[OPENAGC_VK_DESCRIPTOR_SLOTS];
        uint64_t sizes[OPENAGC_VK_DESCRIPTOR_SLOTS];

        openagc_vk_descriptor_objects(command_buffer->descriptors, buffers, offsets, sizes, images);
        if (openagc_vk_layout_ok(command_buffer->descriptors,
                                 command_buffer->compute->pipeline) != OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (openagc_frontend_recording_matches(
                command_buffer->compute->pipeline, buffers, offsets, sizes, images,
                command_buffer->descriptors != NULL && command_buffer->descriptors->sampler != NULL
                    ? command_buffer->descriptors->sampler->sampler
                    : NULL) != OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_dispatch(command_buffer->compute->pipeline, groups_x, groups_y,
                                    groups_z);
}

openagc_result openagc_vk_create_render_pass(openagc_vk_device *device,
                                             openagc_vk_image *color,
                                             openagc_vk_render_pass **out_pass)
{
    openagc_vk_render_pass *pass;
    openagc_result result;

    if (out_pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pass = NULL;
    if (device == NULL || color == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (color->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    pass = (openagc_vk_render_pass *)calloc(1u, sizeof(*pass));
    if (pass == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pass->device = device;
    result = openagc_frontend_render_pass_create(device->frontend, color->image, &pass->pass);
    if (result != OPENAGC_OK) {
        free(pass);
        return result;
    }
    device->child_count++;
    *out_pass = pass;
    return OPENAGC_OK;
}

openagc_result openagc_vk_destroy_render_pass(openagc_vk_render_pass *pass)
{
    openagc_result result;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_render_pass_destroy(pass->pass);
    if (result != OPENAGC_OK) {
        return result;
    }
    pass->device->child_count--;
    free(pass);
    return OPENAGC_OK;
}

openagc_result openagc_vk_render_pass_attach_depth(openagc_vk_render_pass *pass,
                                                   openagc_vk_image *depth)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (depth != NULL && depth->device != pass->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_render_pass_attach_depth(pass->pass,
                                                     depth == NULL ? NULL : depth->image);
}

openagc_result openagc_vk_cmd_begin_render_pass_with_load(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op load_op, openagc_color color)
{
    openagc_result result;

    if (command_buffer == NULL || pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_render_pass_begin_with_load(pass->pass, load_op, color);
    if (result != OPENAGC_OK) {
        return result;
    }
    command_buffer->pass = pass;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_begin_render_pass_with_depth(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op color_op, openagc_color color, openagc_frontend_load_op depth_op,
    float depth, uint32_t stencil)
{
    openagc_result result;

    if (command_buffer == NULL || pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_render_pass_begin_with_depth(pass->pass, color_op, color, depth_op,
                                                           depth, stencil);
    if (result != OPENAGC_OK) {
        return result;
    }
    command_buffer->pass = pass;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_begin_render_pass(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_render_pass *pass)
{
    openagc_result result;

    if (command_buffer == NULL || pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (command_buffer->pass != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_render_pass_begin_with_load(pass->pass, OPENAGC_FRONTEND_LOAD_OP_LOAD,
                                                         (openagc_color){ 0u, 0u, 0u, 0u });
    if (result != OPENAGC_OK) {
        return result;
    }
    command_buffer->pass = pass;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_clear_attachments(openagc_vk_command_buffer *command_buffer,
                                               openagc_color color)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_render_pass_clear(command_buffer->pass->pass, color);
}

openagc_result openagc_vk_cmd_clear_depth(openagc_vk_command_buffer *command_buffer, float depth,
                                         uint32_t stencil)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_render_pass_clear_depth(command_buffer->pass->pass, depth, stencil);
}

openagc_result openagc_vk_cmd_set_viewport(openagc_vk_command_buffer *command_buffer,
                                           uint32_t x, uint32_t y, uint32_t width,
                                           uint32_t height)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_render_pass_set_viewport(command_buffer->pass->pass, x, y, width,
                                                     height);
}

openagc_result openagc_vk_cmd_bind_pipeline(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline)
{
    openagc_result result;

    if (command_buffer == NULL || pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pipeline->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_render_pass_bind_pipeline(command_buffer->pass->pass,
                                                       pipeline->pipeline);
    if (result == OPENAGC_OK) {
        command_buffer->graphics = pipeline;
    }
    return result;
}

openagc_result openagc_vk_cmd_bind_index_buffer(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_buffer *buffer, uint64_t offset,
                                                uint32_t index_type)
{
    uint32_t width = 0u;
    openagc_result result;

    if (command_buffer == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_translate_index_width(OPENAGC_FRONTEND_VULKAN, index_type, &width);
    if (result != OPENAGC_OK) {
        return result;
    }
    if ((offset & ((uint64_t)width - 1u)) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_render_pass_bind_index(command_buffer->pass->pass, buffer->buffer,
                                                     offset);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_render_pass_set_index_width(command_buffer->pass->pass, width);
}

openagc_result openagc_vk_cmd_bind_vertex_buffer(openagc_vk_command_buffer *command_buffer,
                                                 openagc_vk_buffer *buffer, uint64_t offset)
{
    if (command_buffer == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_render_pass_bind_vertex(command_buffer->pass->pass, buffer->buffer,
                                                    offset);
}

openagc_result openagc_vk_cmd_set_scissor(openagc_vk_command_buffer *command_buffer,
                                          uint32_t x, uint32_t y, uint32_t width,
                                          uint32_t height)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_render_pass_set_scissor(command_buffer->pass->pass, x, y, width,
                                                    height);
}

openagc_result openagc_vk_create_query_pool(openagc_vk_device *device,
                                            openagc_frontend_query_kind kind, uint32_t count,
                                            openagc_vk_query_pool **out_pool)
{
    openagc_vk_query_pool *pool;
    openagc_result result;

    if (out_pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pool = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    pool = (openagc_vk_query_pool *)calloc(1u, sizeof(*pool));
    if (pool == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    result = openagc_frontend_query_pool_create(device->frontend, kind, count, &pool->pool);
    if (result != OPENAGC_OK) {
        free(pool);
        return result;
    }
    pool->device = device;
    device->child_count++;
    *out_pool = pool;
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_begin_query(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_query_pool *pool, uint32_t index)
{
    if (command_buffer == NULL || pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pool->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_render_pass_begin_query(command_buffer->pass->pass, pool->pool, index);
}

openagc_result openagc_vk_cmd_end_query(openagc_vk_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_render_pass_end_query(command_buffer->pass->pass);
}

openagc_result openagc_vk_cmd_write_timestamp(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_query_pool *pool, uint32_t index)
{
    if (command_buffer == NULL || pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->recording == 0u || command_buffer->pass != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pool->device != command_buffer->pool->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_query_write_timestamp(pool->pool, index);
}

openagc_result openagc_vk_get_query(const openagc_vk_query_pool *pool, uint32_t index,
                                   uint32_t *available)
{
    if (pool == NULL) {
        if (available != NULL) {
            *available = 0u;
        }
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_query_get(pool->pool, index, available);
}

openagc_result openagc_vk_destroy_query_pool(openagc_vk_query_pool *pool)
{
    openagc_result result;

    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_query_pool_destroy(pool->pool);
    if (result != OPENAGC_OK) {
        return result;
    }
    pool->device->child_count--;
    free(pool);
    return OPENAGC_OK;
}

openagc_result openagc_vk_cmd_end_render_pass(openagc_vk_command_buffer *command_buffer)
{
    openagc_result result;

    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_render_pass_end(command_buffer->pass->pass);
    if (result != OPENAGC_OK) {
        return result;
    }
    command_buffer->pass = NULL;
    return OPENAGC_OK;
}

openagc_result openagc_vk_create_swapchain(openagc_vk_device *device)
{
    return device == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                          : OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}
