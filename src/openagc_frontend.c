/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/frontend.h"
#include "openagc/psbc_metadata.h"
#include "openagc/shader.h"
#include "openagc/pm4_compute_fw940.h"
#include "openagc/pm4_write_fw940.h"
#include "openagc/store_span_code.h"

#include <stdlib.h>
#include <string.h>

#define OPENAGC_FRONTEND_MIN_STAGING_BYTES 4096u
#define OPENAGC_FRONTEND_PIXEL_BYTES 4u
#define OPENAGC_FRONTEND_COPY_COMMANDS 1u
#define OPENAGC_FRONTEND_COPY_WORDS 14u
#define OPENAGC_FRONTEND_TRANSITION_COMMANDS 4u
#define OPENAGC_FRONTEND_HEAP_BYTES 262144u
#define OPENAGC_FRONTEND_BLOCK_ALIGN 256u
#define OPENAGC_FRONTEND_MAX_BLOCKS 128u
#define OPENAGC_FRONTEND_REFLECTION_SLOTS 8u

typedef struct openagc_frontend_block {
    uint64_t offset;
    uint64_t size;
    uint32_t used;
} openagc_frontend_block;

struct openagc_frontend_device {
    openagc_gpu_device *device;
    openagc_gpu_memory *staging;
    openagc_gpu_buffer *staging_buffer;
    openagc_gpu_memory *heap;
    openagc_frontend_block blocks[OPENAGC_FRONTEND_MAX_BLOCKS];
    uint32_t block_count;
    openagc_gpu_command_buffer *copy;
    openagc_gpu_queue *queue;
    openagc_gpu_fence *fence;
    openagc_graphics_command_buffer *transitions;
    uint64_t staging_bytes;
    uint32_t image_count;
    uint32_t buffer_count;
    uint32_t memory_count;
    uint32_t timeline_count;
    uint32_t pipeline_count;
    uint32_t sampler_count;
    uint32_t render_pass_count;
    uint32_t query_count;
};

struct openagc_frontend_sampler {
    openagc_frontend_device *frontend;
    openagc_frontend_kind kind;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t address_mode;
    uint32_t pipeline_refs;
};

struct openagc_frontend_memory_span {
    uint64_t offset;
    uint64_t size;
};

struct openagc_frontend_memory {
    openagc_frontend_device *frontend;
    uint64_t size_bytes;
    uint64_t heap_offset;
    uint32_t bind_count;
    struct openagc_frontend_memory_span binds[8];
};

struct openagc_frontend_image {
    openagc_frontend_device *frontend;
    openagc_gpu_buffer *buffer;
    openagc_graphics_image *image;
    openagc_frontend_memory *memory;
    uint64_t footprint_bytes;
    uint64_t heap_offset;
    uint64_t bind_offset;
    uint32_t placed;
    openagc_frontend_kind kind;
    uint32_t native_format;
    uint32_t native_usage;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    uint32_t render_passes;
};

struct openagc_frontend_render_pass {
    openagc_frontend_device *frontend;
    openagc_frontend_image *color;
    openagc_frontend_image *depth;
    openagc_frontend_buffer *vertex;
    uint64_t vertex_offset;
    openagc_frontend_buffer *index;
    uint64_t index_offset;
    uint32_t index_width;
    openagc_frontend_pipeline *pipeline;
    uint32_t begun;
    uint32_t viewport_x;
    uint32_t viewport_y;
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint32_t scissor_x;
    uint32_t scissor_y;
    uint32_t scissor_width;
    uint32_t scissor_height;
    openagc_frontend_query_pool *query;
};

struct openagc_frontend_query_pool {
    openagc_frontend_device *frontend;
    openagc_frontend_query_kind kind;
    uint32_t count;
    uint32_t active;
    uint32_t active_index;
};

struct openagc_frontend_buffer {
    openagc_frontend_device *frontend;
    openagc_gpu_buffer *buffer;
    openagc_frontend_memory *memory;
    openagc_gpu_buffer_usage usage;
    uint64_t size_bytes;
    uint64_t heap_offset;
    uint64_t bind_offset;
    uint32_t placed;
    uint32_t vertex_binds;
    uint32_t index_binds;
    openagc_frontend_kind kind;
    uint32_t native_usage;
};

struct openagc_frontend_timeline {
    openagc_frontend_device *frontend;
    uint64_t value;
};

struct openagc_frontend_pipeline_layout {
    openagc_frontend_device *frontend;
    uint32_t resource_mask;
    uint32_t texture_mask;
    uint32_t refs;
    uint64_t resource_offset[OPENAGC_FRONTEND_REFLECTION_SLOTS];
    uint64_t resource_size[OPENAGC_FRONTEND_REFLECTION_SLOTS];
};

struct openagc_frontend_pipeline {
    openagc_frontend_device *frontend;
    openagc_shader_pipeline_plan *plan;
    openagc_shader_artifact *vertex;
    openagc_shader_artifact *pixel;
    openagc_shader_artifact *compute;
    uint32_t resource_mask;
    uint32_t texture_mask;
    uint32_t vertex_stride;
    uint32_t vertex_attribute_offset;
    uint32_t vertex_attribute_bytes;
    uint32_t vertex_attribute_count;
    uint32_t vertex_attribute_offsets[8];
    uint32_t vertex_attribute_sizes[8];
    openagc_frontend_vertex_format vertex_attribute_formats[8];
    uint32_t vertex_attribute_rates[8];
    uint32_t primitive;
    uint32_t blend_enable;
    uint32_t blend_src;
    uint32_t blend_dst;
    uint8_t push_constants[128];
    uint32_t push_constant_end;
    uint32_t vertex_input_mask;
    uint32_t pass_binds;
    openagc_frontend_sampler *sampler;
    openagc_frontend_pipeline_layout *layout;
    uint32_t *host_register_program;
    uint32_t host_register_program_dwords;
    uint8_t *psbc_vertex_metadata;
    uint32_t psbc_vertex_metadata_size;
    uint8_t *psbc_pixel_metadata;
    uint32_t psbc_pixel_metadata_size;
    uint64_t psbc_vertex_code_va;
    uint64_t psbc_pixel_code_va;
    uint32_t psbc_pgm_patched;
    uint64_t psbc_vertex_code_offset;
    uint64_t psbc_pixel_code_offset;
    uint64_t psbc_vertex_code_bytes;
    uint64_t psbc_pixel_code_bytes;
    uint32_t psbc_code_bound;
};

static void openagc_frontend_pipeline_release_psbc_code(openagc_frontend_pipeline *pipeline);

static openagc_result openagc_frontend_reset_transitions(
    openagc_frontend_device *frontend)
{
    openagc_result result =
        openagc_graphics_command_buffer_reset(frontend->transitions);

    /* A recording that never started is already reset. */
    return result == OPENAGC_ERROR_BAD_STATE ? OPENAGC_OK : result;
}

static openagc_result openagc_frontend_reset_copy(openagc_frontend_device *frontend)
{
    openagc_result result = openagc_gpu_command_buffer_reset(frontend->copy);

    return result == OPENAGC_ERROR_BAD_STATE ? OPENAGC_OK : result;
}

static void openagc_frontend_block_free(openagc_frontend_device *frontend,
                                        uint64_t offset)
{
    uint32_t index;

    for (index = 0u; index < frontend->block_count; ++index) {
        if (frontend->blocks[index].offset == offset && frontend->blocks[index].used != 0u) {
            break;
        }
    }
    if (index == frontend->block_count) {
        return;
    }
    frontend->blocks[index].used = 0u;
    if (index + 1u < frontend->block_count && frontend->blocks[index + 1u].used == 0u) {
        frontend->blocks[index].size += frontend->blocks[index + 1u].size;
        memmove(&frontend->blocks[index + 1u], &frontend->blocks[index + 2u],
                (frontend->block_count - index - 2u) * sizeof(frontend->blocks[0]));
        frontend->block_count--;
    }
    if (index > 0u && frontend->blocks[index - 1u].used == 0u) {
        frontend->blocks[index - 1u].size += frontend->blocks[index].size;
        memmove(&frontend->blocks[index], &frontend->blocks[index + 1u],
                (frontend->block_count - index - 1u) * sizeof(frontend->blocks[0]));
        frontend->block_count--;
    }
}

static openagc_result openagc_frontend_block_alloc(openagc_frontend_device *frontend,
                                                   uint64_t size, uint64_t *offset)
{
    uint64_t need;
    uint32_t i;

    if (size == 0u || size > UINT64_MAX - (OPENAGC_FRONTEND_BLOCK_ALIGN - 1u)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    need = (size + (OPENAGC_FRONTEND_BLOCK_ALIGN - 1u)) &
           ~(uint64_t)(OPENAGC_FRONTEND_BLOCK_ALIGN - 1u);
    for (i = 0u; i < frontend->block_count; ++i) {
        openagc_frontend_block *block = &frontend->blocks[i];
        if (block->used != 0u || block->size < need) {
            continue;
        }
        if (block->size > need) {
            uint64_t origin = block->offset;
            uint64_t room = block->size;
            if (frontend->block_count == OPENAGC_FRONTEND_MAX_BLOCKS) {
                return OPENAGC_ERROR_CAPACITY;
            }
            memmove(&frontend->blocks[i + 1u], &frontend->blocks[i],
                    (frontend->block_count - i) * sizeof(*block));
            frontend->block_count++;
            frontend->blocks[i].offset = origin;
            frontend->blocks[i].size = need;
            frontend->blocks[i].used = 1u;
            frontend->blocks[i + 1u].offset = origin + need;
            frontend->blocks[i + 1u].size = room - need;
            frontend->blocks[i + 1u].used = 0u;
        } else {
            block->used = 1u;
        }
        *offset = frontend->blocks[i].offset;
        return OPENAGC_OK;
    }
    return OPENAGC_ERROR_CAPACITY;
}

static void openagc_frontend_memory_release_span(openagc_frontend_memory *memory,
                                                 uint64_t offset, uint64_t size)
{
    uint32_t index;

    if (memory == NULL) {
        return;
    }
    for (index = 0u; index < memory->bind_count; ++index) {
        if (memory->binds[index].offset == offset && memory->binds[index].size == size) {
            break;
        }
    }
    if (index == memory->bind_count) {
        return;
    }
    memmove(&memory->binds[index], &memory->binds[index + 1u],
            (memory->bind_count - index - 1u) * sizeof(memory->binds[0]));
    memory->bind_count--;
}

static openagc_result openagc_frontend_memory_claim(openagc_frontend_memory *memory,
                                                    uint64_t offset, uint64_t size)
{
    uint32_t index;

    if ((offset & (OPENAGC_FRONTEND_BLOCK_ALIGN - 1u)) != 0u || size == 0u ||
        size > memory->size_bytes || offset > memory->size_bytes - size) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (memory->bind_count == 8u) {
        return OPENAGC_ERROR_CAPACITY;
    }
    for (index = 0u; index < memory->bind_count; ++index) {
        if (offset < memory->binds[index].offset + memory->binds[index].size &&
            memory->binds[index].offset < offset + size) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    memory->binds[memory->bind_count].offset = offset;
    memory->binds[memory->bind_count].size = size;
    memory->bind_count++;
    return OPENAGC_OK;
}

static void openagc_frontend_image_release(openagc_frontend_image *image)
{
    /* The shared recorders may still reference a partially created image. */
    (void)openagc_frontend_reset_copy(image->frontend);
    (void)openagc_frontend_reset_transitions(image->frontend);
    if (image->image != NULL) {
        (void)openagc_graphics_image_destroy(image->image);
    }
    if (image->buffer != NULL) {
        (void)openagc_gpu_buffer_destroy(image->buffer);
    }
    if (image->memory != NULL) {
        openagc_frontend_memory_release_span(image->memory, image->bind_offset,
                                             image->footprint_bytes);
    }
    if (image->placed != 0u) {
        openagc_frontend_block_free(image->frontend, image->heap_offset);
    }
    free(image);
}

static void openagc_frontend_buffer_release(openagc_frontend_buffer *buffer)
{
    (void)openagc_frontend_reset_copy(buffer->frontend);
    if (buffer->buffer != NULL) {
        (void)openagc_gpu_buffer_destroy(buffer->buffer);
    }
    if (buffer->memory != NULL) {
        openagc_frontend_memory_release_span(buffer->memory, buffer->bind_offset,
                                             buffer->size_bytes);
    }
    if (buffer->placed != 0u) {
        openagc_frontend_block_free(buffer->frontend, buffer->heap_offset);
    }
    free(buffer);
}

static void openagc_frontend_device_release(openagc_frontend_device *frontend)
{
    if (frontend->transitions != NULL) {
        (void)openagc_graphics_command_buffer_destroy(frontend->transitions);
    }
    if (frontend->fence != NULL) {
        (void)openagc_gpu_fence_destroy(frontend->fence);
    }
    if (frontend->queue != NULL) {
        (void)openagc_gpu_queue_destroy(frontend->queue);
    }
    if (frontend->copy != NULL) {
        (void)openagc_gpu_command_buffer_destroy(frontend->copy);
    }
    if (frontend->staging_buffer != NULL) {
        (void)openagc_gpu_buffer_destroy(frontend->staging_buffer);
    }
    if (frontend->staging != NULL) {
        (void)openagc_gpu_memory_destroy(frontend->staging);
    }
    if (frontend->heap != NULL) {
        (void)openagc_gpu_memory_destroy(frontend->heap);
    }
    free(frontend);
}

static openagc_result openagc_frontend_image_state(
    const openagc_frontend_image *image, openagc_graphics_image_state *state,
    openagc_graphics_owner *owner)
{
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_result result = openagc_graphics_image_get_info(image->image, &info);

    if (result != OPENAGC_OK) {
        return result;
    }
    *state = info.state;
    *owner = info.owner;
    return OPENAGC_OK;
}

static openagc_result openagc_frontend_image_record(openagc_frontend_image *image,
                                                    openagc_graphics_image_state state,
                                                    openagc_graphics_owner owner)
{
    openagc_frontend_device *frontend = image->frontend;
    openagc_graphics_image_state before_state;
    openagc_graphics_owner before_owner;
    openagc_graphics_transition_desc desc;
    openagc_result result;

    result = openagc_frontend_image_state(image, &before_state, &before_owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    desc = (openagc_graphics_transition_desc)OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        before_state, before_owner, state, owner);
    result = openagc_frontend_reset_transitions(frontend);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_buffer_begin(frontend->transitions);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_transition(frontend->transitions, image->image, &desc);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_buffer_end(frontend->transitions);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_buffer_apply_host_state(frontend->transitions);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_reset_transitions(frontend);
}

static openagc_result openagc_frontend_copy(openagc_frontend_device *frontend,
                                            openagc_gpu_buffer *source,
                                            uint64_t source_offset,
                                            openagc_gpu_buffer *destination,
                                            uint64_t destination_offset,
                                            uint64_t size_bytes)
{
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    openagc_result result;

    result = openagc_frontend_reset_copy(frontend);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_command_buffer_begin(frontend->copy);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_command_copy_buffer(frontend->copy, source, source_offset,
                                             destination, destination_offset,
                                             size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_command_buffer_end(frontend->copy);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_queue_submit(frontend->queue, frontend->copy, frontend->fence);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_fence_poll(frontend->fence, &fence_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_command_buffer_reset(frontend->copy);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_gpu_fence_reset(frontend->fence);
}

static int openagc_frontend_io_range(const openagc_frontend_device *frontend,
                                     uint64_t limit, uint64_t offset,
                                     uint64_t size_bytes)
{
    return size_bytes != 0u && (offset & 3u) == 0u && (size_bytes & 3u) == 0u &&
           offset <= limit && size_bytes <= limit - offset &&
           size_bytes <= frontend->staging_bytes;
}

openagc_result openagc_frontend_native_format_at(openagc_frontend_kind kind,
                                                 uint32_t index,
                                                 uint32_t *out_native_format)
{
    if (out_native_format == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_native_format = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (index == 0u) {
            *out_native_format = OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM;
            return OPENAGC_OK;
        }
        if (index == 1u) {
            *out_native_format = OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM;
            return OPENAGC_OK;
        }
        if (index == 2u) {
            *out_native_format = OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (index == 0u) {
            *out_native_format = OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8;
            return OPENAGC_OK;
        }
        if (index == 1u) {
            *out_native_format = OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return OPENAGC_ERROR_INVALID_ARGUMENT;
}

static uint32_t openagc_frontend_vertex_format_bytes(openagc_frontend_vertex_format format)
{
    if (format == OPENAGC_FRONTEND_VERTEX_R32_SFLOAT) {
        return 4u;
    }
    if (format == OPENAGC_FRONTEND_VERTEX_R32G32_SFLOAT) {
        return 8u;
    }
    if (format == OPENAGC_FRONTEND_VERTEX_R32G32B32_SFLOAT) {
        return 12u;
    }
    if (format == OPENAGC_FRONTEND_VERTEX_R32G32B32A32_SFLOAT) {
        return 16u;
    }
    return 0u;
}

openagc_result openagc_frontend_translate_vertex_format(
    openagc_frontend_kind kind, uint32_t native, uint32_t components,
    openagc_frontend_vertex_format *out_format, uint32_t *out_bytes)
{
    openagc_frontend_vertex_format format = 0u;

    if (out_format == NULL || out_bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_format = 0u;
    *out_bytes = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (components != 0u) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (native == OPENAGC_FRONTEND_VK_FORMAT_R32_SFLOAT) {
            format = OPENAGC_FRONTEND_VERTEX_R32_SFLOAT;
        } else if (native == OPENAGC_FRONTEND_VK_FORMAT_R32G32_SFLOAT) {
            format = OPENAGC_FRONTEND_VERTEX_R32G32_SFLOAT;
        } else if (native == OPENAGC_FRONTEND_VK_FORMAT_R32G32B32_SFLOAT) {
            format = OPENAGC_FRONTEND_VERTEX_R32G32B32_SFLOAT;
        } else if (native == OPENAGC_FRONTEND_VK_FORMAT_R32G32B32A32_SFLOAT) {
            format = OPENAGC_FRONTEND_VERTEX_R32G32B32A32_SFLOAT;
        } else {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native != OPENAGC_FRONTEND_GL_FLOAT) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        if (components < 1u || components > 4u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        format = components;
    } else {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_format = format;
    *out_bytes = openagc_frontend_vertex_format_bytes(format);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_translate_format(openagc_frontend_kind kind,
                                                 uint32_t native_format,
                                                 openagc_graphics_format *out_format)
{
    if (out_format == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_format = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native_format == OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM) {
            *out_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
            return OPENAGC_OK;
        }
        if (native_format == OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM) {
            *out_format = OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM;
            return OPENAGC_OK;
        }
        if (native_format == OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT) {
            *out_format = OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native_format == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8) {
            *out_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
            return OPENAGC_OK;
        }
        if (native_format == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8) {
            *out_format = OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return OPENAGC_ERROR_INVALID_ARGUMENT;
}

openagc_result openagc_frontend_translate_image_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_graphics_usage *out_usage)
{
    const uint32_t vulkan_supported = OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                      OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                      OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT |
                                      OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                      OPENAGC_FRONTEND_VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                      OPENAGC_FRONTEND_VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    openagc_graphics_usage usage = 0u;

    if (out_usage == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_usage = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native_usage == 0u || (native_usage & ~vulkan_supported) != 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0u) {
            usage |= OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u) {
            usage |= OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT;
        }
        if ((native_usage & (OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT |
                             OPENAGC_FRONTEND_VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) != 0u) {
            usage |= OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
        }
        if (usage == 0u) {
            /* Transfer-only images have no host backend usage to declare. */
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        *out_usage = usage;
        return OPENAGC_OK;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native_usage == OPENAGC_FRONTEND_GL_TEXTURE_2D) {
            *out_usage = OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT |
                         OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_RENDERBUFFER) {
            *out_usage = OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return OPENAGC_ERROR_INVALID_ARGUMENT;
}

openagc_result openagc_frontend_translate_image_layout(
    openagc_frontend_kind kind, uint32_t native_layout,
    openagc_graphics_image_state *out_state, openagc_graphics_owner *out_owner)
{
    if (out_state == NULL || out_owner == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    *out_owner = OPENAGC_GRAPHICS_OWNER_HOST;
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        /* OpenGL has no layout enumerant; zero requests no initial layout. */
        return native_layout == 0u ? OPENAGC_OK : OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (kind != OPENAGC_FRONTEND_VULKAN) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED) {
        return OPENAGC_OK;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        *out_state = OPENAGC_GRAPHICS_STATE_COLOR_TARGET;
        *out_owner = OPENAGC_GRAPHICS_OWNER_GRAPHICS;
        return OPENAGC_OK;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        *out_state = OPENAGC_GRAPHICS_STATE_DEPTH_TARGET;
        *out_owner = OPENAGC_GRAPHICS_OWNER_GRAPHICS;
        return OPENAGC_OK;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        *out_state = OPENAGC_GRAPHICS_STATE_SHADER_READ;
        *out_owner = OPENAGC_GRAPHICS_OWNER_GRAPHICS;
        return OPENAGC_OK;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        *out_state = OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE;
        *out_owner = OPENAGC_GRAPHICS_OWNER_COPY;
        return OPENAGC_OK;
    }
    if (native_layout == OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        *out_state = OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION;
        *out_owner = OPENAGC_GRAPHICS_OWNER_COPY;
        return OPENAGC_OK;
    }
    return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}

openagc_result openagc_frontend_translate_buffer_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_gpu_buffer_usage *out_usage)
{
    const uint32_t vulkan_supported = OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                      OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                      OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                      OPENAGC_FRONTEND_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                      OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                      OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    openagc_gpu_buffer_usage usage = 0u;

    if (out_usage == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_usage = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native_usage == 0u || (native_usage & ~vulkan_supported) != 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_SHADER_READ_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_VERTEX_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_INDEX_BIT;
        }
        if ((native_usage & OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0u) {
            usage |= OPENAGC_GPU_BUFFER_INDIRECT_BIT;
        }
        *out_usage = usage;
        return OPENAGC_OK;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native_usage == OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER) {
            /* GetTexImage writes it; the caller can read it back. */
            *out_usage = OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                         OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_PIXEL_UNPACK_BUFFER) {
            /* BufferData writes it; TexSubImage reads it. */
            *out_usage = OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                         OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_UNIFORM_BUFFER) {
            *out_usage = OPENAGC_GPU_BUFFER_SHADER_READ_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_ARRAY_BUFFER) {
            *out_usage = OPENAGC_GPU_BUFFER_VERTEX_BIT | OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_DRAW_INDIRECT_BUFFER) {
            *out_usage = OPENAGC_GPU_BUFFER_INDIRECT_BIT | OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
            return OPENAGC_OK;
        }
        if (native_usage == OPENAGC_FRONTEND_GL_ELEMENT_ARRAY_BUFFER) {
            *out_usage = OPENAGC_GPU_BUFFER_INDEX_BIT | OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return OPENAGC_ERROR_INVALID_ARGUMENT;
}

openagc_result openagc_frontend_device_create(openagc_gpu_device *device,
                                              const openagc_frontend_device_desc *desc,
                                              openagc_frontend_device **out_frontend)
{
    openagc_frontend_device *frontend;
    openagc_gpu_memory_desc memory_desc;
    openagc_gpu_buffer_desc buffer_desc;
    openagc_gpu_command_buffer_desc copy_desc;
    openagc_gpu_queue_desc queue_desc;
    openagc_graphics_command_buffer_desc transition_desc;
    openagc_result result;

    if (out_frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_frontend = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->staging_bytes < OPENAGC_FRONTEND_MIN_STAGING_BYTES ||
        desc->staging_bytes > OPENAGC_FRONTEND_MAX_STAGING_BYTES ||
        (desc->staging_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    frontend = (openagc_frontend_device *)calloc(1u, sizeof(*frontend));
    if (frontend == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    frontend->device = device;
    frontend->staging_bytes = desc->staging_bytes;
    memory_desc =
        (openagc_gpu_memory_desc)OPENAGC_GPU_MEMORY_DESC_INIT(desc->staging_bytes);
    result = openagc_gpu_memory_allocate(device, &memory_desc, &frontend->staging);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    buffer_desc = (openagc_gpu_buffer_desc)OPENAGC_GPU_BUFFER_DESC_INIT(
        desc->staging_bytes, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                                 OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    result = openagc_gpu_buffer_create(device, &buffer_desc, &frontend->staging_buffer);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    result = openagc_gpu_buffer_bind_memory(frontend->staging_buffer, frontend->staging, 0u);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    memory_desc =
        (openagc_gpu_memory_desc)OPENAGC_GPU_MEMORY_DESC_INIT(OPENAGC_FRONTEND_HEAP_BYTES);
    result = openagc_gpu_memory_allocate(device, &memory_desc, &frontend->heap);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    frontend->blocks[0].offset = 0u;
    frontend->blocks[0].size = OPENAGC_FRONTEND_HEAP_BYTES;
    frontend->blocks[0].used = 0u;
    frontend->block_count = 1u;
    copy_desc = (openagc_gpu_command_buffer_desc)OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_COPY_COMMANDS, OPENAGC_FRONTEND_COPY_WORDS);
    result = openagc_gpu_command_buffer_create(device, &copy_desc, &frontend->copy);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    queue_desc = (openagc_gpu_queue_desc)OPENAGC_GPU_QUEUE_DESC_INIT;
    result = openagc_gpu_queue_create(device, &queue_desc, &frontend->queue);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    result = openagc_gpu_fence_create(device, &frontend->fence);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    transition_desc =
        (openagc_graphics_command_buffer_desc)OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_TRANSITION_COMMANDS);
    result = openagc_graphics_command_buffer_create(device, &transition_desc,
                                                    &frontend->transitions);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    *out_frontend = frontend;
    return OPENAGC_OK;

fail:
    openagc_frontend_device_release(frontend);
    return result;
}

openagc_result openagc_frontend_device_get_capabilities(
    const openagc_frontend_device *frontend,
    openagc_frontend_capabilities *capabilities)
{
    openagc_graphics_capabilities graphics = OPENAGC_GRAPHICS_CAPABILITIES_INIT;
    openagc_result result;

    if (frontend == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_graphics_get_capabilities(frontend->device, &graphics);
    if (result != OPENAGC_OK) {
        return result;
    }
    capabilities->supported_kind_mask = (1u << (OPENAGC_FRONTEND_VULKAN - 1u)) |
                                        (1u << (OPENAGC_FRONTEND_OPENGL - 1u));
    capabilities->host_translation = 1u;
    capabilities->gpu_execution = 0u;
    capabilities->rasterization = 0u;
    capabilities->presentation = 0u;
    capabilities->supported_format_mask = graphics.supported_format_mask;
    capabilities->supported_usage_mask = graphics.supported_usage_mask;
    capabilities->max_width = graphics.max_width;
    capabilities->max_height = graphics.max_height;
    capabilities->max_image_bytes = graphics.max_image_bytes;
    capabilities->staging_bytes = (uint32_t)frontend->staging_bytes;
    capabilities->host_suballocation = 1u;
    capabilities->host_timeline = 1u;
    capabilities->host_pipeline_plans = 1u;
    capabilities->host_image_copy = 1u;
    capabilities->host_buffer_fill = 1u;
    capabilities->heap_bytes = OPENAGC_FRONTEND_HEAP_BYTES;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_device_get_last_write(
    const openagc_frontend_device *frontend, openagc_gpu_submission_view *view)
{
    if (frontend == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_gpu_device_get_last_write(frontend->device, view);
}

openagc_result openagc_frontend_device_destroy(openagc_frontend_device *frontend)
{
    if (frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (frontend->image_count != 0u || frontend->buffer_count != 0u ||
        frontend->memory_count != 0u || frontend->timeline_count != 0u ||
        frontend->pipeline_count != 0u || frontend->sampler_count != 0u ||
        frontend->render_pass_count != 0u || frontend->query_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    openagc_frontend_device_release(frontend);
    return OPENAGC_OK;
}

static openagc_result openagc_frontend_image_create_common(
    openagc_frontend_device *frontend, const openagc_frontend_image_desc *desc,
    int dedicated, openagc_frontend_image **out_image)
{
    openagc_frontend_image *image;
    openagc_graphics_image_desc image_desc;
    openagc_gpu_buffer_desc buffer_desc;
    uint64_t heap_offset = 0u;
    openagc_graphics_format format = 0u;
    openagc_graphics_usage usage = 0u;
    openagc_graphics_image_state state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    openagc_graphics_owner owner = OPENAGC_GRAPHICS_OWNER_HOST;
    uint64_t pitch;
    uint64_t footprint;
    openagc_result result;

    if (out_image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = NULL;
    if (frontend == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_frontend_translate_format(desc->kind, desc->native_format, &format);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_translate_image_usage(desc->kind, desc->native_usage, &usage);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (format == OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT &&
        desc->kind == OPENAGC_FRONTEND_OPENGL) {
        usage = OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT;
    }
    result = openagc_frontend_translate_image_layout(desc->kind, desc->native_layout,
                                                     &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    pitch = desc->row_pitch_bytes != 0u
                ? (uint64_t)desc->row_pitch_bytes
                : (uint64_t)desc->width * OPENAGC_FRONTEND_PIXEL_BYTES;
    if (pitch == 0u || pitch > UINT32_MAX ||
        (uint64_t)desc->height > UINT64_MAX / pitch) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    footprint = pitch * desc->height;
    if (footprint < 4u || (footprint & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    image = (openagc_frontend_image *)calloc(1u, sizeof(*image));
    if (image == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    image->frontend = frontend;
    image->kind = desc->kind;
    image->native_format = desc->native_format;
    image->native_usage = desc->native_usage;
    image->width = desc->width;
    image->height = desc->height;
    image->row_pitch_bytes = (uint32_t)pitch;
    image->footprint_bytes = footprint;
    image_desc = (openagc_graphics_image_desc)OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        desc->width, desc->height, (uint32_t)pitch, format);
    image_desc.usage = usage;
    result = openagc_graphics_image_create(frontend->device, &image_desc, &image->image);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    buffer_desc = (openagc_gpu_buffer_desc)OPENAGC_GPU_BUFFER_DESC_INIT(
        footprint, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                      OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    result = openagc_gpu_buffer_create(frontend->device, &buffer_desc, &image->buffer);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    if (dedicated != 0) {
        result = openagc_frontend_block_alloc(frontend, footprint, &heap_offset);
        if (result != OPENAGC_OK) {
            goto fail;
        }
        image->heap_offset = heap_offset;
        image->placed = 1u;
        result = openagc_gpu_buffer_bind_memory(image->buffer, frontend->heap, heap_offset);
        if (result != OPENAGC_OK) {
            goto fail;
        }
        result = openagc_graphics_image_bind_memory(image->image, frontend->heap, heap_offset);
        if (result != OPENAGC_OK) {
            goto fail;
        }
    }
    if (state != OPENAGC_GRAPHICS_STATE_UNDEFINED ||
        owner != OPENAGC_GRAPHICS_OWNER_HOST) {
        result = openagc_frontend_image_record(image, state, owner);
        if (result != OPENAGC_OK) {
            goto fail;
        }
    }
    frontend->image_count++;
    *out_image = image;
    return OPENAGC_OK;

fail:
    openagc_frontend_image_release(image);
    return result;
}

openagc_result openagc_frontend_image_create(openagc_frontend_device *frontend,
                                             const openagc_frontend_image_desc *desc,
                                             openagc_frontend_image **out_image)
{
    return openagc_frontend_image_create_common(frontend, desc, 1, out_image);
}

openagc_result openagc_frontend_image_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_image_desc *desc,
    openagc_frontend_image **out_image)
{
    return openagc_frontend_image_create_common(frontend, desc, 0, out_image);
}

openagc_result openagc_frontend_image_get_info(const openagc_frontend_image *image,
                                               openagc_frontend_image_info *info)
{
    openagc_graphics_image_info graphics_info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_result result;

    if (image == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_graphics_image_get_info(image->image, &graphics_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    info->kind = image->kind;
    info->native_format = image->native_format;
    info->native_usage = image->native_usage;
    info->format = graphics_info.format;
    info->usage = graphics_info.usage;
    info->state = graphics_info.state;
    info->owner = graphics_info.owner;
    info->width = image->width;
    info->height = image->height;
    info->row_pitch_bytes = image->row_pitch_bytes;
    info->footprint_bytes = image->footprint_bytes;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_image_transition(openagc_frontend_image *image,
                                                 openagc_graphics_image_state state,
                                                 openagc_graphics_owner owner)
{
    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_record(image, state, owner);
}

openagc_result openagc_frontend_image_upload(openagc_frontend_image *image,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes)
{
    openagc_frontend_device *frontend;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (image == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->placed == 0u && image->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_frontend_io_range(image->frontend, image->footprint_bytes, offset,
                                   size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    frontend = image->frontend;
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_memory_write(frontend->staging, 0u, bytes, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(image,
                                               OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
                                               OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    result = openagc_frontend_copy(frontend, frontend->staging_buffer, 0u, image->buffer,
                                   offset, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        return openagc_frontend_image_record(image, state, owner);
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_image_readback(openagc_frontend_image *image,
                                               uint64_t offset, void *bytes,
                                               uint64_t size_bytes)
{
    openagc_frontend_device *frontend;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (image == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->placed == 0u && image->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_frontend_io_range(image->frontend, image->footprint_bytes, offset,
                                   size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    frontend = image->frontend;
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(image,
                                               OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE,
                                               OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    result = openagc_frontend_copy(frontend, image->buffer, offset, frontend->staging_buffer,
                                   0u, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_memory_read(frontend->staging, 0u, bytes, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        return openagc_frontend_image_record(image, state, owner);
    }
    return OPENAGC_OK;
}

static openagc_result openagc_frontend_image_clear_rect(openagc_frontend_image *image,
                                                              openagc_color color, uint32_t x,
                                                              uint32_t y, uint32_t width,
                                                              uint32_t height)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_scissor scissor;
    openagc_graphics_execution_info info = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_result result;

    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->placed == 0u && image->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_reset_transitions(image->frontend);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_buffer_begin(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_bind_color_target(image->frontend->transitions,
                                                        image->image);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    scissor.x = x;
    scissor.y = y;
    scissor.width = width;
    scissor.height = height;
    result = openagc_graphics_command_set_scissor(image->frontend->transitions, &scissor);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_clear_color(image->frontend->transitions, color);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_end(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_apply_host_state(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_execute_host(image->frontend->transitions, &info);
    (void)openagc_frontend_reset_transitions(image->frontend);
    return result;
}

openagc_result openagc_frontend_image_clear(openagc_frontend_image *image, openagc_color color)
{
    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_clear_rect(image, color, 0u, 0u, image->width, image->height);
}

openagc_result openagc_frontend_render_pass_clear(openagc_frontend_render_pass *pass,
                                                  openagc_color color)
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->scissor_width == 0u) {
        x = 0u;
        y = 0u;
        width = pass->color->width;
        height = pass->color->height;
    } else {
        x = pass->scissor_x;
        y = pass->scissor_y;
        width = pass->scissor_width;
        height = pass->scissor_height;
    }
    return openagc_frontend_image_clear_rect(pass->color, color, x, y, width, height);
}

static int openagc_frontend_pack_depth(float depth, uint32_t stencil, uint32_t *depth24);
static openagc_result openagc_frontend_image_clear_depth_rect(openagc_frontend_image *image,
                                                             uint32_t depth24, uint32_t stencil,
                                                             uint32_t x, uint32_t y,
                                                             uint32_t width, uint32_t height);

openagc_result openagc_frontend_render_pass_clear_depth(openagc_frontend_render_pass *pass,
                                                       float depth, uint32_t stencil)
{
    uint32_t depth24 = 0u;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun == 0u || pass->depth == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_frontend_pack_depth(depth, stencil, &depth24)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pass->scissor_width == 0u) {
        x = 0u;
        y = 0u;
        width = pass->depth->width;
        height = pass->depth->height;
    } else {
        x = pass->scissor_x;
        y = pass->scissor_y;
        width = pass->scissor_width;
        height = pass->scissor_height;
    }
    return openagc_frontend_image_clear_depth_rect(pass->depth, depth24, stencil, x, y, width,
                                                   height);
}

openagc_result openagc_frontend_render_pass_clear_bounds(
    const openagc_frontend_render_pass *pass, uint32_t require_depth, uint32_t *out_x,
    uint32_t *out_y, uint32_t *out_width, uint32_t *out_height)
{
    if (pass == NULL || out_x == NULL || out_y == NULL || out_width == NULL || out_height == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun == 0u || pass->color == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (require_depth != 0u && pass->depth == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->scissor_width == 0u) {
        *out_x = 0u;
        *out_y = 0u;
        *out_width = pass->color->width;
        *out_height = pass->color->height;
    } else {
        *out_x = pass->scissor_x;
        *out_y = pass->scissor_y;
        *out_width = pass->scissor_width;
        *out_height = pass->scissor_height;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_image_destroy(openagc_frontend_image *image)
{
    openagc_result result;

    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->render_passes != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (image->image != NULL) {
        result = openagc_graphics_image_destroy(image->image);
        if (result != OPENAGC_OK) {
            return result;
        }
        image->image = NULL;
    }
    if (image->buffer != NULL) {
        result = openagc_gpu_buffer_destroy(image->buffer);
        if (result != OPENAGC_OK) {
            return result;
        }
        image->buffer = NULL;
    }
    image->frontend->image_count--;
    openagc_frontend_image_release(image);
    return OPENAGC_OK;
}

static openagc_result openagc_frontend_buffer_create_common(
    openagc_frontend_device *frontend, const openagc_frontend_buffer_desc *desc,
    int dedicated, openagc_frontend_buffer **out_buffer)
{
    openagc_frontend_buffer *buffer;
    openagc_gpu_buffer_desc buffer_desc;
    openagc_gpu_buffer_usage usage = 0u;
    uint64_t heap_offset = 0u;
    openagc_result result;

    if (out_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;
    if (frontend == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_frontend_translate_buffer_usage(desc->kind, desc->native_usage,
                                                    &usage);
    if (result != OPENAGC_OK) {
        return result;
    }

    buffer = (openagc_frontend_buffer *)calloc(1u, sizeof(*buffer));
    if (buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    buffer->frontend = frontend;
    buffer->kind = desc->kind;
    buffer->native_usage = desc->native_usage;
    buffer->usage = usage;
    buffer->size_bytes = desc->size_bytes;
    buffer_desc =
        (openagc_gpu_buffer_desc)OPENAGC_GPU_BUFFER_DESC_INIT(desc->size_bytes, usage);
    result = openagc_gpu_buffer_create(frontend->device, &buffer_desc, &buffer->buffer);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    if (dedicated != 0) {
        result = openagc_frontend_block_alloc(frontend, desc->size_bytes, &heap_offset);
        if (result != OPENAGC_OK) {
            goto fail;
        }
        buffer->heap_offset = heap_offset;
        buffer->placed = 1u;
        result = openagc_gpu_buffer_bind_memory(buffer->buffer, frontend->heap, heap_offset);
        if (result != OPENAGC_OK) {
            goto fail;
        }
    }
    frontend->buffer_count++;
    *out_buffer = buffer;
    return OPENAGC_OK;

fail:
    openagc_frontend_buffer_release(buffer);
    return result;
}

openagc_result openagc_frontend_buffer_create(openagc_frontend_device *frontend,
                                              const openagc_frontend_buffer_desc *desc,
                                              openagc_frontend_buffer **out_buffer)
{
    return openagc_frontend_buffer_create_common(frontend, desc, 1, out_buffer);
}

openagc_result openagc_frontend_buffer_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_buffer_desc *desc,
    openagc_frontend_buffer **out_buffer)
{
    return openagc_frontend_buffer_create_common(frontend, desc, 0, out_buffer);
}

openagc_result openagc_frontend_buffer_get_info(const openagc_frontend_buffer *buffer,
                                                openagc_frontend_buffer_info *info)
{
    if (buffer == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->kind = buffer->kind;
    info->native_usage = buffer->native_usage;
    info->usage = buffer->usage;
    info->size_bytes = buffer->size_bytes;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_buffer_upload(openagc_frontend_buffer *buffer,
                                              uint64_t offset, const void *bytes,
                                              uint64_t size_bytes)
{
    openagc_frontend_device *frontend;
    openagc_result result;

    if (buffer == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->placed == 0u && buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    frontend = buffer->frontend;
    if (!openagc_frontend_io_range(frontend, buffer->size_bytes, offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    /* Shader-read uniforms have no copy bit; host BufferData writes memory directly. */
    if ((buffer->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        if ((buffer->usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) == 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        return openagc_gpu_buffer_write(buffer->buffer, offset, bytes, size_bytes);
    }
    result = openagc_gpu_memory_write(frontend->staging, 0u, bytes, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_copy(frontend, frontend->staging_buffer, 0u, buffer->buffer,
                                 offset, size_bytes);
}

openagc_result openagc_frontend_buffer_readback(openagc_frontend_buffer *buffer,
                                                uint64_t offset, void *bytes,
                                                uint64_t size_bytes)
{
    openagc_frontend_device *frontend;
    openagc_result result;

    if (buffer == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->placed == 0u && buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    frontend = buffer->frontend;
    if (!openagc_frontend_io_range(frontend, buffer->size_bytes, offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    /* Shader-read uniforms have no copy bit; host GetBufferSubData reads memory directly. */
    if ((buffer->usage & OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT) == 0u) {
        if ((buffer->usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) == 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        return openagc_gpu_buffer_read(buffer->buffer, offset, bytes, size_bytes);
    }
    result = openagc_frontend_copy(frontend, buffer->buffer, offset,
                                   frontend->staging_buffer, 0u, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_gpu_memory_read(frontend->staging, 0u, bytes, size_bytes);
}

openagc_result openagc_frontend_buffer_copy(openagc_frontend_buffer *source,
                                            uint64_t source_offset,
                                            openagc_frontend_buffer *destination,
                                            uint64_t destination_offset,
                                            uint64_t size_bytes)
{
    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((source->placed == 0u && source->memory == NULL) ||
        (destination->placed == 0u && destination->memory == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (source->frontend != destination->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((source->usage & OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT) == 0u ||
        (destination->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (!openagc_frontend_io_range(source->frontend, source->size_bytes, source_offset,
                                   size_bytes) ||
        !openagc_frontend_io_range(destination->frontend, destination->size_bytes,
                                   destination_offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return openagc_frontend_copy(source->frontend, source->buffer, source_offset,
                                 destination->buffer, destination_offset, size_bytes);
}

openagc_result openagc_frontend_buffer_copy_then_fill(openagc_frontend_buffer *source,
                                                      uint64_t source_offset,
                                                      openagc_frontend_buffer *destination,
                                                      uint64_t destination_offset,
                                                      uint64_t size_bytes, uint32_t value,
                                                      uint64_t fill_bytes)
{
    openagc_result result;

    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((source->placed == 0u && source->memory == NULL) ||
        (destination->placed == 0u && destination->memory == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (source->frontend != destination->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((source->usage & OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT) == 0u ||
        (destination->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (!openagc_frontend_io_range(source->frontend, source->size_bytes, source_offset,
                                   size_bytes) ||
        !openagc_frontend_io_range(destination->frontend, destination->size_bytes,
                                   destination_offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if ((fill_bytes & 3u) != 0u || fill_bytes == 0u || fill_bytes > size_bytes ||
        fill_bytes > OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES || (size_bytes & 3u) != 0u ||
        (source_offset & 3u) != 0u || (destination_offset & 3u) != 0u ||
        size_bytes > 0x1ffffcu) {
        result = openagc_frontend_buffer_copy(source, source_offset, destination,
                                              destination_offset, size_bytes);
        if (result != OPENAGC_OK) {
            return result;
        }
        return openagc_frontend_buffer_fill(destination, destination_offset, fill_bytes, value);
    }
    return openagc_gpu_host_dma_write_data(
        source->frontend->device, source->buffer, source_offset, destination->buffer,
        destination_offset, (uint32_t)size_bytes, value, (uint32_t)(fill_bytes / 4u));
}

openagc_result openagc_frontend_buffer_fill(openagc_frontend_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes, uint32_t value)
{
    openagc_frontend_device *frontend;
    uint8_t pattern[256];
    uint64_t patterned = 0u;
    uint32_t index;
    openagc_result result;

    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->placed == 0u && buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((buffer->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    frontend = buffer->frontend;
    if (!openagc_frontend_io_range(frontend, buffer->size_bytes, offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    /* Aligned fills: Steps D/E (≤64 B one packet); Steps F/G (N×64 B as
     * full-width WRITE_DATA rows + one EOP); remainder ≤64 B as one more
     * packet. Larger / unaligned → staging DMA. */
    if ((offset & 3u) == 0u && (size_bytes & 3u) == 0u && size_bytes != 0u &&
        size_bytes <= OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES * OPENAGC_PM4_WRITE_DATA_MAX_ROWS) {
        uint64_t filled = 0u;

        if (size_bytes >= OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES) {
            uint32_t rows =
                (uint32_t)(size_bytes / OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES);
            openagc_result rows_result = openagc_gpu_host_write_data_buffer_rows(
                frontend->device, buffer->buffer, offset, OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES,
                value, OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS, rows);

            if (rows_result != OPENAGC_OK) {
                return rows_result;
            }
            filled = (uint64_t)rows * OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES;
        }
        if (filled < size_bytes) {
            return openagc_gpu_host_write_data(frontend->device, buffer->buffer, offset + filled,
                                               value, (uint32_t)((size_bytes - filled) / 4u));
        }
        return OPENAGC_OK;
    }
    for (index = 0u; index < (uint32_t)sizeof(pattern); index += 4u) {
        memcpy(pattern + index, &value, 4u);
    }
    while (patterned < size_bytes) {
        uint64_t chunk = size_bytes - patterned;

        if (chunk > sizeof(pattern)) {
            chunk = sizeof(pattern);
        }
        result = openagc_gpu_memory_write(frontend->staging, patterned, pattern, chunk);
        if (result != OPENAGC_OK) {
            return result;
        }
        patterned += chunk;
    }
    return openagc_frontend_copy(frontend, frontend->staging_buffer, 0u, buffer->buffer, offset,
                                 size_bytes);
}

openagc_result openagc_frontend_copy_buffer_to_image(openagc_frontend_buffer *source,
                                                     uint64_t source_offset,
                                                     openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     uint64_t size_bytes)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (source == NULL || image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((source->placed == 0u && source->memory == NULL) ||
        (image->placed == 0u && image->memory == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (source->frontend != image->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((source->usage & OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (!openagc_frontend_io_range(source->frontend, source->size_bytes, source_offset,
                                   size_bytes) ||
        !openagc_frontend_io_range(image->frontend, image->footprint_bytes, image_offset,
                                   size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(image, OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
                                               OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    result = openagc_frontend_copy(source->frontend, source->buffer, source_offset, image->buffer,
                                   image_offset, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        return openagc_frontend_image_record(image, state, owner);
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_copy_image_to_buffer(openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     openagc_frontend_buffer *destination,
                                                     uint64_t destination_offset,
                                                     uint64_t size_bytes)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (image == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((image->placed == 0u && image->memory == NULL) ||
        (destination->placed == 0u && destination->memory == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->frontend != destination->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((destination->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (!openagc_frontend_io_range(image->frontend, image->footprint_bytes, image_offset,
                                   size_bytes) ||
        !openagc_frontend_io_range(destination->frontend, destination->size_bytes,
                                   destination_offset, size_bytes)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(image, OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE,
                                               OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    result = openagc_frontend_copy(image->frontend, image->buffer, image_offset,
                                   destination->buffer, destination_offset, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        return openagc_frontend_image_record(image, state, owner);
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_image_copy_rect(openagc_frontend_image *source, uint32_t source_x,
                                                uint32_t source_y,
                                                openagc_frontend_image *destination,
                                                uint32_t destination_x, uint32_t destination_y,
                                                uint32_t width, uint32_t height)
{
    openagc_frontend_device *frontend;
    openagc_graphics_image_state source_state;
    openagc_graphics_image_state destination_state;
    openagc_graphics_owner source_owner;
    openagc_graphics_owner destination_owner;
    uint64_t row_bytes;
    uint64_t chunk_bytes;
    uint32_t row;
    openagc_result result;

    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((source->placed == 0u && source->memory == NULL) ||
        (destination->placed == 0u && destination->memory == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (source->frontend != destination->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    /* A one-image copy would need both copy-owned states at once. */
    if (source == destination || source->native_format != destination->native_format) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (width == 0u || height == 0u || source_x > source->width ||
        width > source->width - source_x || source_y > source->height ||
        height > source->height - source_y || destination_x > destination->width ||
        width > destination->width - destination_x || destination_y > destination->height ||
        height > destination->height - destination_y) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    frontend = source->frontend;
    row_bytes = (uint64_t)width * OPENAGC_FRONTEND_PIXEL_BYTES;
    chunk_bytes = frontend->staging_bytes;
    if (chunk_bytes > row_bytes) {
        chunk_bytes = row_bytes;
    }
    result = openagc_frontend_image_state(source, &source_state, &source_owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_image_state(destination, &destination_state, &destination_owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (source_state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        source_owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(source, OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE,
                                               OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    if (destination_state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        destination_owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(destination,
                                              OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
                                              OPENAGC_GRAPHICS_OWNER_COPY);
        if (result != OPENAGC_OK) {
            return openagc_frontend_image_record(source, source_state, source_owner);
        }
    }
    for (row = 0u; row < height; ++row) {
        uint64_t source_row = (uint64_t)(source_y + row) * source->row_pitch_bytes +
                              (uint64_t)source_x * OPENAGC_FRONTEND_PIXEL_BYTES;
        uint64_t destination_row =
            (uint64_t)(destination_y + row) * destination->row_pitch_bytes +
            (uint64_t)destination_x * OPENAGC_FRONTEND_PIXEL_BYTES;
        uint64_t copied = 0u;

        while (copied < row_bytes) {
            uint64_t chunk = row_bytes - copied;

            if (chunk > chunk_bytes) {
                chunk = chunk_bytes;
            }
            result = openagc_frontend_copy(frontend, source->buffer, source_row + copied,
                                           destination->buffer, destination_row + copied, chunk);
            if (result != OPENAGC_OK) {
                return result;
            }
            copied += chunk;
        }
    }
    if (destination_state != OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION ||
        destination_owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        result = openagc_frontend_image_record(destination, destination_state, destination_owner);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    if (source_state != OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE ||
        source_owner != OPENAGC_GRAPHICS_OWNER_COPY) {
        return openagc_frontend_image_record(source, source_state, source_owner);
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_memory_allocate(openagc_frontend_device *frontend,
                                                uint64_t size_bytes,
                                                openagc_frontend_memory **out_memory)
{
    openagc_frontend_memory *memory;
    uint64_t heap_offset = 0u;
    openagc_result result;

    if (out_memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (size_bytes < 4u || (size_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    memory = (openagc_frontend_memory *)calloc(1u, sizeof(*memory));
    if (memory == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    result = openagc_frontend_block_alloc(frontend, size_bytes, &heap_offset);
    if (result != OPENAGC_OK) {
        free(memory);
        return result;
    }
    memory->frontend = frontend;
    memory->size_bytes = size_bytes;
    memory->heap_offset = heap_offset;
    frontend->memory_count++;
    *out_memory = memory;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_memory_write(openagc_frontend_memory *memory,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes)
{
    if (memory == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (size_bytes == 0u || size_bytes > memory->size_bytes ||
        offset > memory->size_bytes - size_bytes) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return openagc_gpu_memory_write(memory->frontend->heap,
                                    memory->heap_offset + offset, bytes, size_bytes);
}

openagc_result openagc_frontend_memory_read(const openagc_frontend_memory *memory,
                                            uint64_t offset, void *bytes,
                                            uint64_t size_bytes)
{
    if (memory == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (size_bytes == 0u || size_bytes > memory->size_bytes ||
        offset > memory->size_bytes - size_bytes) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return openagc_gpu_memory_read(memory->frontend->heap,
                                   memory->heap_offset + offset, bytes, size_bytes);
}

openagc_result openagc_frontend_memory_destroy(openagc_frontend_memory *memory)
{
    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (memory->bind_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    openagc_frontend_block_free(memory->frontend, memory->heap_offset);
    memory->frontend->memory_count--;
    free(memory);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_buffer_bind_memory(openagc_frontend_buffer *buffer,
                                                   openagc_frontend_memory *memory,
                                                   uint64_t offset)
{
    openagc_result result;

    if (buffer == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->frontend != memory->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (buffer->placed != 0u || buffer->memory != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_memory_claim(memory, offset, buffer->size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_buffer_bind_memory(buffer->buffer, buffer->frontend->heap,
                                            memory->heap_offset + offset);
    if (result != OPENAGC_OK) {
        openagc_frontend_memory_release_span(memory, offset, buffer->size_bytes);
        return result;
    }
    buffer->memory = memory;
    buffer->bind_offset = offset;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_image_bind_memory(openagc_frontend_image *image,
                                                  openagc_frontend_memory *memory,
                                                  uint64_t offset)
{
    openagc_result result;

    if (image == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->frontend != memory->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (image->placed != 0u || image->memory != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_memory_claim(memory, offset, image->footprint_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_buffer_bind_memory(image->buffer, image->frontend->heap,
                                            memory->heap_offset + offset);
    if (result != OPENAGC_OK) {
        openagc_frontend_memory_release_span(memory, offset, image->footprint_bytes);
        return result;
    }
    result = openagc_graphics_image_bind_memory(image->image, image->frontend->heap,
                                                memory->heap_offset + offset);
    if (result != OPENAGC_OK) {
        (void)openagc_gpu_buffer_unbind_memory(image->buffer);
        openagc_frontend_memory_release_span(memory, offset, image->footprint_bytes);
        return result;
    }
    image->memory = memory;
    image->bind_offset = offset;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_buffer_destroy(openagc_frontend_buffer *buffer)
{
    openagc_result result;

    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->vertex_binds != 0u || buffer->index_binds != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (buffer->buffer != NULL) {
        result = openagc_gpu_buffer_destroy(buffer->buffer);
        if (result != OPENAGC_OK) {
            return result;
        }
        buffer->buffer = NULL;
    }
    buffer->frontend->buffer_count--;
    openagc_frontend_buffer_release(buffer);
    return OPENAGC_OK;
}

static int openagc_frontend_sampler_nearest(openagc_frontend_kind kind, uint32_t filter)
{
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        return filter == OPENAGC_FRONTEND_VK_FILTER_NEAREST;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        return filter == OPENAGC_FRONTEND_GL_NEAREST || filter == 0x2700u;
    }
    return 0;
}

openagc_result openagc_frontend_sampler_create(openagc_frontend_device *frontend,
                                               const openagc_frontend_sampler_desc *desc,
                                               openagc_frontend_sampler **out_sampler)
{
    openagc_frontend_sampler *sampler;
    uint32_t clamp;

    if (out_sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = NULL;
    if (frontend == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_FRONTEND_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->kind != OPENAGC_FRONTEND_VULKAN && desc->kind != OPENAGC_FRONTEND_OPENGL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    clamp = desc->kind == OPENAGC_FRONTEND_VULKAN
                ? OPENAGC_FRONTEND_VK_SAMPLER_ADDRESS_CLAMP_TO_EDGE
                : OPENAGC_FRONTEND_GL_CLAMP_TO_EDGE;
    if (!openagc_frontend_sampler_nearest(desc->kind, desc->mag_filter) ||
        !openagc_frontend_sampler_nearest(desc->kind, desc->min_filter) ||
        desc->address_mode != clamp) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    sampler = (openagc_frontend_sampler *)calloc(1u, sizeof(*sampler));
    if (sampler == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    sampler->frontend = frontend;
    sampler->kind = desc->kind;
    sampler->mag_filter = desc->mag_filter;
    sampler->min_filter = desc->min_filter;
    sampler->address_mode = desc->address_mode;
    frontend->sampler_count++;
    *out_sampler = sampler;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_sampler_get_info(const openagc_frontend_sampler *sampler,
                                                 openagc_frontend_sampler_info *info)
{
    if (sampler == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->kind = sampler->kind;
    info->mag_filter = sampler->mag_filter;
    info->min_filter = sampler->min_filter;
    info->address_mode = sampler->address_mode;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_sampler_destroy(openagc_frontend_sampler *sampler)
{
    if (sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler->pipeline_refs != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    sampler->frontend->sampler_count--;
    free(sampler);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_create(
    openagc_frontend_device *frontend, openagc_frontend_image *color,
    openagc_frontend_render_pass **out_pass)
{
    openagc_frontend_render_pass *pass;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (out_pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pass = NULL;
    if (frontend == NULL || color == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (color->frontend != frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_image_state(color, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    pass = (openagc_frontend_render_pass *)calloc(1u, sizeof(*pass));
    if (pass == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pass->frontend = frontend;
    pass->color = color;
    color->render_passes++;
    frontend->render_pass_count++;
    *out_pass = pass;
    return OPENAGC_OK;
}

static int openagc_frontend_pack_depth(float depth, uint32_t stencil, uint32_t *depth24)
{
    float scaled;

    if (depth != depth || depth < 0.f || depth > 1.f || stencil > 255u) {
        return 0;
    }
    scaled = depth * 16777215.f + 0.5f;
    if (scaled > 16777215.f) {
        scaled = 16777215.f;
    }
    *depth24 = (uint32_t)scaled;
    return 1;
}

static openagc_result openagc_frontend_image_clear_depth_rect(openagc_frontend_image *image,
                                                             uint32_t depth24, uint32_t stencil,
                                                             uint32_t x, uint32_t y,
                                                             uint32_t width, uint32_t height)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_scissor scissor;
    openagc_graphics_execution_info info = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_result result;

    if (image->placed == 0u && image->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_image_state(image, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_frontend_reset_transitions(image->frontend);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_buffer_begin(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_command_bind_depth_target(image->frontend->transitions, image->image);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    scissor.x = x;
    scissor.y = y;
    scissor.width = width;
    scissor.height = height;
    result = openagc_graphics_command_set_scissor(image->frontend->transitions, &scissor);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_clear_depth(image->frontend->transitions, depth24, stencil);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_end(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_apply_host_state(image->frontend->transitions);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_reset_transitions(image->frontend);
        return result;
    }
    result = openagc_graphics_command_buffer_execute_host(image->frontend->transitions, &info);
    (void)openagc_frontend_reset_transitions(image->frontend);
    return result;
}

openagc_result openagc_frontend_render_pass_begin_validate_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    uint32_t depth24 = 0u;
    openagc_result result;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((color_op != OPENAGC_FRONTEND_LOAD_OP_LOAD &&
         color_op != OPENAGC_FRONTEND_LOAD_OP_CLEAR) ||
        (depth_op != OPENAGC_FRONTEND_LOAD_OP_LOAD &&
         depth_op != OPENAGC_FRONTEND_LOAD_OP_CLEAR)) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (pass->begun != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (depth_op == OPENAGC_FRONTEND_LOAD_OP_CLEAR &&
        !openagc_frontend_pack_depth(depth, stencil, &depth24)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_image_state(pass->color, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->depth == NULL) {
        if (depth_op == OPENAGC_FRONTEND_LOAD_OP_CLEAR) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    } else {
        result = openagc_frontend_image_state(pass->depth, &state, &owner);
        if (result != OPENAGC_OK) {
            return result;
        }
        if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
            owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    pass->begun = 1u;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_apply_loads(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    uint32_t depth24 = 0u;
    openagc_result result;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((color_op != OPENAGC_FRONTEND_LOAD_OP_LOAD &&
         color_op != OPENAGC_FRONTEND_LOAD_OP_CLEAR) ||
        (depth_op != OPENAGC_FRONTEND_LOAD_OP_LOAD &&
         depth_op != OPENAGC_FRONTEND_LOAD_OP_CLEAR)) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (depth_op == OPENAGC_FRONTEND_LOAD_OP_CLEAR) {
        if (pass->depth == NULL) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (!openagc_frontend_pack_depth(depth, stencil, &depth24)) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        result = openagc_frontend_image_clear_depth_rect(pass->depth, depth24, stencil, 0u, 0u,
                                                         pass->depth->width, pass->depth->height);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    if (color_op == OPENAGC_FRONTEND_LOAD_OP_CLEAR) {
        result = openagc_frontend_image_clear(pass->color, color);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_clear_rect(openagc_frontend_render_pass *pass,
                                                       openagc_color color, uint32_t x, uint32_t y,
                                                       uint32_t width, uint32_t height)
{
    if (pass == NULL || pass->color == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_clear_rect(pass->color, color, x, y, width, height);
}

openagc_result openagc_frontend_render_pass_clear_depth_rect(
    openagc_frontend_render_pass *pass, float depth, uint32_t stencil, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    uint32_t depth24 = 0u;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->depth == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_frontend_pack_depth(depth, stencil, &depth24)) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return openagc_frontend_image_clear_depth_rect(pass->depth, depth24, stencil, x, y, width,
                                                   height);
}

openagc_result openagc_frontend_render_pass_begin_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    openagc_result result;

    result = openagc_frontend_render_pass_begin_validate_with_depth(pass, color_op, depth_op, depth,
                                                                    stencil);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_render_pass_apply_loads(pass, color_op, color, depth_op, depth,
                                                      stencil);
    if (result != OPENAGC_OK) {
        pass->begun = 0u;
        return result;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_begin_with_load(openagc_frontend_render_pass *pass,
                                                           openagc_frontend_load_op load_op,
                                                           openagc_color color)
{
    return openagc_frontend_render_pass_begin_with_depth(pass, load_op, color,
                                                         OPENAGC_FRONTEND_LOAD_OP_LOAD, 0.f, 0u);
}

openagc_result openagc_frontend_render_pass_begin(openagc_frontend_render_pass *pass)
{
    openagc_color color = { 0u, 0u, 0u, 0u };

    return openagc_frontend_render_pass_begin_with_load(pass, OPENAGC_FRONTEND_LOAD_OP_LOAD, color);
}

static openagc_result openagc_frontend_render_pass_rect(openagc_frontend_render_pass *pass,
                                                       uint32_t x, uint32_t y,
                                                       uint32_t width, uint32_t height)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (width == 0u || height == 0u || x > pass->color->width ||
        y > pass->color->height || width > pass->color->width - x ||
        height > pass->color->height - y) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_set_viewport(openagc_frontend_render_pass *pass,
                                                         uint32_t x, uint32_t y,
                                                         uint32_t width, uint32_t height)
{
    openagc_result result = openagc_frontend_render_pass_rect(pass, x, y, width, height);

    if (result != OPENAGC_OK) {
        return result;
    }
    pass->viewport_x = x;
    pass->viewport_y = y;
    pass->viewport_width = width;
    pass->viewport_height = height;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_bind_pipeline(
    openagc_frontend_render_pass *pass, openagc_frontend_pipeline *pipeline)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_result result;

    if (pass == NULL || pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->frontend != pipeline->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_pipeline_get_info(pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.kind != OPENAGC_SHADER_PIPELINE_GRAPHICS || info.gpu_executable != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (pass->pipeline == pipeline) {
        return OPENAGC_OK;
    }
    if (pass->pipeline != NULL) {
        pass->pipeline->pass_binds--;
    }
    pass->pipeline = pipeline;
    pipeline->pass_binds++;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_bind_index(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_buffer *buffer,
                                                        uint64_t offset)
{
    if (pass == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->frontend != buffer->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((buffer->usage & OPENAGC_GPU_BUFFER_INDEX_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (offset > buffer->size_bytes || (offset & 1u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pass->index == buffer && pass->index_offset == offset) {
        return OPENAGC_OK;
    }
    if (pass->index != NULL) {
        pass->index->index_binds--;
    }
    pass->index = buffer;
    pass->index_offset = offset;
    pass->index_width = 0u;
    buffer->index_binds++;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_translate_index_width(openagc_frontend_kind kind, uint32_t native,
                                                      uint32_t *out_width)
{
    if (out_width == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_width = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native == OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT16) {
            *out_width = 2u;
            return OPENAGC_OK;
        }
        if (native == OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32) {
            *out_width = 4u;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native == OPENAGC_FRONTEND_GL_UNSIGNED_SHORT) {
            *out_width = 2u;
            return OPENAGC_OK;
        }
        if (native == OPENAGC_FRONTEND_GL_UNSIGNED_INT) {
            *out_width = 4u;
            return OPENAGC_OK;
        }
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return OPENAGC_ERROR_INVALID_ARGUMENT;
}

openagc_result openagc_frontend_render_pass_set_index_width(openagc_frontend_render_pass *pass,
                                                            uint32_t width)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (width != 2u && width != 4u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (pass->index == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((pass->index_offset & ((uint64_t)width - 1u)) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    pass->index_width = width;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_bind_vertex(openagc_frontend_render_pass *pass,
                                                         openagc_frontend_buffer *buffer,
                                                         uint64_t offset)
{
    if (pass == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->frontend != buffer->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((buffer->usage & OPENAGC_GPU_BUFFER_VERTEX_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (offset > buffer->size_bytes || (offset & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pass->vertex == buffer && pass->vertex_offset == offset) {
        return OPENAGC_OK;
    }
    if (pass->vertex != NULL) {
        pass->vertex->vertex_binds--;
    }
    pass->vertex = buffer;
    pass->vertex_offset = offset;
    buffer->vertex_binds++;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_bind_cb_capture(
    openagc_frontend_render_pass *pass, const openagc_cb_capture_manifest *manifest,
    const uint32_t *words)
{
    if (pass == NULL || manifest == NULL || words == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun == 0u || pass->color == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_gpu_host_cb_bind_from_capture(pass->frontend->device, manifest, words);
}

openagc_result openagc_frontend_device_get_cb_capture_info(
    const openagc_frontend_device *frontend, openagc_cb_capture_info *info)
{
    if (frontend == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_gpu_device_get_cb_capture_info(frontend->device, info);
}

openagc_result openagc_frontend_render_pass_set_scissor(openagc_frontend_render_pass *pass,
                                                        uint32_t x, uint32_t y,
                                                        uint32_t width, uint32_t height)
{
    openagc_result result = openagc_frontend_render_pass_rect(pass, x, y, width, height);

    if (result != OPENAGC_OK) {
        return result;
    }
    pass->scissor_x = x;
    pass->scissor_y = y;
    pass->scissor_width = width;
    pass->scissor_height = height;
    return OPENAGC_OK;
}

static openagc_result openagc_frontend_draw_launch(const openagc_frontend_render_pass *pass)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_shader_artifact_info vertex_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_artifact_info pixel_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_result result = openagc_shader_artifact_require_compiler(pass->pipeline->vertex);

    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_require_compiler(pass->pipeline->pixel);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_pipeline_get_info(pass->pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_get_info(pass->pipeline->vertex, &vertex_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_get_info(pass->pipeline->pixel, &pixel_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    /*
     * A PSBC envelope pair must carry the host SET_* snapshot before the
     * compiler gate. Still never claims gpu_executable or emits DRAW.
     */
    if (vertex_info.psbc_envelope != 0u && pixel_info.psbc_envelope != 0u &&
        info.host_register_program_dwords == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->pipeline->push_constant_end != 0u) {
        uint8_t scratch[128];

        memcpy(scratch, pass->pipeline->push_constants, pass->pipeline->push_constant_end);
        (void)scratch;
    }
    if (info.gpu_executable == 0u || info.compiler_verified == 0u) {
        return OPENAGC_ERROR_NOT_READY;
    }
    return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}

static openagc_result openagc_frontend_fetch_vertex(const openagc_frontend_render_pass *pass,
                                                     uint64_t vertex_index, uint64_t instance_index)
{
    uint32_t attribute;

    for (attribute = 0u; attribute < pass->pipeline->vertex_attribute_count; ++attribute) {
        uint64_t element = pass->pipeline->vertex_attribute_rates[attribute] == 0u
                               ? vertex_index
                               : instance_index;
        uint64_t base = pass->vertex_offset + element * pass->pipeline->vertex_stride;
        uint32_t done;

        for (done = 0u; done < pass->pipeline->vertex_attribute_sizes[attribute]; done += 4u) {
            uint32_t word = 0u;
            openagc_result result = openagc_gpu_buffer_read(
                pass->vertex->buffer,
                base + pass->pipeline->vertex_attribute_offsets[attribute] + done, &word,
                sizeof(word));

            if (result != OPENAGC_OK) {
                return result;
            }
        }
    }
    return OPENAGC_OK;
}

static int openagc_frontend_draw_ready(const openagc_frontend_render_pass *pass)
{
    return pass->begun != 0u && pass->pipeline != NULL && pass->viewport_width != 0u &&
           pass->pipeline->vertex != NULL && pass->pipeline->pixel != NULL;
}

openagc_result openagc_frontend_render_pass_draw(const openagc_frontend_render_pass *pass,
                                                 uint32_t vertex_count, uint32_t instance_count,
                                                 uint32_t first_vertex, uint32_t first_instance)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (!openagc_frontend_draw_ready(pass)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (vertex_count == 0u || instance_count == 0u) {
        return OPENAGC_OK;
    }
    if (pass->pipeline->primitive == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    /*
     * Attribute-less shaders (vertex_input_mask == 0) need no VBO fetch —
     * typical PSBC smoke / SV_VertexID paths. Otherwise require bound
     * vertex buffer + stride + attributes that cover the input mask.
     */
    if (pass->pipeline->vertex_input_mask != 0u) {
        if (pass->vertex == NULL || pass->pipeline->vertex_stride == 0u ||
            pass->pipeline->vertex_attribute_count == 0u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        {
            uint64_t bytes =
                ((uint64_t)first_vertex + (uint64_t)vertex_count) * pass->pipeline->vertex_stride;

            if (pass->vertex_offset > pass->vertex->size_bytes ||
                bytes > pass->vertex->size_bytes - pass->vertex_offset) {
                return OPENAGC_ERROR_OUT_OF_RANGE;
            }
        }
        {
            uint32_t vertex;
            uint32_t instance;

            for (instance = 0u; instance < instance_count; ++instance) {
                for (vertex = 0u; vertex < vertex_count; ++vertex) {
                    openagc_result result = openagc_frontend_fetch_vertex(
                        pass, (uint64_t)first_vertex + (uint64_t)vertex,
                        (uint64_t)first_instance + (uint64_t)instance);

                    if (result != OPENAGC_OK) {
                        return result;
                    }
                }
            }
        }
    }
    return openagc_frontend_draw_launch(pass);
}

openagc_result openagc_frontend_render_pass_draw_indirect(
    const openagc_frontend_render_pass *pass, openagc_frontend_buffer *buffer, uint64_t offset)
{
    uint32_t command[4];
    openagc_result result;

    if (pass == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->frontend != buffer->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((buffer->usage & OPENAGC_GPU_BUFFER_INDIRECT_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if ((offset & 3u) != 0u || offset > buffer->size_bytes ||
        16u > buffer->size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_gpu_buffer_read(buffer->buffer, offset, command, sizeof(command));
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_render_pass_draw(pass, command[0], command[1], command[2], command[3]);
}

static openagc_result openagc_frontend_indexed_vertices_fit(
    const openagc_frontend_render_pass *pass, uint32_t index_count, uint32_t first_index,
    int32_t base_vertex, uint32_t first_instance, uint32_t instance_count)
{
    uint32_t index;

    for (index = 0u; index < index_count; ++index) {
        uint32_t element = 0u;
        int64_t vertex;
        uint64_t end;
        uint32_t width = pass->index_width;
        openagc_result result;

        if (width == 2u) {
            uint16_t half = 0u;

            result = openagc_gpu_buffer_read(
                pass->index->buffer,
                pass->index_offset + ((uint64_t)first_index + (uint64_t)index) * 2u, &half,
                sizeof(half));
            element = half;
        } else {
            result = openagc_gpu_buffer_read(
                pass->index->buffer,
                pass->index_offset + ((uint64_t)first_index + (uint64_t)index) * 4u, &element,
                sizeof(element));
        }

        if (result != OPENAGC_OK) {
            return result;
        }
        vertex = (int64_t)element + (int64_t)base_vertex;
        if (vertex < 0) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        end = ((uint64_t)vertex + 1u) * pass->pipeline->vertex_stride;
        if (pass->vertex_offset > pass->vertex->size_bytes ||
            end > pass->vertex->size_bytes - pass->vertex_offset) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        {
            uint32_t instance;

            for (instance = 0u; instance < instance_count; ++instance) {
                result = openagc_frontend_fetch_vertex(pass, (uint64_t)vertex,
                                                       (uint64_t)first_instance + (uint64_t)instance);
                if (result != OPENAGC_OK) {
                    return result;
                }
            }
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_draw_indexed(
    const openagc_frontend_render_pass *pass, uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
    openagc_result result;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (!openagc_frontend_draw_ready(pass) || pass->index == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (index_count == 0u || instance_count == 0u) {
        return OPENAGC_OK;
    }
    if (pass->index_width != 2u && pass->index_width != 4u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        uint64_t bytes = ((uint64_t)first_index + (uint64_t)index_count) * pass->index_width;

        if (pass->index_offset > pass->index->size_bytes ||
            bytes > pass->index->size_bytes - pass->index_offset) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    if (pass->pipeline->primitive == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pass->pipeline->vertex_input_mask != 0u) {
        if (pass->vertex == NULL || pass->pipeline->vertex_stride == 0u ||
            pass->pipeline->vertex_attribute_count == 0u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        result = openagc_frontend_indexed_vertices_fit(pass, index_count, first_index,
                                                      vertex_offset, first_instance,
                                                      instance_count);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    return openagc_frontend_draw_launch(pass);
}

openagc_result openagc_frontend_render_pass_end(openagc_frontend_render_pass *pass)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun == 0u || pass->query != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    pass->begun = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_query_pool_create(openagc_frontend_device *frontend,
                                                  openagc_frontend_query_kind kind, uint32_t count,
                                                  openagc_frontend_query_pool **out_pool)
{
    openagc_frontend_query_pool *pool;

    if (out_pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pool = NULL;
    if (frontend == NULL || count == 0u || count > 8u) {
        return count == 0u || count > 8u ? OPENAGC_ERROR_OUT_OF_RANGE
                                        : OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (kind != OPENAGC_FRONTEND_QUERY_OCCLUSION && kind != OPENAGC_FRONTEND_QUERY_TIMESTAMP) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    pool = (openagc_frontend_query_pool *)calloc(1u, sizeof(*pool));
    if (pool == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pool->frontend = frontend;
    pool->kind = kind;
    pool->count = count;
    frontend->query_count++;
    *out_pool = pool;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_begin_query(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_query_pool *pool,
                                                        uint32_t index)
{
    if (pass == NULL || pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->frontend != pool->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (pool->kind != OPENAGC_FRONTEND_QUERY_OCCLUSION) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (index >= pool->count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pass->begun == 0u || pass->query != NULL || pool->active != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    pass->query = pool;
    pool->active = 1u;
    pool->active_index = index;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_end_query(openagc_frontend_render_pass *pass)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->query == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    pass->query->active = 0u;
    pass->query = NULL;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_query_write_timestamp(openagc_frontend_query_pool *pool,
                                                     uint32_t index)
{
    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pool->kind != OPENAGC_FRONTEND_QUERY_TIMESTAMP) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (index >= pool->count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pool->active != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_query_get(const openagc_frontend_query_pool *pool, uint32_t index,
                                          uint32_t *available)
{
    if (available == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *available = 0u;
    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= pool->count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (pool->active != 0u && pool->active_index == index) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return OPENAGC_ERROR_NOT_READY;
}

openagc_result openagc_frontend_query_pool_destroy(openagc_frontend_query_pool *pool)
{
    if (pool == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pool->active != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    pool->frontend->query_count--;
    free(pool);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_destroy(openagc_frontend_render_pass *pass)
{
    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun != 0u || pass->query != NULL) {
        return OPENAGC_ERROR_BUSY;
    }
    if (pass->vertex != NULL) {
        pass->vertex->vertex_binds--;
        pass->vertex = NULL;
    }
    if (pass->index != NULL) {
        pass->index->index_binds--;
        pass->index = NULL;
    }
    if (pass->pipeline != NULL) {
        pass->pipeline->pass_binds--;
        pass->pipeline = NULL;
    }
    pass->color->render_passes--;
    if (pass->depth != NULL) {
        pass->depth->render_passes--;
        pass->depth = NULL;
    }
    pass->frontend->render_pass_count--;
    free(pass);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_attach_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_image *depth)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_result result;

    if (pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pass->begun != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (depth == NULL) {
        if (pass->depth != NULL) {
            pass->depth->render_passes--;
            pass->depth = NULL;
        }
        return OPENAGC_OK;
    }
    if (depth->frontend != pass->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (pass->depth == depth) {
        return OPENAGC_OK;
    }
    if (pass->depth != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (depth->width != pass->color->width || depth->height != pass->color->height) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_image_state(depth, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    depth->render_passes++;
    pass->depth = depth;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_timeline_create(
    openagc_frontend_device *frontend, openagc_frontend_timeline **out_timeline)
{
    openagc_frontend_timeline *timeline;

    if (out_timeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_timeline = NULL;
    if (frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    timeline = (openagc_frontend_timeline *)calloc(1u, sizeof(*timeline));
    if (timeline == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    timeline->frontend = frontend;
    frontend->timeline_count++;
    *out_timeline = timeline;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_timeline_signal(openagc_frontend_timeline *timeline)
{
    if (timeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (timeline->value == UINT64_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    timeline->value++;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_timeline_poll(const openagc_frontend_timeline *timeline,
                                              uint64_t value,
                                              openagc_frontend_timeline_info *info)
{
    if (timeline == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (value == 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    info->value = timeline->value;
    info->signaled = timeline->value >= value ? 1u : 0u;
    return info->signaled != 0u ? OPENAGC_OK : OPENAGC_ERROR_NOT_READY;
}

openagc_result openagc_frontend_timeline_destroy(openagc_frontend_timeline *timeline)
{
    if (timeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    timeline->frontend->timeline_count--;
    free(timeline);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_apply_reflection_set(
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_shader_resource_binding *resources,
    openagc_shader_texture_binding *textures, openagc_shader_pipeline_desc *plan)
{
    uint32_t index;

    if (plan == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (resource_count > OPENAGC_FRONTEND_REFLECTION_SLOTS ||
        texture_count > OPENAGC_FRONTEND_REFLECTION_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if ((resource_count != 0u &&
         (uniforms == NULL || offsets == NULL || sizes == NULL || resource_bindings == NULL ||
          resources == NULL)) ||
        (texture_count != 0u &&
         (sampled == NULL || texture_bindings == NULL || textures == NULL))) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    plan->resources = NULL;
    plan->resource_count = 0u;
    plan->textures = NULL;
    plan->texture_count = 0u;
    for (index = 0u; index < resource_count; ++index) {
        const openagc_frontend_buffer *uniform = uniforms[index];
        uint32_t previous;

        if (uniform == NULL || sizes[index] == 0u) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (resource_bindings[index] >= OPENAGC_FRONTEND_REFLECTION_SLOTS ||
            (index != 0u && resource_bindings[index] <= resource_bindings[index - 1u])) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        if (uniform->placed == 0u || uniform->buffer == NULL) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if ((uniform->usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) == 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
        if (offsets[index] > uniform->size_bytes ||
            sizes[index] > uniform->size_bytes - offsets[index]) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        for (previous = 0u; previous < index; ++previous) {
            if (uniforms[previous]->buffer == uniform->buffer) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
        }
        resources[index].set = 0u;
        resources[index].binding = resource_bindings[index];
        resources[index].buffer = uniform->buffer;
        resources[index].offset = offsets[index];
        resources[index].size_bytes = sizes[index];
    }
    for (index = 0u; index < texture_count; ++index) {
        const openagc_frontend_image *image = sampled[index];

        if (image == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (texture_bindings[index] >= OPENAGC_FRONTEND_REFLECTION_SLOTS ||
            (index != 0u && texture_bindings[index] <= texture_bindings[index - 1u])) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        if (image->placed == 0u || image->image == NULL) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        textures[index].set = 0u;
        textures[index].binding = texture_bindings[index];
        textures[index].image = image->image;
    }
    if (resource_count != 0u) {
        plan->resources = resources;
        plan->resource_count = resource_count;
    }
    if (texture_count != 0u) {
        plan->textures = textures;
        plan->texture_count = texture_count;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_apply_reflection(
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_shader_resource_binding *resource, openagc_shader_texture_binding *texture,
    openagc_shader_pipeline_desc *plan)
{
    const openagc_frontend_buffer *uniforms[1];
    const openagc_frontend_image *images[1];
    uint64_t offsets[1];
    uint64_t sizes[1];
    uint32_t resource_bindings[1];
    uint32_t texture_bindings[1];

    if (plan == NULL || resource == NULL || texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((uniform == NULL && (offset != 0u || size_bytes != 0u || resource_binding != 0u)) ||
        (sampled == NULL && texture_binding != 0u)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    uniforms[0] = uniform;
    images[0] = sampled;
    offsets[0] = offset;
    sizes[0] = size_bytes;
    resource_bindings[0] = resource_binding;
    texture_bindings[0] = texture_binding;
    return openagc_frontend_apply_reflection_set(
        uniform == NULL ? NULL : uniforms, offsets, sizes, resource_bindings,
        uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images, texture_bindings,
        sampled == NULL ? 0u : 1u, resource, texture, plan);
}

openagc_result openagc_frontend_graphics_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_pipeline **out_pipeline)
{
    return openagc_frontend_graphics_pipeline_create_with_bindings(
        frontend, vertex, pixel, color_target, NULL, 0u, 0u, 0u, NULL, 0u, out_pipeline);
}

openagc_result openagc_frontend_graphics_pipeline_create_with_bindings(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_frontend_pipeline **out_pipeline)
{
    const openagc_frontend_buffer *uniforms[1];
    const openagc_frontend_image *images[1];
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
    return openagc_frontend_graphics_pipeline_create_with_resources(
        frontend, vertex, pixel, color_target, uniform == NULL ? NULL : uniforms, offsets, sizes,
        resource_bindings, uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images,
        texture_bindings, sampled == NULL ? 0u : 1u, out_pipeline);
}

openagc_result openagc_frontend_graphics_pipeline_create_with_resources(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_frontend_pipeline **out_pipeline)
{
    openagc_shader_pipeline_desc plan = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_resource_binding resources[OPENAGC_FRONTEND_REFLECTION_SLOTS];
    openagc_shader_texture_binding textures[OPENAGC_FRONTEND_REFLECTION_SLOTS];
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_shader_artifact *vertex_artifact = NULL;
    openagc_shader_artifact *pixel_artifact = NULL;
    uint32_t index;
    openagc_result result;

    if (out_pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = NULL;
    if (frontend == NULL || vertex == NULL || pixel == NULL || color_target == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (resource_count > OPENAGC_FRONTEND_REFLECTION_SLOTS ||
        texture_count > OPENAGC_FRONTEND_REFLECTION_SLOTS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (vertex->stage != OPENAGC_SHADER_STAGE_VERTEX ||
        pixel->stage != OPENAGC_SHADER_STAGE_PIXEL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (color_target->frontend != frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    for (index = 0u; index < resource_count; ++index) {
        if (uniforms == NULL || uniforms[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (uniforms[index]->frontend != frontend) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
    }
    for (index = 0u; index < texture_count; ++index) {
        if (sampled == NULL || sampled[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (sampled[index]->frontend != frontend) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
    }
    result = openagc_frontend_image_state(color_target, &state, &owner);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_shader_artifact_intake_host(frontend->device, vertex, &vertex_artifact);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_intake_host(frontend->device, pixel, &pixel_artifact);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(vertex_artifact);
        return result;
    }
    plan.kind = OPENAGC_SHADER_PIPELINE_GRAPHICS;
    plan.vertex = vertex_artifact;
    plan.pixel = pixel_artifact;
    plan.color_target = color_target->image;
    memset(resources, 0, sizeof(resources));
    memset(textures, 0, sizeof(textures));
    result = openagc_frontend_apply_reflection_set(
        resource_count == 0u ? NULL : uniforms, offsets, sizes, resource_bindings, resource_count,
        texture_count == 0u ? NULL : sampled, texture_bindings, texture_count, resources, textures,
        &plan);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(pixel_artifact);
        (void)openagc_shader_artifact_destroy(vertex_artifact);
        return result;
    }
    result = openagc_frontend_pipeline_create(frontend, &plan, out_pipeline);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(pixel_artifact);
        (void)openagc_shader_artifact_destroy(vertex_artifact);
        return result;
    }
    (*out_pipeline)->vertex = vertex_artifact;
    (*out_pipeline)->pixel = pixel_artifact;
    (*out_pipeline)->primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_LIST;
    (*out_pipeline)->blend_src = OPENAGC_FRONTEND_BLEND_ONE;
    (*out_pipeline)->blend_dst = OPENAGC_FRONTEND_BLEND_ZERO;
    /*
     * When both stages are pin-checked PSBC envelopes, attach the host
     * SET_CONTEXT/SET_SH snapshot from retained metadata. Still leaves
     * compiler_verified and gpu_executable at zero; never emits DRAW.
     */
    {
        openagc_shader_artifact_info vertex_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
        openagc_shader_artifact_info pixel_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
        const uint8_t *vertex_metadata = NULL;
        const uint8_t *pixel_metadata = NULL;
        uint32_t vertex_metadata_size = 0u;
        uint32_t pixel_metadata_size = 0u;

        result = openagc_shader_artifact_get_info(vertex_artifact, &vertex_info);
        if (result != OPENAGC_OK) {
            (void)openagc_frontend_pipeline_destroy(*out_pipeline);
            *out_pipeline = NULL;
            return result;
        }
        result = openagc_shader_artifact_get_info(pixel_artifact, &pixel_info);
        if (result != OPENAGC_OK) {
            (void)openagc_frontend_pipeline_destroy(*out_pipeline);
            *out_pipeline = NULL;
            return result;
        }
        if (vertex_info.psbc_envelope != 0u && pixel_info.psbc_envelope != 0u) {
            result = openagc_shader_artifact_get_compiler_metadata(
                vertex_artifact, &vertex_metadata, &vertex_metadata_size);
            if (result != OPENAGC_OK) {
                (void)openagc_frontend_pipeline_destroy(*out_pipeline);
                *out_pipeline = NULL;
                return result;
            }
            result = openagc_shader_artifact_get_compiler_metadata(
                pixel_artifact, &pixel_metadata, &pixel_metadata_size);
            if (result != OPENAGC_OK) {
                (void)openagc_frontend_pipeline_destroy(*out_pipeline);
                *out_pipeline = NULL;
                return result;
            }
            result = openagc_frontend_pipeline_set_psbc_register_snapshot(
                *out_pipeline, vertex_metadata, vertex_metadata_size, pixel_metadata,
                pixel_metadata_size);
            if (result != OPENAGC_OK) {
                (void)openagc_frontend_pipeline_destroy(*out_pipeline);
                *out_pipeline = NULL;
                return result;
            }
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_pipeline_desc *desc,
    openagc_frontend_pipeline **out_pipeline)
{
    openagc_frontend_pipeline *pipeline;
    openagc_shader_pipeline_info plan_info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_result result;

    if (out_pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_pipeline = NULL;
    if (frontend == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    pipeline = (openagc_frontend_pipeline *)calloc(1u, sizeof(*pipeline));
    if (pipeline == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pipeline->frontend = frontend;
    result = openagc_shader_pipeline_plan_create_host(frontend->device, desc,
                                                     &pipeline->plan);
    if (result != OPENAGC_OK) {
        free(pipeline);
        return result;
    }
    result = openagc_shader_pipeline_plan_get_info(pipeline->plan, &plan_info);
    if (result != OPENAGC_OK || plan_info.compiler_verified != 0u ||
        plan_info.gpu_executable != 0u) {
        (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
        free(pipeline);
        return result != OPENAGC_OK ? result : OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    result = openagc_shader_pipeline_plan_vertex_input(pipeline->plan,
                                                      &pipeline->vertex_input_mask);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
        free(pipeline);
        return result;
    }
    frontend->pipeline_count++;
    pipeline->compute = desc->compute;
    if ((desc->resource_count != 0u && desc->resources == NULL) ||
        (desc->texture_count != 0u && desc->textures == NULL) ||
        desc->resource_count > OPENAGC_FRONTEND_REFLECTION_SLOTS ||
        desc->texture_count > OPENAGC_FRONTEND_REFLECTION_SLOTS) {
        (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
        free(pipeline);
        frontend->pipeline_count--;
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    {
        uint32_t index;

        for (index = 0u; index < desc->resource_count; ++index) {
            uint32_t binding = desc->resources[index].binding;

            if (binding >= OPENAGC_FRONTEND_REFLECTION_SLOTS ||
                (pipeline->resource_mask & (1u << binding)) != 0u) {
                (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
                free(pipeline);
                frontend->pipeline_count--;
                return OPENAGC_ERROR_OUT_OF_RANGE;
            }
            pipeline->resource_mask |= 1u << binding;
        }
        for (index = 0u; index < desc->texture_count; ++index) {
            uint32_t binding = desc->textures[index].binding;

            if (binding >= OPENAGC_FRONTEND_REFLECTION_SLOTS ||
                (pipeline->texture_mask & (1u << binding)) != 0u) {
                (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
                free(pipeline);
                frontend->pipeline_count--;
                return OPENAGC_ERROR_OUT_OF_RANGE;
            }
            pipeline->texture_mask |= 1u << binding;
        }
    }
    pipeline->layout = (openagc_frontend_pipeline_layout *)calloc(1u, sizeof(*pipeline->layout));
    if (pipeline->layout == NULL) {
        (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
        free(pipeline);
        frontend->pipeline_count--;
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    pipeline->layout->frontend = frontend;
    pipeline->layout->resource_mask = pipeline->resource_mask;
    pipeline->layout->texture_mask = pipeline->texture_mask;
    pipeline->layout->refs = 1u;
    {
        uint32_t slot;

        for (slot = 0u; slot < OPENAGC_FRONTEND_REFLECTION_SLOTS; ++slot) {
            openagc_gpu_buffer *buffer = NULL;
            openagc_graphics_image *image = NULL;
            uint64_t offset = 0u;
            uint64_t size_bytes = 0u;

            result = openagc_shader_pipeline_plan_slot(pipeline->plan, slot, &buffer, &offset,
                                                      &size_bytes, &image);
            if (result != OPENAGC_OK) {
                free(pipeline->layout);
                (void)openagc_shader_pipeline_plan_destroy(pipeline->plan);
                free(pipeline);
                frontend->pipeline_count--;
                return result;
            }
            pipeline->layout->resource_offset[slot] = offset;
            pipeline->layout->resource_size[slot] = size_bytes;
        }
    }
    *out_pipeline = pipeline;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_info(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_info *info)
{
    openagc_shader_pipeline_info plan_info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_result result;

    if (pipeline == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    result = openagc_shader_pipeline_plan_get_info(pipeline->plan, &plan_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    info->kind = plan_info.kind;
    info->compiler_verified = plan_info.compiler_verified;
    info->gpu_executable = plan_info.gpu_executable;
    info->resource_count = plan_info.resource_count;
    info->texture_count = plan_info.texture_count;
    info->host_register_program_dwords = pipeline->host_register_program_dwords;
    info->psbc_pgm_patched = pipeline->psbc_pgm_patched;
    info->psbc_code_bound = pipeline->psbc_code_bound;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_psbc_register_snapshot(
    openagc_frontend_pipeline *pipeline, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_shader_artifact_info vertex_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_artifact_info pixel_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_psbc_reflection vertex_reflection;
    openagc_psbc_reflection pixel_reflection;
    uint32_t *words = NULL;
    uint8_t *vertex_copy = NULL;
    uint8_t *pixel_copy = NULL;
    uint32_t word_count;
    uint32_t capacity;
    openagc_result result;

    if (pipeline == NULL || vertex_metadata == NULL || pixel_metadata == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_pipeline_get_info(pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.kind != OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (pipeline->vertex == NULL || pipeline->pixel == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_shader_artifact_get_info(pipeline->vertex, &vertex_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_get_info(pipeline->pixel, &pixel_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_metadata_parse_reflection(vertex_metadata, vertex_metadata_size,
                                                   &vertex_reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_validate(&vertex_reflection, 1u, vertex_info.code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (vertex_reflection.has_linkage == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_psbc_metadata_parse_reflection(pixel_metadata, pixel_metadata_size,
                                                   &pixel_reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_validate(&pixel_reflection, 5u, pixel_info.code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (pixel_reflection.has_linkage != 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    capacity = openagc_psbc_reflection_register_program_dwords(&vertex_reflection) +
               openagc_psbc_reflection_register_program_dwords(&pixel_reflection);
    if (capacity == 0u || capacity > 768u) {
        return OPENAGC_ERROR_CAPACITY;
    }
    words = (uint32_t *)malloc((size_t)capacity * sizeof(uint32_t));
    if (words == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    vertex_copy = (uint8_t *)malloc((size_t)vertex_metadata_size);
    pixel_copy = (uint8_t *)malloc((size_t)pixel_metadata_size);
    if (vertex_copy == NULL || pixel_copy == NULL) {
        free(words);
        free(vertex_copy);
        free(pixel_copy);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    memcpy(vertex_copy, vertex_metadata, (size_t)vertex_metadata_size);
    memcpy(pixel_copy, pixel_metadata, (size_t)pixel_metadata_size);
    word_count = openagc_psbc_reflection_encode_register_program(&vertex_reflection, words);
    word_count += openagc_psbc_reflection_encode_register_program(&pixel_reflection,
                                                                 words + word_count);
    if (word_count != capacity) {
        free(words);
        free(vertex_copy);
        free(pixel_copy);
        return OPENAGC_ERROR_INTEGRITY;
    }
    free(pipeline->host_register_program);
    free(pipeline->psbc_vertex_metadata);
    free(pipeline->psbc_pixel_metadata);
    openagc_frontend_pipeline_release_psbc_code(pipeline);
    pipeline->host_register_program = words;
    pipeline->host_register_program_dwords = word_count;
    pipeline->psbc_vertex_metadata = vertex_copy;
    pipeline->psbc_vertex_metadata_size = vertex_metadata_size;
    pipeline->psbc_pixel_metadata = pixel_copy;
    pipeline->psbc_pixel_metadata_size = pixel_metadata_size;
    pipeline->psbc_vertex_code_va = 0u;
    pipeline->psbc_pixel_code_va = 0u;
    pipeline->psbc_pgm_patched = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_host_register_program(
    const openagc_frontend_pipeline *pipeline, uint32_t *words, uint32_t max_words,
    uint32_t *out_count)
{
    if (pipeline == NULL || out_count == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0u;
    if (pipeline->host_register_program_dwords == 0u ||
        pipeline->host_register_program == NULL) {
        return OPENAGC_ERROR_NOT_READY;
    }
    if (words == NULL || max_words < pipeline->host_register_program_dwords) {
        *out_count = pipeline->host_register_program_dwords;
        return OPENAGC_ERROR_CAPACITY;
    }
    memcpy(words, pipeline->host_register_program,
           (size_t)pipeline->host_register_program_dwords * sizeof(uint32_t));
    *out_count = pipeline->host_register_program_dwords;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_patch_psbc_pgm_vas(
    openagc_frontend_pipeline *pipeline, uint64_t vertex_code_va, uint64_t pixel_code_va)
{
    openagc_shader_artifact_info vertex_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_artifact_info pixel_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_psbc_reflection vertex_reflection;
    openagc_psbc_reflection pixel_reflection;
    uint32_t *words = NULL;
    uint32_t word_count;
    uint32_t capacity;
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->host_register_program == NULL || pipeline->psbc_vertex_metadata == NULL ||
        pipeline->psbc_pixel_metadata == NULL) {
        return OPENAGC_ERROR_NOT_READY;
    }
    if (pipeline->vertex == NULL || pipeline->pixel == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_shader_artifact_get_info(pipeline->vertex, &vertex_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_get_info(pipeline->pixel, &pixel_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_metadata_parse_reflection(
        pipeline->psbc_vertex_metadata, pipeline->psbc_vertex_metadata_size, &vertex_reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_validate(&vertex_reflection, 1u, vertex_info.code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_metadata_parse_reflection(
        pipeline->psbc_pixel_metadata, pipeline->psbc_pixel_metadata_size, &pixel_reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_validate(&pixel_reflection, 5u, pixel_info.code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_patch_pgm_va(&vertex_reflection, vertex_code_va);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_patch_pgm_va(&pixel_reflection, pixel_code_va);
    if (result != OPENAGC_OK) {
        return result;
    }
    capacity = openagc_psbc_reflection_register_program_dwords(&vertex_reflection) +
               openagc_psbc_reflection_register_program_dwords(&pixel_reflection);
    if (capacity == 0u || capacity != pipeline->host_register_program_dwords) {
        return OPENAGC_ERROR_INTEGRITY;
    }
    words = (uint32_t *)malloc((size_t)capacity * sizeof(uint32_t));
    if (words == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    word_count = openagc_psbc_reflection_encode_register_program(&vertex_reflection, words);
    word_count += openagc_psbc_reflection_encode_register_program(&pixel_reflection,
                                                                 words + word_count);
    if (word_count != capacity) {
        free(words);
        return OPENAGC_ERROR_INTEGRITY;
    }
    free(pipeline->host_register_program);
    pipeline->host_register_program = words;
    if (pipeline->psbc_code_bound != 0u &&
        (vertex_code_va != pipeline->psbc_vertex_code_va ||
         pixel_code_va != pipeline->psbc_pixel_code_va)) {
        openagc_frontend_pipeline_release_psbc_code(pipeline);
    }
    pipeline->psbc_vertex_code_va = vertex_code_va;
    pipeline->psbc_pixel_code_va = pixel_code_va;
    pipeline->psbc_pgm_patched = 1u;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_record_psbc_register_eop(
    openagc_frontend_pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->host_register_program == NULL || pipeline->host_register_program_dwords == 0u) {
        return OPENAGC_ERROR_NOT_READY;
    }
    return openagc_gpu_host_graphics_register_eop(pipeline->frontend->device,
                                                  pipeline->host_register_program,
                                                  pipeline->host_register_program_dwords);
}

openagc_result openagc_frontend_pipeline_record_psbc_register_eop_if_bound(
    openagc_frontend_pipeline *pipeline)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->psbc_code_bound == 0u) {
        return OPENAGC_OK;
    }
    return openagc_frontend_pipeline_record_psbc_register_eop(pipeline);
}

openagc_result openagc_frontend_pipeline_get_psbc_code_vas(
    const openagc_frontend_pipeline *pipeline, uint64_t *vertex_code_va,
    uint64_t *pixel_code_va)
{
    if (pipeline == NULL || vertex_code_va == NULL || pixel_code_va == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->psbc_pgm_patched == 0u) {
        *vertex_code_va = 0u;
        *pixel_code_va = 0u;
        return OPENAGC_ERROR_NOT_READY;
    }
    *vertex_code_va = pipeline->psbc_vertex_code_va;
    *pixel_code_va = pipeline->psbc_pixel_code_va;
    return OPENAGC_OK;
}

static void openagc_frontend_pipeline_release_psbc_code(openagc_frontend_pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->frontend == NULL || pipeline->psbc_code_bound == 0u) {
        return;
    }
    if (pipeline->psbc_vertex_code_bytes != 0u) {
        openagc_frontend_block_free(pipeline->frontend, pipeline->psbc_vertex_code_offset);
    }
    if (pipeline->psbc_pixel_code_bytes != 0u) {
        openagc_frontend_block_free(pipeline->frontend, pipeline->psbc_pixel_code_offset);
    }
    pipeline->psbc_vertex_code_offset = 0u;
    pipeline->psbc_pixel_code_offset = 0u;
    pipeline->psbc_vertex_code_bytes = 0u;
    pipeline->psbc_pixel_code_bytes = 0u;
    pipeline->psbc_code_bound = 0u;
}

openagc_result openagc_frontend_pipeline_bind_psbc_code(openagc_frontend_pipeline *pipeline)
{
    const uint8_t *vertex_code = NULL;
    const uint8_t *pixel_code = NULL;
    uint32_t vertex_code_size = 0u;
    uint32_t pixel_code_size = 0u;
    uint64_t vertex_offset = 0u;
    uint64_t pixel_offset = 0u;
    uint64_t vertex_va = 0u;
    uint64_t pixel_va = 0u;
    openagc_result result;

    if (pipeline == NULL || pipeline->frontend == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->host_register_program == NULL || pipeline->psbc_vertex_metadata == NULL ||
        pipeline->psbc_pixel_metadata == NULL) {
        return OPENAGC_ERROR_NOT_READY;
    }
    if (pipeline->vertex == NULL || pipeline->pixel == NULL || pipeline->frontend->heap == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_shader_artifact_get_code(pipeline->vertex, &vertex_code, &vertex_code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_shader_artifact_get_code(pipeline->pixel, &pixel_code, &pixel_code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (vertex_code_size == 0u || pixel_code_size == 0u || (vertex_code_size & 3u) != 0u ||
        (pixel_code_size & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    openagc_frontend_pipeline_release_psbc_code(pipeline);

    result = openagc_frontend_block_alloc(pipeline->frontend, vertex_code_size, &vertex_offset);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_block_alloc(pipeline->frontend, pixel_code_size, &pixel_offset);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    result = openagc_gpu_memory_write(pipeline->frontend->heap, vertex_offset, vertex_code,
                                      vertex_code_size);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, pixel_offset);
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    result = openagc_gpu_memory_write(pipeline->frontend->heap, pixel_offset, pixel_code,
                                      pixel_code_size);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, pixel_offset);
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    result = openagc_gpu_memory_get_device_address(pipeline->frontend->heap, vertex_offset,
                                                   &vertex_va);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, pixel_offset);
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    result = openagc_gpu_memory_get_device_address(pipeline->frontend->heap, pixel_offset,
                                                   &pixel_va);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, pixel_offset);
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    result = openagc_frontend_pipeline_patch_psbc_pgm_vas(pipeline, vertex_va, pixel_va);
    if (result != OPENAGC_OK) {
        openagc_frontend_block_free(pipeline->frontend, pixel_offset);
        openagc_frontend_block_free(pipeline->frontend, vertex_offset);
        return result;
    }
    pipeline->psbc_vertex_code_offset = vertex_offset;
    pipeline->psbc_pixel_code_offset = pixel_offset;
    pipeline->psbc_vertex_code_bytes = vertex_code_size;
    pipeline->psbc_pixel_code_bytes = pixel_code_size;
    pipeline->psbc_code_bound = 1u;
    return OPENAGC_OK;
}

static int openagc_frontend_spans_cover(uint32_t mask, const uint32_t *offsets,
                                        const uint32_t *bytes, uint32_t count)
{
    uint32_t location;

    for (location = 0u; location < 8u; ++location) {
        uint32_t start = location * 4u;
        uint32_t index;
        int covered = 0;

        if ((mask & (1u << location)) == 0u) {
            continue;
        }
        for (index = 0u; index < count; ++index) {
            if (offsets[index] <= start && bytes[index] >= (start - offsets[index]) + 4u) {
                covered = 1;
                break;
            }
        }
        if (covered == 0) {
            return 0;
        }
    }
    return 1;
}

openagc_result openagc_frontend_pipeline_set_vertex_attributes(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *bytes, uint32_t count)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_result result;
    uint32_t index;

    if (pipeline == NULL || offsets == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u || count > 8u || stride == 0u || (stride & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < count; ++index) {
        if (bytes[index] == 0u || (offsets[index] & 3u) != 0u || (bytes[index] & 3u) != 0u ||
            offsets[index] > stride || bytes[index] > stride - offsets[index]) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    result = openagc_frontend_pipeline_get_info(pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.kind != OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (openagc_frontend_spans_cover(pipeline->vertex_input_mask, offsets, bytes, count) == 0) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    pipeline->vertex_stride = stride;
    pipeline->vertex_attribute_offset = offsets[0];
    pipeline->vertex_attribute_bytes = bytes[0];
    pipeline->vertex_attribute_count = count;
    for (index = 0u; index < 8u; ++index) {
        if (index < count) {
            pipeline->vertex_attribute_offsets[index] = offsets[index];
            pipeline->vertex_attribute_sizes[index] = bytes[index];
        } else {
            pipeline->vertex_attribute_offsets[index] = 0u;
            pipeline->vertex_attribute_sizes[index] = 0u;
        }
        pipeline->vertex_attribute_formats[index] = 0u;
        pipeline->vertex_attribute_rates[index] = 0u;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_vertex_input(
    openagc_frontend_pipeline *pipeline, uint32_t stride, uint32_t attribute_offset,
    uint32_t attribute_bytes)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (stride == 0u || attribute_bytes == 0u || (stride & 3u) != 0u ||
        (attribute_offset & 3u) != 0u || (attribute_bytes & 3u) != 0u ||
        attribute_offset > stride || attribute_bytes > stride - attribute_offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_pipeline_get_info(pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.kind != OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    {
        uint32_t offset = attribute_offset;
        uint32_t size = attribute_bytes;

        if (openagc_frontend_spans_cover(pipeline->vertex_input_mask, &offset, &size, 1u) == 0) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    pipeline->vertex_stride = stride;
    pipeline->vertex_attribute_offset = attribute_offset;
    pipeline->vertex_attribute_bytes = attribute_bytes;
    pipeline->vertex_attribute_count = 1u;
    pipeline->vertex_attribute_offsets[0] = attribute_offset;
    pipeline->vertex_attribute_sizes[0] = attribute_bytes;
    pipeline->vertex_attribute_formats[0] = 0u;
    pipeline->vertex_attribute_rates[0] = 0u;
    {
        uint32_t slot;

        for (slot = 1u; slot < 8u; ++slot) {
            pipeline->vertex_attribute_offsets[slot] = 0u;
            pipeline->vertex_attribute_sizes[slot] = 0u;
            pipeline->vertex_attribute_formats[slot] = 0u;
            pipeline->vertex_attribute_rates[slot] = 0u;
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_vertex_attribute(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset, uint32_t *bytes)
{
    if (offset == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *offset = 0u;
    *bytes = 0u;
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= pipeline->vertex_attribute_count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    *offset = pipeline->vertex_attribute_offsets[index];
    *bytes = pipeline->vertex_attribute_sizes[index];
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_vertex_formats(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const openagc_frontend_vertex_format *formats, uint32_t count)
{
    uint32_t bytes[8];
    uint32_t index;

    if (pipeline == NULL || offsets == NULL || formats == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u || count > 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < count; ++index) {
        bytes[index] = openagc_frontend_vertex_format_bytes(formats[index]);
        if (bytes[index] == 0u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    }
    {
        openagc_result result =
            openagc_frontend_pipeline_set_vertex_attributes(pipeline, stride, offsets, bytes,
                                                           count);

        if (result != OPENAGC_OK) {
            return result;
        }
    }
    for (index = 0u; index < count; ++index) {
        pipeline->vertex_attribute_formats[index] = formats[index];
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_translate_primitive(openagc_frontend_kind kind, uint32_t native,
                                                    openagc_frontend_primitive *out_primitive)
{
    openagc_frontend_primitive primitive = 0u;

    if (out_primitive == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_primitive = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_POINT_LIST) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_POINT_LIST;
        } else if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_LINE_LIST) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_LINE_LIST;
        } else if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_LINE_STRIP;
        } else if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_LIST;
        } else if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_STRIP;
        } else if (native == OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_FAN;
        } else {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native == OPENAGC_FRONTEND_GL_POINTS) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_POINT_LIST;
        } else if (native == OPENAGC_FRONTEND_GL_LINES) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_LINE_LIST;
        } else if (native == OPENAGC_FRONTEND_GL_LINE_STRIP) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_LINE_STRIP;
        } else if (native == OPENAGC_FRONTEND_GL_TRIANGLES) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_LIST;
        } else if (native == OPENAGC_FRONTEND_GL_TRIANGLE_STRIP) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_STRIP;
        } else if (native == OPENAGC_FRONTEND_GL_TRIANGLE_FAN) {
            primitive = OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_FAN;
        } else {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_primitive = primitive;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_primitive(openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive primitive)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->vertex == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (primitive < OPENAGC_FRONTEND_PRIMITIVE_POINT_LIST ||
        primitive > OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_FAN) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    pipeline->primitive = primitive;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_primitive(const openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive *out_primitive)
{
    if (out_primitive == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_primitive = 0u;
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_primitive = pipeline->primitive;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_translate_blend_factor(openagc_frontend_kind kind, uint32_t native,
                                                       openagc_frontend_blend_factor *out_factor)
{
    openagc_frontend_blend_factor factor = 0u;

    if (out_factor == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_factor = 0u;
    if (kind == OPENAGC_FRONTEND_VULKAN) {
        if (native == OPENAGC_FRONTEND_VK_BLEND_FACTOR_ZERO) {
            factor = OPENAGC_FRONTEND_BLEND_ZERO;
        } else if (native == OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE) {
            factor = OPENAGC_FRONTEND_BLEND_ONE;
        } else if (native == OPENAGC_FRONTEND_VK_BLEND_FACTOR_SRC_ALPHA) {
            factor = OPENAGC_FRONTEND_BLEND_SRC_ALPHA;
        } else if (native == OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) {
            factor = OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA;
        } else {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else if (kind == OPENAGC_FRONTEND_OPENGL) {
        if (native == OPENAGC_FRONTEND_GL_ZERO) {
            factor = OPENAGC_FRONTEND_BLEND_ZERO;
        } else if (native == OPENAGC_FRONTEND_GL_ONE) {
            factor = OPENAGC_FRONTEND_BLEND_ONE;
        } else if (native == OPENAGC_FRONTEND_GL_SRC_ALPHA) {
            factor = OPENAGC_FRONTEND_BLEND_SRC_ALPHA;
        } else if (native == OPENAGC_FRONTEND_GL_ONE_MINUS_SRC_ALPHA) {
            factor = OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA;
        } else {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_factor = factor;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_blend(openagc_frontend_pipeline *pipeline,
                                                   uint32_t enable, openagc_frontend_blend_factor src,
                                                   openagc_frontend_blend_factor dst)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->vertex == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (enable > 1u || src < OPENAGC_FRONTEND_BLEND_ZERO ||
        src > OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA || dst < OPENAGC_FRONTEND_BLEND_ZERO ||
        dst > OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    pipeline->blend_enable = enable;
    pipeline->blend_src = src;
    pipeline->blend_dst = dst;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_blend(const openagc_frontend_pipeline *pipeline,
                                                   uint32_t *enable,
                                                   openagc_frontend_blend_factor *src,
                                                   openagc_frontend_blend_factor *dst)
{
    if (enable == NULL || src == NULL || dst == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *enable = 0u;
    *src = 0u;
    *dst = 0u;
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *enable = pipeline->blend_enable;
    *src = pipeline->blend_src;
    *dst = pipeline->blend_dst;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_write_push_constants(
    openagc_frontend_pipeline *pipeline, uint32_t offset, const void *bytes, uint32_t size)
{
    if (pipeline == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u || (offset & 3u) != 0u || (size & 3u) != 0u || offset > 128u ||
        size > 128u - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    memcpy(pipeline->push_constants + offset, bytes, size);
    if (offset + size > pipeline->push_constant_end) {
        pipeline->push_constant_end = offset + size;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_read_push_constants(
    const openagc_frontend_pipeline *pipeline, uint32_t offset, void *bytes, uint32_t size)
{
    if (pipeline == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0u || offset > 128u || size > 128u - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    memcpy(bytes, pipeline->push_constants + offset, size);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_vertex_rates(
    openagc_frontend_pipeline *pipeline, const uint32_t *rates, uint32_t count)
{
    uint32_t index;

    if (pipeline == NULL || rates == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u || count != pipeline->vertex_attribute_count) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    for (index = 0u; index < count; ++index) {
        if (rates[index] > 1u) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    }
    for (index = 0u; index < count; ++index) {
        pipeline->vertex_attribute_rates[index] = rates[index];
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_vertex_format(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset,
    openagc_frontend_vertex_format *format, uint32_t *bytes)
{
    openagc_result result = openagc_frontend_pipeline_get_vertex_attribute(pipeline, index, offset,
                                                                           bytes);

    if (format == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *format = 0u;
    if (result != OPENAGC_OK) {
        return result;
    }
    *format = pipeline->vertex_attribute_formats[index];
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_sampler(openagc_frontend_pipeline *pipeline,
                                                   openagc_frontend_sampler *sampler)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler != NULL && sampler->frontend != pipeline->frontend) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (sampler != NULL && pipeline->texture_mask == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (pipeline->sampler == sampler) {
        return OPENAGC_OK;
    }
    if (pipeline->sampler != NULL) {
        pipeline->sampler->pipeline_refs--;
    }
    pipeline->sampler = sampler;
    if (sampler != NULL) {
        sampler->pipeline_refs++;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_set_vertex_stride(openagc_frontend_pipeline *pipeline,
                                                          uint32_t stride)
{
    return openagc_frontend_pipeline_set_vertex_input(pipeline, stride, 0u, stride);
}

openagc_result openagc_frontend_pipeline_destroy(openagc_frontend_pipeline *pipeline)
{
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->pass_binds != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (pipeline->layout != NULL && pipeline->layout->refs != 1u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (pipeline->sampler != NULL) {
        pipeline->sampler->pipeline_refs--;
        pipeline->sampler = NULL;
    }
    result = openagc_shader_pipeline_plan_destroy(pipeline->plan);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (pipeline->pixel != NULL) {
        result = openagc_shader_artifact_destroy(pipeline->pixel);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    if (pipeline->vertex != NULL) {
        result = openagc_shader_artifact_destroy(pipeline->vertex);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    free(pipeline->host_register_program);
    free(pipeline->psbc_vertex_metadata);
    free(pipeline->psbc_pixel_metadata);
    openagc_frontend_pipeline_release_psbc_code(pipeline);
    pipeline->frontend->pipeline_count--;
    free(pipeline->layout);
    free(pipeline);
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_get_layout(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout)
{
    if (out_layout == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_layout = NULL;
    if (pipeline == NULL || pipeline->layout == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_layout = pipeline->layout;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_layout_matches(const openagc_frontend_pipeline *pipeline,
                                               const openagc_frontend_pipeline_layout *layout)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (pipeline->resource_mask == 0u && pipeline->texture_mask == 0u) {
        return OPENAGC_OK;
    }
    if (layout == NULL || layout->resource_mask != pipeline->layout->resource_mask ||
        layout->texture_mask != pipeline->layout->texture_mask) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        uint32_t slot;

        for (slot = 0u; slot < OPENAGC_FRONTEND_REFLECTION_SLOTS; ++slot) {
            if (layout->resource_offset[slot] != pipeline->layout->resource_offset[slot] ||
                layout->resource_size[slot] != pipeline->layout->resource_size[slot]) {
                return OPENAGC_ERROR_BAD_STATE;
            }
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_layout_retain(openagc_frontend_pipeline_layout *layout)
{
    if (layout == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    layout->refs++;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_pipeline_layout_release(openagc_frontend_pipeline_layout *layout)
{
    if (layout == NULL || layout->refs == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    layout->refs--;
    return OPENAGC_OK;
}

openagc_result openagc_frontend_dispatch_validate(const openagc_frontend_pipeline *pipeline,
                                                  uint32_t groups_x, uint32_t groups_y,
                                                  uint32_t groups_z)
{
    openagc_frontend_pipeline_info info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_result result;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_pipeline_get_info(pipeline, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.kind != OPENAGC_SHADER_PIPELINE_COMPUTE || pipeline->compute == NULL) {
        return info.kind != OPENAGC_SHADER_PIPELINE_COMPUTE
                   ? OPENAGC_ERROR_UNSUPPORTED_OPERATION
                   : OPENAGC_ERROR_BAD_STATE;
    }
    if (groups_x > OPENAGC_FRONTEND_MAX_DISPATCH_GROUPS ||
        groups_y > OPENAGC_FRONTEND_MAX_DISPATCH_GROUPS ||
        groups_z > OPENAGC_FRONTEND_MAX_DISPATCH_GROUPS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (groups_x == 0u || groups_y == 0u || groups_z == 0u) {
        return OPENAGC_OK;
    }
    result = openagc_shader_artifact_get_info(pipeline->compute, &artifact_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (artifact_info.host_store_const != 0u) {
        openagc_gpu_buffer *buffer = NULL;
        openagc_graphics_image *image = NULL;
        uint64_t offset = 0u;
        uint64_t size_bytes = 0u;

        if (groups_x != 1u || groups_y != 1u || groups_z != 1u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        result = openagc_shader_pipeline_plan_slot(pipeline->plan, 0u, &buffer, &offset,
                                                  &size_bytes, &image);
        if (result != OPENAGC_OK || buffer == NULL || size_bytes < 4u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        return OPENAGC_OK;
    }
    if (artifact_info.host_store_span != 0u) {
        openagc_gpu_buffer *buffer = NULL;
        openagc_graphics_image *image = NULL;
        uint64_t offset = 0u;
        uint64_t size_bytes = 0u;
        uint32_t spans;

        if (groups_x != 1u || groups_y != 1u || groups_z != 1u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        result = openagc_shader_pipeline_plan_slot(pipeline->plan, 0u, &buffer, &offset,
                                                  &size_bytes, &image);
        if (result != OPENAGC_OK || buffer == NULL || size_bytes < OPENAGC_STORE_SPAN_BYTES) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        spans = (uint32_t)(size_bytes / OPENAGC_STORE_SPAN_BYTES);
        if (spans > OPENAGC_PM4_COMPUTE_SPAN_MAX) {
            spans = OPENAGC_PM4_COMPUTE_SPAN_MAX;
        }
        if (spans == 0u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        return OPENAGC_OK;
    }
    result = openagc_shader_artifact_require_compiler(pipeline->compute);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.gpu_executable == 0u || info.compiler_verified == 0u) {
        return OPENAGC_ERROR_NOT_READY;
    }
    return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}

openagc_result openagc_frontend_dispatch(const openagc_frontend_pipeline *pipeline,
                                         uint32_t groups_x, uint32_t groups_y,
                                         uint32_t groups_z)
{
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_result result;

    result = openagc_frontend_dispatch_validate(pipeline, groups_x, groups_y, groups_z);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (pipeline == NULL || groups_x == 0u || groups_y == 0u || groups_z == 0u) {
        return OPENAGC_OK;
    }
    result = openagc_shader_artifact_get_info(pipeline->compute, &artifact_info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (artifact_info.host_store_const != 0u) {
        openagc_gpu_buffer *buffer = NULL;
        openagc_graphics_image *image = NULL;
        uint64_t offset = 0u;
        uint64_t size_bytes = 0u;

        result = openagc_shader_pipeline_plan_slot(pipeline->plan, 0u, &buffer, &offset,
                                                  &size_bytes, &image);
        if (result != OPENAGC_OK || buffer == NULL || size_bytes < 4u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        return openagc_gpu_host_store_const(pipeline->frontend->device, buffer, offset);
    }
    if (artifact_info.host_store_span != 0u) {
        openagc_gpu_buffer *buffer = NULL;
        openagc_graphics_image *image = NULL;
        uint64_t offset = 0u;
        uint64_t size_bytes = 0u;
        uint32_t spans;

        result = openagc_shader_pipeline_plan_slot(pipeline->plan, 0u, &buffer, &offset,
                                                  &size_bytes, &image);
        if (result != OPENAGC_OK || buffer == NULL || size_bytes < OPENAGC_STORE_SPAN_BYTES) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        spans = (uint32_t)(size_bytes / OPENAGC_STORE_SPAN_BYTES);
        if (spans > OPENAGC_PM4_COMPUTE_SPAN_MAX) {
            spans = OPENAGC_PM4_COMPUTE_SPAN_MAX;
        }
        if (spans == 1u) {
            return openagc_gpu_host_store_span(pipeline->frontend->device, buffer, offset);
        }
        return openagc_gpu_host_store_span_n(pipeline->frontend->device, buffer, offset, spans);
    }
    return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}

openagc_result openagc_frontend_resources_present(uint32_t has_buffer, uint32_t has_image,
                                                 uint32_t has_sampler)
{
    return has_buffer != 0u || has_image != 0u || has_sampler != 0u
               ? OPENAGC_OK
               : OPENAGC_ERROR_BAD_STATE;
}

openagc_result openagc_frontend_recording_covers(const openagc_frontend_pipeline *pipeline,
                                                 uint32_t resource_mask, uint32_t texture_mask)
{
    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((resource_mask & pipeline->resource_mask) != pipeline->resource_mask ||
        (texture_mask & pipeline->texture_mask) != pipeline->texture_mask) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_recording_matches(
    const openagc_frontend_pipeline *pipeline, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler)
{
    uint32_t slot;
    uint32_t needs_sampler = 0u;

    if (pipeline == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    for (slot = 0u; slot < OPENAGC_FRONTEND_REFLECTION_SLOTS; ++slot) {
        openagc_gpu_buffer *required_buffer = NULL;
        openagc_graphics_image *required_image = NULL;
        uint64_t required_offset = 0u;
        uint64_t required_size = 0u;
        const openagc_frontend_buffer *recorded_buffer =
            buffers == NULL ? NULL : buffers[slot];
        const openagc_frontend_image *recorded_image = images == NULL ? NULL : images[slot];
        openagc_result result = openagc_shader_pipeline_plan_slot(
            pipeline->plan, slot, &required_buffer, &required_offset, &required_size,
            &required_image);

        if (result != OPENAGC_OK) {
            return result;
        }
        if (required_buffer != NULL &&
            (recorded_buffer == NULL || recorded_buffer->buffer != required_buffer ||
             offsets == NULL || sizes == NULL || offsets[slot] != required_offset ||
             sizes[slot] != required_size)) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (required_image != NULL &&
            (recorded_image == NULL || recorded_image->image != required_image)) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (required_image != NULL) {
            needs_sampler = 1u;
        }
    }
    if (needs_sampler != 0u &&
        (pipeline->sampler == NULL || sampler != pipeline->sampler)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return OPENAGC_OK;
}

openagc_result openagc_frontend_render_pass_layout_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_pipeline_layout *layout)
{
    if (pass == NULL || pass->pipeline == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_layout_matches(pass->pipeline, layout);
}

openagc_result openagc_frontend_render_pass_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler)
{
    if (pass == NULL || pass->pipeline == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_recording_matches(pass->pipeline, buffers, offsets, sizes, images,
                                             sampler);
}

openagc_result openagc_frontend_render_pass_covers(const openagc_frontend_render_pass *pass,
                                                   uint32_t resource_mask, uint32_t texture_mask)
{
    if (pass == NULL || pass->pipeline == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_recording_covers(pass->pipeline, resource_mask, texture_mask);
}
