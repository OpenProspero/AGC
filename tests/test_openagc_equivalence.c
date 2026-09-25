/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/opengl.h"
#include "openagc/vulkan.h"

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

static int test_same_backend_state(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_image_desc vk_image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc gl_image_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
    openagc_shader_artifact_desc artifact = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_desc vertex_stage = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_desc pixel_stage = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_frontend_pipeline_info vk_plan = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_frontend_pipeline_info gl_plan = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_frontend_image_info vk_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_image_info gl_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    static const uint8_t code_hash[32] = {
        0x9fu, 0x86u, 0xd0u, 0x81u, 0x88u, 0x4cu, 0x7du, 0x65u,
        0x9au, 0x2fu, 0xeau, 0xa0u, 0xc5u, 0x5au, 0xd0u, 0x15u,
        0xa3u, 0xbfu, 0x4fu, 0x1bu, 0x2bu, 0x0bu, 0x82u, 0x2cu,
        0xd1u, 0x5du, 0x6cu, 0x15u, 0xb0u, 0xf0u, 0x0au, 0x08u
    };
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_color clear = { 0x11u, 0x22u, 0x33u, 0x44u };
    uint8_t vk_pixels[64];
    uint8_t gl_pixels[64];
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_image *sampled = NULL;
    openagc_vk_image *target = NULL;
    openagc_vk_buffer_desc uniform_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 256u);
    openagc_frontend_buffer_info vk_buffer_info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_frontend_buffer_info gl_buffer_info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_vk_buffer *vk_uniform = NULL;
    openagc_vk_buffer *vk_bound = NULL;
    openagc_vk_memory *vk_memory = NULL;
    openagc_gl_buffer *gl_bound = NULL;
    openagc_gl_memory *gl_memory = NULL;
    openagc_vk_image_view_desc sampled_view_desc = OPENAGC_VK_IMAGE_VIEW_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    openagc_vk_image_view *sampled_view = NULL;
    openagc_vk_image_view *color_view = NULL;
    openagc_vk_descriptor_set *descriptors = NULL;
    openagc_vk_sampler *vk_sampler = NULL;
    openagc_gl_sampler *gl_sampler = NULL;
    openagc_gl_buffer *gl_uniform = NULL;
    openagc_vk_pipeline *vk_pipeline = NULL;
    openagc_vk_pipeline *vk_graphics = NULL;
    openagc_vk_render_pass *vk_pass = NULL;
    openagc_vk_buffer *vk_vertex = NULL;
    openagc_gl_buffer *gl_vertex = NULL;
    openagc_gl_program *gl_graphics = NULL;
    openagc_vk_semaphore *wait_sem = NULL;
    openagc_vk_semaphore *signal_sem = NULL;
    openagc_vk_command_buffer *kick = NULL;
    openagc_vk_command_buffer *upload = NULL;
    openagc_frontend_timeline_info sync_info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_gl_context *gl = NULL;
    openagc_gl_texture *texture = NULL;
    openagc_gl_texture *attached = NULL;
    openagc_gl_framebuffer *texture_fb = NULL;
    openagc_vk_format_properties vk_format = OPENAGC_VK_FORMAT_PROPERTIES_INIT;
    uint32_t gl_native = 0u;
    openagc_graphics_format gl_backend = 0u;
    openagc_gl_renderbuffer *renderbuffer = NULL;
    openagc_gl_framebuffer *framebuffer = NULL;
    openagc_gl_program *gl_program = NULL;

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    openagc_vk_buffer_desc transfer_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        256u);
    uint8_t pattern[16];
    uint8_t vk_upload[16];
    uint8_t gl_upload[16];
    openagc_vk_buffer *transfer = NULL;
    openagc_gl_buffer *unpack = NULL;
    openagc_gl_buffer *pack = NULL;
    uint32_t byte = 0u;
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &sampled), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &target), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, sampled, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, target, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_clear_color(commands, target, clear), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_memory(device, 256u, &vk_memory), OPENAGC_OK);
    EXPECT(openagc_vk_create_unbound_buffer(device, &transfer_desc, &vk_bound), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(vk_bound, 0u, code, 4u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_bind_buffer_memory(vk_bound, vk_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_vk_memory_write(vk_memory, 0u, code, 4u), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_readback(vk_bound, 0u, vk_pixels, 4u), OPENAGC_OK);
    CHECK(memcmp(vk_pixels, code, 4u) == 0);
    EXPECT(openagc_vk_free_memory(vk_memory), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_allocate_memory(gl, 256u, &gl_memory), OPENAGC_OK);
    EXPECT(openagc_gl_create_unbound_buffer(gl, OPENAGC_GL_PIXEL_UNPACK_BUFFER, 64u, &gl_bound),
           OPENAGC_OK);
    EXPECT(openagc_gl_bind_buffer_memory(gl_bound, gl_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(gl_bound, 0u, code, 4u), OPENAGC_OK);
    EXPECT(openagc_gl_free_memory(gl_memory), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_destroy_buffer(vk_bound), OPENAGC_OK);
    EXPECT(openagc_vk_free_memory(vk_memory), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_bound), OPENAGC_OK);
    EXPECT(openagc_gl_free_memory(gl_memory), OPENAGC_OK);
    EXPECT(openagc_gl_create_texture(gl, &gl_image_desc, &texture), OPENAGC_OK);
    EXPECT(openagc_gl_bind_texture_for_sampling(gl, texture), OPENAGC_OK);
    EXPECT(openagc_gl_create_renderbuffer(gl, &gl_image_desc, &renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_clear(framebuffer, clear), OPENAGC_OK);

    EXPECT(openagc_vk_image_get_info(sampled, &vk_info), OPENAGC_OK);
    EXPECT(openagc_gl_texture_get_info(texture, &gl_info), OPENAGC_OK);
    CHECK(vk_info.state == gl_info.state && vk_info.owner == gl_info.owner);
    CHECK(vk_info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ);
    CHECK(vk_info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);

    EXPECT(openagc_vk_image_get_info(target, &vk_info), OPENAGC_OK);
    EXPECT(openagc_gl_renderbuffer_get_info(renderbuffer, &gl_info), OPENAGC_OK);
    CHECK(vk_info.state == gl_info.state && vk_info.owner == gl_info.owner);
    CHECK(vk_info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);
    memset(vk_pixels, 0, sizeof(vk_pixels));
    memset(gl_pixels, 0, sizeof(gl_pixels));
    EXPECT(openagc_vk_image_readback(target, 0u, vk_pixels, sizeof(vk_pixels)), OPENAGC_OK);
    EXPECT(openagc_gl_read_pixels(framebuffer, 0u, gl_pixels, sizeof(gl_pixels)), OPENAGC_OK);
    CHECK(memcmp(vk_pixels, gl_pixels, sizeof(vk_pixels)) == 0);

    for (byte = 0u; byte < sizeof(pattern); ++byte) {
        pattern[byte] = (uint8_t)(0x80u + byte);
    }
    EXPECT(openagc_vk_create_buffer(device, &transfer_desc, &transfer), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(transfer, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &upload), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer_to_image(upload, transfer, 0u, sampled, 0u,
                                               sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(upload), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, upload, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_PIXEL_UNPACK_BUFFER, 64u, &unpack),
           OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(unpack, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    EXPECT(openagc_gl_tex_sub_image_from_buffer(texture, unpack, 0u, 0u, sizeof(pattern)),
           OPENAGC_OK);
    memset(vk_upload, 0, sizeof(vk_upload));
    memset(gl_upload, 0, sizeof(gl_upload));
    EXPECT(openagc_vk_image_readback(sampled, 0u, vk_upload, sizeof(vk_upload)), OPENAGC_OK);
    EXPECT(openagc_gl_get_tex_image(texture, 0u, gl_upload, sizeof(gl_upload)), OPENAGC_OK);
    CHECK(memcmp(vk_upload, gl_upload, sizeof(pattern)) == 0);
    CHECK(memcmp(vk_upload, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_vk_command_buffer_begin(upload), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image_to_buffer(upload, sampled, 0u, transfer, 32u,
                                               sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(upload), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, upload, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_PIXEL_PACK_BUFFER, 64u, &pack), OPENAGC_OK);
    EXPECT(openagc_gl_get_tex_image_to_buffer(texture, 0u, pack, 0u, sizeof(pattern)),
           OPENAGC_OK);
    memset(vk_upload, 0, sizeof(vk_upload));
    memset(gl_upload, 0, sizeof(gl_upload));
    EXPECT(openagc_vk_buffer_readback(transfer, 32u, vk_upload, sizeof(vk_upload)), OPENAGC_OK);
    EXPECT(openagc_gl_get_buffer_sub_data(pack, 0u, gl_upload, sizeof(gl_upload)), OPENAGC_OK);
    CHECK(memcmp(vk_upload, gl_upload, sizeof(pattern)) == 0);
    CHECK(memcmp(vk_upload, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_gl_destroy_buffer(pack), OPENAGC_OK);
    EXPECT(openagc_vk_image_get_info(sampled, &vk_info), OPENAGC_OK);
    EXPECT(openagc_gl_texture_get_info(texture, &gl_info), OPENAGC_OK);
    CHECK(vk_info.state == gl_info.state && vk_info.owner == gl_info.owner);
    EXPECT(openagc_vk_destroy_command_buffer(upload), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(transfer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(unpack), OPENAGC_OK);

    EXPECT(openagc_vk_get_format_properties(instance, 0u, &vk_format), OPENAGC_OK);
    EXPECT(openagc_gl_get_format(gl, 0u, &gl_native, &gl_backend), OPENAGC_OK);
    CHECK(vk_format.backend_format == gl_backend);
    CHECK(gl_backend == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    CHECK(vk_format.format == OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    CHECK(gl_native == OPENAGC_GL_RGBA8);
    EXPECT(openagc_vk_get_format_properties(instance, 2u, &vk_format), OPENAGC_OK);
    CHECK(vk_format.format == OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT(openagc_vk_get_format_properties(instance, 3u, &vk_format),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_get_format(gl, 1u, &gl_native, &gl_backend), OPENAGC_OK);
    CHECK(gl_native == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8);
    EXPECT(openagc_gl_get_format(gl, 2u, &gl_native, &gl_backend), OPENAGC_ERROR_OUT_OF_RANGE);

    EXPECT(openagc_gl_create_texture(gl, &gl_image_desc, &attached), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(gl, &texture_fb), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_texture(texture_fb, attached), OPENAGC_OK);
    EXPECT(openagc_gl_texture_get_info(attached, &gl_info), OPENAGC_OK);
    EXPECT(openagc_vk_image_get_info(target, &vk_info), OPENAGC_OK);
    CHECK(gl_info.state == vk_info.state && gl_info.owner == vk_info.owner);
    EXPECT(openagc_gl_destroy_texture(attached), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_destroy_framebuffer(texture_fb), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(attached), OPENAGC_OK);

    artifact.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact.code = code;
    artifact.code_size = 4u;
    artifact.workgroup_x = 8u;
    artifact.workgroup_y = 1u;
    artifact.workgroup_z = 1u;
    memcpy(artifact.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_vk_create_compute_pipeline(device, &artifact, &vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_gl_create_program(gl, &artifact, &gl_program), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_cmd_dispatch(commands, 0u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 65536u, 1u, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gl_bind_texture_for_sampling(gl, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_use_program(gl, gl_program), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 0u, 1u), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 65536u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_bind_texture_for_sampling(gl, texture), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_bind_texture_for_sampling(gl, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);
    EXPECT(openagc_vk_pipeline_get_info(vk_pipeline, &vk_plan), OPENAGC_OK);
    EXPECT(openagc_gl_program_get_info(gl_program, &gl_plan), OPENAGC_OK);
    CHECK(vk_plan.kind == gl_plan.kind);
    CHECK(vk_plan.compiler_verified == 0u && gl_plan.compiler_verified == 0u);
    CHECK(vk_plan.gpu_executable == 0u && gl_plan.gpu_executable == 0u);

    EXPECT(openagc_vk_create_buffer(device, &uniform_desc, &vk_uniform), OPENAGC_OK);
    EXPECT(openagc_vk_create_descriptor_set(device, &descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_set(descriptors, vk_uniform), OPENAGC_OK);
    EXPECT(openagc_vk_create_image_view(device, target, &sampled_view_desc, &color_view),
           OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_image(descriptors, color_view),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_create_image_view(device, sampled, &sampled_view_desc, &sampled_view),
           OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_image(descriptors, sampled_view), OPENAGC_OK);
    EXPECT(openagc_vk_create_sampler(device, OPENAGC_FRONTEND_VK_FILTER_LINEAR,
                                     OPENAGC_FRONTEND_VK_FILTER_NEAREST,
                                     OPENAGC_FRONTEND_VK_SAMPLER_ADDRESS_CLAMP_TO_EDGE,
                                     &vk_sampler),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_create_sampler(device, OPENAGC_FRONTEND_VK_FILTER_NEAREST,
                                     OPENAGC_FRONTEND_VK_FILTER_NEAREST,
                                     OPENAGC_FRONTEND_VK_SAMPLER_ADDRESS_CLAMP_TO_EDGE,
                                     &vk_sampler),
           OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_sampler(descriptors, vk_sampler), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_sampler(vk_sampler), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_create_sampler(gl, OPENAGC_FRONTEND_GL_LINEAR, OPENAGC_FRONTEND_GL_NEAREST,
                                     OPENAGC_FRONTEND_GL_CLAMP_TO_EDGE, &gl_sampler),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_create_sampler(gl, OPENAGC_FRONTEND_GL_NEAREST, OPENAGC_FRONTEND_GL_NEAREST,
                                     OPENAGC_FRONTEND_GL_CLAMP_TO_EDGE, &gl_sampler),
           OPENAGC_OK);
    EXPECT(openagc_gl_bind_sampler(gl, gl_sampler), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_sampler(gl_sampler), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_destroy_image_view(sampled_view), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_image_get_info(sampled, &vk_info), OPENAGC_OK);
    EXPECT(openagc_gl_texture_get_info(texture, &gl_info), OPENAGC_OK);
    CHECK(vk_info.state == gl_info.state && vk_info.owner == gl_info.owner);
    EXPECT(openagc_vk_destroy_buffer(vk_uniform), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 256u, &gl_uniform),
           OPENAGC_OK);
    EXPECT(openagc_gl_bind_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, gl_uniform), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_uniform), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_buffer_get_info(vk_uniform, &vk_buffer_info), OPENAGC_OK);
    EXPECT(openagc_gl_buffer_get_info(gl_uniform, &gl_buffer_info), OPENAGC_OK);
    CHECK(vk_buffer_info.usage == gl_buffer_info.usage);
    CHECK((vk_buffer_info.usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) != 0u);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_sampler(vk_sampler), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image_view(sampled_view), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image_view(color_view), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_uniform), OPENAGC_OK);
    EXPECT(openagc_gl_bind_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_uniform), OPENAGC_OK);

    EXPECT(openagc_vk_create_render_pass(device, sampled, &vk_pass), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(target), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 5u, 4u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_set_scissor(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
    openagc_vk_buffer_desc vertex_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 64u);
    openagc_vk_buffer_desc not_vertex_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64u);
    openagc_vk_buffer *not_vertex = NULL;
    EXPECT(openagc_vk_create_buffer(device, &not_vertex_desc, &not_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &vertex_desc, &vk_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, not_vertex, 0u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_destroy_buffer(not_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, vk_vertex, 0u), OPENAGC_OK);
    openagc_vk_buffer_desc index_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        64u);
    openagc_vk_buffer *vk_index = NULL;
    openagc_gl_buffer *gl_index = NULL;
    EXPECT(openagc_vk_create_buffer(device, &index_desc, &vk_index), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_index_buffer(commands, vk_vertex, 0u,
                                           OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_cmd_bind_index_buffer(commands, vk_index, 0u,
                                           OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32),
           OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_index), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_ELEMENT_ARRAY_BUFFER, 64u, &gl_index),
           OPENAGC_OK);
    {
        uint32_t indices[16] = { 0u };

        EXPECT(openagc_gl_buffer_data(gl_index, 0u, indices, sizeof(indices)), OPENAGC_OK);
    }
    EXPECT(openagc_gl_bind_index_buffer(framebuffer, gl_index, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_index), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_destroy_buffer(vk_vertex), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_ARRAY_BUFFER, 64u, &gl_vertex), OPENAGC_OK);
    EXPECT(openagc_gl_bind_vertex_buffer(framebuffer, gl_vertex, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_vertex), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_viewport(framebuffer, 0u, 0u, 5u, 4u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_viewport(framebuffer, 0u, 0u, 4u, 4u), OPENAGC_OK);
    EXPECT(openagc_gl_scissor(framebuffer, 0u, 0u, 4u, 4u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_index), OPENAGC_OK);
    EXPECT(openagc_vk_create_semaphore(device, &wait_sem), OPENAGC_OK);
    EXPECT(openagc_vk_create_semaphore(device, &signal_sem), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &kick), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(kick), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(kick), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_wait(device, commands, wait_sem, 1u, signal_sem),
           OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_queue_submit_wait(device, kick, NULL, 0u, wait_sem), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_wait(device, commands, wait_sem, 1u, signal_sem), OPENAGC_OK);
    EXPECT(openagc_vk_semaphore_poll(signal_sem, 1u, &sync_info), OPENAGC_OK);
    EXPECT(openagc_vk_semaphore_poll(signal_sem, 2u, &sync_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_client_wait(gl, 1u, &sync_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_finish(gl), OPENAGC_OK);
    EXPECT(openagc_gl_client_wait(gl, 1u, &sync_info), OPENAGC_OK);
    EXPECT(openagc_gl_client_wait(gl, 2u, &sync_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_destroy_command_buffer(kick), OPENAGC_OK);

    EXPECT(openagc_vk_destroy_semaphore(wait_sem), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_semaphore(signal_sem), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_pipeline(vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_program), OPENAGC_OK);
    {
        openagc_shader_binding_decl binding = {
            0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
        };
        openagc_vk_buffer_desc reflected_desc = OPENAGC_VK_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64u);
        openagc_vk_buffer *reflected = NULL;
        openagc_gl_buffer *gl_reflected = NULL;
        openagc_vk_pipeline *bound_pipeline = NULL;
        openagc_gl_program *bound_program = NULL;
        artifact.bindings = &binding;
        artifact.binding_count = 1u;
        EXPECT(openagc_vk_create_compute_pipeline(device, &artifact, &bound_pipeline),
               OPENAGC_ERROR_INVALID_ARGUMENT);
        EXPECT(openagc_gl_create_program(gl, &artifact, &bound_program),
               OPENAGC_ERROR_INVALID_ARGUMENT);
        EXPECT(openagc_vk_create_buffer(device, &reflected_desc, &reflected), OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_reflected),
               OPENAGC_OK);
        EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
                   device, &artifact, reflected, 0u, 16u, 0u, NULL, 0u, &bound_pipeline),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_program_with_bindings(gl, &artifact, gl_reflected, 0u, 16u, 0u,
                                                       NULL, 0u, &bound_program),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(bound_pipeline, &vk_plan), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(bound_program, &gl_plan), OPENAGC_OK);
        CHECK(vk_plan.resource_count == 1u && gl_plan.resource_count == 1u);
        CHECK(vk_plan.compiler_verified == 0u && gl_plan.gpu_executable == 0u);
        EXPECT(openagc_vk_destroy_buffer(reflected), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_gl_destroy_buffer(gl_reflected), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, bound_pipeline), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_descriptor_set(device, &descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, reflected, 0u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, bound_pipeline, &descriptors),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_set(descriptors, reflected), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, reflected, 0u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        {
            openagc_vk_pipeline *same_pipeline = NULL;
            openagc_vk_pipeline *wide_pipeline = NULL;
            openagc_frontend_pipeline_layout *layout = NULL;
            openagc_gl_program *same_program = NULL;

            EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
                       device, &artifact, reflected, 0u, 16u, 0u, NULL, 0u, &same_pipeline),
                   OPENAGC_OK);
            EXPECT(openagc_vk_pipeline_get_layout(bound_pipeline, &layout), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, same_pipeline), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
            EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
                       device, &artifact, reflected, 0u, 32u, 0u, NULL, 0u, &wide_pipeline),
                   OPENAGC_OK);
            EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, wide_pipeline), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
            EXPECT(openagc_gl_create_program_with_bindings(gl, &artifact, gl_reflected, 0u, 16u, 0u,
                                                           NULL, 0u, &same_program),
                   OPENAGC_OK);
            EXPECT(openagc_gl_program_accepts_layout(same_program, layout), OPENAGC_OK);
            EXPECT(openagc_gl_program_accepts_layout(bound_program, layout), OPENAGC_OK);
            EXPECT(openagc_vk_destroy_pipeline(same_pipeline), OPENAGC_OK);
            EXPECT(openagc_vk_destroy_pipeline(wide_pipeline), OPENAGC_OK);
            EXPECT(openagc_gl_destroy_program(same_program), OPENAGC_OK);
        }
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        descriptors = NULL;
        EXPECT(openagc_gl_use_program(gl, bound_program), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, gl_reflected), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_reflected, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_bind_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_pipeline(bound_pipeline), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(bound_program), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(reflected), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_reflected), OPENAGC_OK);
        artifact.bindings = NULL;
        artifact.binding_count = 0u;
    }
    {
        openagc_shader_binding_decl pair[2] = {
            { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u },
            { 0u, 1u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u }
        };
        openagc_vk_buffer_desc pair_desc = OPENAGC_VK_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64u);
        openagc_vk_buffer *first = NULL;
        openagc_vk_buffer *second = NULL;
        const openagc_vk_buffer *vk_list[2];
        openagc_gl_buffer *gl_first = NULL;
        openagc_gl_buffer *gl_second = NULL;
        const openagc_gl_buffer *gl_list[2];
        uint64_t pair_offsets[2] = { 0u, 0u };
        uint64_t pair_sizes[2] = { 16u, 16u };
        uint32_t pair_bindings[2] = { 0u, 1u };
        openagc_vk_buffer *wrong = NULL;
        openagc_gl_buffer *gl_wrong = NULL;
        openagc_vk_pipeline *pair_pipeline = NULL;
        openagc_gl_program *pair_program = NULL;
        artifact.bindings = pair;
        artifact.binding_count = 2u;
        EXPECT(openagc_vk_create_buffer(device, &pair_desc, &first), OPENAGC_OK);
        EXPECT(openagc_vk_create_buffer(device, &pair_desc, &second), OPENAGC_OK);
        EXPECT(openagc_vk_create_buffer(device, &pair_desc, &wrong), OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_first),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_second),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_wrong),
               OPENAGC_OK);
        vk_list[0] = first;
        vk_list[1] = second;
        gl_list[0] = gl_first;
        gl_list[1] = gl_second;
        EXPECT(openagc_vk_create_compute_pipeline_with_resources(
                   device, &artifact, vk_list, pair_offsets, pair_sizes, pair_bindings, 2u,
                   NULL, NULL, 0u, &pair_pipeline),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_program_with_resources(
                   gl, &artifact, gl_list, pair_offsets, pair_sizes, pair_bindings, 2u, NULL,
                   NULL, 0u, &pair_program),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(pair_pipeline, &vk_plan), OPENAGC_OK);
        CHECK(vk_plan.resource_count == 2u && vk_plan.gpu_executable == 0u);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, pair_pipeline), OPENAGC_OK);
        EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, pair_pipeline, &descriptors),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_set_at(descriptors, first, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_set_at(descriptors, wrong, 1u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_set_at(descriptors, second, 1u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, first, 0u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, second, 1u, 0u, 32u),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, second, 1u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        descriptors = NULL;
        EXPECT(openagc_gl_use_program(gl, pair_program), OPENAGC_OK);
        EXPECT(openagc_gl_bind_uniform_base(gl, 0u, gl_first), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_base(gl, 1u, gl_wrong), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_base(gl, 1u, gl_second), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_first, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_uniform_range(gl, 1u, gl_second, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_bind_uniform_base(gl, 0u, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_bind_uniform_base(gl, 1u, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_pipeline(pair_pipeline), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(pair_program), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(first), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(second), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(wrong), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_first), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_second), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_wrong), OPENAGC_OK);
        artifact.bindings = NULL;
        artifact.binding_count = 0u;
    }
    {
        openagc_shader_texture_decl sampled_decl = {
            0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM
        };
        openagc_vk_pipeline *textured = NULL;
        openagc_gl_program *gl_textured = NULL;
        artifact.textures = &sampled_decl;
        artifact.texture_count = 1u;
        EXPECT(openagc_vk_create_compute_pipeline(device, &artifact, &textured),
               OPENAGC_ERROR_INVALID_ARGUMENT);
        EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
                   device, &artifact, NULL, 0u, 0u, 0u, target, 1u, &textured),
               OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
                   device, &artifact, NULL, 0u, 0u, 0u, sampled, 1u, &textured),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_program_with_bindings(gl, &artifact, NULL, 0u, 0u, 0u, texture,
                                                       1u, &gl_textured),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(textured, &vk_plan), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_textured, &gl_plan), OPENAGC_OK);
        CHECK(vk_plan.texture_count == 1u && gl_plan.texture_count == 1u);
        CHECK(vk_plan.resource_count == 0u && gl_plan.gpu_executable == 0u);
        EXPECT(openagc_vk_destroy_image(sampled), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_gl_destroy_texture(texture), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, textured), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, textured, &descriptors),
               OPENAGC_OK);
        EXPECT(openagc_vk_create_image_view(device, sampled, &sampled_view_desc, &sampled_view),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_image(descriptors, sampled_view), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_image_at(descriptors, sampled_view, 1u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_sampler(device, OPENAGC_FRONTEND_VK_FILTER_NEAREST,
                                         OPENAGC_FRONTEND_VK_FILTER_NEAREST,
                                         OPENAGC_FRONTEND_VK_SAMPLER_ADDRESS_CLAMP_TO_EDGE,
                                         &vk_sampler),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_sampler(descriptors, vk_sampler), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_pipeline_set_sampler(textured, vk_sampler), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_sampler(vk_sampler), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        descriptors = NULL;
        EXPECT(openagc_vk_destroy_image_view(sampled_view), OPENAGC_OK);
        sampled_view = NULL;
        EXPECT(openagc_gl_use_program(gl, gl_textured), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_texture_for_sampling(gl, texture), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_texture_unit(gl, 1u, texture), OPENAGC_OK);
        EXPECT(openagc_gl_bind_sampler(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_sampler(gl, gl_sampler), OPENAGC_OK);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_program_set_sampler(gl_textured, gl_sampler), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_sampler(gl_sampler), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_bind_texture_unit(gl, 1u, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_bind_texture_for_sampling(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_pipeline(textured), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(gl_textured), OPENAGC_OK);
        artifact.textures = NULL;
        artifact.texture_count = 0u;
    }
    vertex_stage.stage = OPENAGC_SHADER_STAGE_VERTEX;
    vertex_stage.code = code;
    vertex_stage.code_size = 4u;
    vertex_stage.vertex_position_written = 1u;
    vertex_stage.varying_output_mask = 3u;
    vertex_stage.varying_input_mask = 1u;
    memcpy(vertex_stage.code_sha256, code_hash, sizeof(code_hash));
    pixel_stage.stage = OPENAGC_SHADER_STAGE_PIXEL;
    pixel_stage.code = code;
    pixel_stage.code_size = 4u;
    pixel_stage.varying_input_mask = 2u;
    pixel_stage.color_output_mask = 1u;
    pixel_stage.color_export_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
    memcpy(pixel_stage.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_vk_create_graphics_pipeline(device, &vertex_stage, &pixel_stage, sampled,
                                               &vk_graphics),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_create_graphics_pipeline(device, &vertex_stage, &pixel_stage, target,
                                               &vk_graphics),
           OPENAGC_OK);
    EXPECT(openagc_gl_create_graphics_program(gl, &vertex_stage, &pixel_stage, renderbuffer,
                                              &gl_graphics),
           OPENAGC_OK);
    EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_pipeline(vk_graphics), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_create_buffer(device, &index_desc, &vk_index), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_index_buffer(commands, vk_index, 0u,
                                           OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 3u, 1u, 0u, 0, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_create_buffer(device, &vertex_desc, &vk_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, vk_vertex, 0u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 3u, 1u, 0u, 0, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_pipeline_set_vertex_input(vk_graphics, 4u, 4u, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_pipeline_set_vertex_input(vk_graphics, 8u, 4u, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_pipeline_set_vertex_stride(vk_graphics, 4u), OPENAGC_OK);
    {
        uint32_t saved_mask = vertex_stage.varying_input_mask;
        openagc_vk_pipeline *split = NULL;
        openagc_gl_program *gl_split = NULL;
        uint32_t offsets[2] = { 0u, 4u };
        uint32_t sizes[2] = { 4u, 4u };

        vertex_stage.varying_input_mask = 3u;
        EXPECT(openagc_vk_create_graphics_pipeline(device, &vertex_stage, &pixel_stage, target,
                                                   &split),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_set_vertex_input(split, 8u, 0u, 4u), OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_pipeline_set_vertex_attributes(split, 8u, offsets, sizes, 2u),
               OPENAGC_OK);
        {
            uint32_t read_offset = 0u;
            uint32_t read_bytes = 0u;

            EXPECT(openagc_vk_pipeline_get_vertex_attribute(split, 1u, &read_offset, &read_bytes),
                   OPENAGC_OK);
            CHECK(read_offset == 4u && read_bytes == 4u);
            EXPECT(openagc_vk_pipeline_get_vertex_attribute(split, 2u, &read_offset, &read_bytes),
                   OPENAGC_ERROR_OUT_OF_RANGE);
        }
        EXPECT(openagc_gl_create_graphics_program(gl, &vertex_stage, &pixel_stage, renderbuffer,
                                                  &gl_split),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_attributes(gl_split, 8u, offsets, sizes, 2u),
               OPENAGC_OK);
        {
            uint32_t read_offset = 0u;
            uint32_t read_bytes = 0u;

            EXPECT(openagc_gl_program_get_vertex_attribute(gl_split, 0u, &read_offset, &read_bytes),
                   OPENAGC_OK);
            CHECK(read_offset == 0u && read_bytes == 4u);
        }
        EXPECT(openagc_vk_destroy_pipeline(split), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(gl_split), OPENAGC_OK);
        vertex_stage.varying_input_mask = saved_mask;
    }
    EXPECT(openagc_gl_program_set_vertex_input(gl_graphics, 4u, 0u, 4u), OPENAGC_OK);
    {
        uint32_t attribute_offset = 0u;
        uint32_t vk_native[1] = { OPENAGC_FRONTEND_VK_FORMAT_R32_SFLOAT };
        uint32_t gl_components[1] = { 1u };
        openagc_frontend_vertex_format vk_format = 0u;
        openagc_frontend_vertex_format gl_format = 0u;
        uint32_t vk_bytes = 0u;
        uint32_t gl_bytes = 0u;

        EXPECT(openagc_vk_pipeline_set_vertex_formats(vk_graphics, 4u, &attribute_offset, vk_native,
                                                      1u),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_formats(gl_graphics, 4u, &attribute_offset,
                                                     gl_components, OPENAGC_FRONTEND_GL_FLOAT, 1u),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_vertex_format(vk_graphics, 0u, &attribute_offset, &vk_format,
                                                     &vk_bytes),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_get_vertex_format(gl_graphics, 0u, &attribute_offset, &gl_format,
                                                    &gl_bytes),
               OPENAGC_OK);
        CHECK(vk_format == gl_format && vk_format == OPENAGC_FRONTEND_VERTEX_R32_SFLOAT);
        CHECK(vk_bytes == 4u && gl_bytes == 4u);
        vk_native[0] = OPENAGC_FRONTEND_VK_FORMAT_R32G32_SFLOAT;
        EXPECT(openagc_vk_pipeline_set_vertex_formats(vk_graphics, 4u, &attribute_offset, vk_native,
                                                      1u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_pipeline_get_vertex_format(vk_graphics, 0u, &attribute_offset, &vk_format,
                                                     &vk_bytes),
               OPENAGC_OK);
        CHECK(vk_format == OPENAGC_FRONTEND_VERTEX_R32_SFLOAT && vk_bytes == 4u);
        EXPECT(openagc_gl_program_set_vertex_formats(gl_graphics, 4u, &attribute_offset,
                                                     gl_components, 0x1402u, 1u),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    }
    {
        uint32_t per_instance[1] = { 1u };
        uint32_t per_vertex[1] = { 0u };
        uint32_t refused[1] = { 2u };

        EXPECT(openagc_vk_pipeline_set_vertex_rates(vk_graphics, per_instance, 1u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_divisors(gl_graphics, per_instance, 1u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 1u, 1u, 0u, 15u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_draw(commands, 1u, 1u, 0u, 16u), OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_pipeline_set_vertex_rates(vk_graphics, refused, 1u),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_vk_cmd_draw(commands, 1u, 1u, 0u, 16u), OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_pipeline_set_vertex_rates(vk_graphics, per_vertex, 1u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_divisors(gl_graphics, per_vertex, 1u), OPENAGC_OK);
    }
    {
        openagc_frontend_primitive vk_primitive = 0u;
        openagc_frontend_primitive gl_primitive = 0u;

        EXPECT(openagc_vk_pipeline_get_primitive(vk_graphics, &vk_primitive), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_primitive(gl_graphics, &gl_primitive), OPENAGC_OK);
        CHECK(vk_primitive == gl_primitive &&
              vk_primitive == OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_LIST);
        EXPECT(openagc_vk_pipeline_set_topology(
                   vk_graphics, OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_LINE_LIST),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_set_primitive(gl_graphics, OPENAGC_FRONTEND_GL_LINES),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_primitive(vk_graphics, &vk_primitive), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_primitive(gl_graphics, &gl_primitive), OPENAGC_OK);
        CHECK(vk_primitive == gl_primitive && vk_primitive == OPENAGC_FRONTEND_PRIMITIVE_LINE_LIST);
        EXPECT(openagc_vk_pipeline_set_topology(vk_graphics, 7u),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_vk_pipeline_get_primitive(vk_graphics, &vk_primitive), OPENAGC_OK);
        CHECK(vk_primitive == OPENAGC_FRONTEND_PRIMITIVE_LINE_LIST);
        EXPECT(openagc_vk_cmd_draw(commands, 1u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_pipeline_set_topology(
                   vk_graphics, OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_set_primitive(gl_graphics, OPENAGC_FRONTEND_GL_TRIANGLES),
               OPENAGC_OK);
    }
    {
        uint8_t written[4] = { 1u, 2u, 3u, 4u };
        uint8_t vk_read[4] = { 0u };
        uint8_t gl_read[4] = { 0u };

        EXPECT(openagc_vk_cmd_push_constants(commands, vk_graphics, 0u, written, 4u),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_uniform(gl_graphics, 0u, written, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_read_push_constants(vk_graphics, 0u, vk_read, 4u),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_read_uniform(gl_graphics, 0u, gl_read, 4u), OPENAGC_OK);
        CHECK(memcmp(vk_read, gl_read, 4u) == 0 && memcmp(vk_read, written, 4u) == 0);
        EXPECT(openagc_vk_cmd_push_constants(commands, vk_graphics, 128u, written, 4u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_pipeline_read_push_constants(vk_graphics, 0u, vk_read, 4u),
               OPENAGC_OK);
        CHECK(memcmp(vk_read, written, 4u) == 0);
    }
    {
        uint32_t vk_enable = 1u;
        uint32_t gl_enable = 1u;
        openagc_frontend_blend_factor vk_src = 0u;
        openagc_frontend_blend_factor vk_dst = 0u;
        openagc_frontend_blend_factor gl_src = 0u;
        openagc_frontend_blend_factor gl_dst = 0u;

        EXPECT(openagc_vk_pipeline_get_blend(vk_graphics, &vk_enable, &vk_src, &vk_dst),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_get_blend(gl_graphics, &gl_enable, &gl_src, &gl_dst),
               OPENAGC_OK);
        CHECK(vk_enable == 0u && gl_enable == 0u);
        CHECK(vk_src == gl_src && vk_src == OPENAGC_FRONTEND_BLEND_ONE);
        CHECK(vk_dst == gl_dst && vk_dst == OPENAGC_FRONTEND_BLEND_ZERO);
        EXPECT(openagc_vk_pipeline_set_blend(vk_graphics, 1u,
                                             OPENAGC_FRONTEND_VK_BLEND_FACTOR_SRC_ALPHA,
                                             OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_set_blend(gl_graphics, 1u, OPENAGC_FRONTEND_GL_SRC_ALPHA,
                                            OPENAGC_FRONTEND_GL_ONE_MINUS_SRC_ALPHA),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_blend(vk_graphics, &vk_enable, &vk_src, &vk_dst),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_get_blend(gl_graphics, &gl_enable, &gl_src, &gl_dst),
               OPENAGC_OK);
        CHECK(vk_enable == 1u && gl_enable == 1u);
        CHECK(vk_src == gl_src && vk_src == OPENAGC_FRONTEND_BLEND_SRC_ALPHA);
        CHECK(vk_dst == gl_dst && vk_dst == OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA);
        EXPECT(openagc_vk_pipeline_set_blend(vk_graphics, 1u, 2u,
                                             OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_vk_pipeline_get_blend(vk_graphics, &vk_enable, &vk_src, &vk_dst),
               OPENAGC_OK);
        CHECK(vk_src == OPENAGC_FRONTEND_BLEND_SRC_ALPHA);
    }
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
    {
        openagc_vk_buffer_desc indirect_desc = OPENAGC_VK_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            16u);
        openagc_vk_buffer *indirect = NULL;
        uint32_t record[4] = { 1u, 1u, 0u, 0u };

        EXPECT(openagc_vk_create_buffer(device, &indirect_desc, &indirect), OPENAGC_OK);
        EXPECT(openagc_vk_buffer_upload(indirect, 0u, record, sizeof(record)), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw_indirect(commands, indirect, 0u), OPENAGC_ERROR_NOT_READY);
        record[0] = 17u;
        EXPECT(openagc_vk_buffer_upload(indirect, 0u, record, sizeof(record)), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw_indirect(commands, indirect, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
        record[0] = 0u;
        EXPECT(openagc_vk_buffer_upload(indirect, 0u, record, sizeof(record)), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw_indirect(commands, indirect, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(indirect), OPENAGC_OK);
    }
    {
        openagc_vk_buffer *unbound = NULL;

        EXPECT(openagc_vk_create_unbound_buffer(device, &vertex_desc, &unbound), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, unbound, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 1u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, vk_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(unbound), OPENAGC_OK);
    }
    EXPECT(openagc_vk_cmd_draw(commands, 17u, 1u, 0u, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_draw(commands, 0u, 1u, 0u, 0u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 0u, 0u, 0u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 0u, 1u, 0u, 0, 0u), OPENAGC_OK);
    {
        uint32_t indices[16] = { 0u };

        EXPECT(openagc_vk_buffer_upload(vk_index, 0u, indices, sizeof(indices)), OPENAGC_OK);
    }
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 3u, 1u, 0u, 0, 0u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 1u, 1u, 0u, 15, 0u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 1u, 1u, 0u, 16, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 1u, 1u, 0u, -1, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_draw_indexed(commands, 17u, 1u, 0u, 0, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    {
        uint16_t shorts[32] = { 0u };

        shorts[0] = 16u;
        EXPECT(openagc_vk_buffer_upload(vk_index, 0u, shorts, sizeof(shorts)), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_index_buffer(commands, vk_index, 0u,
                                               OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT16),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw_indexed(commands, 1u, 1u, 0u, 0, 0u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        shorts[0] = 15u;
        EXPECT(openagc_vk_buffer_upload(vk_index, 0u, shorts, sizeof(shorts)), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw_indexed(commands, 1u, 1u, 0u, 0, 0u),
               OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_draw_indexed(commands, 32u, 1u, 0u, 0, 0u),
               OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_draw_indexed(commands, 33u, 1u, 0u, 0, 0u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_cmd_bind_index_buffer(commands, vk_index, 2u,
                                               OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32),
               OPENAGC_ERROR_OUT_OF_RANGE);
    }
    EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_vertex), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_index), OPENAGC_OK);
    EXPECT(openagc_gl_bind_program(framebuffer, gl_graphics), OPENAGC_OK);
    EXPECT(openagc_gl_bind_framebuffer(gl, framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_graphics), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_draw_arrays(gl, 0u, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_draw_elements(gl, 3u, OPENAGC_GL_UNSIGNED_INT, 0u),
           OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_draw_elements(gl, 3u, 0x1401u, 0u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_bind_index_buffer(framebuffer, gl_index, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_draw_elements(gl, 0u, OPENAGC_GL_UNSIGNED_INT, 0u), OPENAGC_OK);
    EXPECT(openagc_gl_draw_elements(gl, 3u, OPENAGC_GL_UNSIGNED_INT, 0u),
           OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_draw_elements(gl, 17u, OPENAGC_GL_UNSIGNED_INT, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_draw_elements(gl, 32u, OPENAGC_GL_UNSIGNED_SHORT, 0u),
           OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_draw_elements(gl, 33u, OPENAGC_GL_UNSIGNED_SHORT, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_draw_elements(gl, 1u, OPENAGC_GL_UNSIGNED_SHORT, 1u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_plan), OPENAGC_OK);
    EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_plan), OPENAGC_OK);
    CHECK(vk_plan.kind == OPENAGC_SHADER_PIPELINE_GRAPHICS);
    CHECK(vk_plan.kind == gl_plan.kind);
    CHECK(vk_plan.gpu_executable == 0u && gl_plan.gpu_executable == 0u);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
    {
        openagc_gl_buffer *indirect = NULL;
        uint32_t record[4] = { 1u, 1u, 0u, 0u };

        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_DRAW_INDIRECT_BUFFER, 16u, &indirect),
               OPENAGC_OK);
        EXPECT(openagc_gl_buffer_data(indirect, 0u, record, sizeof(record)), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays_indirect(gl, indirect, 0u), OPENAGC_ERROR_NOT_READY);
        record[0] = 17u;
        EXPECT(openagc_gl_buffer_data(indirect, 0u, record, sizeof(record)), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays_indirect(gl, indirect, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_gl_destroy_buffer(indirect), OPENAGC_OK);
    }
    {
        openagc_gl_buffer *unbound = NULL;

        EXPECT(openagc_gl_create_unbound_buffer(gl, OPENAGC_GL_ARRAY_BUFFER, 64u, &unbound),
               OPENAGC_OK);
        EXPECT(openagc_gl_bind_vertex_buffer(framebuffer, unbound, 0u), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 1u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_vertex_buffer(framebuffer, gl_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(unbound), OPENAGC_OK);
    }
    EXPECT(openagc_vk_destroy_pipeline(vk_graphics), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_graphics), OPENAGC_OK);
    {
        openagc_shader_binding_decl uniform_binding = {
            0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
        };
        openagc_shader_texture_decl texture_binding = {
            0u, 0u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM
        };
        openagc_vk_buffer_desc uniform_binding_desc = OPENAGC_VK_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64u);
        openagc_vk_buffer *graphics_uniform = NULL;
        openagc_gl_buffer *gl_graphics_uniform = NULL;
        uint8_t before[64];
        uint8_t after[64];
        vertex_stage.bindings = &uniform_binding;
        vertex_stage.binding_count = 1u;
        pixel_stage.textures = &texture_binding;
        pixel_stage.texture_count = 1u;
        EXPECT(openagc_vk_create_graphics_pipeline(device, &vertex_stage, &pixel_stage, target,
                                                   &vk_graphics),
               OPENAGC_ERROR_INVALID_ARGUMENT);
        EXPECT(openagc_vk_create_buffer(device, &uniform_binding_desc, &graphics_uniform),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_graphics_uniform),
               OPENAGC_OK);
        EXPECT(openagc_vk_create_graphics_pipeline_with_bindings(
                   device, &vertex_stage, &pixel_stage, target, graphics_uniform, 0u, 16u, 0u,
                   sampled, 0u, &vk_graphics),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_graphics_program_with_bindings(
                   gl, &vertex_stage, &pixel_stage, renderbuffer, gl_graphics_uniform, 0u, 16u, 0u,
                   texture, 0u, &gl_graphics),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_set_vertex_stride(vk_graphics, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_stride(gl_graphics, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_plan), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_plan), OPENAGC_OK);
        CHECK(vk_plan.resource_count == 1u && vk_plan.texture_count == 1u);
        CHECK(gl_plan.resource_count == 1u && gl_plan.texture_count == 1u);
        CHECK(vk_plan.gpu_executable == 0u && gl_plan.gpu_executable == 0u);
        EXPECT(openagc_vk_image_readback(target, 0u, before, sizeof(before)), OPENAGC_OK);
        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, vk_graphics, &descriptors),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, graphics_uniform, 0u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_create_image_view(device, sampled, &sampled_view_desc, &sampled_view),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_image(descriptors, sampled_view), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_create_buffer(device, &vertex_desc, &vk_vertex), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, vk_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_sampler(descriptors, vk_sampler), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_pipeline_set_sampler(vk_graphics, vk_sampler), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_image_readback(target, 0u, after, sizeof(after)), OPENAGC_OK);
        CHECK(memcmp(before, after, sizeof(before)) == 0);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_set_sampler(vk_graphics, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_sampler(vk_sampler), OPENAGC_OK);
        descriptors = NULL;
        EXPECT(openagc_vk_destroy_image_view(sampled_view), OPENAGC_OK);
        sampled_view = NULL;
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(vk_vertex), OPENAGC_OK);
        EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_read_pixels(framebuffer, 0u, before, sizeof(before)), OPENAGC_OK);
        EXPECT(openagc_gl_bind_framebuffer(gl, framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_program(framebuffer, gl_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_viewport(framebuffer, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_graphics_uniform, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_texture_for_sampling(gl, texture), OPENAGC_OK);
        EXPECT(openagc_gl_bind_vertex_buffer(framebuffer, gl_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_sampler(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_sampler(gl, gl_sampler), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_program_set_sampler(gl_graphics, gl_sampler), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_read_pixels(framebuffer, 0u, after, sizeof(after)), OPENAGC_OK);
        CHECK(memcmp(before, after, sizeof(before)) == 0);
        EXPECT(openagc_gl_bind_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_bind_texture_for_sampling(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_pipeline(vk_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(gl_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(graphics_uniform), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_graphics_uniform), OPENAGC_OK);
        vk_graphics = NULL;
        gl_graphics = NULL;
        framebuffer = NULL;
    }
    {
        openagc_shader_binding_decl pair[2] = {
            { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u },
            { 0u, 1u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u }
        };
        openagc_vk_buffer_desc pair_desc = OPENAGC_VK_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64u);
        openagc_vk_buffer *first = NULL;
        openagc_vk_buffer *second = NULL;
        const openagc_vk_buffer *vk_list[2];
        openagc_gl_buffer *gl_first = NULL;
        openagc_gl_buffer *gl_second = NULL;
        const openagc_gl_buffer *gl_list[2];
        uint64_t pair_offsets[2] = { 0u, 0u };
        uint64_t pair_sizes[2] = { 16u, 16u };
        uint32_t pair_bindings[2] = { 0u, 1u };
        vertex_stage.bindings = pair;
        vertex_stage.binding_count = 2u;
        pixel_stage.textures = NULL;
        pixel_stage.texture_count = 0u;
        EXPECT(openagc_vk_create_buffer(device, &pair_desc, &first), OPENAGC_OK);
        EXPECT(openagc_vk_create_buffer(device, &pair_desc, &second), OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_first),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_second),
               OPENAGC_OK);
        vk_list[0] = first;
        vk_list[1] = second;
        gl_list[0] = gl_first;
        gl_list[1] = gl_second;
        EXPECT(openagc_vk_create_graphics_pipeline_with_resources(
                   device, &vertex_stage, &pixel_stage, target, vk_list, pair_offsets, pair_sizes,
                   pair_bindings, 2u, NULL, NULL, 0u, &vk_graphics),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_graphics_program_with_resources(
                   gl, &vertex_stage, &pixel_stage, renderbuffer, gl_list, pair_offsets,
                   pair_sizes, pair_bindings, 2u, NULL, NULL, 0u, &gl_graphics),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_set_vertex_stride(vk_graphics, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_vertex_stride(gl_graphics, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_plan), OPENAGC_OK);
        CHECK(vk_plan.resource_count == 2u && vk_plan.texture_count == 0u);
        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, vk_graphics, &descriptors),
               OPENAGC_OK);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, first, 0u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, second, 1u, 0u, 16u),
               OPENAGC_OK);
        EXPECT(openagc_vk_create_buffer(device, &vertex_desc, &vk_vertex), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_vertex_buffer(commands, vk_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
        descriptors = NULL;
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(vk_vertex), OPENAGC_OK);
        EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_framebuffer(gl, framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_program(framebuffer, gl_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_viewport(framebuffer, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_vertex_buffer(framebuffer, gl_vertex, 0u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_first, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_bind_uniform_range(gl, 1u, gl_second, 0u, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_bind_uniform_base(gl, 0u, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_bind_uniform_base(gl, 1u, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_pipeline(vk_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_program(gl_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(first), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_buffer(second), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_first), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_second), OPENAGC_OK);
        vk_graphics = NULL;
        gl_graphics = NULL;
        framebuffer = NULL;
        vertex_stage.bindings = NULL;
        vertex_stage.binding_count = 0u;
    }
    {
        openagc_vk_query_pool *occlusion = NULL;
        openagc_vk_query_pool *timestamps = NULL;
        openagc_gl_query *gl_occlusion = NULL;
        uint32_t available = 1u;

        EXPECT(openagc_vk_create_query_pool(device, OPENAGC_FRONTEND_QUERY_OCCLUSION, 1u,
                                            &occlusion),
               OPENAGC_OK);
        EXPECT(openagc_vk_get_query(occlusion, 0u, &available), OPENAGC_ERROR_NOT_READY);
        CHECK(available == 0u);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_begin_query(commands, occlusion, 0u), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        {
            openagc_color clear = { 0x55u, 0x66u, 0x77u, 0x88u };
            uint8_t pixel[4] = { 0u, 0u, 0u, 0u };

            EXPECT(openagc_vk_cmd_begin_render_pass_with_load(
                       commands, vk_pass, OPENAGC_FRONTEND_LOAD_OP_CLEAR, clear),
                   OPENAGC_OK);
            EXPECT(openagc_vk_cmd_begin_query(commands, occlusion, 0u), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_end_query(commands), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_set_scissor(commands, 0u, 0u, 1u, 1u), OPENAGC_OK);
            {
                openagc_color pixel_clear = { 0xaau, 0u, 0u, 0xffu };
                uint8_t neighbor[4] = { 0u, 0u, 0u, 0u };

                EXPECT(openagc_vk_cmd_clear_attachments(commands, pixel_clear), OPENAGC_OK);
                EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
                EXPECT(openagc_vk_image_readback(target, 0u, pixel, sizeof(pixel)), OPENAGC_OK);
                EXPECT(openagc_vk_image_readback(target, 4u, neighbor, sizeof(neighbor)),
                       OPENAGC_OK);
                CHECK(pixel[0] == 0xaau && neighbor[0] == 0x55u);
            }
            EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
        }
        EXPECT(openagc_vk_cmd_begin_query(commands, occlusion, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_destroy_query_pool(occlusion), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_vk_cmd_end_query(commands), OPENAGC_OK);
        EXPECT(openagc_vk_get_query(occlusion, 0u, &available), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_create_query_pool(device, OPENAGC_FRONTEND_QUERY_TIMESTAMP, 1u,
                                            &timestamps),
               OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_write_timestamp(commands, timestamps, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_vk_cmd_write_timestamp(commands, timestamps, 0u), OPENAGC_OK);
        EXPECT(openagc_vk_get_query(timestamps, 0u, &available), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_query_pool(occlusion), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_query_pool(timestamps), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
        vk_pass = NULL;
        EXPECT(openagc_gl_create_query(gl, OPENAGC_FRONTEND_QUERY_OCCLUSION, 1u, &gl_occlusion),
               OPENAGC_OK);
        EXPECT(openagc_gl_begin_query(gl, gl_occlusion, 0u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_framebuffer(gl, framebuffer), OPENAGC_OK);
        {
            openagc_color clear = { 0x55u, 0x66u, 0x77u, 0x88u };
            uint8_t pixel[4] = { 0u, 0u, 0u, 0u };

            EXPECT(openagc_gl_framebuffer_begin(framebuffer, OPENAGC_FRONTEND_LOAD_OP_CLEAR, clear),
                   OPENAGC_OK);
            EXPECT(openagc_gl_read_pixels(framebuffer, 0u, pixel, sizeof(pixel)), OPENAGC_OK);
            CHECK(pixel[0] == 0x55u && pixel[3] == 0x88u);
            EXPECT(openagc_gl_scissor(framebuffer, 0u, 0u, 1u, 1u), OPENAGC_OK);
            {
                openagc_color pixel_clear = { 0xaau, 0u, 0u, 0xffu };
                uint8_t neighbor[4] = { 0u, 0u, 0u, 0u };

                EXPECT(openagc_gl_clear_scissor(framebuffer, pixel_clear), OPENAGC_OK);
                EXPECT(openagc_gl_read_pixels(framebuffer, 0u, pixel, sizeof(pixel)), OPENAGC_OK);
                EXPECT(openagc_gl_read_pixels(framebuffer, 4u, neighbor, sizeof(neighbor)),
                       OPENAGC_OK);
                CHECK(pixel[0] == 0xaau && neighbor[0] == 0x55u);
            }
        }
        EXPECT(openagc_gl_begin_query(gl, gl_occlusion, 0u), OPENAGC_OK);
        EXPECT(openagc_gl_get_query(gl_occlusion, 0u, &available), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_end_query(gl), OPENAGC_OK);
        EXPECT(openagc_gl_get_query(gl_occlusion, 0u, &available), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_query(gl_occlusion), OPENAGC_OK);
        framebuffer = NULL;
    }
    EXPECT(openagc_gl_destroy_buffer(gl_vertex), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_index), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_bind_sampler(gl, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_sampler(gl_sampler), OPENAGC_OK);
    EXPECT(openagc_gl_bind_texture_for_sampling(gl, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(texture), OPENAGC_OK);
    {
        openagc_vk_image_desc depth_desc = OPENAGC_VK_IMAGE_DESC_INIT(
            OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT,
            OPENAGC_FRONTEND_VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
        openagc_gl_image_desc gl_depth_desc = OPENAGC_GL_IMAGE_DESC_INIT(
            OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8, 4u, 4u);
        openagc_gl_image_desc gl_color_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
        openagc_vk_image *depth = NULL;
        openagc_gl_renderbuffer *gl_depth = NULL;
        openagc_gl_renderbuffer *gl_color = NULL;
        openagc_gl_framebuffer *depth_fb = NULL;
        openagc_frontend_image_info depth_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;

        EXPECT(openagc_vk_create_image(device, &depth_desc, &depth), OPENAGC_OK);
        EXPECT(openagc_vk_image_get_info(depth, &depth_info), OPENAGC_OK);
        CHECK(depth_info.native_format == OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT);
        EXPECT(openagc_vk_create_render_pass(device, depth, &vk_pass), OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_image_barrier(
                   commands, depth,
                   OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
               OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_image_get_info(depth, &depth_info), OPENAGC_OK);
        CHECK(depth_info.state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET);
        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_render_pass_attach_depth(vk_pass, depth), OPENAGC_OK);
        {
            openagc_color keep = { 0u, 0u, 0u, 0u };
            uint8_t color_before[4];
            uint8_t color_after[4];
            uint8_t packed[8];

            EXPECT(openagc_vk_image_readback(target, 0u, color_before, sizeof(color_before)),
                   OPENAGC_OK);
            EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_begin_render_pass_with_depth(
                       commands, vk_pass, OPENAGC_FRONTEND_LOAD_OP_LOAD, keep,
                       OPENAGC_FRONTEND_LOAD_OP_CLEAR, 1.f, 0x5au),
                   OPENAGC_OK);
            EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
            CHECK(packed[0] == 0xffu && packed[1] == 0xffu && packed[2] == 0xffu &&
                  packed[3] == 0x5au);
            CHECK(packed[4] == 0xffu && packed[7] == 0x5au);
            EXPECT(openagc_vk_cmd_set_scissor(commands, 0u, 0u, 1u, 1u), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_clear_depth(commands, 0.f, 0u), OPENAGC_OK);
            EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
            CHECK(packed[0] == 0u && packed[1] == 0u && packed[2] == 0u && packed[3] == 0u);
            CHECK(packed[4] == 0xffu && packed[7] == 0x5au);
            EXPECT(openagc_vk_image_readback(target, 0u, color_after, sizeof(color_after)),
                   OPENAGC_OK);
            CHECK(memcmp(color_before, color_after, sizeof(color_before)) == 0);
            EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
            EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        }
        EXPECT(openagc_vk_destroy_image(depth), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_vk_render_pass_attach_depth(vk_pass, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);
        vk_pass = NULL;
        EXPECT(openagc_vk_destroy_image(depth), OPENAGC_OK);

        EXPECT(openagc_gl_create_framebuffer(gl, &depth_fb), OPENAGC_OK);
        EXPECT(openagc_gl_create_renderbuffer(gl, &gl_depth_desc, &gl_depth), OPENAGC_OK);
        EXPECT(openagc_gl_renderbuffer_get_info(gl_depth, &depth_info), OPENAGC_OK);
        CHECK(depth_info.native_format == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8);
        EXPECT(openagc_gl_framebuffer_renderbuffer(depth_fb, gl_depth),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_gl_framebuffer_depth_renderbuffer(depth_fb, gl_depth),
               OPENAGC_ERROR_BAD_STATE);
        EXPECT(openagc_gl_create_renderbuffer(gl, &gl_color_desc, &gl_color), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(depth_fb, gl_color), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_depth_renderbuffer(depth_fb, gl_depth), OPENAGC_OK);
        EXPECT(openagc_gl_renderbuffer_get_info(gl_depth, &depth_info), OPENAGC_OK);
        CHECK(depth_info.state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET);
        {
            openagc_color keep = { 0u, 0u, 0u, 0u };
            uint8_t packed[8];

            EXPECT(openagc_gl_clear_depth(depth_fb, 1.f, 0x5au), OPENAGC_ERROR_BAD_STATE);
            EXPECT(openagc_gl_framebuffer_begin_with_depth(
                       depth_fb, OPENAGC_FRONTEND_LOAD_OP_LOAD, keep,
                       OPENAGC_FRONTEND_LOAD_OP_CLEAR, 1.f, 0x5au),
                   OPENAGC_OK);
            EXPECT(openagc_gl_renderbuffer_read(gl_depth, 0u, packed, sizeof(packed)),
                   OPENAGC_OK);
            CHECK(packed[0] == 0xffu && packed[3] == 0x5au && packed[4] == 0xffu &&
                  packed[7] == 0x5au);
            EXPECT(openagc_gl_scissor(depth_fb, 0u, 0u, 1u, 1u), OPENAGC_OK);
            EXPECT(openagc_gl_clear_depth(depth_fb, 0.f, 0u), OPENAGC_OK);
            EXPECT(openagc_gl_clear_depth(depth_fb, 2.f, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
            EXPECT(openagc_gl_renderbuffer_read(gl_depth, 0u, packed, sizeof(packed)),
                   OPENAGC_OK);
            CHECK(packed[0] == 0u && packed[3] == 0u && packed[4] == 0xffu &&
                  packed[7] == 0x5au);
        }
        EXPECT(openagc_gl_destroy_renderbuffer(gl_depth), OPENAGC_ERROR_BUSY);
        EXPECT(openagc_gl_destroy_framebuffer(depth_fb), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_renderbuffer(gl_depth), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_renderbuffer(gl_color), OPENAGC_OK);
    }
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(sampled), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(target), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_same_backend_state() != 0) {
        return 1;
    }
    puts("OpenAGC Vulkan and OpenGL share one backend");
    return 0;
}
