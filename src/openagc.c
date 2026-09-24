/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <float.h>
#else
#include <math.h>
#endif

#define OPENAGC_MAX_WIDTH 8192u
#define OPENAGC_MAX_HEIGHT 8192u
#define OPENAGC_MAX_PIXELS 16777216u
#define OPENAGC_MAX_COMMANDS 4096u

typedef enum openagc_frame_state {
    OPENAGC_FRAME_IDLE,
    OPENAGC_FRAME_RECORDING,
    OPENAGC_FRAME_COMPLETED
} openagc_frame_state;

struct openagc_device {
    openagc_context *context;
    openagc_command *commands;
    uint32_t width;
    uint32_t height;
    uint32_t max_commands;
    uint32_t command_count;
    uint64_t frame_number;
    openagc_frame_state state;
};

static int openagc_is_finite(float value)
{
#if defined(_MSC_VER)
    return _finite((double)value) != 0;
#else
    return isfinite(value) != 0;
#endif
}

static int openagc_product_fits(size_t left, size_t right)
{
    return right == 0u || left <= (size_t)-1 / right;
}

static openagc_result openagc_ps5_firmware_gate(const openagc_context_desc *desc)
{
    /* No firmware has been qualified or probed through a trusted source. */
    (void)desc;
    return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
}

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
    openagc_context *context;

    if (out_context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    if (desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->backend == OPENAGC_BACKEND_PS5) {
        return openagc_ps5_firmware_gate(desc);
    }
    if (desc->backend != OPENAGC_BACKEND_HOST_REFERENCE) {
        return OPENAGC_ERROR_UNSUPPORTED_BACKEND;
    }
    if (desc->firmware_major != 0u || desc->firmware_minor != 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }

    context = (openagc_context *)calloc(1u, sizeof(*context));
    if (context == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    context->backend = desc->backend;
    *out_context = context;
    return OPENAGC_OK;
}

openagc_result openagc_context_destroy(openagc_context *context)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (context->device_count != 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    free(context);
    return OPENAGC_OK;
}

openagc_result openagc_device_create(openagc_context *context,
                                     const openagc_device_desc *desc,
                                     openagc_device **out_device)
{
    openagc_device *device;

    if (out_device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = NULL;
    if (context == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (context->backend != OPENAGC_BACKEND_HOST_REFERENCE) {
        return OPENAGC_ERROR_UNSUPPORTED_BACKEND;
    }
    if (desc->width == 0u || desc->width > OPENAGC_MAX_WIDTH ||
        desc->height == 0u || desc->height > OPENAGC_MAX_HEIGHT ||
        desc->max_commands == 0u || desc->max_commands > OPENAGC_MAX_COMMANDS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (!openagc_product_fits((size_t)desc->width, (size_t)desc->height) ||
        !openagc_product_fits((size_t)desc->max_commands, sizeof(openagc_command)) ||
        context->device_count == (size_t)-1) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    if ((size_t)desc->width * (size_t)desc->height > OPENAGC_MAX_PIXELS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    device = (openagc_device *)calloc(1u, sizeof(*device));
    if (device == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    device->commands = (openagc_command *)calloc((size_t)desc->max_commands,
                                                  sizeof(openagc_command));
    if (device->commands == NULL) {
        free(device);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    device->context = context;
    device->width = desc->width;
    device->height = desc->height;
    device->max_commands = desc->max_commands;
    context->device_count++;
    *out_device = device;
    return OPENAGC_OK;
}

openagc_result openagc_device_destroy(openagc_device *device)
{
    openagc_context *context;

    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    context = device->context;
    free(device->commands);
    free(device);
    context->device_count--;
    return OPENAGC_OK;
}

openagc_result openagc_device_get_capabilities(const openagc_device *device,
                                               openagc_capabilities *capabilities)
{
    if (device == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->backend = OPENAGC_BACKEND_HOST_REFERENCE;
    capabilities->gpu_execution = 0u;
    capabilities->video_output = 0u;
    capabilities->records_commands = 1u;
    capabilities->max_width = OPENAGC_MAX_WIDTH;
    capabilities->max_height = OPENAGC_MAX_HEIGHT;
    capabilities->max_pixels = OPENAGC_MAX_PIXELS;
    capabilities->max_commands = OPENAGC_MAX_COMMANDS;
    return OPENAGC_OK;
}

openagc_result openagc_frame_begin(openagc_device *device)
{
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->state == OPENAGC_FRAME_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    device->command_count = 0u;
    device->state = OPENAGC_FRAME_RECORDING;
    return OPENAGC_OK;
}

openagc_result openagc_frame_clear(openagc_device *device, openagc_color color)
{
    openagc_command *command;

    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->state != OPENAGC_FRAME_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (device->command_count == device->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &device->commands[device->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_COMMAND_CLEAR;
    command->data.clear = color;
    device->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_frame_rect(openagc_device *device, const openagc_rect *rect)
{
    openagc_command *command;

    if (device == NULL || rect == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->state != OPENAGC_FRAME_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (!openagc_is_finite(rect->x) || !openagc_is_finite(rect->y) ||
        !openagc_is_finite(rect->width) || !openagc_is_finite(rect->height) ||
        rect->x < 0.0f || rect->y < 0.0f ||
        rect->width <= 0.0f || rect->height <= 0.0f ||
        rect->x > (float)device->width || rect->y > (float)device->height ||
        (double)rect->width > (double)device->width - (double)rect->x ||
        (double)rect->x > (double)device->width - (double)rect->width ||
        (double)rect->height > (double)device->height - (double)rect->y ||
        (double)rect->y > (double)device->height - (double)rect->height) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (device->command_count == device->max_commands) {
        return OPENAGC_ERROR_CAPACITY;
    }
    command = &device->commands[device->command_count];
    memset(command, 0, sizeof(*command));
    command->type = OPENAGC_COMMAND_RECTANGLE;
    command->data.rectangle.x = rect->x == 0.0f ? 0.0f : rect->x;
    command->data.rectangle.y = rect->y == 0.0f ? 0.0f : rect->y;
    command->data.rectangle.width = rect->width;
    command->data.rectangle.height = rect->height;
    command->data.rectangle.color = rect->color;
    device->command_count++;
    return OPENAGC_OK;
}

openagc_result openagc_frame_present(openagc_device *device)
{
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->state != OPENAGC_FRAME_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (device->frame_number == UINT64_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    device->frame_number++;
    device->state = OPENAGC_FRAME_COMPLETED;
    return OPENAGC_OK;
}

openagc_result openagc_device_get_last_frame(const openagc_device *device,
                                             openagc_frame_view *view)
{
    if (device == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (view->struct_size != sizeof(*view)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (device->state != OPENAGC_FRAME_COMPLETED) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    view->width = device->width;
    view->height = device->height;
    view->command_count = device->command_count;
    view->frame_number = device->frame_number;
    view->commands = device->commands;
    return OPENAGC_OK;
}
