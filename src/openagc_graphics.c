/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/graphics.h"
#include "openagc_gpu_internal.h"
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
    OPENAGC_GRAPHICS_HOST_APPLIED
} openagc_graphics_recording_state;

typedef struct openagc_graphics_shadow {
    openagc_graphics_image *image;
    uint64_t initial_revision;
    openagc_graphics_image_state initial_state;
    openagc_graphics_owner initial_owner;
    openagc_graphics_image_state recorded_state;
    openagc_graphics_owner recorded_owner;
} openagc_graphics_shadow;

struct openagc_graphics_image {
    openagc_gpu_device *device;
    openagc_gpu_memory *memory;
    uint64_t footprint_bytes;
    uint64_t memory_offset;
    uint32_t image_id;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    openagc_graphics_format format;
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
    command_buffer->scissor_valid = 0u;
}

static int openagc_graphics_supported_state(openagc_graphics_image_state state,
                                            openagc_graphics_owner owner)
{
    return (state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
            owner == OPENAGC_GRAPHICS_OWNER_HOST) ||
           (state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
            owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
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
    capabilities->supported_format_mask =
        (1u << (OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM - 1u)) |
        (1u << (OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM - 1u));
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
        desc->format != OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->tiling != OPENAGC_GRAPHICS_TILING_HOST_LINEAR ||
        desc->usage != OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT ||
        desc->mip_levels != 1u || desc->array_layers != 1u ||
        desc->sample_count != 1u) {
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
    image->width = desc->width;
    image->height = desc->height;
    image->row_pitch_bytes = desc->row_pitch_bytes;
    image->footprint_bytes = footprint;
    image->image_id = ++device->next_graphics_image_id;
    image->state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    image->owner = OPENAGC_GRAPHICS_OWNER_HOST;
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
        command_buffer->target == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
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
    if (command_buffer->command_count == command_buffer->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &command_buffer->commands[command_buffer->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_GRAPHICS_COMMAND_SET_SCISSOR;
    command->image_id = command_buffer->target->image_id;
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
    }
    command_buffer->state = OPENAGC_GRAPHICS_HOST_APPLIED;
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
        command_buffer->state != OPENAGC_GRAPHICS_HOST_APPLIED) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    view->command_count = command_buffer->command_count;
    view->host_state_applied =
        command_buffer->state == OPENAGC_GRAPHICS_HOST_APPLIED ? 1u : 0u;
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

void openagc_graphics_shader_target_retain(openagc_graphics_image *image)
{
    image->pipeline_references++;
}

void openagc_graphics_shader_target_release(openagc_graphics_image *image)
{
    image->pipeline_references--;
}
