/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"

#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int test_policy(void)
{
    openagc_context_desc desc = OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_PS5);
    openagc_device_desc device_desc = OPENAGC_DEVICE_DESC_INIT(100u, 100u, 1u);
    openagc_capabilities capabilities = OPENAGC_CAPABILITIES_INIT;
    openagc_frame_view view = OPENAGC_FRAME_VIEW_INIT;
    openagc_color color = { 0u, 0u, 0u, 255u };
    openagc_rect rect = { 0.0f, 0.0f, 10.0f, 10.0f, { 1u, 2u, 3u, 255u } };
    openagc_context *context = 0;
    openagc_device *device = 0;
    uint32_t dummy = 0u;
    openagc_context *unreachable_context = (openagc_context *)(void *)&dummy;
    openagc_device *unreachable_device = (openagc_device *)(void *)&dummy;

    CHECK(openagc_api_version() == OPENAGC_API_VERSION);
    CHECK(openagc_result_string(OPENAGC_ERROR_UNSUPPORTED_FIRMWARE) != 0);
    desc.firmware_major = 9u;
    desc.firmware_minor = 40u;
    CHECK(openagc_context_create(&desc, &context) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == 0);
    desc.firmware_major = 0u;
    desc.firmware_minor = 0u;
    CHECK(openagc_context_create(&desc, &context) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == 0);
    desc.firmware_major = 99u;
    desc.firmware_minor = 99u;
    CHECK(openagc_context_create(&desc, &context) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == 0);
    desc.backend = OPENAGC_BACKEND_HOST_REFERENCE;
    CHECK(openagc_context_create(&desc, &context) == OPENAGC_ERROR_UNSUPPORTED_BACKEND);
    desc.backend = OPENAGC_BACKEND_PS5;
    desc.api_version++;
    CHECK(openagc_context_create(&desc, &context) == OPENAGC_ERROR_INCOMPATIBLE_VERSION);

    /* Even if called out of order with a fabricated handle, stubs cannot submit. */
    CHECK(openagc_device_create(unreachable_context, &device_desc, &device) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(device == 0);
    CHECK(openagc_device_get_capabilities(unreachable_device, &capabilities) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(capabilities.gpu_execution == 0u && capabilities.video_output == 0u);
    CHECK(openagc_frame_begin(unreachable_device) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frame_clear(unreachable_device, color) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frame_rect(unreachable_device, &rect) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frame_present(unreachable_device) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_device_get_last_frame(unreachable_device, &view) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_device_destroy(unreachable_device) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_context_destroy(unreachable_context) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

static int test_gpu_policy(void)
{
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_command_buffer_desc command_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(1u, 7u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_gpu_capabilities capabilities = OPENAGC_GPU_CAPABILITIES_INIT;
    openagc_gpu_recording_view recording = OPENAGC_GPU_RECORDING_VIEW_INIT;
    openagc_gpu_submission_view submission = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    openagc_gpu_device *created_device = 0;
    openagc_gpu_memory *created_memory = 0;
    openagc_gpu_buffer *created_buffer = 0;
    openagc_gpu_command_buffer *created_command = 0;
    openagc_gpu_queue *created_queue = 0;
    openagc_gpu_fence *created_fence = 0;
    uint32_t dummy = 0u;
    openagc_context *context = (openagc_context *)(void *)&dummy;
    openagc_gpu_device *device = (openagc_gpu_device *)(void *)&dummy;
    openagc_gpu_memory *memory = (openagc_gpu_memory *)(void *)&dummy;
    openagc_gpu_buffer *buffer = (openagc_gpu_buffer *)(void *)&dummy;
    openagc_gpu_command_buffer *command_buffer =
        (openagc_gpu_command_buffer *)(void *)&dummy;
    openagc_gpu_queue *queue = (openagc_gpu_queue *)(void *)&dummy;
    openagc_gpu_fence *fence = (openagc_gpu_fence *)(void *)&dummy;

    CHECK(openagc_gpu_device_create(context, &device_desc, &created_device) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_device == 0);
    CHECK(openagc_gpu_device_get_capabilities(device, &capabilities) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(capabilities.gpu_execution == 0u && capabilities.video_output == 0u);
    CHECK(openagc_gpu_memory_allocate(device, &memory_desc, &created_memory) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_memory == 0);
    CHECK(openagc_gpu_buffer_create(device, &buffer_desc, &created_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_buffer == 0);
    CHECK(openagc_gpu_command_buffer_create(device, &command_desc, &created_command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_command == 0);
    CHECK(openagc_gpu_queue_create(device, &queue_desc, &created_queue) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_queue == 0);
    CHECK(openagc_gpu_fence_create(device, &created_fence) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_fence == 0);

    CHECK(openagc_gpu_memory_write(memory, 0u, &dummy, sizeof(dummy)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_memory_read(memory, 0u, &dummy, sizeof(dummy)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_buffer_bind_memory(buffer, memory, 0u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_command_buffer_begin(command_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_command_copy_buffer(command_buffer, buffer, 0u, buffer, 4u, 4u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_command_buffer_end(command_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_command_buffer_get_recording(command_buffer, &recording) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_queue_submit(queue, command_buffer, fence) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_queue_get_last_submission(queue, &submission) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_fence_poll(fence, &fence_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(fence_info.signaled == 0u && submission.gpu_submitted == 0u);
    CHECK(openagc_gpu_command_buffer_reset(command_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_fence_reset(fence) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_buffer_destroy(buffer) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_memory_destroy(memory) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_command_buffer_destroy(command_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_queue_destroy(queue) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_fence_destroy(fence) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_gpu_device_destroy(device) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

static int test_graphics_policy(void)
{
    openagc_graphics_capabilities capabilities = OPENAGC_GRAPHICS_CAPABILITIES_INIT;
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_image_info info = OPENAGC_GRAPHICS_IMAGE_INFO_INIT;
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(4u);
    openagc_graphics_transition_desc transition = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_scissor scissor = { 0u, 0u, 4u, 4u };
    openagc_graphics_recording_view view = OPENAGC_GRAPHICS_RECORDING_VIEW_INIT;
    openagc_color color = { 1u, 2u, 3u, 255u };
    openagc_graphics_image *created_image = 0;
    openagc_graphics_command_buffer *created_command = 0;
    uint32_t dummy = 0u;
    openagc_gpu_device *device = (openagc_gpu_device *)(void *)&dummy;
    openagc_gpu_memory *memory = (openagc_gpu_memory *)(void *)&dummy;
    openagc_graphics_image *image = (openagc_graphics_image *)(void *)&dummy;
    openagc_graphics_command_buffer *command =
        (openagc_graphics_command_buffer *)(void *)&dummy;

    CHECK(openagc_graphics_get_capabilities(device, &capabilities) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(capabilities.gpu_execution == 0u && capabilities.rasterization == 0u);
    CHECK(openagc_graphics_image_create(device, &image_desc, &created_image) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_image == 0);
    CHECK(openagc_graphics_image_bind_memory(image, memory, 0u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_image_get_info(image, &info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_buffer_create(device, &command_desc, &created_command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_command == 0);
    CHECK(openagc_graphics_command_buffer_begin(command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_transition(command, image, &transition) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_bind_color_target(command, image) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_set_scissor(command, &scissor) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_clear_color(command, color) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_buffer_end(command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_buffer_apply_host_state(command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_buffer_get_recording(command, &view) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(view.gpu_submitted == 0u);
    CHECK(openagc_graphics_command_buffer_reset(command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_command_buffer_destroy(command) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_graphics_image_destroy(image) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

static int test_shader_policy(void)
{
    openagc_shader_capabilities capabilities = OPENAGC_SHADER_CAPABILITIES_INIT;
    openagc_shader_artifact_desc artifact_desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_binding_decl binding = { 0u, 0u, 1u, 16u };
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_pipeline_info pipeline_info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_shader_artifact *created_artifact = 0;
    openagc_shader_pipeline_plan *created_plan = 0;
    uint32_t dummy = 0u;
    openagc_gpu_device *device = (openagc_gpu_device *)(void *)&dummy;
    openagc_shader_artifact *artifact = (openagc_shader_artifact *)(void *)&dummy;
    openagc_shader_pipeline_plan *plan =
        (openagc_shader_pipeline_plan *)(void *)&dummy;

    CHECK(openagc_shader_get_capabilities(device, &capabilities) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(capabilities.compiler_available == 0u && capabilities.gpu_execution == 0u);
    CHECK(openagc_shader_artifact_intake_host(device, &artifact_desc, &created_artifact) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_artifact == 0);
    CHECK(openagc_shader_artifact_get_info(artifact, &artifact_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_shader_artifact_get_binding(artifact, 0u, &binding) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_shader_artifact_require_compiler(artifact) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_shader_artifact_destroy(artifact) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &created_plan) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_plan == 0);
    CHECK(openagc_shader_pipeline_plan_get_info(plan, &pipeline_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_shader_pipeline_plan_destroy(plan) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

int main(void)
{
    if (test_policy() != 0 || test_gpu_policy() != 0 ||
        test_graphics_policy() != 0 || test_shader_policy() != 0) {
        return 1;
    }
    puts("OpenAGC PS5 policy tests passed");
    return 0;
}
