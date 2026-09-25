/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/opengl.h"

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

static int test_gl_derives_the_shared_backend(void)
{
    openagc_gl_context_desc context_desc = OPENAGC_GL_CONTEXT_DESC_INIT;
    openagc_gl_image_desc image_desc = OPENAGC_GL_IMAGE_DESC_INIT(OPENAGC_GL_RGBA8, 4u, 4u);
    openagc_gl_image_desc depth_desc = OPENAGC_GL_IMAGE_DESC_INIT(0x1902u, 4u, 4u);
    openagc_gl_capabilities caps = OPENAGC_GL_CAPABILITIES_INIT;
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_timeline_info fence = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_gl_context *context = NULL;
    openagc_gl_texture *texture = NULL;
    openagc_gl_renderbuffer *renderbuffer = NULL;
    openagc_gl_framebuffer *framebuffer = NULL;
    openagc_gl_buffer *pixel_buffer = NULL;
    openagc_gl_program *program = NULL;
    openagc_shader_artifact_desc artifact = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_frontend_pipeline_info pipeline = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    static const uint8_t code_hash[32] = {
        0x9fu, 0x86u, 0xd0u, 0x81u, 0x88u, 0x4cu, 0x7du, 0x65u,
        0x9au, 0x2fu, 0xeau, 0xa0u, 0xc5u, 0x5au, 0xd0u, 0x15u,
        0xa3u, 0xbfu, 0x4fu, 0x1bu, 0x2bu, 0x0bu, 0x82u, 0x2cu,
        0xd1u, 0x5du, 0x6cu, 0x15u, 0xb0u, 0xf0u, 0x0au, 0x08u
    };
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_color clear = { 0x11u, 0x22u, 0x33u, 0x44u };
    uint8_t pattern[64];
    uint8_t pixels[64];
    uint32_t i;

    EXPECT(openagc_gl_context_create(NULL, &context), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gl_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gl_get_capabilities(context, &caps), OPENAGC_OK);
    CHECK(caps.gpu_execution == 0u && caps.presentation == 0u &&
          caps.default_framebuffer == 0u);
    EXPECT(openagc_gl_draw_arrays(context, 0u, 3u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_bind_default_framebuffer(context),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_create_texture(context, &depth_desc, &texture),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    for (i = 0u; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)(0x40u + i);
    }
    EXPECT(openagc_gl_create_texture(context, &image_desc, &texture), OPENAGC_OK);
    EXPECT(openagc_gl_tex_sub_image(texture, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    memset(pixels, 0, sizeof(pixels));
    EXPECT(openagc_gl_get_tex_image(texture, 0u, pixels, sizeof(pixels)), OPENAGC_OK);
    CHECK(memcmp(pixels, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_gl_texture_get_info(texture, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED);
    CHECK(info.owner == OPENAGC_GRAPHICS_OWNER_HOST);

    EXPECT(openagc_gl_bind_texture_for_sampling(context, texture), OPENAGC_OK);
    EXPECT(openagc_gl_memory_barrier(context, OPENAGC_GL_TEXTURE_FETCH_BARRIER_BIT),
           OPENAGC_OK);
    EXPECT(openagc_gl_memory_barrier(context, 0x1u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_texture_get_info(texture, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ);
    CHECK(info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);

    EXPECT(openagc_gl_create_renderbuffer(context, &image_desc, &renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_framebuffer(context, &framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_framebuffer_renderbuffer(framebuffer, renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_renderbuffer_get_info(renderbuffer, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);
    CHECK(info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_gl_clear(framebuffer, clear), OPENAGC_OK);
    memset(pixels, 0, sizeof(pixels));
    EXPECT(openagc_gl_read_pixels(framebuffer, 0u, pixels, sizeof(pixels)), OPENAGC_OK);
    for (i = 0u; i < 16u; ++i) {
        CHECK(pixels[i * 4u] == clear.r && pixels[i * 4u + 1u] == clear.g &&
              pixels[i * 4u + 2u] == clear.b && pixels[i * 4u + 3u] == clear.a);
    }
    EXPECT(openagc_gl_renderbuffer_get_info(renderbuffer, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);

    EXPECT(openagc_gl_fence_poll(context, 1u, &fence), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gl_finish(context), OPENAGC_OK);
    EXPECT(openagc_gl_fence_poll(context, 1u, &fence), OPENAGC_OK);
    CHECK(fence.value == 1u && fence.signaled == 1u);

    EXPECT(openagc_gl_create_buffer(context, 0x8894u, 64u, &pixel_buffer),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_create_buffer(context, OPENAGC_GL_PIXEL_UNPACK_BUFFER, 64u,
                                    &pixel_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(pixel_buffer, 0u, pattern, 16u), OPENAGC_OK);
    EXPECT(openagc_gl_get_buffer_sub_data(pixel_buffer, 0u, pixels, 16u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_bind_buffer(context, OPENAGC_GL_PIXEL_UNPACK_BUFFER, pixel_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(pixel_buffer), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_bind_buffer(context, OPENAGC_GL_PIXEL_UNPACK_BUFFER, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(pixel_buffer), OPENAGC_OK);
    EXPECT(openagc_gl_create_buffer(context, OPENAGC_GL_PIXEL_PACK_BUFFER, 64u, &pixel_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gl_buffer_data(pixel_buffer, 0u, pattern, 16u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_get_buffer_sub_data(pixel_buffer, 0u, pixels, 16u), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_buffer(pixel_buffer), OPENAGC_OK);

    artifact.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact.code = code;
    artifact.code_size = 4u;
    artifact.workgroup_x = 8u;
    artifact.workgroup_y = 1u;
    artifact.workgroup_z = 1u;
    memcpy(artifact.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_gl_create_program(context, &artifact, &program), OPENAGC_OK);
    EXPECT(openagc_gl_program_get_info(program, &pipeline), OPENAGC_OK);
    CHECK(pipeline.kind == OPENAGC_SHADER_PIPELINE_COMPUTE);
    CHECK(pipeline.compiler_verified == 0u && pipeline.gpu_executable == 0u);
    EXPECT(openagc_gl_use_program(context, program), OPENAGC_OK);
    EXPECT(openagc_gl_draw_arrays(context, 0u, 3u), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gl_destroy_program(program), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_use_program(context, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_program(program), OPENAGC_OK);

    EXPECT(openagc_gl_context_destroy(context), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_destroy_texture(texture), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_destroy_renderbuffer(renderbuffer), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_destroy_framebuffer(framebuffer), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_renderbuffer(renderbuffer), OPENAGC_OK);
    EXPECT(openagc_gl_bind_texture_for_sampling(context, texture), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(texture), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gl_bind_texture_for_sampling(context, NULL), OPENAGC_OK);
    EXPECT(openagc_gl_destroy_texture(texture), OPENAGC_OK);
    EXPECT(openagc_gl_context_destroy(context), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_gl_derives_the_shared_backend() != 0) {
        return 1;
    }
    puts("OpenAGC OpenGL subset tests passed");
    return 0;
}
