/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"
#include "openagc/vulkan.h"
#include "openagc/opengl.h"

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
    openagc_graphics_execution_info execution = OPENAGC_GRAPHICS_EXECUTION_INFO_INIT;
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
    CHECK(capabilities.host_state_recording == 0u &&
          capabilities.host_clear_simulation == 0u);
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
    CHECK(openagc_graphics_command_buffer_execute_host(command, &execution) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(execution.cleared_pixels == 0u && execution.gpu_submitted == 0u);
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
    openagc_shader_texture_decl texture = { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM };
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
    CHECK(openagc_shader_artifact_get_texture(artifact, 0u, &texture) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    {
        const uint8_t *metadata = (const uint8_t *)0;
        uint32_t metadata_size = 0u;

        CHECK(openagc_shader_artifact_get_compiler_metadata(artifact, &metadata,
                                                            &metadata_size) ==
              OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
        CHECK(metadata == 0 && metadata_size == 0u);
    }
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

static int test_frontend_policy(void)
{
    openagc_frontend_capabilities capabilities = OPENAGC_FRONTEND_CAPABILITIES_INIT;
    openagc_frontend_device_desc device_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_image_desc image_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u, 0u);
    openagc_frontend_image_info image_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_buffer_desc buffer_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        64u);
    openagc_frontend_buffer_info buffer_info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_frontend_timeline_info timeline_info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_frontend_pipeline_info pipeline_info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_shader_pipeline_desc plan_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_graphics_format format = 0u;
    openagc_graphics_usage usage = 0u;
    openagc_graphics_image_state state = OPENAGC_GRAPHICS_STATE_UNDEFINED;
    openagc_graphics_owner owner = OPENAGC_GRAPHICS_OWNER_HOST;
    openagc_gpu_buffer_usage buffer_usage = 0u;
    uint32_t native = 0u;
    openagc_frontend_device *created_frontend = 0;
    openagc_frontend_image *created_image = 0;
    openagc_frontend_buffer *created_buffer = 0;
    uint32_t dummy = 0u;
    openagc_gpu_device *device = (openagc_gpu_device *)(void *)&dummy;
    openagc_frontend_device *frontend = (openagc_frontend_device *)(void *)&dummy;
    openagc_frontend_image *image = (openagc_frontend_image *)(void *)&dummy;
    openagc_frontend_buffer *buffer = (openagc_frontend_buffer *)(void *)&dummy;
    openagc_frontend_timeline *timeline = (openagc_frontend_timeline *)(void *)&dummy;
    openagc_frontend_timeline *created_timeline = 0;
    openagc_frontend_pipeline *pipeline = (openagc_frontend_pipeline *)(void *)&dummy;
    openagc_frontend_pipeline *created_pipeline = 0;
    uint8_t bytes[4] = { 0u, 0u, 0u, 0u };

    CHECK(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 0u, &native) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 37u, &format) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_VULKAN, 4u, &usage) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 0u, &state,
                                                  &owner) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_translate_buffer_usage(OPENAGC_FRONTEND_VULKAN, 1u,
                                                  &buffer_usage) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_device_create(device, &device_desc, &created_frontend) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_frontend == 0);
    CHECK(openagc_frontend_device_get_capabilities(frontend, &capabilities) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(capabilities.host_translation == 0u && capabilities.gpu_execution == 0u);
    CHECK(openagc_frontend_device_destroy(frontend) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_create(frontend, &image_desc, &created_image) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_image == 0);
    CHECK(openagc_frontend_image_get_info(image, &image_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_transition(image, OPENAGC_GRAPHICS_STATE_SHADER_READ,
                                            OPENAGC_GRAPHICS_OWNER_GRAPHICS) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_upload(image, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_readback(image, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_clear(0, (openagc_color){ 0u, 0u, 0u, 0u }) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_frontend_image_destroy(image) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_create(frontend, &buffer_desc, &created_buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_buffer == 0);
    CHECK(openagc_frontend_buffer_get_info(buffer, &buffer_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_upload(buffer, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_readback(buffer, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_copy(buffer, 0u, buffer, 0u, sizeof(bytes)) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_copy(0, 0u, buffer, 0u, sizeof(bytes)) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_frontend_buffer_fill(buffer, 0u, 4u, 0u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_buffer_fill(0, 0u, 4u, 0u) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_frontend_image_copy_rect(image, 0u, 0u, image, 0u, 0u, 1u, 1u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_image_copy_rect(0, 0u, 0u, image, 0u, 0u, 1u, 1u) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_frontend_buffer_destroy(buffer) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_timeline_create(0, &created_timeline) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_frontend_timeline_create(frontend, &created_timeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_timeline == 0);
    CHECK(openagc_frontend_timeline_signal(timeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_timeline_poll(timeline, 1u, &timeline_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_timeline_destroy(timeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    plan_desc.struct_size--;
    CHECK(openagc_frontend_pipeline_create(frontend, &plan_desc, &created_pipeline) ==
          OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    plan_desc.struct_size++;
    CHECK(openagc_frontend_pipeline_create(frontend, &plan_desc, &created_pipeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_pipeline == 0);
    CHECK(openagc_frontend_pipeline_get_info(pipeline, &pipeline_info) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_frontend_pipeline_destroy(pipeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

static int test_vulkan_policy(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_capabilities caps = OPENAGC_VK_CAPABILITIES_INIT;
    openagc_vk_queue_family family = OPENAGC_VK_QUEUE_FAMILY_INIT;
    openagc_frontend_timeline_info info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    uint32_t dummy = 0u;
    openagc_vk_instance *instance = (openagc_vk_instance *)(void *)&dummy;
    openagc_vk_instance *created = 0;
    openagc_vk_device *device = (openagc_vk_device *)(void *)&dummy;
    openagc_vk_device *created_device = 0;
    openagc_vk_fence *fence = (openagc_vk_fence *)(void *)&dummy;
    openagc_vk_fence *created_fence = 0;
    openagc_vk_pipeline *created_pipeline = 0;
    uint8_t bytes[4] = { 0u, 0u, 0u, 0u };

    CHECK(openagc_vk_instance_create(&instance_desc, &created) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created == 0);
    CHECK(openagc_vk_get_capabilities(instance, &caps) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(caps.gpu_execution == 0u && caps.presentation == 0u);
    CHECK(openagc_vk_get_queue_family(instance, 0u, &family) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_device_create(instance, &device_desc, &created_device) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_device == 0);
    CHECK(openagc_vk_cmd_draw((openagc_vk_command_buffer *)(void *)&dummy, 3u, 1u, 0u, 0u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_cmd_draw(0, 3u, 1u, 0u, 0u) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_cmd_dispatch(0, 1u, 1u, 1u) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_sampler(0, 0u, 0u, 0u, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_allocate_memory(0, 64u, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_bind_buffer_memory(0, 0, 0u) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_compute_pipeline(0, 0, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_graphics_pipeline(0, 0, 0, 0, 0) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_graphics_pipeline(
              device, 0, 0, (openagc_vk_image *)(void *)&dummy, &created_pipeline) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_pipeline == 0);
    CHECK(openagc_vk_create_command_pool(0, 0, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_allocate_command_buffer(0, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_image(device, 0, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_image_view(0, 0, 0, 0) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_cmd_clear_color(0, 0, (openagc_color){ 0u, 0u, 0u, 0u }) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_create_swapchain(device) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_create_fence(device, &created_fence) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(created_fence == 0);
    CHECK(openagc_vk_queue_submit(device, fence) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_fence_poll(fence, 1u, &info) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_buffer_upload(0, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_cmd_copy_image((openagc_vk_command_buffer *)(void *)&dummy,
                                    (openagc_vk_image *)(void *)&dummy, 0u, 0u,
                                    (openagc_vk_image *)(void *)&dummy, 0u, 0u, 1u, 1u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_cmd_copy_image(0, 0, 0u, 0u, 0, 0u, 0u, 1u, 1u) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_cmd_fill_buffer((openagc_vk_command_buffer *)(void *)&dummy,
                                     (openagc_vk_buffer *)(void *)&dummy, 0u, 4u, 0u) ==
          OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_cmd_fill_buffer(0, 0, 0u, 4u, 0u) == OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_cmd_update_buffer((openagc_vk_command_buffer *)(void *)&dummy,
                                       (openagc_vk_buffer *)(void *)&dummy, 0u, bytes,
                                       sizeof(bytes)) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(openagc_vk_cmd_update_buffer(0, 0, 0u, bytes, sizeof(bytes)) ==
          OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(openagc_vk_instance_destroy(instance) == OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    return 0;
}

int main(void)
{
    if (test_policy() != 0 || test_gpu_policy() != 0 ||
        test_graphics_policy() != 0 || test_shader_policy() != 0 ||
        test_frontend_policy() != 0 || test_vulkan_policy() != 0 ||
        openagc_gl_draw_arrays(0, 0u, 3u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_create_graphics_program(0, 0, 0, 0, 0) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_clear_buffer_sub_data(0, 0u, 4u, 0u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_copy_buffer_sub_data(0, 0u, 0, 0u, 4u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_copy_buffer_then_clear_sub_data(0, 0u, 0, 0u, 64u, 0u, 16u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_buffer_copy_then_fill(0, 0u, 0, 0u, 64u, 0u, 16u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_buffer_sub_data(0, 0u, 0, 4u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_store_const(0, 0, 0u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_store_span(0, 0, 0u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_store_span2(0, 0, 0u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_store_span_n(0, 0, 0u, 4u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_write_data(0, 0, 0u, 0u, 1u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_write_data_memory(0, 0, 0u, 0u, 1u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_write_data_rows(0, 0, 0u, 16u, 0u, 1u, 1u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_write_data_grid(0, 0, 0u, 128u, 0u, 16u, 2u, 8u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_write_data_buffer_rows(0, 0, 0u, 16u, 0u, 1u, 1u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_dma_write_data(0, 0, 0u, 0, 0u, 64u, 0u, 1u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_host_graphics_register_eop(0, 0, 1u) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_cb_capture_verify(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_cb_capture_encode_invent(OPENAGC_CB_CAPTURE_KIND_CB_BIND, 0, 0u, 0) !=
            OPENAGC_ERROR_UNSUPPORTED_OPERATION ||
        openagc_gpu_host_cb_bind_from_capture(0, 0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_device_get_cb_capture_info(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_ib_dump_parse(0, 0, 0u, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_render_pass_bind_cb_capture(0, 0, 0) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_device_get_cb_capture_info(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_device_reserve_synthetic_va(0, 256u, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_pipeline_patch_psbc_pgm_vas(0, 0u, 0u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_pipeline_record_psbc_register_eop(0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_pipeline_record_psbc_register_eop_if_bound(0) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_device_get_last_write(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_vk_device_get_last_write(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_context_get_last_write(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_pipeline_bind_psbc_code(0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gpu_memory_get_device_address(0, 0u, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_shader_artifact_get_code(0, 0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_copy_tex_sub_image(0, 0, 0u, 0u, 0u, 0u, 1u, 1u) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_frontend_graphics_pipeline_create(0, 0, 0, 0, 0) !=
            OPENAGC_ERROR_INVALID_ARGUMENT ||
        openagc_gl_context_create(0, 0) != OPENAGC_ERROR_INVALID_ARGUMENT) {
        return 1;
    }
    puts("OpenAGC PS5 policy tests passed");
    return 0;
}
