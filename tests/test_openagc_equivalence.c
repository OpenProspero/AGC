/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/opengl.h"
#include "openagc/vulkan.h"
#include "openagc/pm4_graphics_fw940.h"
#include "openagc/psbc_metadata.h"
#include "openagc/store_const_code.h"
#include "openagc/store_span_code.h"

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
                /* Load/clear pixel writes wait for queue submit. */
                EXPECT(openagc_vk_image_readback(target, 0u, pixel, sizeof(pixel)), OPENAGC_OK);
                CHECK(pixel[0] == 0x11u && pixel[3] == 0x44u);
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
        EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
        {
            uint8_t pixel[4] = { 0u, 0u, 0u, 0u };
            uint8_t neighbor[4] = { 0u, 0u, 0u, 0u };

            EXPECT(openagc_vk_image_readback(target, 0u, pixel, sizeof(pixel)), OPENAGC_OK);
            EXPECT(openagc_vk_image_readback(target, 4u, neighbor, sizeof(neighbor)),
                   OPENAGC_OK);
            CHECK(pixel[0] == 0xaau && neighbor[0] == 0x55u);
        }
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
            EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
            {
                uint8_t depth_before[8];

                memcpy(depth_before, packed, sizeof(depth_before));
                EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
                EXPECT(openagc_vk_cmd_begin_render_pass_with_depth(
                           commands, vk_pass, OPENAGC_FRONTEND_LOAD_OP_LOAD, keep,
                           OPENAGC_FRONTEND_LOAD_OP_CLEAR, 1.f, 0x5au),
                       OPENAGC_OK);
                EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
                CHECK(memcmp(packed, depth_before, sizeof(packed)) == 0);
                EXPECT(openagc_vk_cmd_set_scissor(commands, 0u, 0u, 1u, 1u), OPENAGC_OK);
                EXPECT(openagc_vk_cmd_clear_depth(commands, 0.f, 0u), OPENAGC_OK);
                EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
                CHECK(memcmp(packed, depth_before, sizeof(packed)) == 0);
                EXPECT(openagc_vk_image_readback(target, 0u, color_after, sizeof(color_after)),
                       OPENAGC_OK);
                CHECK(memcmp(color_before, color_after, sizeof(color_before)) == 0);
                EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
                EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
                EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
                EXPECT(openagc_vk_image_readback(depth, 0u, packed, sizeof(packed)), OPENAGC_OK);
                CHECK(packed[0] == 0u && packed[1] == 0u && packed[2] == 0u && packed[3] == 0u);
                CHECK(packed[4] == 0xffu && packed[7] == 0x5au);
                EXPECT(openagc_vk_image_readback(target, 0u, color_after, sizeof(color_after)),
                       OPENAGC_OK);
                CHECK(memcmp(color_before, color_after, sizeof(color_before)) == 0);
            }
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

static int test_copy_and_fill_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc gl_image_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
    openagc_vk_buffer_desc vk_buffer_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        64u);
    openagc_vk_image_desc vk_image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_buffer *input = NULL;
    openagc_vk_buffer *scratch = NULL;
    openagc_vk_image *source = NULL;
    openagc_vk_image *destination = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_texture *source_texture = NULL;
    openagc_gl_texture *destination_texture = NULL;
    openagc_gl_framebuffer *framebuffer = NULL;
    openagc_gl_buffer *pack = NULL;
    uint8_t pattern[64];
    uint8_t vk_pixels[64];
    uint8_t gl_pixels[64];
    uint8_t vk_words[16];
    uint8_t gl_words[16];
    uint32_t value = 0x11223344u;
    uint32_t index;
    uint32_t observed;

    for (index = 0u; index < sizeof(pattern); ++index) {
        pattern[index] = (uint8_t)(index + 1u);
    }
    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &vk_buffer_desc, &input), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &vk_buffer_desc, &scratch), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(input, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &source), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer_to_image(commands, input, 0u, source, 0u, sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image(commands, source, 0u, 0u, destination, 1u, 1u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_fill_buffer(commands, scratch, 0u, sizeof(vk_words), value), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer(commands, input, 0u, scratch, 16u, 16u), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);
    EXPECT(openagc_gl_create_texture(gl, &gl_image_desc, &source_texture), OPENAGC_OK);
    EXPECT(openagc_gl_create_texture(gl, &gl_image_desc, &destination_texture), OPENAGC_OK);
    EXPECT(openagc_gl_tex_sub_image(source_texture, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_texture(framebuffer, source_texture), OPENAGC_OK);
    EXPECT(openagc_gl_copy_tex_sub_image(framebuffer, destination_texture, 1u, 1u, 0u, 0u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_PIXEL_PACK_BUFFER, 64u, &pack), OPENAGC_OK);
    EXPECT(openagc_gl_clear_buffer_sub_data(pack, 0u, sizeof(gl_words), value), OPENAGC_OK);
    {
        openagc_gl_buffer *gl_src = NULL;
        openagc_gl_buffer *gl_dst = NULL;
        uint8_t vk_copy[16];
        uint8_t gl_copy[16];

        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_PIXEL_UNPACK_BUFFER, 64u, &gl_src),
               OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_PIXEL_PACK_BUFFER, 64u, &gl_dst),
               OPENAGC_OK);
        EXPECT(openagc_gl_buffer_data(gl_src, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
        EXPECT(openagc_gl_copy_buffer_sub_data(gl_src, 0u, gl_dst, 0u, 16u), OPENAGC_OK);
        memset(vk_copy, 0, sizeof(vk_copy));
        memset(gl_copy, 0, sizeof(gl_copy));
        EXPECT(openagc_vk_buffer_readback(scratch, 16u, vk_copy, sizeof(vk_copy)), OPENAGC_OK);
        EXPECT(openagc_gl_get_buffer_sub_data(gl_dst, 0u, gl_copy, sizeof(gl_copy)), OPENAGC_OK);
        CHECK(memcmp(vk_copy, gl_copy, sizeof(vk_copy)) == 0);
        CHECK(memcmp(vk_copy, pattern, sizeof(vk_copy)) == 0);
        {
            uint8_t composite[64];
            uint32_t observed;
            uint32_t i;
            openagc_vk_command_buffer *composite_cmds = NULL;
            openagc_vk_buffer *vk_src = NULL;
            openagc_vk_buffer *vk_dst = NULL;
            openagc_vk_buffer_desc both =
                OPENAGC_VK_BUFFER_DESC_INIT(OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            64u);

            EXPECT(openagc_vk_create_buffer(device, &both, &vk_src), OPENAGC_OK);
            EXPECT(openagc_vk_create_buffer(device, &both, &vk_dst), OPENAGC_OK);
            EXPECT(openagc_vk_buffer_upload(vk_src, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
            EXPECT(openagc_vk_allocate_command_buffer(pool, &composite_cmds), OPENAGC_OK);
            EXPECT(openagc_vk_command_buffer_begin(composite_cmds), OPENAGC_OK);
            EXPECT(openagc_vk_cmd_copy_buffer(composite_cmds, vk_src, 0u, vk_dst, 0u, 64u),
                   OPENAGC_OK);
            EXPECT(openagc_vk_cmd_fill_buffer(composite_cmds, vk_dst, 0u, 16u, value), OPENAGC_OK);
            EXPECT(openagc_vk_command_buffer_end(composite_cmds), OPENAGC_OK);
            EXPECT(openagc_vk_queue_submit_commands(device, composite_cmds, NULL), OPENAGC_OK);
            EXPECT(openagc_gl_copy_buffer_then_clear_sub_data(gl_src, 0u, gl_dst, 0u, 64u, value,
                                                             16u),
                   OPENAGC_OK);
            memset(composite, 0, sizeof(composite));
            EXPECT(openagc_vk_buffer_readback(vk_dst, 0u, composite, sizeof(composite)),
                   OPENAGC_OK);
            for (i = 0u; i < 4u; ++i) {
                memcpy(&observed, composite + i * 4u, 4u);
                CHECK(observed == value);
            }
            EXPECT(openagc_gl_get_buffer_sub_data(gl_dst, 0u, composite, sizeof(composite)),
                   OPENAGC_OK);
            for (i = 0u; i < 4u; ++i) {
                memcpy(&observed, composite + i * 4u, 4u);
                CHECK(observed == value);
            }
            EXPECT(openagc_vk_destroy_command_buffer(composite_cmds), OPENAGC_OK);
            EXPECT(openagc_vk_destroy_buffer(vk_dst), OPENAGC_OK);
            EXPECT(openagc_vk_destroy_buffer(vk_src), OPENAGC_OK);
        }
        EXPECT(openagc_gl_destroy_buffer(gl_src), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_buffer(gl_dst), OPENAGC_OK);
    }

    memset(vk_words, 0, sizeof(vk_words));
    memset(gl_words, 0, sizeof(gl_words));
    EXPECT(openagc_vk_buffer_readback(scratch, 0u, vk_words, sizeof(vk_words)), OPENAGC_OK);
    EXPECT(openagc_gl_get_buffer_sub_data(pack, 0u, gl_words, sizeof(gl_words)), OPENAGC_OK);
    CHECK(memcmp(vk_words, gl_words, sizeof(vk_words)) == 0);
    for (index = 0u; index < 4u; ++index) {
        memcpy(&observed, vk_words + index * 4u, 4u);
        CHECK(observed == value);
    }

    {
        uint8_t inline_bytes[16];
        uint8_t before[16];
        uint8_t vk_inline[16];
        uint8_t gl_inline[16];
        openagc_gl_buffer *gl_uniform = NULL;
        uint32_t byte;

        for (byte = 0u; byte < sizeof(inline_bytes); ++byte) {
            inline_bytes[byte] = (uint8_t)(0xc0u + byte);
        }
        EXPECT(openagc_vk_buffer_readback(scratch, 32u, before, sizeof(before)), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_update_buffer(commands, scratch, 32u, inline_bytes,
                                            sizeof(inline_bytes)),
               OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        memset(vk_inline, 0, sizeof(vk_inline));
        EXPECT(openagc_vk_buffer_readback(scratch, 32u, vk_inline, sizeof(vk_inline)),
               OPENAGC_OK);
        /* UpdateBuffer is deferred like copies; bytes stay unchanged until submit. */
        CHECK(memcmp(vk_inline, before, sizeof(before)) == 0);
        EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 64u, &gl_uniform),
               OPENAGC_OK);
        EXPECT(openagc_gl_buffer_sub_data(gl_uniform, 0u, inline_bytes, sizeof(inline_bytes)),
               OPENAGC_OK);
        memset(vk_inline, 0, sizeof(vk_inline));
        memset(gl_inline, 0, sizeof(gl_inline));
        EXPECT(openagc_vk_buffer_readback(scratch, 32u, vk_inline, sizeof(vk_inline)),
               OPENAGC_OK);
        EXPECT(openagc_gl_get_buffer_sub_data(gl_uniform, 0u, gl_inline, sizeof(gl_inline)),
               OPENAGC_OK);
        CHECK(memcmp(vk_inline, inline_bytes, sizeof(inline_bytes)) == 0);
        CHECK(memcmp(gl_inline, inline_bytes, sizeof(inline_bytes)) == 0);
        EXPECT(openagc_gl_destroy_buffer(gl_uniform), OPENAGC_OK);
    }

    memset(vk_pixels, 0, sizeof(vk_pixels));
    memset(gl_pixels, 0, sizeof(gl_pixels));
    EXPECT(openagc_vk_image_readback(destination, 0u, vk_pixels, sizeof(vk_pixels)), OPENAGC_OK);
    EXPECT(openagc_gl_get_tex_image(destination_texture, 0u, gl_pixels, sizeof(gl_pixels)),
           OPENAGC_OK);
    CHECK(memcmp(vk_pixels, gl_pixels, sizeof(vk_pixels)) == 0);
    CHECK(vk_pixels[20] == 1u && vk_pixels[27] == 8u);
    CHECK(vk_pixels[36] == 17u && vk_pixels[43] == 24u);
    CHECK(vk_pixels[19] == 0u && vk_pixels[28] == 0u);

    EXPECT(openagc_gl_destroy_buffer(pack), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(destination_texture), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(source_texture), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(destination), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(source), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(scratch), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(input), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

static int test_store_const_dispatch_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_shader_artifact_desc artifact = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_binding_decl binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 4u
    };
    openagc_vk_buffer_desc buffer_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        16u);
    uint8_t code_hash[32] = {
        0x48, 0x47, 0x46, 0xd3, 0x32, 0x1b, 0x61, 0x85, 0x50, 0xe2, 0x6e, 0x5a, 0xb5, 0xa2,
        0xde, 0x56, 0x4d, 0x76, 0xf8, 0x6a, 0x61, 0x15, 0x72, 0x0b, 0x20, 0x1a, 0x39, 0x05,
        0xe8, 0x27, 0xd2, 0xe5
    };
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_buffer *vk_buffer = NULL;
    openagc_vk_pipeline *vk_pipeline = NULL;
    openagc_vk_descriptor_set *descriptors = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_buffer *gl_buffer = NULL;
    openagc_gl_program *gl_program = NULL;
    uint32_t vk_word = 0u;
    uint32_t gl_word = 0u;
    uint8_t zeros[16];

    memset(zeros, 0, sizeof(zeros));
    artifact.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact.code = openagc_store_const_code;
    artifact.code_size = OPENAGC_STORE_CONST_CODE_SIZE;
    artifact.bindings = &binding;
    artifact.binding_count = 1u;
    artifact.workgroup_x = 1u;
    artifact.workgroup_y = 1u;
    artifact.workgroup_z = 1u;
    memcpy(artifact.code_sha256, code_hash, sizeof(code_hash));

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);

    EXPECT(openagc_vk_create_buffer(device, &buffer_desc, &vk_buffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 16u, &gl_buffer), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(vk_buffer, 0u, zeros, sizeof(zeros)), OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(gl_buffer, 0u, zeros, sizeof(zeros)), OPENAGC_OK);

    EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
               device, &artifact, vk_buffer, 0u, 16u, 0u, NULL, 0u, &vk_pipeline),
           OPENAGC_OK);
    EXPECT(openagc_gl_create_program_with_bindings(gl, &artifact, gl_buffer, 0u, 16u, 0u, NULL, 0u,
                                                   &gl_program),
           OPENAGC_OK);

    EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, vk_pipeline, &descriptors),
           OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, vk_buffer, 0u, 0u, 16u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 2u, 1u, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_readback(vk_buffer, 0u, &vk_word, sizeof(vk_word)), OPENAGC_OK);
    CHECK(vk_word == 0u);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_use_program(gl, gl_program), OPENAGC_OK);
    EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_buffer, 0u, 16u), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 2u, 1u, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gl_bind_uniform_base(gl, 0u, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);

    EXPECT(openagc_vk_buffer_readback(vk_buffer, 0u, &vk_word, sizeof(vk_word)), OPENAGC_OK);
    EXPECT(openagc_gl_get_buffer_sub_data(gl_buffer, 0u, &gl_word, sizeof(gl_word)), OPENAGC_OK);
    CHECK(vk_word == OPENAGC_STORE_CONST_VALUE);
    CHECK(gl_word == OPENAGC_STORE_CONST_VALUE);
    CHECK(vk_word == gl_word);

    EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_pipeline(vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_program), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_buffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_buffer), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

static int test_store_span_dispatch_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_shader_artifact_desc artifact = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_binding_decl binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, OPENAGC_STORE_SPAN_BYTES
    };
    openagc_vk_buffer_desc buffer_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        128u);
    uint8_t code_hash[32] = {
        0xa7, 0x0a, 0xe4, 0xfd, 0xe2, 0x4b, 0xe0, 0x34, 0x0a, 0x81, 0x8b, 0xee, 0x67, 0x40,
        0x27, 0x2e, 0x57, 0xb7, 0xc9, 0x56, 0x07, 0xf1, 0x24, 0x43, 0x7e, 0x1c, 0x94, 0x39,
        0x81, 0x9b, 0xa2, 0xc4
    };
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_buffer *vk_buffer = NULL;
    openagc_vk_pipeline *vk_pipeline = NULL;
    openagc_vk_descriptor_set *descriptors = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_buffer *gl_buffer = NULL;
    openagc_gl_program *gl_program = NULL;
    uint32_t vk_words[32];
    uint32_t gl_words[32];
    uint8_t zeros[128];
    uint32_t i;

    memset(zeros, 0, sizeof(zeros));
    artifact.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact.code = openagc_store_span_code;
    artifact.code_size = OPENAGC_STORE_SPAN_CODE_SIZE;
    artifact.bindings = &binding;
    artifact.binding_count = 1u;
    artifact.workgroup_x = 8u;
    artifact.workgroup_y = 1u;
    artifact.workgroup_z = 1u;
    memcpy(artifact.code_sha256, code_hash, sizeof(code_hash));

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);

    EXPECT(openagc_vk_create_buffer(device, &buffer_desc, &vk_buffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(gl, OPENAGC_GL_UNIFORM_BUFFER, 128u, &gl_buffer), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(vk_buffer, 0u, zeros, sizeof(zeros)), OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(gl_buffer, 0u, zeros, sizeof(zeros)), OPENAGC_OK);

    EXPECT(openagc_vk_create_compute_pipeline_with_bindings(
               device, &artifact, vk_buffer, 0u, 128u, 0u, NULL, 0u, &vk_pipeline),
           OPENAGC_OK);
    EXPECT(openagc_gl_create_program_with_bindings(gl, &artifact, gl_buffer, 0u, 128u, 0u, NULL, 0u,
                                                   &gl_program),
           OPENAGC_OK);

    EXPECT(openagc_vk_create_descriptor_set_for_pipeline(device, vk_pipeline, &descriptors),
           OPENAGC_OK);
    EXPECT(openagc_vk_update_descriptor_buffer_range(descriptors, vk_buffer, 0u, 0u, 128u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_compute_pipeline(commands, vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_bind_descriptor_set(commands, descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_dispatch(commands, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_use_program(gl, gl_program), OPENAGC_OK);
    EXPECT(openagc_gl_bind_uniform_range(gl, 0u, gl_buffer, 0u, 128u), OPENAGC_OK);
    EXPECT(openagc_gl_dispatch_compute(gl, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gl_bind_uniform_base(gl, 0u, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_use_program(gl, NULL), OPENAGC_OK);

    EXPECT(openagc_vk_buffer_readback(vk_buffer, 0u, vk_words, sizeof(vk_words)), OPENAGC_OK);
    EXPECT(openagc_gl_get_buffer_sub_data(gl_buffer, 0u, gl_words, sizeof(gl_words)), OPENAGC_OK);
    for (i = 0u; i < 32u; ++i) {
        CHECK(vk_words[i] == OPENAGC_STORE_SPAN_VALUE);
        CHECK(gl_words[i] == OPENAGC_STORE_SPAN_VALUE);
    }

    EXPECT(openagc_vk_destroy_descriptor_set(descriptors), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_pipeline(vk_pipeline), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_program), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(vk_buffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(gl_buffer), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

/* 32x8 color clear exercises host WRITE_DATA MAX_COLS x MAX_ROWS tiling on both frontends. */
static int test_write_data_grid_clear_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_image_desc vk_image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 32u, 8u);
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc gl_image_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 32u, 8u);
    openagc_color clear = { 0xa5u, 0x5au, 0x3cu, 0xffu };
    uint8_t vk_pixels[32u * 8u * 4u];
    uint8_t gl_pixels[32u * 8u * 4u];
    uint32_t i;
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_image *target = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_renderbuffer *renderbuffer = NULL;
    openagc_gl_framebuffer *framebuffer = NULL;

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &target), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, target, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_clear_color(commands, target, clear), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);
    EXPECT(openagc_gl_create_renderbuffer(gl, &gl_image_desc, &renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(gl, &framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_clear(framebuffer, clear), OPENAGC_OK);

    memset(vk_pixels, 0, sizeof(vk_pixels));
    memset(gl_pixels, 0, sizeof(gl_pixels));
    EXPECT(openagc_vk_image_readback(target, 0u, vk_pixels, sizeof(vk_pixels)), OPENAGC_OK);
    EXPECT(openagc_gl_read_pixels(framebuffer, 0u, gl_pixels, sizeof(gl_pixels)), OPENAGC_OK);
    CHECK(memcmp(vk_pixels, gl_pixels, sizeof(vk_pixels)) == 0);
    for (i = 0u; i < sizeof(vk_pixels); i += 4u) {
        CHECK(vk_pixels[i] == clear.r && vk_pixels[i + 1u] == clear.g &&
              vk_pixels[i + 2u] == clear.b && vk_pixels[i + 3u] == clear.a);
    }

    EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(renderbuffer), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(target), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

/* Same scissored depth clear path on VK (deferred submit) and GL (immediate). */
static int test_depth_clear_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_image_desc color_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_vk_image_desc depth_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc gl_color_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
    openagc_gl_image_desc gl_depth_desc = OPENAGC_GL_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8, 4u, 4u);
    openagc_color keep = { 0u, 0u, 0u, 0u };
    uint8_t vk_packed[64];
    uint8_t gl_packed[64];
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_image *color = NULL;
    openagc_vk_image *depth = NULL;
    openagc_vk_render_pass *pass = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_renderbuffer *gl_color = NULL;
    openagc_gl_renderbuffer *gl_depth = NULL;
    openagc_gl_framebuffer *fb = NULL;

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &color_desc, &color), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &depth_desc, &depth), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, color, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, depth,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
    EXPECT(openagc_vk_create_render_pass(device, color, &pass), OPENAGC_OK);
    EXPECT(openagc_vk_render_pass_attach_depth(pass, depth), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_begin_render_pass_with_depth(
               commands, pass, OPENAGC_FRONTEND_LOAD_OP_LOAD, keep,
               OPENAGC_FRONTEND_LOAD_OP_CLEAR, 1.f, 0x5au),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_set_scissor(commands, 0u, 0u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_clear_depth(commands, 0.f, 0u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
    memset(vk_packed, 0, sizeof(vk_packed));
    EXPECT(openagc_vk_image_readback(depth, 0u, vk_packed, sizeof(vk_packed)), OPENAGC_OK);

    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(gl, &fb), OPENAGC_OK);
    EXPECT(openagc_gl_create_renderbuffer(gl, &gl_color_desc, &gl_color), OPENAGC_OK);
    EXPECT(openagc_gl_create_renderbuffer(gl, &gl_depth_desc, &gl_depth), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_renderbuffer(fb, gl_color), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_depth_renderbuffer(fb, gl_depth), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_begin_with_depth(
               fb, OPENAGC_FRONTEND_LOAD_OP_LOAD, keep, OPENAGC_FRONTEND_LOAD_OP_CLEAR, 1.f,
               0x5au),
           OPENAGC_OK);
    EXPECT(openagc_gl_scissor(fb, 0u, 0u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gl_clear_depth(fb, 0.f, 0u), OPENAGC_OK);
    memset(gl_packed, 0, sizeof(gl_packed));
    EXPECT(openagc_gl_renderbuffer_read(gl_depth, 0u, gl_packed, sizeof(gl_packed)), OPENAGC_OK);
    CHECK(memcmp(vk_packed, gl_packed, sizeof(vk_packed)) == 0);
    CHECK(vk_packed[0] == 0u && vk_packed[3] == 0u && vk_packed[4] == 0xffu &&
          vk_packed[7] == 0x5au);

    EXPECT(openagc_gl_destroy_framebuffer(fb), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(gl_depth), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(gl_color), OPENAGC_OK);
    EXPECT(openagc_vk_render_pass_attach_depth(pass, NULL), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_render_pass(pass), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(depth), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(color), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

static int load_path(const char *primary, const char *fallback, uint8_t *bytes, uint32_t max,
                     uint32_t *out_size)
{
    FILE *handle = fopen(primary, "rb");
    size_t n;

    if (handle == NULL) {
        handle = fopen(fallback, "rb");
    }
    if (handle == NULL) {
        return 0;
    }
    n = fread(bytes, 1u, max, handle);
    fclose(handle);
    if (n == 0u || n >= max) {
        return 0;
    }
    *out_size = (uint32_t)n;
    return 1;
}

/* VK and GL auto-attach identical host register programs from PSBC envelopes. */
static int test_psbc_register_snapshot_equivalence(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_image_desc vk_image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_gl_context_desc gl_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc gl_image_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
    openagc_shader_artifact_desc vertex_stage = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_desc pixel_stage = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_frontend_pipeline_info vk_info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_frontend_pipeline_info gl_info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    static const uint8_t vert_sha[32] = {
        0xda, 0x3d, 0xb5, 0xeb, 0x80, 0xb2, 0x23, 0x4c, 0xd4, 0xee, 0x28, 0x5a, 0xb1, 0xc2,
        0xef, 0x31, 0xd6, 0xf1, 0x8d, 0x66, 0xfe, 0x9d, 0xcf, 0x9b, 0xc5, 0x44, 0x37, 0x8f,
        0x0d, 0xb6, 0x7d, 0x2f
    };
    static const uint8_t frag_sha[32] = {
        0x28, 0xc5, 0x6f, 0x1c, 0xaa, 0xb7, 0xa7, 0x71, 0x9d, 0x09, 0xc8, 0x59, 0xb6, 0x90,
        0x77, 0xda, 0x00, 0x86, 0xb9, 0x5b, 0x17, 0x5f, 0x52, 0x90, 0x6c, 0x36, 0x63, 0x9b,
        0x30, 0xa0, 0x79, 0x81
    };
    uint8_t vert_code[64];
    uint8_t frag_code[64];
    uint8_t vert_meta[4096];
    uint8_t frag_meta[4096];
    uint32_t vert_code_n = 0u;
    uint32_t frag_code_n = 0u;
    uint32_t vert_meta_n = 0u;
    uint32_t frag_meta_n = 0u;
    uint32_t vk_words[128];
    uint32_t gl_words[128];
    uint32_t vk_count = 0u;
    uint32_t gl_count = 0u;
    uint8_t pinned[32];
    uint32_t i;
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_image *target = NULL;
    openagc_vk_pipeline *vk_graphics = NULL;
    openagc_gl_context *gl = NULL;
    openagc_gl_renderbuffer *renderbuffer = NULL;
    openagc_gl_program *gl_graphics = NULL;

    CHECK(load_path("tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin",
                    "../tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin", vert_code,
                    sizeof(vert_code), &vert_code_n));
    CHECK(load_path("tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin",
                    "../tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin", frag_code,
                    sizeof(frag_code), &frag_code_n));
    CHECK(load_path("tests/fixtures/psbc_smoke/smoke.vert.metadata.json",
                    "../tests/fixtures/psbc_smoke/smoke.vert.metadata.json", vert_meta,
                    sizeof(vert_meta) - 1u, &vert_meta_n));
    CHECK(load_path("tests/fixtures/psbc_smoke/smoke.frag.metadata.json",
                    "../tests/fixtures/psbc_smoke/smoke.frag.metadata.json", frag_meta,
                    sizeof(frag_meta) - 1u, &frag_meta_n));
    CHECK(vert_code_n == 44u && frag_code_n == 48u);
    vert_meta[vert_meta_n] = 0;
    frag_meta[frag_meta_n] = 0;
    CHECK(openagc_psbc_decode_sha256_hex(OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256,
                                         pinned) == 1);

    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &vk_image_desc, &target), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, target, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);

    EXPECT(openagc_gl_context_create(&gl_desc, &gl), OPENAGC_OK);
    EXPECT(openagc_gl_create_renderbuffer(gl, &gl_image_desc, &renderbuffer), OPENAGC_OK);
    {
        openagc_gl_framebuffer *fb = NULL;
        openagc_color clear = { 0u, 0u, 0u, 0u };

        EXPECT(openagc_gl_create_framebuffer(gl, &fb), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(fb, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_clear(fb, clear), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_framebuffer(fb), OPENAGC_OK);
    }

    vertex_stage.compiler = OPENAGC_SHADER_COMPILER_OPENGNM_PSBC;
    vertex_stage.compiler_metadata_version = OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION;
    vertex_stage.toolchain_release_minor = 3u;
    memcpy(vertex_stage.compiler_source_revision, OPENAGC_SHADER_PINNED_PSBC_REVISION,
           sizeof(vertex_stage.compiler_source_revision));
    for (i = 0u; i < 32u; ++i) {
        vertex_stage.compiler_binary_sha256[i] = pinned[i];
    }
    vertex_stage.stage = OPENAGC_SHADER_STAGE_VERTEX;
    vertex_stage.code = vert_code;
    vertex_stage.code_size = vert_code_n;
    vertex_stage.vertex_position_written = 1u;
    vertex_stage.compiler_metadata = vert_meta;
    vertex_stage.compiler_metadata_size = vert_meta_n;
    memcpy(vertex_stage.code_sha256, vert_sha, 32);

    pixel_stage.compiler = OPENAGC_SHADER_COMPILER_OPENGNM_PSBC;
    pixel_stage.compiler_metadata_version = OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION;
    pixel_stage.toolchain_release_minor = 3u;
    memcpy(pixel_stage.compiler_source_revision, OPENAGC_SHADER_PINNED_PSBC_REVISION,
           sizeof(pixel_stage.compiler_source_revision));
    for (i = 0u; i < 32u; ++i) {
        pixel_stage.compiler_binary_sha256[i] = pinned[i];
    }
    pixel_stage.stage = OPENAGC_SHADER_STAGE_PIXEL;
    pixel_stage.code = frag_code;
    pixel_stage.code_size = frag_code_n;
    pixel_stage.color_output_mask = 1u;
    pixel_stage.color_export_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
    pixel_stage.compiler_metadata = frag_meta;
    pixel_stage.compiler_metadata_size = frag_meta_n;
    memcpy(pixel_stage.code_sha256, frag_sha, 32);

    EXPECT(openagc_vk_create_graphics_pipeline(device, &vertex_stage, &pixel_stage, target,
                                               &vk_graphics),
           OPENAGC_OK);
    EXPECT(openagc_gl_create_graphics_program(gl, &vertex_stage, &pixel_stage, renderbuffer,
                                              &gl_graphics),
           OPENAGC_OK);

    /* Auto-attached at create when both stages are psbc_envelope. */
    EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
    EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_info), OPENAGC_OK);
    CHECK(vk_info.compiler_verified == 0u && vk_info.gpu_executable == 0u);
    CHECK(gl_info.compiler_verified == 0u && gl_info.gpu_executable == 0u);
    CHECK(vk_info.host_register_program_dwords == gl_info.host_register_program_dwords);
    /* 3*(3+4) context/shader + 9 linkage + 3*(9+4) pixel = 69. */
    CHECK(vk_info.host_register_program_dwords == 69u);
    EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u, &vk_count),
           OPENAGC_OK);
    EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, gl_words, 128u, &gl_count),
           OPENAGC_OK);
    CHECK(vk_count == 69u && gl_count == 69u);
    CHECK(memcmp(vk_words, gl_words, sizeof(uint32_t) * 69u) == 0);
    CHECK(vk_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(vk_words[1] == 433u && vk_words[2] == 128u);
    /* Vertex linkage context pairs trail the vertex context+shader block. */
    CHECK(vk_words[21] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(vk_words[22] == 603u && vk_words[23] == 131200u);

    /* Attribute-less PSBC draw: bind pipeline, no VBO, draw → NOT_READY. */
    {
        openagc_vk_render_pass *vk_pass = NULL;
        openagc_gl_framebuffer *gl_fb = NULL;

        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);

        EXPECT(openagc_gl_create_framebuffer(gl, &gl_fb), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(gl_fb, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_framebuffer(gl, gl_fb), OPENAGC_OK);
        EXPECT(openagc_gl_viewport(gl_fb, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_program(gl_fb, gl_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_bind_framebuffer(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_framebuffer(gl_fb), OPENAGC_OK);
    }

    /* Explicit re-attach stays idempotent and still refuses execution claims. */
    EXPECT(openagc_vk_pipeline_set_psbc_register_snapshot(vk_graphics, vert_meta, vert_meta_n,
                                                          frag_meta, frag_meta_n),
           OPENAGC_OK);
    EXPECT(openagc_gl_program_set_psbc_register_snapshot(gl_graphics, vert_meta, vert_meta_n,
                                                         frag_meta, frag_meta_n),
           OPENAGC_OK);
    EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
    CHECK(vk_info.host_register_program_dwords == 69u);
    CHECK(vk_info.compiler_verified == 0u && vk_info.gpu_executable == 0u);
    CHECK(vk_info.psbc_pgm_patched == 0u);

    /* Same host code VAs on both frontends: PGM patch stays equivalent; EOP record is host-only. */
    {
        const uint64_t vertex_va = UINT64_C(0x20001000);
        const uint64_t pixel_va = UINT64_C(0x20002000);
        uint64_t vk_v = 0u;
        uint64_t vk_p = 0u;
        uint64_t gl_v = 0u;
        uint64_t gl_p = 0u;

        EXPECT(openagc_vk_pipeline_patch_psbc_pgm_vas(vk_graphics, vertex_va, pixel_va),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_patch_psbc_pgm_vas(gl_graphics, vertex_va, pixel_va),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_info), OPENAGC_OK);
        CHECK(vk_info.psbc_pgm_patched == 1u && gl_info.psbc_pgm_patched == 1u);
        CHECK(vk_info.gpu_executable == 0u && gl_info.gpu_executable == 0u);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u,
                                                             &vk_count),
               OPENAGC_OK);
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, gl_words, 128u,
                                                            &gl_count),
               OPENAGC_OK);
        CHECK(vk_count == 69u && gl_count == 69u);
        CHECK(memcmp(vk_words, gl_words, sizeof(uint32_t) * 69u) == 0);
        /* Vertex shader SET_SH PGM_LO follows three context pairs (9 dwords). */
        CHECK(vk_words[9] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(vk_words[10] == 72u);
        CHECK(vk_words[11] == (uint32_t)(vertex_va >> 8));
        CHECK(vk_words[14] == (uint32_t)(vertex_va >> 40));
        EXPECT(openagc_vk_pipeline_get_psbc_code_vas(vk_graphics, &vk_v, &vk_p), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_psbc_code_vas(gl_graphics, &gl_v, &gl_p), OPENAGC_OK);
        CHECK(vk_v == vertex_va && vk_p == pixel_va && gl_v == vertex_va && gl_p == pixel_va);
        EXPECT(openagc_vk_pipeline_record_psbc_register_eop(vk_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_program_record_psbc_register_eop(gl_graphics), OPENAGC_OK);
    }

    /* Heap-backed code bind patches PGM from real synthetic VAs (still not gpu_executable). */
    {
        uint64_t vk_v = 0u;
        uint64_t vk_p = 0u;
        uint64_t gl_v = 0u;
        uint64_t gl_p = 0u;

        EXPECT(openagc_vk_pipeline_bind_psbc_code(vk_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_program_bind_psbc_code(gl_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_info), OPENAGC_OK);
        CHECK(vk_info.psbc_pgm_patched == 1u && gl_info.psbc_pgm_patched == 1u);
        CHECK(vk_info.gpu_executable == 0u && gl_info.gpu_executable == 0u);
        EXPECT(openagc_vk_pipeline_get_psbc_code_vas(vk_graphics, &vk_v, &vk_p), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_psbc_code_vas(gl_graphics, &gl_v, &gl_p), OPENAGC_OK);
        CHECK((vk_v & 0xffu) == 0u && (vk_p & 0xffu) == 0u);
        CHECK((gl_v & 0xffu) == 0u && (gl_p & 0xffu) == 0u);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u,
                                                             &vk_count),
               OPENAGC_OK);
        CHECK(vk_words[11] == (uint32_t)(vk_v >> 8));
        CHECK(vk_words[14] == (uint32_t)(vk_v >> 40));
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, gl_words, 128u,
                                                            &gl_count),
               OPENAGC_OK);
        CHECK(vk_count == 69u && gl_count == 69u);
        CHECK(vk_words[11] == (uint32_t)(vk_v >> 8));
        CHECK(vk_words[14] == (uint32_t)(vk_v >> 40));
        CHECK(gl_words[11] == (uint32_t)(gl_v >> 8));
        CHECK(gl_words[14] == (uint32_t)(gl_v >> 40));
        /* Vertex block 30 dwords; pixel has 9 context pairs (27) then shader PGM. */
        CHECK(vk_words[57] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(vk_words[58] == 8u);
        CHECK(vk_words[59] == (uint32_t)(vk_p >> 8));
        CHECK(gl_words[59] == (uint32_t)(gl_p >> 8));
        CHECK(vk_info.psbc_code_bound == 1u && gl_info.psbc_code_bound == 1u);
    }

    /*
     * End-to-end Step U host vehicle: bind_psbc_code then VK submit /
     * GL bind_program records 69 register dwords + EOP (93) with
     * gpu_submitted=0. Draw stays NOT_READY.
     */
    {
        openagc_vk_render_pass *vk_pass = NULL;
        openagc_gl_framebuffer *gl_fb = NULL;
        openagc_gpu_submission_view vk_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        openagc_gpu_submission_view gl_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        const uint32_t step_u_words =
            OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(3u, 4u, 9u, 4u);

        EXPECT(openagc_vk_create_render_pass(device, target, &vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_begin_render_pass(commands, vk_pass), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_bind_pipeline(commands, vk_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_set_viewport(commands, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_vk_cmd_end_render_pass(commands), OPENAGC_OK);
        EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
        EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_OK);
        EXPECT(openagc_vk_device_get_last_write(device, &vk_view), OPENAGC_OK);
        CHECK(vk_view.gpu_submitted == 0u);
        CHECK(vk_view.word_count == step_u_words);
        CHECK(vk_view.words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(vk_view.words[69] == OPENAGC_PM4_EOP_HEADER);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u,
                                                             &vk_count),
               OPENAGC_OK);
        CHECK(vk_count == 69u);
        CHECK(memcmp(vk_view.words, vk_words, sizeof(uint32_t) * 69u) == 0);
        EXPECT(openagc_vk_destroy_render_pass(vk_pass), OPENAGC_OK);

        EXPECT(openagc_gl_create_framebuffer(gl, &gl_fb), OPENAGC_OK);
        EXPECT(openagc_gl_framebuffer_renderbuffer(gl_fb, renderbuffer), OPENAGC_OK);
        EXPECT(openagc_gl_bind_framebuffer(gl, gl_fb), OPENAGC_OK);
        EXPECT(openagc_gl_viewport(gl_fb, 0u, 0u, 4u, 4u), OPENAGC_OK);
        EXPECT(openagc_gl_bind_program(gl_fb, gl_graphics), OPENAGC_OK);
        EXPECT(openagc_gl_draw_arrays(gl, 0u, 3u), OPENAGC_ERROR_NOT_READY);
        EXPECT(openagc_gl_context_get_last_write(gl, &gl_view), OPENAGC_OK);
        CHECK(gl_view.gpu_submitted == 0u);
        CHECK(gl_view.word_count == step_u_words);
        CHECK(gl_view.words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(gl_view.words[69] == OPENAGC_PM4_EOP_HEADER);
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, gl_words, 128u,
                                                            &gl_count),
               OPENAGC_OK);
        CHECK(gl_count == 69u);
        CHECK(memcmp(gl_view.words, gl_words, sizeof(uint32_t) * 69u) == 0);
        /* Same Step-U shape on both frontends (headers + EOP); heap VAs may differ. */
        CHECK(vk_view.words[0] == gl_view.words[0]);
        CHECK(vk_view.words[9] == gl_view.words[9]);
        CHECK(vk_view.words[21] == gl_view.words[21]);
        CHECK(vk_view.words[57] == gl_view.words[57]);
        CHECK(vk_view.words[69] == gl_view.words[69]);
        EXPECT(openagc_gl_bind_framebuffer(gl, NULL), OPENAGC_OK);
        EXPECT(openagc_gl_destroy_framebuffer(gl_fb), OPENAGC_OK);

        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_info), OPENAGC_OK);
        CHECK(vk_info.compiler_verified == 0u && vk_info.gpu_executable == 0u);
        CHECK(gl_info.compiler_verified == 0u && gl_info.gpu_executable == 0u);
    }

    /* The 34 linker context records, three uconfig records and COLOR0
     * defaults below are from PS5_Vulkan's public C1 triangle capture at
     * commit 3a6f00df (golden/c1-triangle/c1-triangle-1.json, region 0,
     * chunks 0x5000/0x5100/0x6000/0x0400). The target BASE is replaced
     * with a synthetic host address. This is not a FW9.40 qualification. */
    {
        openagc_frontend_agc_register
            link_cx[OPENAGC_FRONTEND_AGC_LINK_CONTEXT_COUNT];
        openagc_frontend_agc_register
            link_uc[OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT];
        /* COLOR0 offsets/defaults follow the public PS5_Vulkan C1 capture;
         * BASE is a synthetic host address, never a console submission. */
        openagc_frontend_agc_register
            target_cx[OPENAGC_FRONTEND_AGC_TARGET_CONTEXT_COUNT] = {
                {0x318u, 0x01000000u}, {0x31bu, 0u}, {0x31cu, 0x8828u},
                {0x31du, 0u}, {0x31eu, 0x48u}, {0x31fu, 0u},
                {0x321u, 0u}, {0x323u, 0u}, {0x324u, 0u},
                {0x325u, 0u}, {0x390u, 0u}, {0x398u, 0u},
                {0x3a0u, 0u}, {0x3a8u, 0u}, {0x3b0u, 0x03bfc86fu},
                {0x3b8u, 0x4dc6c000u}
            };
        openagc_frontend_agc_register
            target_vk[OPENAGC_FRONTEND_AGC_TARGET_CONTEXT_COUNT];
        openagc_frontend_agc_register
            target_gl[OPENAGC_FRONTEND_AGC_TARGET_CONTEXT_COUNT];
        uint32_t linked_vk[256];
        uint32_t linked_gl[256];
        uint32_t linked_vk_n = 0u;
        uint32_t linked_gl_n = 0u;
        openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        uint64_t vk_v = 0u, vk_p = 0u;

        for (i = 0u; i < 32u; ++i) {
            link_cx[i].offset = 0x191u + i;
            link_cx[i].value = i;
        }
        link_cx[32] = (openagc_frontend_agc_register){0x2d5u, 0x12010u};
        link_cx[33] = (openagc_frontend_agc_register){0x29bu, 2u};
        link_uc[0] = (openagc_frontend_agc_register){0x25bu, 0x10080u};
        link_uc[1] = (openagc_frontend_agc_register){0x262u, 0u};
        link_uc[2] = (openagc_frontend_agc_register){0x242u, 4u};

        EXPECT(openagc_vk_pipeline_set_agc_target_registers(
                   vk_graphics, target_cx, 16u), OPENAGC_ERROR_NOT_READY);

        EXPECT(openagc_vk_pipeline_set_agc_linked_registers(
                   vk_graphics, link_cx, 33u, link_uc,
                   OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        link_uc[1].offset = 0x400u;
        EXPECT(openagc_gl_program_set_agc_linked_registers(
                   gl_graphics, link_cx, OPENAGC_FRONTEND_AGC_LINK_CONTEXT_COUNT,
                   link_uc, OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT),
               OPENAGC_ERROR_OUT_OF_RANGE);
        link_uc[1].offset = 0x262u;
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u,
                                                             &vk_count), OPENAGC_OK);
        CHECK(vk_count == 69u);
        EXPECT(openagc_vk_pipeline_set_agc_linked_registers(
                   vk_graphics, link_cx, OPENAGC_FRONTEND_AGC_LINK_CONTEXT_COUNT,
                   link_uc, OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_agc_linked_registers(
                   gl_graphics, link_cx, OPENAGC_FRONTEND_AGC_LINK_CONTEXT_COUNT,
                   link_uc, OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, linked_vk, 256u,
                                                             &linked_vk_n), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, linked_gl, 256u,
                                                            &linked_gl_n), OPENAGC_OK);
        CHECK(linked_vk_n == 171u && linked_gl_n == 171u);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, vk_words, 128u,
                                                             &vk_count),
               OPENAGC_ERROR_CAPACITY);
        CHECK(vk_count == 171u);
        CHECK(linked_vk[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        for (i = 0u; i < OPENAGC_FRONTEND_AGC_LINK_CONTEXT_COUNT; ++i) {
            CHECK(linked_vk[3u * i] ==
                  openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
            CHECK(linked_vk[3u * i + 1u] == link_cx[i].offset);
            CHECK(linked_vk[3u * i + 2u] == link_cx[i].value);
        }
        CHECK(linked_vk[138] ==
              (openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
               OPENAGC_PM4_RESET_FILTER_CAM));
        CHECK(linked_vk[139] == 603u && linked_vk[140] == 0x10080u);
        CHECK(linked_vk[142] == 0x262u && linked_vk[143] == 0u);
        CHECK(linked_vk[145] == 0x242u && linked_vk[146] == 4u);
        for (i = 0u; i < OPENAGC_FRONTEND_AGC_LINK_UCONFIG_COUNT; ++i) {
            CHECK(linked_vk[138u + 3u * i] ==
                  (openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
                   OPENAGC_PM4_RESET_FILTER_CAM));
            CHECK(linked_vk[139u + 3u * i] == link_uc[i].offset);
            CHECK(linked_vk[140u + 3u * i] == link_uc[i].value);
        }
        CHECK(linked_vk[147] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        for (i = 0u; i < linked_vk_n; ++i) {
            /* Separate backend heaps give the four PGM VA dwords their
             * own values; every other word must be identical. */
            if (i != 149u && i != 152u && i != 161u && i != 164u) {
                CHECK(linked_vk[i] == linked_gl[i]);
            }
        }

        EXPECT(openagc_vk_pipeline_get_psbc_code_vas(vk_graphics, &vk_v, &vk_p), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_patch_psbc_pgm_vas(vk_graphics, vk_v, vk_p), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, linked_vk, 256u,
                                                             &linked_vk_n), OPENAGC_OK);
        CHECK(linked_vk_n == 171u && linked_vk[139] == 603u);
        EXPECT(openagc_vk_pipeline_set_agc_target_registers(
                   vk_graphics, target_cx, 15u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        target_cx[0].value = 0u;
        EXPECT(openagc_gl_program_set_agc_target_registers(
                   gl_graphics, target_cx, 16u), OPENAGC_ERROR_INVALID_ARGUMENT);
        target_cx[0].value = 0x01000000u;
        target_cx[1].offset = 0x31au;
        EXPECT(openagc_vk_pipeline_set_agc_target_registers(
                   vk_graphics, target_cx, 16u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        target_cx[1].offset = 0x31bu;
        EXPECT(openagc_vk_pipeline_set_agc_target_registers(
                   vk_graphics, target_cx, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_agc_target_registers(
                   gl_graphics, target_cx, 16u), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, linked_vk,
                                                             256u, &linked_vk_n), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, linked_gl,
                                                            256u, &linked_gl_n), OPENAGC_OK);
        CHECK(linked_vk_n == 219u && linked_gl_n == 219u);
        for (i = 0u; i < OPENAGC_FRONTEND_AGC_TARGET_CONTEXT_COUNT; ++i) {
            CHECK(linked_vk[3u * i] ==
                  openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
            CHECK(linked_vk[3u * i + 1u] == target_cx[i].offset);
            CHECK(linked_vk[3u * i + 2u] == target_cx[i].value);
            CHECK(linked_vk[3u * i] == linked_gl[3u * i]);
            CHECK(linked_vk[3u * i + 1u] == linked_gl[3u * i + 1u]);
            CHECK(linked_vk[3u * i + 2u] == linked_gl[3u * i + 2u]);
        }
        CHECK(linked_vk[1] == 0x318u && linked_vk[2] == 0x01000000u);
        CHECK(linked_vk[7] == 0x31cu && linked_vk[8] == 0x8828u);
        CHECK(linked_vk[49] == link_cx[0].offset);

        /* One public AGC default table drives both native format dialects.
         * The caller supplies a synthetic aligned VA and 256-byte rows. */
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                   target_cx, 16u, 0x100000000ull, 63u, 32u, target_vk, 16u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                   target_cx, 16u, 0x100000001ull, 64u, 32u, target_vk, 16u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                   target_cx, 16u, 0xffffffffff00ull, 64u, 32u, target_vk, 16u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT,
                   target_cx, 16u, 0x100000000ull, 64u, 32u, target_vk, 16u),
               OPENAGC_ERROR_UNSUPPORTED_OPERATION);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                   target_cx, 16u, 0x100000000ull, 64u, 32u, target_vk, 15u),
               OPENAGC_ERROR_CAPACITY);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                   target_cx, 16u, 0x100000000ull, 64u, 32u, target_vk, 16u),
               OPENAGC_OK);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_OPENGL,
                   OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8,
                   target_cx, 16u, 0x100000000ull, 64u, 32u, target_gl, 16u),
               OPENAGC_OK);
        for (i = 0u; i < 16u; ++i) {
            CHECK(target_vk[i].offset == target_gl[i].offset);
            CHECK(target_vk[i].value == target_gl[i].value);
        }
        CHECK(target_vk[0].value == 0x01000000u);
        CHECK(target_vk[2].value == 0x8028u);
        CHECK(target_vk[4].value == 0x48u);
        CHECK(target_vk[14].value == ((63u << 14) | 31u));
        CHECK(target_vk[15].value == 0x4dc00000u);
        EXPECT(openagc_frontend_agc_build_linear_target(
                   OPENAGC_FRONTEND_VULKAN,
                   OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM,
                   target_cx, 16u, 0x100000000ull, 64u, 32u, target_gl, 16u),
               OPENAGC_OK);
        CHECK(target_gl[2].value == 0x8828u);
        EXPECT(openagc_vk_pipeline_set_agc_target_registers(
                   vk_graphics, target_vk, 16u), OPENAGC_OK);
        EXPECT(openagc_gl_program_set_agc_target_registers(
                   gl_graphics, target_vk, 16u), OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, linked_vk,
                                                             256u, &linked_vk_n), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_host_register_program(gl_graphics, linked_gl,
                                                            256u, &linked_gl_n), OPENAGC_OK);
        CHECK(linked_vk_n == 219u && linked_gl_n == 219u);
        CHECK(linked_vk[8] == 0x8028u && linked_vk[8] == linked_gl[8]);
        EXPECT(openagc_vk_pipeline_patch_psbc_pgm_vas(vk_graphics, vk_v, vk_p),
               OPENAGC_OK);
        EXPECT(openagc_vk_pipeline_get_host_register_program(vk_graphics, linked_vk,
                                                             256u, &linked_vk_n), OPENAGC_OK);
        CHECK(linked_vk_n == 219u && linked_vk[2] == 0x01000000u);
        EXPECT(openagc_vk_pipeline_record_psbc_register_eop(vk_graphics), OPENAGC_OK);
        EXPECT(openagc_vk_device_get_last_write(device, &view), OPENAGC_OK);
        CHECK(view.gpu_submitted == 0u && view.word_count == 243u);
        CHECK(view.words[219] == OPENAGC_PM4_EOP_HEADER);
        EXPECT(openagc_vk_pipeline_get_info(vk_graphics, &vk_info), OPENAGC_OK);
        EXPECT(openagc_gl_program_get_info(gl_graphics, &gl_info), OPENAGC_OK);
        CHECK(vk_info.gpu_executable == 0u && gl_info.gpu_executable == 0u);
    }

    EXPECT(openagc_vk_destroy_pipeline(vk_graphics), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(gl_graphics), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(renderbuffer), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(target), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(gl), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_same_backend_state() != 0 ||
        test_copy_and_fill_equivalence() != 0 ||
        test_store_const_dispatch_equivalence() != 0 ||
        test_store_span_dispatch_equivalence() != 0 ||
        test_write_data_grid_clear_equivalence() != 0 ||
        test_depth_clear_equivalence() != 0 ||
        test_psbc_register_snapshot_equivalence() != 0) {
        return 1;
    }
    puts("OpenAGC Vulkan and OpenGL share one backend");
    return 0;
}
