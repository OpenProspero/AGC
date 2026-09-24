/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/graphics.h"

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
    CHECK(caps.supported_format_mask == 3u && caps.max_width == 4096u);
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
    desc.usage = OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
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

int main(void)
{
    if (test_formats_footprint_and_binding() != 0 ||
        test_recording_order_capacity_and_no_pixels() != 0 ||
        test_ownership_stale_and_atomic_apply() != 0) {
        return 1;
    }
    puts("OpenAGC graphics validation tests passed");
    return 0;
}
