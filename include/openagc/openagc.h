/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_OPENAGC_H
#define OPENAGC_OPENAGC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_API_VERSION 1u

typedef struct openagc_context openagc_context;
typedef struct openagc_device openagc_device;

/* Fixed-width values keep the public ABI independent of enum representation. */
typedef uint32_t openagc_result;
enum {
    OPENAGC_OK = 0u,
    OPENAGC_ERROR_INVALID_ARGUMENT = 1u,
    OPENAGC_ERROR_INCOMPATIBLE_VERSION = 2u,
    OPENAGC_ERROR_UNSUPPORTED_BACKEND = 3u,
    OPENAGC_ERROR_UNSUPPORTED_FIRMWARE = 4u,
    OPENAGC_ERROR_OUT_OF_RANGE = 5u,
    OPENAGC_ERROR_BAD_STATE = 6u,
    OPENAGC_ERROR_CAPACITY = 7u,
    OPENAGC_ERROR_OUT_OF_MEMORY = 8u,
    OPENAGC_ERROR_OVERFLOW = 9u,
    OPENAGC_ERROR_BUSY = 10u,
    OPENAGC_ERROR_OWNERSHIP = 11u,
    OPENAGC_ERROR_NOT_READY = 12u,
    OPENAGC_ERROR_UNSUPPORTED_OPERATION = 13u,
    OPENAGC_ERROR_INTEGRITY = 14u
};

typedef uint32_t openagc_backend;
enum {
    OPENAGC_BACKEND_HOST_REFERENCE = 1u,
    OPENAGC_BACKEND_PS5 = 2u
};

typedef uint32_t openagc_command_type;
enum {
    OPENAGC_COMMAND_CLEAR = 1u,
    OPENAGC_COMMAND_RECTANGLE = 2u
};

typedef struct openagc_context_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_backend backend;
    /* Untrusted diagnostic hints; these never authorize PS5 hardware access. */
    uint32_t firmware_major;
    uint32_t firmware_minor;
} openagc_context_desc;

typedef struct openagc_device_desc {
    uint32_t struct_size;
    uint32_t width;
    uint32_t height;
    uint32_t max_commands;
} openagc_device_desc;

typedef struct openagc_capabilities {
    uint32_t struct_size;
    openagc_backend backend;
    uint32_t gpu_execution;
    uint32_t video_output;
    uint32_t records_commands;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_pixels;
    uint32_t max_commands;
} openagc_capabilities;

typedef struct openagc_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} openagc_color;

typedef struct openagc_rect {
    float x;
    float y;
    float width;
    float height;
    openagc_color color;
} openagc_rect;

typedef union openagc_command_data {
    openagc_color clear;
    openagc_rect rectangle;
} openagc_command_data;

typedef struct openagc_command {
    openagc_command_type type;
    openagc_command_data data;
} openagc_command;

typedef struct openagc_frame_view {
    uint32_t struct_size;
    uint32_t width;
    uint32_t height;
    uint32_t command_count;
    uint64_t frame_number;
    /* Valid only until the next successful frame_begin or device_destroy. */
    const openagc_command *commands;
} openagc_frame_view;

#define OPENAGC_CONTEXT_DESC_INIT(backend_value) \
    { (uint32_t)sizeof(openagc_context_desc), OPENAGC_API_VERSION, (backend_value), 0u, 0u }
#define OPENAGC_DEVICE_DESC_INIT(width_value, height_value, capacity_value) \
    { (uint32_t)sizeof(openagc_device_desc), (width_value), (height_value), (capacity_value) }
#define OPENAGC_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_capabilities), 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_FRAME_VIEW_INIT \
    { (uint32_t)sizeof(openagc_frame_view), 0u, 0u, 0u, 0u, (const openagc_command *)0 }

uint32_t openagc_api_version(void);
const char *openagc_result_string(openagc_result result);

/* Versioned structs require struct_size == sizeof(struct) for this API version. */
openagc_result openagc_context_create(const openagc_context_desc *desc,
                                      openagc_context **out_context);
/* Returns BAD_STATE while any devices belonging to the context are alive. */
openagc_result openagc_context_destroy(openagc_context *context);
openagc_result openagc_device_create(openagc_context *context,
                                     const openagc_device_desc *desc,
                                     openagc_device **out_device);
openagc_result openagc_device_destroy(openagc_device *device);
/* Set capabilities->struct_size before calling. */
openagc_result openagc_device_get_capabilities(const openagc_device *device,
                                               openagc_capabilities *capabilities);

openagc_result openagc_frame_begin(openagc_device *device);
openagc_result openagc_frame_clear(openagc_device *device, openagc_color color);
openagc_result openagc_frame_rect(openagc_device *device, const openagc_rect *rect);
/* Host present finalizes a recording; it does not display or submit pixels. */
openagc_result openagc_frame_present(openagc_device *device);
/* Set view->struct_size before calling; a completed frame must exist. */
openagc_result openagc_device_get_last_frame(const openagc_device *device,
                                             openagc_frame_view *view);

#ifdef __cplusplus
}
#endif

#endif
