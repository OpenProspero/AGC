/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/graphics.h"
#include "openagc/pm4_write_fw940.h"
#include "openagc_gpu_internal.h"
#include "openagc_graphics_internal.h"
#include "openagc_shader_internal.h"

#include <stdlib.h>
#include <string.h>

#define OPENAGC_GRAPHICS_MAX_DIMENSION 4096u
#define OPENAGC_GRAPHICS_MAX_IMAGES 64u
#define OPENAGC_GRAPHICS_MAX_COMMAND_BUFFERS 32u
#define OPENAGC_GRAPHICS_MAX_COMMANDS 128u
#define OPENAGC_GRAPHICS_PIXEL_BYTES 4u

typedef enum openagc_graphics_recording_state {
    OPENAGC_GRAPHICS_INITIAL,
    OPENAGC_GRAPHICS_RECORDING,
    OPENAGC_GRAPHICS_EXECUTABLE,
    OPENAGC_GRAPHICS_HOST_APPLIED,
    OPENAGC_GRAPHICS_HOST_EXECUTED
} openagc_graphics_recording_state;

typedef struct openagc_graphics_shadow {
    openagc_graphics_image *image;
    uint64_t initial_revision;
    openagc_graphics_image_state initial_state;
    openagc_graphics_owner initial_owner;
    openagc_graphics_image_state recorded_state;
    openagc_graphics_owner recorded_owner;
    uint64_t applied_revision;
} openagc_graphics_shadow;

struct openagc_graphics_image {
    openagc_gpu_device *device;
    openagc_graphics_image *next;
    openagc_gpu_memory *memory;
    uint64_t footprint_bytes;
    uint64_t memory_offset;
    uint32_t image_id;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    openagc_graphics_format format;
    openagc_graphics_usage usage;
    uint32_t command_references;
    uint32_t pipeline_references;
    uint64_t state_revision;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
};

struct openagc_graphics_command_buffer {
    openagc_gpu_device *device;
    openagc_graphics_command *commands;
    openagc_graphics_shadow *shadows;
    openagc_graphics_image *target;
    openagc_graphics_image *depth_target;
    uint32_t max_commands;
    uint32_t command_count;
    uint32_t shadow_count;
    uint32_t scissor_valid;
    openagc_graphics_recording_state state;
};

static openagc_graphics_shadow *openagc_graphics_find_shadow(
    const openagc_graphics_command_buffer *command_buffer,
    const openagc_graphics_image *image)
{
    uint32_t i;

    for (i = 0u; i < command_buffer->shadow_count; ++i) {
        if (command_buffer->shadows[i].image == image) {
            return &command_buffer->shadows[i];
        }
    }
    return NULL;
}

static void openagc_graphics_current_state(
    const openagc_graphics_command_buffer *command_buffer,
    const openagc_graphics_image *image,
    openagc_graphics_image_state *state, openagc_graphics_owner *owner)
{
    const openagc_graphics_shadow *shadow =
        openagc_graphics_find_shadow(command_buffer, image);

    *state = shadow != NULL ? shadow->recorded_state : image->state;
    *owner = shadow != NULL ? shadow->recorded_owner : image->owner;
}

static openagc_result openagc_graphics_retain_image(
    openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_image *image, openagc_graphics_shadow **out_shadow)
{
    openagc_graphics_shadow *shadow =
        openagc_graphics_find_shadow(command_buffer, image);

    if (shadow != NULL) {
        *out_shadow = shadow;
        return OPENAGC_OK;
    }
    if (command_buffer->shadow_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    if (image->command_references == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    shadow = &command_buffer->shadows[command_buffer->shadow_count];
    shadow->image = image;
    shadow->initial_revision = image->state_revision;
    shadow->initial_state = image->state;
    shadow->initial_owner = image->owner;
    shadow->recorded_state = image->state;
    shadow->recorded_owner = image->owner;
    image->command_references++;
    command_buffer->shadow_count++;
    *out_shadow = shadow;
    return OPENAGC_OK;
}

static void openagc_graphics_release_images(openagc_graphics_command_buffer *command_buffer)
{
    uint32_t i;

    for (i = 0u; i < command_buffer->shadow_count; ++i) {
        command_buffer->shadows[i].image->command_references--;
    }
    command_buffer->shadow_count = 0u;
    command_buffer->command_count = 0u;
    command_buffer->target = NULL;
    command_buffer->depth_target = NULL;
    command_buffer->scissor_valid = 0u;
}

static int openagc_graphics_supported_state(openagc_graphics_image_state state,
                                            openagc_graphics_owner owner)
{
    return (state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
            owner == OPENAGC_GRAPHICS_OWNER_HOST) ||
           (state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
            owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS) ||
           (state == OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE &&
            owner == OPENAGC_GRAPHICS_OWNER_COPY) ||
           (state == OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION &&
            owner == OPENAGC_GRAPHICS_OWNER_COPY) ||
           (state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
            owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS) ||
           (state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET &&
            owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
}

static openagc_graphics_usage openagc_graphics_required_usage(
    openagc_graphics_image_state state)
{
    if (state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET) {
        return OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT;
    }
    if (state == OPENAGC_GRAPHICS_STATE_SHADER_READ) {
        return OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
    }
    if (state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET) {
        return OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT;
    }
    return 0u;
}

static void openagc_graphics_unlink_image(openagc_gpu_device *device,
                                          const openagc_graphics_image *image)
{
    openagc_graphics_image **link = &device->images;

    while (*link != NULL) {
        if (*link == image) {
            *link = image->next;
            return;
        }
        link = &(*link)->next;
    }
}

static void openagc_graphics_fill_scissor(openagc_graphics_image *image,
                                          const openagc_graphics_scissor *scissor,
                                          openagc_color color)
{
    uint8_t texel[OPENAGC_GRAPHICS_PIXEL_BYTES];
    uint8_t *base = image->memory->bytes + (size_t)image->memory_offset;
    uint32_t row;

    texel[0] = image->format == OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM ? color.b : color.r;
    texel[1] = color.g;
    texel[2] = image->format == OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM ? color.r : color.b;
    texel[3] = color.a;
    for (row = 0u; row < scissor->height; ++row) {
        uint8_t *pixel = base +
            ((size_t)scissor->y + row) * image->row_pitch_bytes +
            (size_t)scissor->x * OPENAGC_GRAPHICS_PIXEL_BYTES;
        uint32_t column;

        for (column = 0u; column < scissor->width; ++column) {
            memcpy(pixel, texel, sizeof(texel));
            pixel += OPENAGC_GRAPHICS_PIXEL_BYTES;
        }
    }
}

/*
 * Width ≤16 and height ≤8: console-proven WRITE_DATA (Steps D–F).
 * Contiguous span uses one packet; pitched rows use one packet per row.
 */
static int openagc_graphics_scissor_fits_write_data(
    const openagc_graphics_scissor *scissor)
{
    if (scissor->width == 0u || scissor->height == 0u) {
        return 0;
    }
    if (scissor->width > OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS ||
        scissor->height > OPENAGC_PM4_WRITE_DATA_MAX_ROWS) {
        return 0;
    }
    return 1;
}

static openagc_result openagc_graphics_clear_scissor_write_data_value(
    openagc_graphics_image *image, const openagc_graphics_scissor *scissor, uint32_t value)
{
    uint64_t offset;
    uint32_t dwords;
    uint64_t pixels;

    offset = image->memory_offset +
             (uint64_t)scissor->y * (uint64_t)image->row_pitch_bytes +
             (uint64_t)scissor->x * OPENAGC_GRAPHICS_PIXEL_BYTES;
    dwords = scissor->width;
    pixels = (uint64_t)scissor->width * (uint64_t)scissor->height;
    if (scissor->height == 1u ||
        (scissor->x == 0u &&
         scissor->width * OPENAGC_GRAPHICS_PIXEL_BYTES == image->row_pitch_bytes &&
         pixels <= OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS)) {
        return openagc_gpu_host_write_data_memory(image->device, image->memory, offset, value,
                                                  (uint32_t)pixels);
    }
    return openagc_gpu_host_write_data_rows(image->device, image->memory, offset,
                                            image->row_pitch_bytes, value, dwords,
                                            scissor->height);
}

static int openagc_graphics_clear_uses_write_data(const openagc_graphics_image *image,
                                                  const openagc_graphics_scissor *scissor)
{
    if (image->format != OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
        image->format != OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM) {
        return 0;
    }
    return openagc_graphics_scissor_fits_write_data(scissor);
}

static openagc_result openagc_graphics_clear_scissor_write_data(
    openagc_graphics_image *image, const openagc_graphics_scissor *scissor, openagc_color color)
{
    uint8_t texel[OPENAGC_GRAPHICS_PIXEL_BYTES];
    uint32_t value;

    texel[0] = image->format == OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM ? color.b : color.r;
    texel[1] = color.g;
    texel[2] = image->format == OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM ? color.r : color.b;
    texel[3] = color.a;
    memcpy(&value, texel, sizeof(value));
    return openagc_graphics_clear_scissor_write_data_value(image, scissor, value);
}

static int openagc_graphics_depth_clear_uses_write_data(
    const openagc_graphics_image *image, const openagc_graphics_scissor *scissor)
{
    if (image->format != OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT) {
        return 0;
    }
    return openagc_graphics_scissor_fits_write_data(scissor);
}

static openagc_result openagc_graphics_clear_depth_scissor_write_data(
    openagc_graphics_image *image, const openagc_graphics_scissor *scissor, uint32_t depth24,
    uint32_t stencil)
{
    uint8_t texel[OPENAGC_GRAPHICS_PIXEL_BYTES];
    uint32_t value;

    texel[0] = (uint8_t)depth24;
    texel[1] = (uint8_t)(depth24 >> 8);
    texel[2] = (uint8_t)(depth24 >> 16);
    texel[3] = (uint8_t)stencil;
    memcpy(&value, texel, sizeof(value));
    return openagc_graphics_clear_scissor_write_data_value(image, scissor, value);
}

static void openagc_graphics_fill_depth_scissor(openagc_graphics_image *image,
                                                const openagc_graphics_scissor *scissor,
                                                uint32_t depth24, uint32_t stencil)
{
    uint8_t texel[OPENAGC_GRAPHICS_PIXEL_BYTES];
    uint8_t *base = image->memory->bytes + (size_t)image->memory_offset;
    uint32_t row;

    /* D24_UNORM_S8_UINT keeps depth in the low 24 bits and stencil in the high byte. */
    texel[0] = (uint8_t)depth24;
    texel[1] = (uint8_t)(depth24 >> 8);
    texel[2] = (uint8_t)(depth24 >> 16);
    texel[3] = (uint8_t)stencil;
    for (row = 0u; row < scissor->height; ++row) {
        uint8_t *pixel = base + ((size_t)scissor->y + row) * image->row_pitch_bytes +
                         (size_t)scissor->x * OPENAGC_GRAPHICS_PIXEL_BYTES;
        uint32_t column;

        for (column = 0u; column < scissor->width; ++column) {
            memcpy(pixel, texel, sizeof(texel));
            pixel += OPENAGC_GRAPHICS_PIXEL_BYTES;
        }
    }
}

openagc_result openagc_graphics_get_capabilities(
    const openagc_gpu_device *device, openagc_graphics_capabilities *capabilities)
{
    if (device == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->backend = OPENAGC_BACKEND_HOST_REFERENCE;
    capabilities->gpu_execution = 0u;
    capabilities->rasterization = 0u;
    capabilities->video_output = 0u;
    capabilities->host_state_recording = 1u;
    capabilities->host_clear_simulation = 1u;
    capabilities->supported_format_mask =
        (1u << (OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM - 1u)) |
        (1u << (OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM - 1u)) |
        (1u << (OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT - 1u));
    capabilities->supported_usage_mask =
        OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT |
        OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT |
        OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT;
    capabilities->max_width = OPENAGC_GRAPHICS_MAX_DIMENSION;
    capabilities->max_height = OPENAGC_GRAPHICS_MAX_DIMENSION;
    capabilities->max_image_bytes = OPENAGC_GPU_MAX_ALLOCATION_BYTES;
    capabilities->max_images = OPENAGC_GRAPHICS_MAX_IMAGES;
    capabilities->max_command_buffers = OPENAGC_GRAPHICS_MAX_COMMAND_BUFFERS;
    capabilities->max_commands_per_buffer = OPENAGC_GRAPHICS_MAX_COMMANDS;
    capabilities->row_pitch_alignment = OPENAGC_GRAPHICS_PIXEL_BYTES;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_image_create(openagc_gpu_device *device,
                                              const openagc_graphics_image_desc *desc,
                                              openagc_graphics_image **out_image)
{
    openagc_graphics_image *image;
    uint64_t minimum_pitch;
    uint64_t footprint;

    if (out_image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_image = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GRAPHICS_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->format != OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
        desc->format != OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM &&
        desc->format != OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->tiling != OPENAGC_GRAPHICS_TILING_HOST_LINEAR ||
        desc->mip_levels != 1u || desc->array_layers != 1u ||
        desc->sample_count != 1u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->format == OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT) {
        if (desc->usage != OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    } else if (desc->usage == 0u ||
               (desc->usage & ~(OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT |
                                OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT)) != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->width == 0u || desc->height == 0u ||
        desc->width > OPENAGC_GRAPHICS_MAX_DIMENSION ||
        desc->height > OPENAGC_GRAPHICS_MAX_DIMENSION) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    minimum_pitch = (uint64_t)desc->width * OPENAGC_GRAPHICS_PIXEL_BYTES;
    if (desc->row_pitch_bytes < minimum_pitch ||
        (desc->row_pitch_bytes & (OPENAGC_GRAPHICS_PIXEL_BYTES - 1u)) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if ((uint64_t)desc->row_pitch_bytes > UINT64_MAX / desc->height) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    footprint = (uint64_t)desc->row_pitch_bytes * desc->height;
    if (footprint > OPENAGC_GPU_MAX_ALLOCATION_BYTES) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (device->image_count >= OPENAGC_GRAPHICS_MAX_IMAGES) {
        return OPENAGC_ERROR_CAPACITY;
    }
    if (device->next_graphics_image_id == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }

    image = (openagc_graphics_image *)calloc(1u, sizeof(*image));
    if (image == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    image->device = device;
    image->format = desc->format;
    image->usage = desc->usage;
    image->width = desc->width;
    image->height = desc->height;
    image->row_pitch_bytes = desc->row_pitch_bytes;
    image->footprint_bytes = footprint;
    image->image_id = ++device->next_graphics_image_id;
    image->state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    image->owner = OPENAGC_GRAPHICS_OWNER_HOST;
    image->next = device->images;
    device->images = image;
    device->image_count++;
    *out_image = image;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_image_bind_memory(openagc_graphics_image *image,
                                                   openagc_gpu_memory *memory,
                                                   uint64_t memory_offset)
{
    if (image == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->device != memory->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (image->memory != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((memory_offset & (OPENAGC_GRAPHICS_PIXEL_BYTES - 1u)) != 0u ||
        memory_offset > memory->size_bytes ||
        image->footprint_bytes > memory->size_bytes - memory_offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    image->memory = memory;
    image->memory_offset = memory_offset;
    memory->bound_images++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_image_get_info(const openagc_graphics_image *image,
                                                openagc_graphics_image_info *info)
{
    if (image == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->image_id = image->image_id;
    info->format = image->format;
    info->usage = image->usage;
    info->width = image->width;
    info->height = image->height;
    info->row_pitch_bytes = image->row_pitch_bytes;
    info->footprint_bytes = image->footprint_bytes;
    info->state = image->state;
    info->owner = image->owner;
    info->bound = image->memory != NULL ? 1u : 0u;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_image_destroy(openagc_graphics_image *image)
{
    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->command_references != 0u ||
        image->pipeline_references != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (image->memory != NULL) {
        image->memory->bound_images--;
    }
    openagc_graphics_unlink_image(image->device, image);
    image->device->image_count--;
    free(image);
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_create(
    openagc_gpu_device *device, const openagc_graphics_command_buffer_desc *desc,
    openagc_graphics_command_buffer **out_command_buffer)
{
    openagc_graphics_command_buffer *command_buffer;

    if (out_command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GRAPHICS_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->max_commands == 0u ||
        desc->max_commands > OPENAGC_GRAPHICS_MAX_COMMANDS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (device->graphics_command_buffer_count >= OPENAGC_GRAPHICS_MAX_COMMAND_BUFFERS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command_buffer = (openagc_graphics_command_buffer *)calloc(1u, sizeof(*command_buffer));
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    command_buffer->commands = (openagc_graphics_command *)calloc(
        (size_t)desc->max_commands, sizeof(*command_buffer->commands));
    command_buffer->shadows = (openagc_graphics_shadow *)calloc(
        (size_t)desc->max_commands, sizeof(*command_buffer->shadows));
    if (command_buffer->commands == NULL || command_buffer->shadows == NULL) {
        free(command_buffer->shadows);
        free(command_buffer->commands);
        free(command_buffer);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    command_buffer->device = device;
    command_buffer->max_commands = desc->max_commands;
    device->graphics_command_buffer_count++;
    *out_command_buffer = command_buffer;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_begin(
    openagc_graphics_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_INITIAL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->state = OPENAGC_GRAPHICS_RECORDING;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_transition(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image,
    const openagc_graphics_transition_desc *desc)
{
    openagc_graphics_image_state current_state;
    openagc_graphics_owner current_owner;
    openagc_graphics_shadow *shadow;
    openagc_graphics_command *command;
    openagc_result result;

    if (command_buffer == NULL || image == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->device != command_buffer->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (image->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_graphics_supported_state(desc->before_state, desc->before_owner) ||
        !openagc_graphics_supported_state(desc->after_state, desc->after_owner)) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->before_state == desc->after_state &&
        desc->before_owner == desc->after_owner) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((openagc_graphics_required_usage(desc->after_state) & ~image->usage) != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    openagc_graphics_current_state(command_buffer, image, &current_state, &current_owner);
    if (current_state != desc->before_state || current_owner != desc->before_owner) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    result = openagc_graphics_retain_image(command_buffer, image, &shadow);
    if (result != OPENAGC_OK) {
        return result;
    }
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_TRANSITION;
    command->image_id = image->image_id;
    command->data.transition.before_state = desc->before_state;
    command->data.transition.before_owner = desc->before_owner;
    command->data.transition.after_state = desc->after_state;
    command->data.transition.after_owner = desc->after_owner;
    shadow->recorded_state = desc->after_state;
    shadow->recorded_owner = desc->after_owner;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_bind_color_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_shadow *shadow;
    openagc_graphics_command *command;
    openagc_result result;

    if (command_buffer == NULL || image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->device != command_buffer->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((image->usage & OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->memory == NULL || command_buffer->target != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_graphics_current_state(command_buffer, image, &state, &owner);
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    result = openagc_graphics_retain_image(command_buffer, image, &shadow);
    if (result != OPENAGC_OK) {
        return result;
    }
    (void)shadow;
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_BIND_COLOR_TARGET;
    command->image_id = image->image_id;
    command_buffer->target = image;
    command_buffer->scissor_valid = 0u;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_bind_depth_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_shadow *shadow;
    openagc_graphics_command *command;
    openagc_result result;

    if (command_buffer == NULL || image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->device != command_buffer->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (image->format != OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ||
        (image->usage & OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->memory == NULL || command_buffer->depth_target != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_graphics_current_state(command_buffer, image, &state, &owner);
    if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    result = openagc_graphics_retain_image(command_buffer, image, &shadow);
    if (result != OPENAGC_OK) {
        return result;
    }
    (void)shadow;
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_BIND_DEPTH_TARGET;
    command->image_id = image->image_id;
    command_buffer->depth_target = image;
    command_buffer->scissor_valid = 0u;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_set_scissor(
    openagc_graphics_command_buffer *command_buffer, const openagc_graphics_scissor *scissor)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_command *command;

    if (command_buffer == NULL || scissor == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING ||
        (command_buffer->target == NULL && command_buffer->depth_target == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->target != NULL) {
        openagc_graphics_current_state(command_buffer, command_buffer->target, &state, &owner);
        if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
            owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (scissor->width == 0u || scissor->height == 0u ||
            scissor->x > command_buffer->target->width ||
            scissor->y > command_buffer->target->height ||
            scissor->width > command_buffer->target->width - scissor->x ||
            scissor->height > command_buffer->target->height - scissor->y) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    if (command_buffer->depth_target != NULL) {
        openagc_graphics_current_state(command_buffer, command_buffer->depth_target, &state,
                                       &owner);
        if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
            owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (scissor->width == 0u || scissor->height == 0u ||
            scissor->x > command_buffer->depth_target->width ||
            scissor->y > command_buffer->depth_target->height ||
            scissor->width > command_buffer->depth_target->width - scissor->x ||
            scissor->height > command_buffer->depth_target->height - scissor->y) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_SET_SCISSOR;
    command->image_id = command_buffer->target != NULL ? command_buffer->target->image_id
                                                       : command_buffer->depth_target->image_id;
    command->data.scissor = *scissor;
    command_buffer->scissor_valid = 1u;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_clear_color(
    openagc_graphics_command_buffer *command_buffer, openagc_color color)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_command *command;

    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING ||
        command_buffer->target == NULL || command_buffer->scissor_valid == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_graphics_current_state(command_buffer, command_buffer->target, &state, &owner);
    if (state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_CLEAR_COLOR;
    command->image_id = command_buffer->target->image_id;
    command->data.clear = color;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_clear_depth(
    openagc_graphics_command_buffer *command_buffer, uint32_t depth24, uint32_t stencil)
{
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    openagc_graphics_command *command;

    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (depth24 > 16777215u || stencil > 255u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING ||
        command_buffer->depth_target == NULL || command_buffer->scissor_valid == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_graphics_current_state(command_buffer, command_buffer->depth_target, &state, &owner);
    if (state != OPENAGC_GRAPHICS_STATE_DEPTH_TARGET ||
        owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_CLEAR_DEPTH;
    command->image_id = command_buffer->depth_target->image_id;
    command->data.depth.depth24 = depth24;
    command->data.depth.stencil = stencil;
    command_buffer->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_end(
    openagc_graphics_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (command_buffer->command_count == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->state = OPENAGC_GRAPHICS_EXECUTABLE;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_apply_host_state(
    openagc_graphics_command_buffer *command_buffer)
{
    uint32_t i;

    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_EXECUTABLE) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    for (i = 0u; i < command_buffer->shadow_count; ++i) {
        const openagc_graphics_shadow *shadow = &command_buffer->shadows[i];
        if (shadow->image->state_revision != shadow->initial_revision ||
            shadow->image->state != shadow->initial_state ||
            shadow->image->owner != shadow->initial_owner) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (shadow->image->state_revision == UINT64_MAX) {
            return OPENAGC_ERROR_OVERFLOW;
        }
    }
    for (i = 0u; i < command_buffer->shadow_count; ++i) {
        openagc_graphics_shadow *shadow = &command_buffer->shadows[i];
        shadow->image->state = shadow->recorded_state;
        shadow->image->owner = shadow->recorded_owner;
        shadow->image->state_revision++;
        shadow->applied_revision = shadow->image->state_revision;
    }
    command_buffer->state = OPENAGC_GRAPHICS_HOST_APPLIED;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_execute_host(
    openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_execution_info *info)
{
    openagc_graphics_scissor scissor;
    uint64_t cleared_pixels = 0u;
    uint32_t clear_count = 0u;
    uint32_t i;

    if (command_buffer == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_HOST_APPLIED ||
        (command_buffer->target == NULL && command_buffer->depth_target == NULL)) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    for (i = 0u; i < command_buffer->shadow_count; ++i) {
        const openagc_graphics_shadow *shadow = &command_buffer->shadows[i];

        if (shadow->image->state_revision != shadow->applied_revision ||
            shadow->image->state != shadow->recorded_state ||
            shadow->image->owner != shadow->recorded_owner) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        if (shadow->image->memory == NULL) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    for (i = 0u; i < command_buffer->command_count; ++i) {
        if (command_buffer->commands[i].type == OPENAGC_GRAPHICS_COMMAND_CLEAR_COLOR ||
            command_buffer->commands[i].type == OPENAGC_GRAPHICS_COMMAND_CLEAR_DEPTH) {
            clear_count++;
        }
    }
    if (clear_count == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }

    memset(&scissor, 0, sizeof(scissor));
    for (i = 0u; i < command_buffer->command_count; ++i) {
        const openagc_graphics_command *command = &command_buffer->commands[i];

        if (command->type == OPENAGC_GRAPHICS_COMMAND_SET_SCISSOR) {
            scissor = command->data.scissor;
        } else if (command->type == OPENAGC_GRAPHICS_COMMAND_CLEAR_COLOR) {
            if (openagc_graphics_clear_uses_write_data(command_buffer->target, &scissor) != 0) {
                openagc_result clear_result = openagc_graphics_clear_scissor_write_data(
                    command_buffer->target, &scissor, command->data.clear);

                if (clear_result != OPENAGC_OK) {
                    return clear_result;
                }
            } else {
                openagc_graphics_fill_scissor(command_buffer->target, &scissor,
                                              command->data.clear);
            }
            cleared_pixels += (uint64_t)scissor.width * scissor.height;
        } else if (command->type == OPENAGC_GRAPHICS_COMMAND_CLEAR_DEPTH) {
            if (openagc_graphics_depth_clear_uses_write_data(command_buffer->depth_target,
                                                             &scissor) != 0) {
                openagc_result clear_result = openagc_graphics_clear_depth_scissor_write_data(
                    command_buffer->depth_target, &scissor, command->data.depth.depth24,
                    command->data.depth.stencil);

                if (clear_result != OPENAGC_OK) {
                    return clear_result;
                }
            } else {
                openagc_graphics_fill_depth_scissor(command_buffer->depth_target, &scissor,
                                                    command->data.depth.depth24,
                                                    command->data.depth.stencil);
            }
            cleared_pixels += (uint64_t)scissor.width * scissor.height;
        }
    }
    command_buffer->state = OPENAGC_GRAPHICS_HOST_EXECUTED;
    info->target_image_id = command_buffer->target != NULL
                                ? command_buffer->target->image_id
                                : command_buffer->depth_target->image_id;
    info->clear_count = clear_count;
    info->gpu_submitted = 0u;
    info->cleared_pixels = cleared_pixels;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_get_recording(
    const openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_recording_view *view)
{
    if (command_buffer == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (view->struct_size != sizeof(*view)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (command_buffer->state != OPENAGC_GRAPHICS_EXECUTABLE &&
        command_buffer->state != OPENAGC_GRAPHICS_HOST_APPLIED &&
        command_buffer->state != OPENAGC_GRAPHICS_HOST_EXECUTED) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    view->command_count = command_buffer->command_count;
    view->host_state_applied =
        (command_buffer->state == OPENAGC_GRAPHICS_HOST_APPLIED ||
         command_buffer->state == OPENAGC_GRAPHICS_HOST_EXECUTED)
            ? 1u
            : 0u;
    view->gpu_submitted = 0u;
    view->commands = command_buffer->commands;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_reset(
    openagc_graphics_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state == OPENAGC_GRAPHICS_INITIAL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_graphics_release_images(command_buffer);
    command_buffer->state = OPENAGC_GRAPHICS_INITIAL;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_command_buffer_destroy(
    openagc_graphics_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    openagc_graphics_release_images(command_buffer);
    command_buffer->device->graphics_command_buffer_count--;
    free(command_buffer->shadows);
    free(command_buffer->commands);
    free(command_buffer);
    return OPENAGC_OK;
}

openagc_result openagc_graphics_shader_target_validate(
    const openagc_gpu_device *device, const openagc_graphics_image *image,
    openagc_graphics_format format, uint32_t *image_id)
{
    if (image == NULL || image_id == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((image->usage & OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->memory == NULL ||
        image->state != OPENAGC_GRAPHICS_STATE_COLOR_TARGET ||
        image->owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->format != format) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->pipeline_references == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    *image_id = image->image_id;
    return OPENAGC_OK;
}

openagc_result openagc_graphics_shader_texture_validate(
    const openagc_gpu_device *device, const openagc_graphics_image *image,
    openagc_graphics_format format)
{
    if (image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (image->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if ((image->usage & OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->memory == NULL ||
        image->state != OPENAGC_GRAPHICS_STATE_SHADER_READ ||
        image->owner != OPENAGC_GRAPHICS_OWNER_GRAPHICS) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (image->format != format) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (image->pipeline_references == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    return OPENAGC_OK;
}

void openagc_graphics_shader_image_retain(openagc_graphics_image *image)
{
    image->pipeline_references++;
}

void openagc_graphics_shader_image_release(openagc_graphics_image *image)
{
    image->pipeline_references--;
}

openagc_result openagc_graphics_image_copy_access(
    const openagc_gpu_device *device, const openagc_gpu_memory *memory,
    uint64_t offset, uint64_t size_bytes, openagc_graphics_image_state required)
{
    const openagc_graphics_image *image;

    for (image = device->images; image != NULL; image = image->next) {
        if (image->memory != memory) {
            continue;
        }
        if (image->memory_offset + image->footprint_bytes <= offset ||
            offset + size_bytes <= image->memory_offset) {
            continue;
        }
        if (image->state != required ||
            image->owner != OPENAGC_GRAPHICS_OWNER_COPY) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return OPENAGC_OK;
}
