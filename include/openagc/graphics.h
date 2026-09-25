/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_GRAPHICS_H
#define OPENAGC_GRAPHICS_H

#include "openagc/driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_GRAPHICS_API_VERSION 1u

typedef struct openagc_graphics_image openagc_graphics_image;
typedef struct openagc_graphics_command_buffer openagc_graphics_command_buffer;

typedef uint32_t openagc_graphics_format;
enum {
    OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM = 1u,
    OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM = 2u,
    OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT = 3u
};

typedef uint32_t openagc_graphics_tiling;
enum {
    OPENAGC_GRAPHICS_TILING_HOST_LINEAR = 1u,
    OPENAGC_GRAPHICS_TILING_NATIVE_OPTIMAL = 2u
};

typedef uint32_t openagc_graphics_usage;
enum {
    OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT = 1u,
    /* Validated as a pipeline-plan texture input; nothing executes it. */
    OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT = 2u,
    OPENAGC_GRAPHICS_USAGE_SCANOUT_BIT = 4u,
    /* Stored as D24S8. No depth test runs. */
    OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT = 8u
};

typedef uint32_t openagc_graphics_image_state;
enum {
    OPENAGC_GRAPHICS_STATE_UNDEFINED = 0u,
    OPENAGC_GRAPHICS_STATE_COLOR_TARGET = 1u,
    OPENAGC_GRAPHICS_STATE_HOST_READ = 2u,
    OPENAGC_GRAPHICS_STATE_PRESENT = 3u,
    /* The copy queue may read this state and refuses to write it. */
    OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE = 4u,
    /* The copy queue may write this state and refuses to read it. */
    OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION = 5u,
    /* The graphics owner holds it for sampling; no copy may touch it. */
    OPENAGC_GRAPHICS_STATE_SHADER_READ = 6u,
    /* Graphics-owned depth attachment. No depth test runs. */
    OPENAGC_GRAPHICS_STATE_DEPTH_TARGET = 7u
};

typedef uint32_t openagc_graphics_owner;
enum {
    OPENAGC_GRAPHICS_OWNER_HOST = 0u,
    OPENAGC_GRAPHICS_OWNER_GRAPHICS = 1u,
    OPENAGC_GRAPHICS_OWNER_COPY = 2u
};

typedef uint32_t openagc_graphics_command_type;
enum {
    OPENAGC_GRAPHICS_COMMAND_TRANSITION = 1u,
    OPENAGC_GRAPHICS_COMMAND_BIND_COLOR_TARGET = 2u,
    OPENAGC_GRAPHICS_COMMAND_SET_SCISSOR = 3u,
    OPENAGC_GRAPHICS_COMMAND_CLEAR_COLOR = 4u,
    OPENAGC_GRAPHICS_COMMAND_BIND_DEPTH_TARGET = 5u,
    OPENAGC_GRAPHICS_COMMAND_CLEAR_DEPTH = 6u
};

typedef struct openagc_graphics_capabilities {
    uint32_t struct_size;
    openagc_backend backend;
    uint32_t gpu_execution;
    uint32_t rasterization;
    uint32_t video_output;
    uint32_t host_state_recording;
    uint32_t host_clear_simulation;
    uint32_t supported_format_mask;
    uint32_t supported_usage_mask;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_image_bytes;
    uint32_t max_images;
    uint32_t max_command_buffers;
    uint32_t max_commands_per_buffer;
    uint32_t row_pitch_alignment;
} openagc_graphics_capabilities;

typedef struct openagc_graphics_image_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_graphics_format format;
    openagc_graphics_tiling tiling;
    openagc_graphics_usage usage;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    uint32_t mip_levels;
    uint32_t array_layers;
    uint32_t sample_count;
} openagc_graphics_image_desc;

typedef struct openagc_graphics_image_info {
    uint32_t struct_size;
    uint32_t image_id;
    openagc_graphics_format format;
    openagc_graphics_usage usage;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    uint64_t footprint_bytes;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    uint32_t bound;
} openagc_graphics_image_info;

typedef struct openagc_graphics_command_buffer_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t max_commands;
} openagc_graphics_command_buffer_desc;

typedef struct openagc_graphics_transition_desc {
    uint32_t struct_size;
    openagc_graphics_image_state before_state;
    openagc_graphics_owner before_owner;
    openagc_graphics_image_state after_state;
    openagc_graphics_owner after_owner;
} openagc_graphics_transition_desc;

typedef struct openagc_graphics_scissor {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} openagc_graphics_scissor;

typedef struct openagc_graphics_transition_record {
    openagc_graphics_image_state before_state;
    openagc_graphics_image_state after_state;
    openagc_graphics_owner before_owner;
    openagc_graphics_owner after_owner;
} openagc_graphics_transition_record;

typedef struct openagc_graphics_depth_clear {
    /* Low 24 bits are the UNORM depth. Stencil is a separate 0..255 value. */
    uint32_t depth24;
    uint32_t stencil;
} openagc_graphics_depth_clear;

typedef union openagc_graphics_command_data {
    openagc_graphics_transition_record transition;
    openagc_graphics_scissor scissor;
    openagc_color clear;
    openagc_graphics_depth_clear depth;
} openagc_graphics_command_data;

typedef struct openagc_graphics_command {
    openagc_graphics_command_type type;
    uint32_t image_id;
    openagc_graphics_command_data data;
} openagc_graphics_command;

typedef struct openagc_graphics_recording_view {
    uint32_t struct_size;
    uint32_t command_count;
    uint32_t host_state_applied;
    uint32_t gpu_submitted;
    /* Pointer-free commands; valid until successful reset or destruction. */
    const openagc_graphics_command *commands;
} openagc_graphics_recording_view;

typedef struct openagc_graphics_execution_info {
    uint32_t struct_size;
    uint32_t target_image_id;
    uint32_t clear_count;
    uint32_t gpu_submitted;
    uint64_t cleared_pixels;
} openagc_graphics_execution_info;

#define OPENAGC_GRAPHICS_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_graphics_capabilities), 0u, 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_GRAPHICS_IMAGE_DESC_INIT(width_value, height_value, pitch_value, format_value) \
    { (uint32_t)sizeof(openagc_graphics_image_desc), OPENAGC_GRAPHICS_API_VERSION, \
      (format_value), OPENAGC_GRAPHICS_TILING_HOST_LINEAR, \
      OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT, (width_value), (height_value), \
      (pitch_value), 1u, 1u, 1u }
#define OPENAGC_GRAPHICS_IMAGE_INFO_INIT \
    { (uint32_t)sizeof(openagc_graphics_image_info), 0u, 0u, 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u }
#define OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(capacity_value) \
    { (uint32_t)sizeof(openagc_graphics_command_buffer_desc), \
      OPENAGC_GRAPHICS_API_VERSION, (capacity_value) }
#define OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(before_value, before_owner_value, \
                                              after_value, after_owner_value) \
    { (uint32_t)sizeof(openagc_graphics_transition_desc), (before_value), \
      (before_owner_value), (after_value), (after_owner_value) }
#define OPENAGC_GRAPHICS_RECORDING_VIEW_INIT \
    { (uint32_t)sizeof(openagc_graphics_recording_view), 0u, 0u, 0u, \
      (const openagc_graphics_command *)0 }
#define OPENAGC_GRAPHICS_EXECUTION_INFO_INIT \
    { (uint32_t)sizeof(openagc_graphics_execution_info), 0u, 0u, 0u, 0u }

openagc_result openagc_graphics_get_capabilities(
    const openagc_gpu_device *device, openagc_graphics_capabilities *capabilities);

openagc_result openagc_graphics_image_create(openagc_gpu_device *device,
                                              const openagc_graphics_image_desc *desc,
                                              openagc_graphics_image **out_image);
openagc_result openagc_graphics_image_bind_memory(openagc_graphics_image *image,
                                                   openagc_gpu_memory *memory,
                                                   uint64_t memory_offset);
openagc_result openagc_graphics_image_get_info(const openagc_graphics_image *image,
                                                openagc_graphics_image_info *info);
openagc_result openagc_graphics_image_destroy(openagc_graphics_image *image);

openagc_result openagc_graphics_command_buffer_create(
    openagc_gpu_device *device, const openagc_graphics_command_buffer_desc *desc,
    openagc_graphics_command_buffer **out_command_buffer);
openagc_result openagc_graphics_command_buffer_begin(
    openagc_graphics_command_buffer *command_buffer);
openagc_result openagc_graphics_command_transition(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image,
    const openagc_graphics_transition_desc *desc);
openagc_result openagc_graphics_command_bind_color_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image);
/* D24S8 only. No depth test is recorded. */
openagc_result openagc_graphics_command_bind_depth_target(
    openagc_graphics_command_buffer *command_buffer, openagc_graphics_image *image);
openagc_result openagc_graphics_command_set_scissor(
    openagc_graphics_command_buffer *command_buffer, const openagc_graphics_scissor *scissor);
openagc_result openagc_graphics_command_clear_color(
    openagc_graphics_command_buffer *command_buffer, openagc_color color);
/* depth24 is 0..16777215 and stencil is 0..255. The write is a CPU fill. */
openagc_result openagc_graphics_command_clear_depth(
    openagc_graphics_command_buffer *command_buffer, uint32_t depth24, uint32_t stencil);
openagc_result openagc_graphics_command_buffer_end(
    openagc_graphics_command_buffer *command_buffer);
/* Commits logical host metadata only: no pixels, PM4, GPU work, or queue submit. */
openagc_result openagc_graphics_command_buffer_apply_host_state(
    openagc_graphics_command_buffer *command_buffer);
/* CPU-clears the recorded scissors into bound host-linear image memory. */
openagc_result openagc_graphics_command_buffer_execute_host(
    openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_execution_info *info);
openagc_result openagc_graphics_command_buffer_get_recording(
    const openagc_graphics_command_buffer *command_buffer,
    openagc_graphics_recording_view *view);
openagc_result openagc_graphics_command_buffer_reset(
    openagc_graphics_command_buffer *command_buffer);
openagc_result openagc_graphics_command_buffer_destroy(
    openagc_graphics_command_buffer *command_buffer);

#ifdef __cplusplus
}
#endif

#endif
