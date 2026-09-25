/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/graphics.h"
#include "openagc/pm4_fw940.h"
#include "openagc/pm4_write_fw940.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define EXPECT(expression, expected) do { \
    openagc_result actual = (expression); \
    if (actual != (expected)) { \
        fprintf(stderr, "%s:%d: %s returned %s; expected %s\n", \
                __FILE__, __LINE__, #expression, openagc_result_string(actual), \
                openagc_result_string(expected)); \
        return 1; \
    } \
} while (0)

static int make_device(uint64_t budget, openagc_context **context,
                       openagc_gpu_device **device)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;

    device_desc.memory_budget_bytes = budget;
    EXPECT(openagc_context_create(&context_desc, context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(*context, &device_desc, device), OPENAGC_OK);
    return 0;
}

static int test_formats_footprint_and_binding(void)
{
    openagc_context *context = NULL;
    openagc_context *other_context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_device *other_device = NULL;
    openagc_graphics_capabilities caps = OPENAGC_GRAPHICS_CAPABILITIES_INIT;
    openagc_graphics_image_desc desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        64u, 32u, 256u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(8192u);
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_memory *other_memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_image *bgra = NULL;
    openagc_graphics_image *candidate = NULL;

    CHECK(make_device(16384u, &context, &device) == 0);
    CHECK(make_device(16384u, &other_context, &other_device) == 0);
    caps.struct_size--;
    EXPECT(openagc_graphics_get_capabilities(device, &caps),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    caps.struct_size++;
    EXPECT(openagc_graphics_get_capabilities(device, &caps), OPENAGC_OK);
    CHECK(caps.backend == OPENAGC_BACKEND_HOST_REFERENCE);
    CHECK(caps.gpu_execution == 0u && caps.rasterization == 0u);
    CHECK(caps.video_output == 0u && caps.host_state_recording == 1u);
    CHECK(caps.host_clear_simulation == 1u);
    CHECK(caps.supported_format_mask == 7u && caps.max_width == 4096u);
    CHECK(caps.supported_usage_mask == 11u);
    CHECK(caps.max_image_bytes == 16777216u && caps.row_pitch_alignment == 4u);

    desc.struct_size--;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.struct_size++;
    desc.api_version++;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.api_version = OPENAGC_GRAPHICS_API_VERSION;
    desc.format = OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
    desc.tiling = OPENAGC_GRAPHICS_TILING_NATIVE_OPTIMAL;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.tiling = OPENAGC_GRAPHICS_TILING_HOST_LINEAR;
    desc.usage = 0u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.usage = OPENAGC_GRAPHICS_USAGE_SCANOUT_BIT;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.usage = OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT |
                 OPENAGC_GRAPHICS_USAGE_SCANOUT_BIT;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.usage = OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT;
    desc.sample_count = 2u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.sample_count = 1u;
    desc.mip_levels = 2u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.mip_levels = 1u;
    desc.array_layers = 2u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.array_layers = 1u;
    desc.width = 0u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 4097u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 64u;
    desc.height = 0u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.height = 32u;
    desc.row_pitch_bytes = 252u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.row_pitch_bytes = 257u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.row_pitch_bytes = UINT32_MAX - 3u;
    EXPECT(openagc_graphics_image_create(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.row_pitch_bytes = 256u;
    CHECK(candidate == NULL);

    EXPECT(openagc_graphics_image_create(device, &desc, &image), OPENAGC_OK);
    info.struct_size--;
    EXPECT(openagc_graphics_image_get_info(image, &info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    info.struct_size++;
    EXPECT(openagc_graphics_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.image_id == 1u && info.format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    CHECK(info.usage == OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT);
    CHECK(info.width == 64u && info.height == 32u && info.row_pitch_bytes == 256u);
    CHECK(info.footprint_bytes == 8192u && info.bound == 0u);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED);
    CHECK(info.owner == OPENAGC_GRAPHICS_OWNER_HOST);

    desc.width = 2u;
    desc.height = 2u;
    desc.row_pitch_bytes = 8u;
    desc.format = OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM;
    EXPECT(openagc_graphics_image_create(device, &desc, &bgra), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(bgra, &info), OPENAGC_OK);
    CHECK(info.image_id == 2u && info.footprint_bytes == 16u);
    EXPECT(openagc_graphics_image_destroy(bgra), OPENAGC_OK);

    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(other_device, &memory_desc, &other_memory),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, other_memory, 0u),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 1u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_graphics_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.bound == 1u);

    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(other_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(other_device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    EXPECT(openagc_context_destroy(other_context), OPENAGC_OK);
    return 0;
}

static int test_recording_order_capacity_and_no_pixels(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(128u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        8u, 4u, 32u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_transition_desc acquire = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc release = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST);
    openagc_graphics_scissor scissor = { 0u, 0u, 8u, 4u };
    openagc_color color = { 10u, 20u, 30u, 255u };
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_graphics_recording_view view = OPENAGC_GRAPHICS_RECORDING_VIEW_INIT;
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_command_buffer *command_buffer = NULL;
    openagc_graphics_command expected[4];
    uint8_t fill[128];
    uint8_t readback[128];

    CHECK(make_device(4096u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u), OPENAGC_OK);
    command_desc.max_commands = 0u;
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_ERROR_OUT_OF_RANGE);
    command_desc.max_commands = 129u;
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_ERROR_OUT_OF_RANGE);
    command_desc.max_commands = 4u;
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_OK);
    memset(fill, 0x5a, sizeof(fill));
    EXPECT(openagc_gpu_memory_write(memory, 0u, fill, sizeof(fill)), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color),
           OPENAGC_ERROR_BAD_STATE);
    scissor.x = UINT32_MAX;
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor),
           OPENAGC_ERROR_OUT_OF_RANGE);
    scissor.x = 7u;
    scissor.width = 2u;
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor),
           OPENAGC_ERROR_OUT_OF_RANGE);
    scissor.x = 0u;
    scissor.width = 8u;
    scissor.height = 0u;
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor),
           OPENAGC_ERROR_OUT_OF_RANGE);
    scissor.height = 4u;
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color),
           OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &release),
           OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_OK);
    view.struct_size--;
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    view.struct_size++;
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_OK);
    CHECK(view.command_count == 4u && view.gpu_submitted == 0u);
    CHECK(view.host_state_applied == 0u);
    CHECK(view.commands[0].type == OPENAGC_GRAPHICS_COMMAND_TRANSITION);
    CHECK(view.commands[1].type == OPENAGC_GRAPHICS_COMMAND_BIND_COLOR_TARGET);
    CHECK(view.commands[2].type == OPENAGC_GRAPHICS_COMMAND_SET_SCISSOR);
    CHECK(view.commands[3].type == OPENAGC_GRAPHICS_COMMAND_CLEAR_COLOR);
    CHECK(view.commands[0].image_id == 1u && view.commands[3].image_id == 1u);
    CHECK(view.commands[2].data.scissor.width == 8u);
    CHECK(view.commands[3].data.clear.r == 10u &&
          view.commands[3].data.clear.a == 255u);
    memcpy(expected, view.commands, sizeof(expected));
    EXPECT(openagc_graphics_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_graphics_command_buffer_apply_host_state(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_OK);
    CHECK(view.host_state_applied == 1u && view.gpu_submitted == 0u);
    EXPECT(openagc_graphics_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(fill, readback, sizeof(fill)) == 0);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command_buffer),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_begin(command_buffer),
           OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_graphics_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &release),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);

    EXPECT(openagc_graphics_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_OK);
    CHECK(memcmp(expected, view.commands, sizeof(expected)) == 0);
    EXPECT(openagc_graphics_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_ownership_stale_and_atomic_apply(void)
{
    openagc_context *context_a = NULL;
    openagc_context *context_b = NULL;
    openagc_gpu_device *device_a = NULL;
    openagc_gpu_device *device_b = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(2u);
    openagc_graphics_transition_desc acquire = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc release = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST);
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_gpu_memory *memory_a = NULL;
    openagc_gpu_memory *memory_a2 = NULL;
    openagc_gpu_memory *memory_b = NULL;
    openagc_graphics_image *image_a = NULL;
    openagc_graphics_image *image_a2 = NULL;
    openagc_graphics_image *image_b = NULL;
    openagc_graphics_image *unbound = NULL;
    openagc_graphics_command_buffer *first = NULL;
    openagc_graphics_command_buffer *second = NULL;

    CHECK(make_device(256u, &context_a, &device_a) == 0);
    CHECK(make_device(128u, &context_b, &device_b) == 0);
    EXPECT(openagc_gpu_memory_allocate(device_a, &memory_desc, &memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device_a, &memory_desc, &memory_a2), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device_b, &memory_desc, &memory_b), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device_a, &image_desc, &image_a), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device_a, &image_desc, &image_a2), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device_b, &image_desc, &image_b), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device_a, &image_desc, &unbound), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image_a, memory_a, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image_a2, memory_a2, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image_b, memory_b, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device_a, &command_desc, &first),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device_a, &command_desc, &second),
           OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image_b, &acquire),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_graphics_command_bind_color_target(first, image_b),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_graphics_command_transition(first, unbound, &acquire),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_bind_color_target(first, unbound),
           OPENAGC_ERROR_BAD_STATE);
    acquire.after_owner = OPENAGC_GRAPHICS_OWNER_COPY;
    EXPECT(openagc_graphics_command_transition(first, image_a, &acquire),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    acquire.after_owner = OPENAGC_GRAPHICS_OWNER_GRAPHICS;
    acquire.after_state = OPENAGC_GRAPHICS_STATE_PRESENT;
    EXPECT(openagc_graphics_command_transition(first, image_a, &acquire),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    acquire.after_state = OPENAGC_GRAPHICS_STATE_COLOR_TARGET;
    acquire.after_state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    acquire.after_owner = OPENAGC_GRAPHICS_OWNER_HOST;
    EXPECT(openagc_graphics_command_transition(first, image_a, &acquire),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    acquire.after_state = OPENAGC_GRAPHICS_STATE_COLOR_TARGET;
    acquire.after_owner = OPENAGC_GRAPHICS_OWNER_GRAPHICS;
    EXPECT(openagc_graphics_command_transition(first, image_a, &acquire),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image_a2, &acquire),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(first), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image_a2, &acquire),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(first),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_image_get_info(image_a, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED);
    EXPECT(openagc_graphics_image_get_info(image_a2, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);
    EXPECT(openagc_graphics_command_buffer_reset(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_reset(second), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image_a, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image_a, &release), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image_a, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(first), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(image_a, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(second),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_image_destroy(image_a), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_graphics_command_buffer_destroy(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_destroy(second), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image_a), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image_a2), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image_b), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(unbound), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_a2), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_b), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_a), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_b), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_a), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_b), OPENAGC_OK);
    return 0;
}

static int test_usage_mask_and_sampled_state(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(128u);
    openagc_graphics_image_desc sampled_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_image_desc both_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_image_desc target_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(8u);
    openagc_graphics_transition_desc to_shader_read = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_SHADER_READ, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc to_target = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc target_to_shader_read =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
            OPENAGC_GRAPHICS_STATE_SHADER_READ, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc shader_read_to_destination =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_SHADER_READ, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
            OPENAGC_GRAPHICS_OWNER_COPY);
    openagc_graphics_transition_desc destination_to_shader_read =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
            OPENAGC_GRAPHICS_OWNER_COPY,
            OPENAGC_GRAPHICS_STATE_SHADER_READ, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_gpu_buffer_desc buffer_desc = OPENAGC_GPU_BUFFER_DESC_INIT(
        16u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                 OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_command_buffer_desc copy_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(1u, 14u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_graphics_image_info image_info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *sampled = NULL;
    openagc_graphics_image *both = NULL;
    openagc_graphics_image *target_only = NULL;
    openagc_gpu_buffer *image_buffer = NULL;
    openagc_gpu_buffer *staging_buffer = NULL;
    openagc_gpu_command_buffer *copy = NULL;
    openagc_graphics_command_buffer *command = NULL;
    openagc_gpu_queue *queue = NULL;
    openagc_gpu_fence *fence = NULL;
    uint8_t staging[16];
    uint8_t readback[64];
    uint32_t i;

    CHECK(make_device(4096u, &context, &device) == 0);
    sampled_desc.usage = OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
    both_desc.usage = OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT |
                      OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &sampled_desc, &sampled), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(sampled, &image_info), OPENAGC_OK);
    CHECK(image_info.usage == OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT &&
          image_info.image_id == 1u);
    EXPECT(openagc_graphics_image_create(device, &both_desc, &both), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &target_desc, &target_only),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(target_only, &image_info), OPENAGC_OK);
    CHECK(image_info.usage == OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT);
    EXPECT(openagc_graphics_image_bind_memory(sampled, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(both, memory, 16u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(target_only, memory, 32u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &image_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(image_buffer, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &staging_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(staging_buffer, memory, 48u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_create(device, &copy_desc, &copy), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command),
           OPENAGC_OK);
    EXPECT(openagc_gpu_queue_create(device, &queue_desc, &queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_create(device, &fence), OPENAGC_OK);
    for (i = 0u; i < sizeof(staging); ++i) {
        staging[i] = (uint8_t)(0x20u + i);
    }
    EXPECT(openagc_gpu_memory_write(memory, 48u, staging, sizeof(staging)), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled, &to_target),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_transition(command, target_only, &to_shader_read),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_transition(command, sampled, &to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command, sampled),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(sampled, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
          image_info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_graphics_image_destroy(sampled), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, both, &to_target), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command, both), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(both, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          image_info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);

    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, both, &target_to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);

    EXPECT(openagc_gpu_command_buffer_begin(copy), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(copy, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_copy_buffer(copy, image_buffer, 0u, staging_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_end(copy), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, copy, fence), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_reset(copy), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled,
                                               &shader_read_to_destination),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(copy), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(copy, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_end(copy), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled,
                                               &destination_to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, copy, fence), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_ERROR_NOT_READY);
    CHECK(fence_info.signaled == 0u);
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    for (i = 0u; i < sizeof(staging); ++i) {
        CHECK(readback[i] == 0u);
    }
    CHECK(readback[16] == 0u && readback[32] == 0u);

    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled,
                                               &shader_read_to_destination),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, copy, fence), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_OK);
    CHECK(fence_info.signaled == 1u);
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(readback, staging, sizeof(staging)) == 0);
    CHECK(readback[16] == 0u && readback[63] == staging[15]);

    EXPECT(openagc_graphics_command_buffer_destroy(command), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(sampled), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(both), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(target_only), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_destroy(copy), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_destroy(queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_destroy(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(image_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(staging_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_clear_execution(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(256u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        8u, 4u, 40u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_transition_desc acquire = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_scissor scissor = { 2u, 1u, 3u, 2u };
    openagc_color color = { 10u, 20u, 30u, 255u };
    openagc_graphics_image_info image_info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_graphics_recording_view view = OPENAGC_GRAPHICS_RECORDING_VIEW_INIT;
    openagc_graphics_execution_info info = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_command_buffer *command_buffer = NULL;
    uint8_t fill[256];
    uint8_t expected[256];
    uint8_t readback[256];
    uint8_t *pixel;
    uint32_t x;
    uint32_t y;

    CHECK(make_device(4096u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 8u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_OK);
    memset(fill, 0x5a, sizeof(fill));
    EXPECT(openagc_gpu_memory_write(memory, 0u, fill, sizeof(fill)), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command_buffer), OPENAGC_OK);
    info.struct_size--;
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    info.struct_size++;
    EXPECT(openagc_graphics_command_buffer_execute_host(NULL, &info),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info), OPENAGC_OK);
    CHECK(info.target_image_id == 1u && info.clear_count == 1u);
    CHECK(info.cleared_pixels == 6u && info.gpu_submitted == 0u);

    memset(expected, 0x5a, sizeof(expected));
    for (y = 0u; y < scissor.height; ++y) {
        for (x = 0u; x < scissor.width; ++x) {
            pixel = &expected[8u + (scissor.y + y) * 40u + (scissor.x + x) * 4u];
            pixel[0] = color.r;
            pixel[1] = color.g;
            pixel[2] = color.b;
            pixel[3] = color.a;
        }
    }
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(expected, readback, sizeof(readback)) == 0);

    EXPECT(openagc_graphics_image_get_info(image, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          image_info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view), OPENAGC_OK);
    CHECK(view.command_count == 4u && view.host_state_applied == 1u);
    CHECK(view.gpu_submitted == 0u);

    EXPECT(openagc_graphics_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info),
           OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_graphics_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_clear_order_and_staleness(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(32u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM);
    openagc_graphics_command_buffer_desc first_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_command_buffer_desc second_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(8u);
    openagc_graphics_transition_desc acquire = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc release = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST);
    openagc_graphics_scissor full = { 0u, 0u, 2u, 2u };
    openagc_graphics_scissor cell = { 1u, 1u, 1u, 1u };
    openagc_color first_color = { 10u, 20u, 30u, 40u };
    openagc_color second_color = { 200u, 100u, 50u, 60u };
    openagc_graphics_execution_info info = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_command_buffer *first = NULL;
    openagc_graphics_command_buffer *second = NULL;
    uint8_t fill[32];
    uint8_t expected[32];
    uint8_t readback[32];
    uint32_t i;

    CHECK(make_device(1024u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &first_desc, &first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &second_desc, &second), OPENAGC_OK);
    memset(fill, 0x5a, sizeof(fill));
    EXPECT(openagc_gpu_memory_write(memory, 0u, fill, sizeof(fill)), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(first, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(first, &full), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(first, first_color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(first), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image, &release), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_reset(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(first, &info),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(fill, readback, sizeof(readback)) == 0);

    EXPECT(openagc_graphics_command_buffer_reset(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image, &release), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(second, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(second, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(second, &full), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(second, first_color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(second, &cell), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(second, second_color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(second), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(second, &info), OPENAGC_OK);
    CHECK(info.target_image_id == 1u && info.clear_count == 2u);
    CHECK(info.cleared_pixels == 5u && info.gpu_submitted == 0u);

    memset(expected, 0x5a, sizeof(expected));
    for (i = 0u; i < 3u; ++i) {
        expected[i * 4u] = first_color.b;
        expected[i * 4u + 1u] = first_color.g;
        expected[i * 4u + 2u] = first_color.r;
        expected[i * 4u + 3u] = first_color.a;
    }
    expected[12u] = second_color.b;
    expected[13u] = second_color.g;
    expected[14u] = second_color.r;
    expected[15u] = second_color.a;
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(expected, readback, sizeof(readback)) == 0);

    EXPECT(openagc_graphics_command_buffer_reset(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image, &release), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(first, &info),
           OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_graphics_command_buffer_reset(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(first, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(first, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(first, &info),
           OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_graphics_command_buffer_destroy(first), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_destroy(second), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_copy_ownership_handoff(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_memory_desc staging_desc = OPENAGC_GPU_MEMORY_DESC_INIT(32u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM);
    openagc_gpu_buffer_desc image_buffer_desc = OPENAGC_GPU_BUFFER_DESC_INIT(
        16u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                 OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_buffer_desc staging_buffer_desc = OPENAGC_GPU_BUFFER_DESC_INIT(
        16u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                 OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_command_buffer_desc copy_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(2u, 14u);
    openagc_graphics_command_buffer_desc graphics_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(6u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_graphics_execution_info execution = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_gpu_recording_view copy_view = OPENAGC_GPU_RECORDING_VIEW_INIT;
    openagc_gpu_submission_view submission = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    openagc_graphics_image_info image_info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_graphics_transition_desc to_destination =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
            OPENAGC_GRAPHICS_OWNER_COPY);
    openagc_graphics_transition_desc to_source =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
            OPENAGC_GRAPHICS_OWNER_COPY,
            OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE, OPENAGC_GRAPHICS_OWNER_COPY);
    openagc_graphics_transition_desc to_target =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE, OPENAGC_GRAPHICS_OWNER_COPY,
            OPENAGC_GRAPHICS_STATE_COLOR_TARGET,
            OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc target_to_destination =
        OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
            OPENAGC_GRAPHICS_STATE_COLOR_TARGET,
            OPENAGC_GRAPHICS_OWNER_GRAPHICS,
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION,
            OPENAGC_GRAPHICS_OWNER_COPY);
    openagc_graphics_transition_desc to_host = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION, OPENAGC_GRAPHICS_OWNER_COPY,
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST);
    openagc_graphics_scissor scissor = { 1u, 1u, 1u, 1u };
    openagc_color clear_color = { 10u, 20u, 30u, 40u };
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_memory *staging_memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_gpu_buffer *image_buffer = NULL;
    openagc_gpu_buffer *staging_buffer = NULL;
    openagc_gpu_buffer *spare_buffer = NULL;
    openagc_gpu_command_buffer *upload = NULL;
    openagc_gpu_command_buffer *readback = NULL;
    openagc_graphics_command_buffer *graphics = NULL;
    openagc_gpu_queue *queue = NULL;
    openagc_gpu_fence *fence = NULL;
    uint8_t staging[32];
    uint8_t pattern[64];
    uint8_t compare[64];
    uint8_t snapshot[64];
    uint64_t submission_id;
    uint32_t i;

    CHECK(make_device(4096u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &staging_desc, &staging_memory),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &image_buffer_desc, &image_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(image_buffer, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &staging_buffer_desc, &staging_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(staging_buffer, staging_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &staging_buffer_desc, &spare_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(spare_buffer, staging_memory, 16u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_create(device, &copy_desc, &upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_create(device, &copy_desc, &readback), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &graphics_desc, &graphics),
           OPENAGC_OK);
    EXPECT(openagc_gpu_queue_create(device, &queue_desc, &queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_create(device, &fence), OPENAGC_OK);
    memset(pattern, 0x5a, sizeof(pattern));
    EXPECT(openagc_gpu_memory_write(memory, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    for (i = 0u; i < sizeof(staging); ++i) {
        staging[i] = (uint8_t)(0x40u + i);
    }
    EXPECT(openagc_gpu_memory_write(staging_memory, 0u, staging, sizeof(staging)),
           OPENAGC_OK);

    EXPECT(openagc_gpu_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_reset(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(readback), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(readback, image_buffer, 0u, staging_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_reset(readback), OPENAGC_OK);

    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(graphics, image, &to_destination),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(image, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION &&
          image_info.owner == OPENAGC_GRAPHICS_OWNER_COPY);

    EXPECT(openagc_gpu_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, image_buffer, 0u, staging_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_end(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_get_recording(upload, &copy_view), OPENAGC_OK);
    CHECK(copy_view.command_count == 1u);
    EXPECT(openagc_gpu_queue_submit(queue, upload, fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    CHECK(submission.word_count == 31u && submission.gpu_submitted == 0u);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(memory, 0u, compare, sizeof(compare)), OPENAGC_OK);
    CHECK(memcmp(compare, staging, 16u) == 0);
    CHECK(compare[16] == 0x5a && compare[63] == 0x5a);

    EXPECT(openagc_graphics_command_buffer_reset(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(graphics, image, &to_source), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    memset(pattern, 0x77, 16u);
    EXPECT(openagc_gpu_memory_write(memory, 0u, pattern, 16u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(readback), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(readback, image_buffer, 0u, staging_buffer, 0u,
                                           16u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_end(readback), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, readback, fence), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_fence_reset(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, readback, fence), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(staging_memory, 0u, compare, sizeof(staging)),
           OPENAGC_OK);
    CHECK(compare[0] == 0x77 && compare[15] == 0x77);

    EXPECT(openagc_graphics_command_buffer_reset(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(graphics, image, &to_target), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_reset(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_reset(upload), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_reset(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(graphics, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(graphics, &scissor), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(graphics, clear_color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(graphics, &execution), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(memory, 0u, compare, sizeof(compare)), OPENAGC_OK);
    CHECK(compare[0] == 0x77 && compare[11] == 0x77);
    CHECK(compare[12] == clear_color.b && compare[13] == clear_color.g);
    CHECK(compare[14] == clear_color.r && compare[15] == clear_color.a);

    EXPECT(openagc_graphics_command_buffer_reset(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(graphics, image, &target_to_destination),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, staging_buffer, 0u, image_buffer, 0u,
                                           16u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_end(upload), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_reset(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(graphics, image, &to_host), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(graphics), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(memory, 0u, snapshot, sizeof(snapshot)), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    submission_id = submission.submission_id;
    EXPECT(openagc_gpu_fence_reset(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, upload, fence), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_ERROR_NOT_READY);
    CHECK(fence_info.signaled == 0u);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    CHECK(submission.submission_id == submission_id);
    EXPECT(openagc_gpu_memory_read(memory, 0u, compare, sizeof(compare)), OPENAGC_OK);
    CHECK(memcmp(compare, snapshot, sizeof(snapshot)) == 0);

    EXPECT(openagc_graphics_command_buffer_destroy(graphics), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_reset(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(upload, staging_buffer, 0u, spare_buffer, 0u,
                                           16u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_end(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_submit(queue, upload, fence), OPENAGC_OK);

    EXPECT(openagc_gpu_command_buffer_destroy(upload), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_destroy(readback), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_destroy(queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_destroy(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(image_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(staging_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(spare_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(staging_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_depth_target_state(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(256u);
    openagc_graphics_image_desc depth_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT);
    openagc_graphics_image_desc color_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_transition_desc to_depth = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_DEPTH_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc depth_to_color = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_DEPTH_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *depth = NULL;
    openagc_graphics_image *color = NULL;
    openagc_graphics_command_buffer *command = NULL;
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;

    depth_desc.usage = OPENAGC_GRAPHICS_USAGE_DEPTH_STENCIL_BIT;
    CHECK(make_device(4096u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &depth_desc, &depth), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &color_desc, &color), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(depth, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(color, memory, 64u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, color, &to_depth),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_transition(command, depth, &to_depth), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, depth, &depth_to_color),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_bind_color_target(command, depth),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_image_get_info(depth, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    {
        openagc_graphics_scissor full = { 0u, 0u, 4u, 4u };
        openagc_graphics_scissor one = { 0u, 0u, 1u, 1u };
        openagc_graphics_execution_info exec = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
        uint8_t packed[8];

        EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_bind_depth_target(command, color),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_graphics_command_bind_depth_target(command, depth), OPENAGC_OK);
        EXPECT(openagc_graphics_command_set_scissor(command, &full), OPENAGC_OK);
        EXPECT(openagc_graphics_command_clear_depth(command, 16777215u, 0x5au), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_execute_host(command, &exec), OPENAGC_OK);
        CHECK(exec.gpu_submitted == 0u && exec.cleared_pixels == 16u);
        {
            openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;

            EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
            CHECK(write_view.word_count == OPENAGC_PM4_WRITE_DATA_CLEAR_EOP_WORDS);
            CHECK(write_view.words[0] == 0xc0123700u);
            CHECK(write_view.words[4] == 0x5affffffu);
            CHECK(write_view.words[20] == OPENAGC_PM4_EOP_HEADER);
        }
        EXPECT(openagc_gpu_memory_read(memory, 0u, packed, sizeof(packed)), OPENAGC_OK);
        CHECK(packed[0] == 0xffu && packed[1] == 0xffu && packed[2] == 0xffu &&
              packed[3] == 0x5au);
        CHECK(packed[4] == 0xffu && packed[7] == 0x5au);
        EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_bind_depth_target(command, depth), OPENAGC_OK);
        EXPECT(openagc_graphics_command_set_scissor(command, &one), OPENAGC_OK);
        EXPECT(openagc_graphics_command_clear_depth(command, 0u, 0u), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
        EXPECT(openagc_graphics_command_buffer_execute_host(command, &exec), OPENAGC_OK);
        EXPECT(openagc_gpu_memory_read(memory, 0u, packed, sizeof(packed)), OPENAGC_OK);
        CHECK(packed[0] == 0u && packed[1] == 0u && packed[2] == 0u && packed[3] == 0u);
        CHECK(packed[4] == 0xffu && packed[7] == 0x5au);
    }
    EXPECT(openagc_graphics_command_buffer_destroy(command), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(depth), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(color), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_clear_write_data_tile(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_transition_desc acquire = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_scissor scissor = { 0u, 0u, 4u, 4u };
    openagc_color color = { 0xa5u, 0xa5u, 0xa5u, 0xa5u };
    openagc_graphics_execution_info info = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
    openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_gpu_memory *memory = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_command_buffer *command_buffer = NULL;
    uint32_t words[16];
    uint32_t i;

    CHECK(make_device(4096u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command_buffer, image, &acquire), OPENAGC_OK);
    EXPECT(openagc_graphics_command_bind_color_target(command_buffer, image), OPENAGC_OK);
    EXPECT(openagc_graphics_command_set_scissor(command_buffer, &scissor), OPENAGC_OK);
    EXPECT(openagc_graphics_command_clear_color(command_buffer, color), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_execute_host(command_buffer, &info), OPENAGC_OK);
    CHECK(info.cleared_pixels == 16u && info.gpu_submitted == 0u);
    EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
    CHECK(write_view.word_count == OPENAGC_PM4_WRITE_DATA_CLEAR_EOP_WORDS);
    CHECK(write_view.words[0] == 0xc0123700u);
    CHECK(write_view.words[4] == 0xa5a5a5a5u);
    CHECK(write_view.words[19] == 0xa5a5a5a5u);
    CHECK(write_view.words[20] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_memory_read(memory, 0u, words, sizeof(words)), OPENAGC_OK);
    for (i = 0u; i < 16u; ++i) {
        CHECK(words[i] == 0xa5a5a5a5u);
    }
    EXPECT(openagc_graphics_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_formats_footprint_and_binding() != 0 ||
        test_depth_target_state() != 0 ||
        test_recording_order_capacity_and_no_pixels() != 0 ||
        test_ownership_stale_and_atomic_apply() != 0 ||
        test_usage_mask_and_sampled_state() != 0 ||
        test_host_clear_execution() != 0 ||
        test_host_clear_order_and_staleness() != 0 ||
        test_copy_ownership_handoff() != 0 ||
        test_host_clear_write_data_tile() != 0) {
        return 1;
    }
    puts("OpenAGC graphics validation tests passed");
    return 0;
}
