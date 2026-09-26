/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/opengl.h"

#include <stdlib.h>
#include <string.h>

struct openagc_gl_context {
    openagc_context *context;
    openagc_gpu_device *gpu;
    openagc_frontend_device *frontend;
    openagc_frontend_timeline *timeline;
    openagc_gl_texture *bound_sample;
    openagc_gl_texture *samples[8];
    openagc_gl_buffer *bound_unpack;
    openagc_gl_buffer *bound_pack;
    openagc_gl_buffer *bound_uniform;
    openagc_gl_buffer *uniforms[8];
    uint64_t uniform_offsets[8];
    uint64_t uniform_sizes[8];
    openagc_gl_program *bound_program;
    openagc_gl_sampler *bound_sampler;
    openagc_gl_framebuffer *draw_target;
    uint32_t child_count;
};

struct openagc_gl_texture {
    openagc_gl_context *context;
    openagc_frontend_image *image;
    uint32_t attachments;
};

struct openagc_gl_renderbuffer {
    openagc_gl_context *context;
    openagc_frontend_image *image;
    uint32_t attachments;
};

struct openagc_gl_framebuffer {
    openagc_gl_context *context;
    openagc_gl_renderbuffer *color;
    openagc_gl_renderbuffer *depth;
    openagc_gl_texture *color_texture;
    openagc_frontend_render_pass *pass;
};

struct openagc_gl_memory {
    openagc_gl_context *context;
    openagc_frontend_memory *memory;
};

struct openagc_gl_buffer {
    openagc_gl_context *context;
    openagc_frontend_buffer *buffer;
    uint32_t target;
};

struct openagc_gl_query {
    openagc_gl_context *context;
    openagc_frontend_query_pool *pool;
};

struct openagc_gl_sampler {
    openagc_gl_context *context;
    openagc_frontend_sampler *sampler;
};

struct openagc_gl_program {
    openagc_gl_context *context;
    openagc_shader_artifact *artifact;
    openagc_frontend_pipeline *pipeline;
};

static openagc_result openagc_gl_derive(openagc_frontend_image *image,
                                       openagc_graphics_image_state state,
                                       openagc_graphics_owner owner)
{
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_result result = openagc_frontend_image_get_info(image, &info);

    if (result != OPENAGC_OK) {
        return result;
    }
    if (info.state == state && info.owner == owner) {
        return OPENAGC_OK;
    }
    return openagc_frontend_image_transition(image, state, owner);
}

static openagc_result openagc_gl_make_image(openagc_gl_context *context,
                                            const openagc_gl_image_desc *desc,
                                            uint32_t native_usage,
                                            openagc_frontend_image **out_image)
{
    openagc_frontend_image_desc image_desc;

    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_GL_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->internal_format != OPENAGC_GL_RGBA8 &&
        desc->internal_format != OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->internal_format == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8 &&
        native_usage != OPENAGC_FRONTEND_GL_RENDERBUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    image_desc = (openagc_frontend_image_desc)OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, desc->internal_format, native_usage, 0u, desc->width,
        desc->height, 0u);
    return openagc_frontend_image_create(context->frontend, &image_desc, out_image);
}

openagc_result openagc_gl_context_create(const openagc_gl_context_desc *desc,
                                         openagc_gl_context **out_context)
{
    openagc_gl_context *context;
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc gpu_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_result result;

    if (out_context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_context = NULL;
    if (desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) || desc->api_version != OPENAGC_GL_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    context = (openagc_gl_context *)calloc(1u, sizeof(*context));
    if (context == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    result = openagc_context_create(&context_desc, &context->context);
    if (result != OPENAGC_OK) {
        free(context);
        return result;
    }
    result = openagc_gpu_device_create(context->context, &gpu_desc, &context->gpu);
    if (result != OPENAGC_OK) {
        (void)openagc_context_destroy(context->context);
        free(context);
        return result;
    }
    result = openagc_frontend_device_create(context->gpu, &frontend_desc, &context->frontend);
    if (result != OPENAGC_OK) {
        (void)openagc_gpu_device_destroy(context->gpu);
        (void)openagc_context_destroy(context->context);
        free(context);
        return result;
    }
    result = openagc_frontend_timeline_create(context->frontend, &context->timeline);
    if (result != OPENAGC_OK) {
        (void)openagc_frontend_device_destroy(context->frontend);
        (void)openagc_gpu_device_destroy(context->gpu);
        (void)openagc_context_destroy(context->context);
        free(context);
        return result;
    }
    *out_context = context;
    return OPENAGC_OK;
}

openagc_result openagc_gl_context_destroy(openagc_gl_context *context)
{
    openagc_result result;

    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (context->child_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_timeline_destroy(context->timeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_device_destroy(context->frontend);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_gpu_device_destroy(context->gpu);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_context_destroy(context->context);
    if (result != OPENAGC_OK) {
        return result;
    }
    free(context);
    return OPENAGC_OK;
}

openagc_result openagc_gl_context_get_last_write(const openagc_gl_context *context,
                                                  openagc_gpu_submission_view *view)
{
    if (context == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_device_get_last_write(context->frontend, view);
}

openagc_result openagc_gl_get_capabilities(const openagc_gl_context *context,
                                           openagc_gl_capabilities *capabilities)
{
    if (context == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->gpu_execution = 0u;
    capabilities->presentation = 0u;
    capabilities->default_framebuffer = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_gl_get_format(const openagc_gl_context *context, uint32_t index,
                                     uint32_t *native_format,
                                     openagc_graphics_format *backend_format)
{
    openagc_result result;

    if (context == NULL || native_format == NULL || backend_format == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *native_format = 0u;
    *backend_format = 0u;
    result = openagc_frontend_native_format_at(OPENAGC_FRONTEND_OPENGL, index, native_format);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_translate_format(OPENAGC_FRONTEND_OPENGL, *native_format,
                                             backend_format);
}

openagc_result openagc_gl_create_texture(openagc_gl_context *context,
                                         const openagc_gl_image_desc *desc,
                                         openagc_gl_texture **out_texture)
{
    openagc_gl_texture *texture;
    openagc_result result;

    if (out_texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_texture = NULL;
    if (context == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    texture = (openagc_gl_texture *)calloc(1u, sizeof(*texture));
    if (texture == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    texture->context = context;
    result = openagc_gl_make_image(context, desc, OPENAGC_GL_TEXTURE_2D, &texture->image);
    if (result != OPENAGC_OK) {
        free(texture);
        return result;
    }
    context->child_count++;
    *out_texture = texture;
    return OPENAGC_OK;
}

openagc_result openagc_gl_tex_sub_image(openagc_gl_texture *texture, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes)
{
    if (texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_upload(texture->image, offset, bytes, size_bytes);
}

openagc_result openagc_gl_tex_sub_image_from_buffer(openagc_gl_texture *texture,
                                                    openagc_gl_buffer *buffer,
                                                    uint64_t buffer_offset,
                                                    uint64_t image_offset, uint64_t size_bytes)
{
    if (texture == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (texture->context != buffer->context ||
        buffer->target != OPENAGC_GL_PIXEL_UNPACK_BUFFER) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_copy_buffer_to_image(buffer->buffer, buffer_offset, texture->image,
                                                 image_offset, size_bytes);
}

openagc_result openagc_gl_get_tex_image(openagc_gl_texture *texture, uint64_t offset,
                                        void *bytes, uint64_t size_bytes)
{
    if (texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_readback(texture->image, offset, bytes, size_bytes);
}

openagc_result openagc_gl_get_tex_image_to_buffer(openagc_gl_texture *texture,
                                                  uint64_t image_offset,
                                                  openagc_gl_buffer *buffer,
                                                  uint64_t buffer_offset, uint64_t size_bytes)
{
    if (texture == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (texture->context != buffer->context || buffer->target != OPENAGC_GL_PIXEL_PACK_BUFFER) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_copy_image_to_buffer(texture->image, image_offset, buffer->buffer,
                                                 buffer_offset, size_bytes);
}

openagc_result openagc_gl_create_sampler(openagc_gl_context *context, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_gl_sampler **out_sampler)
{
    openagc_gl_sampler *sampler;
    openagc_frontend_sampler_desc desc;
    openagc_result result;

    if (out_sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_sampler = NULL;
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    sampler = (openagc_gl_sampler *)calloc(1u, sizeof(*sampler));
    if (sampler == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    sampler->context = context;
    desc = (openagc_frontend_sampler_desc)OPENAGC_FRONTEND_SAMPLER_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, mag_filter, min_filter, address_mode);
    result = openagc_frontend_sampler_create(context->frontend, &desc, &sampler->sampler);
    if (result != OPENAGC_OK) {
        free(sampler);
        return result;
    }
    context->child_count++;
    *out_sampler = sampler;
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_sampler(openagc_gl_context *context, openagc_gl_sampler *sampler)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler != NULL && sampler->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    context->bound_sampler = sampler;
    return OPENAGC_OK;
}

openagc_result openagc_gl_destroy_sampler(openagc_gl_sampler *sampler)
{
    openagc_result result;

    if (sampler == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler->context->bound_sampler == sampler) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_sampler_destroy(sampler->sampler);
    if (result != OPENAGC_OK) {
        return result;
    }
    sampler->context->child_count--;
    free(sampler);
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_texture_for_sampling(openagc_gl_context *context,
                                                    openagc_gl_texture *texture)
{
    return openagc_gl_bind_texture_unit(context, 0u, texture);
}

openagc_result openagc_gl_bind_texture_unit(openagc_gl_context *context, uint32_t unit,
                                          openagc_gl_texture *texture)
{
    openagc_result result;

    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (unit >= 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (texture == NULL) {
        uint32_t slot;

        context->samples[unit] = NULL;
        context->bound_sample = NULL;
        for (slot = 0u; slot < 8u; ++slot) {
            if (context->samples[slot] != NULL) {
                context->bound_sample = context->samples[slot];
                break;
            }
        }
        return OPENAGC_OK;
    }
    if (texture->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_gl_derive(texture->image, OPENAGC_GRAPHICS_STATE_SHADER_READ,
                               OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    if (result != OPENAGC_OK) {
        return result;
    }
    context->bound_sample = texture;
    context->samples[unit] = texture;
    return OPENAGC_OK;
}

openagc_result openagc_gl_texture_get_info(const openagc_gl_texture *texture,
                                           openagc_frontend_image_info *info)
{
    if (texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_get_info(texture->image, info);
}

openagc_result openagc_gl_destroy_texture(openagc_gl_texture *texture)
{
    openagc_result result;

    if (texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (texture->attachments != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    {
        uint32_t slot;

        for (slot = 0u; slot < 8u; ++slot) {
            if (texture->context->samples[slot] == texture) {
                return OPENAGC_ERROR_BUSY;
            }
        }
    }
    result = openagc_frontend_image_destroy(texture->image);
    if (result != OPENAGC_OK) {
        return result;
    }
    texture->context->child_count--;
    free(texture);
    return OPENAGC_OK;
}

openagc_result openagc_gl_create_renderbuffer(openagc_gl_context *context,
                                              const openagc_gl_image_desc *desc,
                                              openagc_gl_renderbuffer **out_renderbuffer)
{
    openagc_gl_renderbuffer *renderbuffer;
    openagc_result result;

    if (out_renderbuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_renderbuffer = NULL;
    if (context == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    renderbuffer = (openagc_gl_renderbuffer *)calloc(1u, sizeof(*renderbuffer));
    if (renderbuffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    renderbuffer->context = context;
    result = openagc_gl_make_image(context, desc, OPENAGC_GL_RENDERBUFFER,
                                   &renderbuffer->image);
    if (result != OPENAGC_OK) {
        free(renderbuffer);
        return result;
    }
    context->child_count++;
    *out_renderbuffer = renderbuffer;
    return OPENAGC_OK;
}

openagc_result openagc_gl_renderbuffer_get_info(const openagc_gl_renderbuffer *renderbuffer,
                                                openagc_frontend_image_info *info)
{
    if (renderbuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_get_info(renderbuffer->image, info);
}

openagc_result openagc_gl_renderbuffer_read(openagc_gl_renderbuffer *renderbuffer,
                                            uint64_t offset, void *bytes, uint64_t size_bytes)
{
    if (renderbuffer == NULL || bytes == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_image_readback(renderbuffer->image, offset, bytes, size_bytes);
}

openagc_result openagc_gl_destroy_renderbuffer(openagc_gl_renderbuffer *renderbuffer)
{
    openagc_result result;

    if (renderbuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (renderbuffer->attachments != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_image_destroy(renderbuffer->image);
    if (result != OPENAGC_OK) {
        return result;
    }
    renderbuffer->context->child_count--;
    free(renderbuffer);
    return OPENAGC_OK;
}

openagc_result openagc_gl_create_framebuffer(openagc_gl_context *context,
                                             openagc_gl_framebuffer **out_framebuffer)
{
    openagc_gl_framebuffer *framebuffer;

    if (out_framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_framebuffer = NULL;
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    framebuffer = (openagc_gl_framebuffer *)calloc(1u, sizeof(*framebuffer));
    if (framebuffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    framebuffer->context = context;
    context->child_count++;
    *out_framebuffer = framebuffer;
    return OPENAGC_OK;
}

openagc_result openagc_gl_framebuffer_renderbuffer(openagc_gl_framebuffer *framebuffer,
                                                   openagc_gl_renderbuffer *renderbuffer)
{
    openagc_result result;

    if (framebuffer == NULL || renderbuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (renderbuffer->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (framebuffer->color != NULL || framebuffer->color_texture != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_gl_derive(renderbuffer->image, OPENAGC_GRAPHICS_STATE_COLOR_TARGET,
                               OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_render_pass_create(framebuffer->context->frontend,
                                                 renderbuffer->image, &framebuffer->pass);
    if (result != OPENAGC_OK) {
        return result;
    }
    framebuffer->color = renderbuffer;
    renderbuffer->attachments++;
    return OPENAGC_OK;
}

openagc_result openagc_gl_framebuffer_depth_renderbuffer(
    openagc_gl_framebuffer *framebuffer, openagc_gl_renderbuffer *renderbuffer)
{
    openagc_result result;

    if (framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (renderbuffer == NULL) {
        result = openagc_frontend_render_pass_attach_depth(framebuffer->pass, NULL);
        if (result != OPENAGC_OK) {
            return result;
        }
        if (framebuffer->depth != NULL) {
            framebuffer->depth->attachments--;
            framebuffer->depth = NULL;
        }
        return OPENAGC_OK;
    }
    if (renderbuffer->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (framebuffer->depth == renderbuffer) {
        return OPENAGC_OK;
    }
    if (framebuffer->depth != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_gl_derive(renderbuffer->image, OPENAGC_GRAPHICS_STATE_DEPTH_TARGET,
                               OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_render_pass_attach_depth(framebuffer->pass, renderbuffer->image);
    if (result != OPENAGC_OK) {
        return result;
    }
    framebuffer->depth = renderbuffer;
    renderbuffer->attachments++;
    return OPENAGC_OK;
}

static openagc_frontend_image *openagc_gl_color_image(const openagc_gl_framebuffer *framebuffer)
{
    if (framebuffer->color != NULL) {
        return framebuffer->color->image;
    }
    if (framebuffer->color_texture != NULL) {
        return framebuffer->color_texture->image;
    }
    return NULL;
}

openagc_result openagc_gl_framebuffer_texture(openagc_gl_framebuffer *framebuffer,
                                              openagc_gl_texture *texture)
{
    openagc_result result;

    if (framebuffer == NULL || texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (texture->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (framebuffer->color != NULL || framebuffer->color_texture != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    result = openagc_gl_derive(texture->image, OPENAGC_GRAPHICS_STATE_COLOR_TARGET,
                               OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_render_pass_create(framebuffer->context->frontend,
                                                 texture->image, &framebuffer->pass);
    if (result != OPENAGC_OK) {
        return result;
    }
    framebuffer->color_texture = texture;
    texture->attachments++;
    return OPENAGC_OK;
}

static openagc_result openagc_gl_pass_rect(openagc_gl_framebuffer *framebuffer, int viewport,
                                          uint32_t x, uint32_t y, uint32_t width,
                                          uint32_t height)
{
    if (framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return viewport != 0
               ? openagc_frontend_render_pass_set_viewport(framebuffer->pass, x, y, width, height)
               : openagc_frontend_render_pass_set_scissor(framebuffer->pass, x, y, width, height);
}

openagc_result openagc_gl_viewport(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height)
{
    return openagc_gl_pass_rect(framebuffer, 1, x, y, width, height);
}

openagc_result openagc_gl_bind_program(openagc_gl_framebuffer *framebuffer,
                                        openagc_gl_program *program)
{
    openagc_result result;

    if (framebuffer == NULL || program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (program->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    result = openagc_frontend_render_pass_bind_pipeline(framebuffer->pass, program->pipeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    /* Immediate host record of Step-U register+EOP when PSBC code is heap-bound. */
    return openagc_frontend_pipeline_record_psbc_register_eop_if_bound(program->pipeline);
}

openagc_result openagc_gl_bind_index_buffer(openagc_gl_framebuffer *framebuffer,
                                            openagc_gl_buffer *buffer, uint64_t offset)
{
    if (framebuffer == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_render_pass_bind_index(framebuffer->pass, buffer->buffer, offset);
}

openagc_result openagc_gl_bind_vertex_buffer(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_buffer *buffer, uint64_t offset)
{
    if (framebuffer == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_render_pass_bind_vertex(framebuffer->pass, buffer->buffer, offset);
}

openagc_result openagc_gl_scissor(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height)
{
    return openagc_gl_pass_rect(framebuffer, 0, x, y, width, height);
}

openagc_result openagc_gl_clear(openagc_gl_framebuffer *framebuffer, openagc_color color)
{
    if (framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (openagc_gl_color_image(framebuffer) == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_image_clear(openagc_gl_color_image(framebuffer), color);
}

openagc_result openagc_gl_clear_scissor(openagc_gl_framebuffer *framebuffer, openagc_color color)
{
    if (framebuffer == NULL || framebuffer->pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_render_pass_clear(framebuffer->pass, color);
}

openagc_result openagc_gl_clear_depth(openagc_gl_framebuffer *framebuffer, float depth,
                                     uint32_t stencil)
{
    if (framebuffer == NULL || framebuffer->pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_render_pass_clear_depth(framebuffer->pass, depth, stencil);
}

openagc_result openagc_gl_framebuffer_begin(openagc_gl_framebuffer *framebuffer,
                                            openagc_frontend_load_op load_op, openagc_color color)
{
    if (framebuffer == NULL || framebuffer->pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_render_pass_begin_with_load(framebuffer->pass, load_op, color);
}

openagc_result openagc_gl_framebuffer_begin_with_depth(
    openagc_gl_framebuffer *framebuffer, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil)
{
    if (framebuffer == NULL || framebuffer->pass == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_render_pass_begin_with_depth(framebuffer->pass, color_op, color,
                                                         depth_op, depth, stencil);
}

openagc_result openagc_gl_read_pixels(openagc_gl_framebuffer *framebuffer, uint64_t offset,
                                      void *bytes, uint64_t size_bytes)
{
    if (framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (openagc_gl_color_image(framebuffer) == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_image_readback(openagc_gl_color_image(framebuffer), offset, bytes,
                                           size_bytes);
}

openagc_result openagc_gl_copy_tex_sub_image(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_texture *texture, uint32_t xoffset,
                                             uint32_t yoffset, uint32_t x, uint32_t y,
                                             uint32_t width, uint32_t height)
{
    openagc_frontend_image *source;

    if (framebuffer == NULL || texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (texture->context != framebuffer->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    source = openagc_gl_color_image(framebuffer);
    if (source == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    return openagc_frontend_image_copy_rect(source, x, y, texture->image, xoffset, yoffset, width,
                                            height);
}

openagc_result openagc_gl_destroy_framebuffer(openagc_gl_framebuffer *framebuffer)
{
    if (framebuffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer->pass != NULL) {
        (void)openagc_frontend_render_pass_end(framebuffer->pass);
        if (openagc_frontend_render_pass_destroy(framebuffer->pass) != OPENAGC_OK) {
            return OPENAGC_ERROR_BUSY;
        }
        framebuffer->pass = NULL;
    }
    if (framebuffer->color != NULL) {
        framebuffer->color->attachments--;
        framebuffer->color = NULL;
    }
    if (framebuffer->color_texture != NULL) {
        framebuffer->color_texture->attachments--;
        framebuffer->color_texture = NULL;
    }
    if (framebuffer->depth != NULL) {
        framebuffer->depth->attachments--;
        framebuffer->depth = NULL;
    }
    /* Deleting the bound target unbinds it; the context keeps no stale pointer. */
    if (framebuffer->context->draw_target == framebuffer) {
        framebuffer->context->draw_target = NULL;
    }
    framebuffer->context->child_count--;
    free(framebuffer);
    return OPENAGC_OK;
}

openagc_result openagc_gl_memory_barrier(openagc_gl_context *context, uint32_t barriers)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (barriers != OPENAGC_GL_TEXTURE_FETCH_BARRIER_BIT || context->bound_sample == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return openagc_gl_derive(context->bound_sample->image, OPENAGC_GRAPHICS_STATE_SHADER_READ,
                             OPENAGC_GRAPHICS_OWNER_GRAPHICS);
}

openagc_result openagc_gl_finish(openagc_gl_context *context)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_timeline_signal(context->timeline);
}

openagc_result openagc_gl_client_wait(const openagc_gl_context *context, uint64_t value,
                                      openagc_frontend_timeline_info *info)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_timeline_poll(context->timeline, value, info);
}

openagc_result openagc_gl_fence_poll(const openagc_gl_context *context, uint64_t value,
                                     openagc_frontend_timeline_info *info)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_timeline_poll(context->timeline, value, info);
}

openagc_result openagc_gl_allocate_memory(openagc_gl_context *context, uint64_t size_bytes,
                                          openagc_gl_memory **out_memory)
{
    openagc_gl_memory *memory;
    openagc_result result;

    if (out_memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    memory = (openagc_gl_memory *)calloc(1u, sizeof(*memory));
    if (memory == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    memory->context = context;
    result = openagc_frontend_memory_allocate(context->frontend, size_bytes, &memory->memory);
    if (result != OPENAGC_OK) {
        free(memory);
        return result;
    }
    context->child_count++;
    *out_memory = memory;
    return OPENAGC_OK;
}

openagc_result openagc_gl_free_memory(openagc_gl_memory *memory)
{
    openagc_result result;

    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_memory_destroy(memory->memory);
    if (result != OPENAGC_OK) {
        return result;
    }
    memory->context->child_count--;
    free(memory);
    return OPENAGC_OK;
}

openagc_result openagc_gl_create_buffer(openagc_gl_context *context, uint32_t target,
                                        uint64_t size_bytes, openagc_gl_buffer **out_buffer)
{
    openagc_gl_buffer *buffer;
    openagc_frontend_buffer_desc desc;
    openagc_result result;

    if (out_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;
    if (context == NULL || size_bytes == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (target != OPENAGC_GL_PIXEL_PACK_BUFFER && target != OPENAGC_GL_PIXEL_UNPACK_BUFFER &&
        target != OPENAGC_GL_UNIFORM_BUFFER &&         target != OPENAGC_GL_ARRAY_BUFFER &&
        target != OPENAGC_GL_ELEMENT_ARRAY_BUFFER &&
        target != OPENAGC_GL_DRAW_INDIRECT_BUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    buffer = (openagc_gl_buffer *)calloc(1u, sizeof(*buffer));
    if (buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    buffer->context = context;
    buffer->target = target;
    desc = (openagc_frontend_buffer_desc)OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, target, size_bytes);
    result = openagc_frontend_buffer_create(context->frontend, &desc, &buffer->buffer);
    if (result != OPENAGC_OK) {
        free(buffer);
        return result;
    }
    context->child_count++;
    *out_buffer = buffer;
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_uniform_base(openagc_gl_context *context, uint32_t index,
                                          openagc_gl_buffer *buffer)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (buffer != NULL &&
        (buffer->context != context || buffer->target != OPENAGC_GL_UNIFORM_BUFFER)) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    context->uniforms[index] = buffer;
    context->uniform_offsets[index] = 0u;
    context->uniform_sizes[index] = 0u;
    context->bound_uniform = buffer;
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_uniform_range(openagc_gl_context *context, uint32_t index,
                                            openagc_gl_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes)
{
    openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_result result = openagc_gl_bind_uniform_base(context, index, buffer);

    if (result != OPENAGC_OK) {
        return result;
    }
    if (buffer == NULL || size_bytes == 0u || (offset & 3u) != 0u || (size_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = openagc_frontend_buffer_get_info(buffer->buffer, &info);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (offset > info.size_bytes || size_bytes > info.size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    context->uniform_offsets[index] = offset;
    context->uniform_sizes[index] = size_bytes;
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_buffer(openagc_gl_context *context, uint32_t target,
                                      openagc_gl_buffer *buffer)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (target != OPENAGC_GL_PIXEL_PACK_BUFFER && target != OPENAGC_GL_PIXEL_UNPACK_BUFFER &&
        target != OPENAGC_GL_UNIFORM_BUFFER &&         target != OPENAGC_GL_ARRAY_BUFFER &&
        target != OPENAGC_GL_ELEMENT_ARRAY_BUFFER &&
        target != OPENAGC_GL_DRAW_INDIRECT_BUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (buffer != NULL && (buffer->context != context || buffer->target != target)) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (target == OPENAGC_GL_PIXEL_UNPACK_BUFFER) {
        context->bound_unpack = buffer;
    } else if (target == OPENAGC_GL_PIXEL_PACK_BUFFER) {
        context->bound_pack = buffer;
    } else if (target == OPENAGC_GL_UNIFORM_BUFFER) {
        context->bound_uniform = buffer;
        context->uniforms[0] = buffer;
        context->uniform_offsets[0] = 0u;
        context->uniform_sizes[0] = 0u;
    } else {
        context->bound_uniform = buffer;
    }
    return OPENAGC_OK;
}

openagc_result openagc_gl_buffer_data(openagc_gl_buffer *buffer, uint64_t offset,
                                      const void *bytes, uint64_t size_bytes)
{
    return openagc_gl_buffer_sub_data(buffer, offset, bytes, size_bytes);
}

openagc_result openagc_gl_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                          const void *bytes, uint64_t size_bytes)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->target != OPENAGC_GL_PIXEL_UNPACK_BUFFER &&
        buffer->target != OPENAGC_GL_UNIFORM_BUFFER &&
        buffer->target != OPENAGC_GL_ARRAY_BUFFER &&
        buffer->target != OPENAGC_GL_ELEMENT_ARRAY_BUFFER &&
        buffer->target != OPENAGC_GL_DRAW_INDIRECT_BUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return openagc_frontend_buffer_upload(buffer->buffer, offset, bytes, size_bytes);
}

openagc_result openagc_gl_get_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                              void *bytes, uint64_t size_bytes)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->target == OPENAGC_GL_PIXEL_UNPACK_BUFFER ||
        buffer->target == OPENAGC_GL_ARRAY_BUFFER ||
        buffer->target == OPENAGC_GL_ELEMENT_ARRAY_BUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return openagc_frontend_buffer_readback(buffer->buffer, offset, bytes, size_bytes);
}

openagc_result openagc_gl_clear_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                                uint64_t size_bytes, uint32_t value)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_buffer_fill(buffer->buffer, offset, size_bytes, value);
}

openagc_result openagc_gl_copy_buffer_sub_data(openagc_gl_buffer *source, uint64_t source_offset,
                                               openagc_gl_buffer *destination,
                                               uint64_t destination_offset, uint64_t size_bytes)
{
    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (source->context != destination->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_buffer_copy(source->buffer, source_offset, destination->buffer,
                                        destination_offset, size_bytes);
}

openagc_result openagc_gl_copy_buffer_then_clear_sub_data(
    openagc_gl_buffer *source, uint64_t source_offset, openagc_gl_buffer *destination,
    uint64_t destination_offset, uint64_t size_bytes, uint32_t value, uint64_t fill_bytes)
{
    if (source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (source->context != destination->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_buffer_copy_then_fill(source->buffer, source_offset,
                                                  destination->buffer, destination_offset,
                                                  size_bytes, value, fill_bytes);
}

openagc_result openagc_gl_buffer_get_info(const openagc_gl_buffer *buffer,
                                          openagc_frontend_buffer_info *info)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_buffer_get_info(buffer->buffer, info);
}

openagc_result openagc_gl_destroy_buffer(openagc_gl_buffer *buffer)
{
    openagc_result result;

    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->context->bound_unpack == buffer || buffer->context->bound_pack == buffer ||
        buffer->context->bound_uniform == buffer) {
        return OPENAGC_ERROR_BUSY;
    }
    {
        uint32_t slot;

        for (slot = 0u; slot < 8u; ++slot) {
            if (buffer->context->uniforms[slot] == buffer) {
                return OPENAGC_ERROR_BUSY;
            }
        }
    }
    result = openagc_frontend_buffer_destroy(buffer->buffer);
    if (result != OPENAGC_OK) {
        return result;
    }
    buffer->context->child_count--;
    free(buffer);
    return OPENAGC_OK;
}

openagc_result openagc_gl_create_unbound_buffer(openagc_gl_context *context, uint32_t target,
                                                uint64_t size_bytes,
                                                openagc_gl_buffer **out_buffer)
{
    openagc_gl_buffer *buffer;
    openagc_frontend_buffer_desc desc;
    openagc_result result;

    if (out_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;
    if (context == NULL || size_bytes == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (target != OPENAGC_GL_PIXEL_PACK_BUFFER && target != OPENAGC_GL_PIXEL_UNPACK_BUFFER &&
        target != OPENAGC_GL_UNIFORM_BUFFER &&         target != OPENAGC_GL_ARRAY_BUFFER &&
        target != OPENAGC_GL_ELEMENT_ARRAY_BUFFER &&
        target != OPENAGC_GL_DRAW_INDIRECT_BUFFER) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    buffer = (openagc_gl_buffer *)calloc(1u, sizeof(*buffer));
    if (buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    buffer->context = context;
    buffer->target = target;
    desc = (openagc_frontend_buffer_desc)OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, target, size_bytes);
    result = openagc_frontend_buffer_create_unbound(context->frontend, &desc, &buffer->buffer);
    if (result != OPENAGC_OK) {
        free(buffer);
        return result;
    }
    context->child_count++;
    *out_buffer = buffer;
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_buffer_memory(openagc_gl_buffer *buffer,
                                             openagc_gl_memory *memory, uint64_t offset)
{
    if (buffer == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->context != memory->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_buffer_bind_memory(buffer->buffer, memory->memory, offset);
}

openagc_result openagc_gl_create_graphics_program(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_program **out_program)
{
    return openagc_gl_create_graphics_program_with_bindings(
        context, vertex, pixel, color_target, NULL, 0u, 0u, 0u, NULL, 0u, out_program);
}

openagc_result openagc_gl_create_graphics_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program)
{
    const openagc_gl_buffer *uniforms[1];
    const openagc_gl_texture *images[1];
    uint64_t offsets[1];
    uint64_t sizes[1];
    uint32_t resource_bindings[1];
    uint32_t texture_bindings[1];

    uniforms[0] = uniform;
    images[0] = sampled;
    offsets[0] = offset;
    sizes[0] = size_bytes;
    resource_bindings[0] = resource_binding;
    texture_bindings[0] = texture_binding;
    return openagc_gl_create_graphics_program_with_resources(
        context, vertex, pixel, color_target, uniform == NULL ? NULL : uniforms, offsets, sizes,
        resource_bindings, uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images,
        texture_bindings, sampled == NULL ? 0u : 1u, out_program);
}

openagc_result openagc_gl_create_graphics_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program)
{
    const openagc_frontend_buffer *frontend_buffers[8];
    const openagc_frontend_image *frontend_images[8];
    openagc_gl_program *program;
    uint32_t index;
    openagc_result result;

    if (out_program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = NULL;
    if (context == NULL || color_target == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (resource_count > 8u || texture_count > 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (color_target->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    for (index = 0u; index < resource_count; ++index) {
        if (uniforms == NULL || uniforms[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (uniforms[index]->context != context) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_buffers[index] = uniforms[index]->buffer;
    }
    for (index = 0u; index < texture_count; ++index) {
        if (sampled == NULL || sampled[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (sampled[index]->context != context) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_images[index] = sampled[index]->image;
    }
    program = (openagc_gl_program *)calloc(1u, sizeof(*program));
    if (program == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    program->context = context;
    result = openagc_frontend_graphics_pipeline_create_with_resources(
        context->frontend, vertex, pixel, color_target->image,
        resource_count == 0u ? NULL : frontend_buffers, offsets, sizes, resource_bindings,
        resource_count, texture_count == 0u ? NULL : frontend_images, texture_bindings,
        texture_count, &program->pipeline);
    if (result != OPENAGC_OK) {
        free(program);
        return result;
    }
    context->child_count++;
    *out_program = program;
    return OPENAGC_OK;
}

openagc_result openagc_gl_create_program(openagc_gl_context *context,
                                         const openagc_shader_artifact_desc *desc,
                                         openagc_gl_program **out_program)
{
    return openagc_gl_create_program_with_bindings(context, desc, NULL, 0u, 0u, 0u, NULL, 0u,
                                                   out_program);
}

openagc_result openagc_gl_create_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program)
{
    const openagc_gl_buffer *uniforms[1];
    const openagc_gl_texture *images[1];
    uint64_t offsets[1];
    uint64_t sizes[1];
    uint32_t resource_bindings[1];
    uint32_t texture_bindings[1];

    uniforms[0] = uniform;
    images[0] = sampled;
    offsets[0] = offset;
    sizes[0] = size_bytes;
    resource_bindings[0] = resource_binding;
    texture_bindings[0] = texture_binding;
    return openagc_gl_create_program_with_resources(
        context, desc, uniform == NULL ? NULL : uniforms, offsets, sizes, resource_bindings,
        uniform == NULL ? 0u : 1u, sampled == NULL ? NULL : images, texture_bindings,
        sampled == NULL ? 0u : 1u, out_program);
}

openagc_result openagc_gl_create_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program)
{
    openagc_gl_program *program;
    openagc_shader_pipeline_desc plan = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_resource_binding resources[8];
    openagc_shader_texture_binding textures[8];
    const openagc_frontend_buffer *frontend_buffers[8];
    const openagc_frontend_image *frontend_images[8];
    uint32_t index;
    openagc_result result;

    if (out_program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_program = NULL;
    if (context == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->stage != OPENAGC_SHADER_STAGE_COMPUTE) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (resource_count > 8u || texture_count > 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < resource_count; ++index) {
        if (uniforms == NULL || uniforms[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (uniforms[index]->context != context) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_buffers[index] = uniforms[index]->buffer;
    }
    for (index = 0u; index < texture_count; ++index) {
        if (sampled == NULL || sampled[index] == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (sampled[index]->context != context) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        frontend_images[index] = sampled[index]->image;
    }
    program = (openagc_gl_program *)calloc(1u, sizeof(*program));
    if (program == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    program->context = context;
    result = openagc_shader_artifact_intake_host(context->gpu, desc, &program->artifact);
    if (result != OPENAGC_OK) {
        free(program);
        return result;
    }
    plan.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    plan.compute = program->artifact;
    memset(resources, 0, sizeof(resources));
    memset(textures, 0, sizeof(textures));
    result = openagc_frontend_apply_reflection_set(
        resource_count == 0u ? NULL : frontend_buffers, offsets, sizes, resource_bindings,
        resource_count, texture_count == 0u ? NULL : frontend_images, texture_bindings,
        texture_count, resources, textures, &plan);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(program->artifact);
        free(program);
        return result;
    }
    result = openagc_frontend_pipeline_create(context->frontend, &plan, &program->pipeline);
    if (result != OPENAGC_OK) {
        (void)openagc_shader_artifact_destroy(program->artifact);
        free(program);
        return result;
    }
    context->child_count++;
    *out_program = program;
    return OPENAGC_OK;
}

openagc_result openagc_gl_program_set_vertex_stride(openagc_gl_program *program, uint32_t stride)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_stride(program->pipeline, stride);
}

openagc_result openagc_gl_program_set_sampler(openagc_gl_program *program,
                                              openagc_gl_sampler *sampler)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (sampler != NULL && sampler->context != program->context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    return openagc_frontend_pipeline_set_sampler(
        program->pipeline, sampler == NULL ? NULL : sampler->sampler);
}

openagc_result openagc_gl_program_set_vertex_input(openagc_gl_program *program, uint32_t stride,
                                                  uint32_t attribute_offset,
                                                  uint32_t attribute_bytes)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_input(program->pipeline, stride, attribute_offset,
                                                     attribute_bytes);
}

openagc_result openagc_gl_program_set_vertex_attributes(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_attributes(program->pipeline, stride, offsets, bytes,
                                                          count);
}

openagc_result openagc_gl_program_uniform(openagc_gl_program *program, uint32_t offset,
                                          const void *bytes, uint32_t size)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_write_push_constants(program->pipeline, offset, bytes, size);
}

openagc_result openagc_gl_program_read_uniform(const openagc_gl_program *program, uint32_t offset,
                                               void *bytes, uint32_t size)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_read_push_constants(program->pipeline, offset, bytes, size);
}

openagc_result openagc_gl_program_set_blend(openagc_gl_program *program, uint32_t enable,
                                            uint32_t src_factor, uint32_t dst_factor)
{
    openagc_frontend_blend_factor src = 0u;
    openagc_frontend_blend_factor dst = 0u;
    openagc_result result;

    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_translate_blend_factor(OPENAGC_FRONTEND_OPENGL, src_factor, &src);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_frontend_translate_blend_factor(OPENAGC_FRONTEND_OPENGL, dst_factor, &dst);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_pipeline_set_blend(program->pipeline, enable, src, dst);
}

openagc_result openagc_gl_program_get_blend(const openagc_gl_program *program, uint32_t *enable,
                                            openagc_frontend_blend_factor *src,
                                            openagc_frontend_blend_factor *dst)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_blend(program->pipeline, enable, src, dst);
}

openagc_result openagc_gl_program_set_primitive(openagc_gl_program *program, uint32_t mode)
{
    openagc_frontend_primitive primitive = 0u;
    openagc_result result;

    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_translate_primitive(OPENAGC_FRONTEND_OPENGL, mode, &primitive);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_pipeline_set_primitive(program->pipeline, primitive);
}

openagc_result openagc_gl_program_get_primitive(const openagc_gl_program *program,
                                                openagc_frontend_primitive *out_primitive)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_primitive(program->pipeline, out_primitive);
}

openagc_result openagc_gl_program_set_vertex_divisors(openagc_gl_program *program,
                                                     const uint32_t *divisors, uint32_t count)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_vertex_rates(program->pipeline, divisors, count);
}

openagc_result openagc_gl_program_set_vertex_formats(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets,
    const uint32_t *components, uint32_t type, uint32_t count)
{
    openagc_frontend_vertex_format shared[8];
    uint32_t bytes[8];
    uint32_t index;
    openagc_result result;

    if (program == NULL || offsets == NULL || components == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u || count > 8u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < count; ++index) {
        result = openagc_frontend_translate_vertex_format(OPENAGC_FRONTEND_OPENGL, type,
                                                          components[index], &shared[index],
                                                          &bytes[index]);
        if (result != OPENAGC_OK) {
            return result;
        }
    }
    return openagc_frontend_pipeline_set_vertex_formats(program->pipeline, stride, offsets, shared,
                                                       count);
}

openagc_result openagc_gl_program_get_vertex_format(const openagc_gl_program *program,
                                                   uint32_t index, uint32_t *offset,
                                                   openagc_frontend_vertex_format *format,
                                                   uint32_t *bytes)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_vertex_format(program->pipeline, index, offset, format,
                                                      bytes);
}

openagc_result openagc_gl_program_get_vertex_attribute(const openagc_gl_program *program,
                                                       uint32_t index, uint32_t *offset,
                                                       uint32_t *bytes)
{
    if (program == NULL) {
        if (offset != NULL) {
            *offset = 0u;
        }
        if (bytes != NULL) {
            *bytes = 0u;
        }
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_vertex_attribute(program->pipeline, index, offset, bytes);
}

openagc_result openagc_gl_program_accepts_layout(
    const openagc_gl_program *program, const openagc_frontend_pipeline_layout *layout)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_layout_matches(program->pipeline, layout);
}

openagc_result openagc_gl_program_get_info(const openagc_gl_program *program,
                                           openagc_frontend_pipeline_info *info)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_info(program->pipeline, info);
}

openagc_result openagc_gl_program_set_psbc_register_snapshot(
    openagc_gl_program *program, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_psbc_register_snapshot(
        program->pipeline, vertex_metadata, vertex_metadata_size, pixel_metadata,
        pixel_metadata_size);
}

openagc_result openagc_gl_program_set_agc_linked_registers(
    openagc_gl_program *program,
    const openagc_frontend_agc_register *context_records, uint32_t context_count,
    const openagc_frontend_agc_register *uconfig_records, uint32_t uconfig_count)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_agc_linked_registers(
        program->pipeline, context_records, context_count,
        uconfig_records, uconfig_count);
}

openagc_result openagc_gl_program_set_agc_target_registers(
    openagc_gl_program *program,
    const openagc_frontend_agc_register *target_records, uint32_t target_count)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_set_agc_target_registers(
        program->pipeline, target_records, target_count);
}

openagc_result openagc_gl_program_get_host_register_program(
    const openagc_gl_program *program, uint32_t *words, uint32_t max_words,
    uint32_t *out_count)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_host_register_program(program->pipeline, words,
                                                               max_words, out_count);
}

openagc_result openagc_gl_program_patch_psbc_pgm_vas(openagc_gl_program *program,
                                                     uint64_t vertex_code_va,
                                                     uint64_t pixel_code_va)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_patch_psbc_pgm_vas(program->pipeline, vertex_code_va,
                                                        pixel_code_va);
}

openagc_result openagc_gl_program_record_psbc_register_eop(openagc_gl_program *program)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_record_psbc_register_eop(program->pipeline);
}

openagc_result openagc_gl_program_get_psbc_code_vas(const openagc_gl_program *program,
                                                    uint64_t *vertex_code_va,
                                                    uint64_t *pixel_code_va)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_get_psbc_code_vas(program->pipeline, vertex_code_va,
                                                       pixel_code_va);
}

openagc_result openagc_gl_program_bind_psbc_code(openagc_gl_program *program)
{
    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_pipeline_bind_psbc_code(program->pipeline);
}

openagc_result openagc_gl_use_program(openagc_gl_context *context, openagc_gl_program *program)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (program != NULL && program->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    context->bound_program = program;
    return OPENAGC_OK;
}

openagc_result openagc_gl_destroy_program(openagc_gl_program *program)
{
    openagc_result result;

    if (program == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (program->context->bound_program == program) {
        return OPENAGC_ERROR_BUSY;
    }
    result = openagc_frontend_pipeline_destroy(program->pipeline);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (program->artifact == NULL) {
        program->context->child_count--;
        free(program);
        return OPENAGC_OK;
    }
    result = openagc_shader_artifact_destroy(program->artifact);
    if (result != OPENAGC_OK) {
        return result;
    }
    program->context->child_count--;
    free(program);
    return OPENAGC_OK;
}

openagc_result openagc_gl_bind_framebuffer(openagc_gl_context *context,
                                           openagc_gl_framebuffer *framebuffer)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer != NULL && framebuffer->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    context->draw_target = framebuffer;
    return OPENAGC_OK;
}

static void openagc_gl_recorded_objects(const openagc_gl_context *context,
                                       const openagc_frontend_buffer **buffers, uint64_t *offsets,
                                       uint64_t *sizes, const openagc_frontend_image **images)
{
    uint32_t slot;

    for (slot = 0u; slot < 8u; ++slot) {
        buffers[slot] = context->uniforms[slot] == NULL ? NULL : context->uniforms[slot]->buffer;
        offsets[slot] = context->uniform_offsets[slot];
        sizes[slot] = context->uniform_sizes[slot];
        images[slot] = context->samples[slot] == NULL ? NULL : context->samples[slot]->image;
    }
}

openagc_result openagc_gl_draw_arrays_indirect(openagc_gl_context *context,
                                              openagc_gl_buffer *buffer, uint64_t offset)
{
    if (context == NULL || buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (context->draw_target == NULL || context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (openagc_frontend_render_pass_begin(context->draw_target->pass) != OPENAGC_OK &&
        context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[8];
        const openagc_frontend_image *images[8];
        uint64_t offsets[8];
        uint64_t sizes[8];

        openagc_gl_recorded_objects(context, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_matches(
                context->draw_target->pass, buffers, offsets, sizes, images,
                context->bound_sampler == NULL ? NULL : context->bound_sampler->sampler) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_render_pass_draw_indirect(context->draw_target->pass, buffer->buffer,
                                                     offset);
}

openagc_result openagc_gl_draw_arrays(openagc_gl_context *context, uint32_t first,
                                      uint32_t count)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (context->draw_target == NULL || context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (openagc_frontend_render_pass_begin(context->draw_target->pass) != OPENAGC_OK &&
        context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[8];
        const openagc_frontend_image *images[8];
        uint64_t offsets[8];
        uint64_t sizes[8];

        openagc_gl_recorded_objects(context, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_matches(
                context->draw_target->pass, buffers, offsets, sizes, images,
                context->bound_sampler == NULL ? NULL : context->bound_sampler->sampler) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_render_pass_draw(context->draw_target->pass, count, 1u, first, 0u);
}

openagc_result openagc_gl_draw_elements(openagc_gl_context *context, uint32_t count,
                                        uint32_t type, uint64_t byte_offset)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    uint32_t width = 0u;
    openagc_result result = openagc_frontend_translate_index_width(OPENAGC_FRONTEND_OPENGL, type,
                                                                   &width);

    if (result != OPENAGC_OK) {
        return result;
    }
    if ((byte_offset & ((uint64_t)width - 1u)) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (context->draw_target == NULL || context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (openagc_frontend_render_pass_begin(context->draw_target->pass) != OPENAGC_OK &&
        context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[8];
        const openagc_frontend_image *images[8];
        uint64_t offsets[8];
        uint64_t sizes[8];

        openagc_gl_recorded_objects(context, buffers, offsets, sizes, images);
        if (openagc_frontend_render_pass_matches(
                context->draw_target->pass, buffers, offsets, sizes, images,
                context->bound_sampler == NULL ? NULL : context->bound_sampler->sampler) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    result = openagc_frontend_render_pass_set_index_width(context->draw_target->pass, width);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_frontend_render_pass_draw_indexed(
        context->draw_target->pass, count, 1u, (uint32_t)(byte_offset / width), 0, 0u);
}

openagc_result openagc_gl_create_query(openagc_gl_context *context, openagc_frontend_query_kind kind,
                                       uint32_t count, openagc_gl_query **out_query)
{
    openagc_gl_query *query;
    openagc_result result;

    if (out_query == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_query = NULL;
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    query = (openagc_gl_query *)calloc(1u, sizeof(*query));
    if (query == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    result = openagc_frontend_query_pool_create(context->frontend, kind, count, &query->pool);
    if (result != OPENAGC_OK) {
        free(query);
        return result;
    }
    query->context = context;
    context->child_count++;
    *out_query = query;
    return OPENAGC_OK;
}

openagc_result openagc_gl_begin_query(openagc_gl_context *context, openagc_gl_query *query,
                                     uint32_t index)
{
    openagc_result result;

    if (context == NULL || query == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (query->context != context) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (context->draw_target == NULL || context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    result = openagc_frontend_render_pass_begin(context->draw_target->pass);
    if (result != OPENAGC_OK && result != OPENAGC_ERROR_BAD_STATE) {
        return result;
    }
    return openagc_frontend_render_pass_begin_query(context->draw_target->pass, query->pool, index);
}

openagc_result openagc_gl_end_query(openagc_gl_context *context)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (context->draw_target == NULL || context->draw_target->pass == NULL) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return openagc_frontend_render_pass_end_query(context->draw_target->pass);
}

openagc_result openagc_gl_get_query(const openagc_gl_query *query, uint32_t index,
                                   uint32_t *available)
{
    if (query == NULL) {
        if (available != NULL) {
            *available = 0u;
        }
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return openagc_frontend_query_get(query->pool, index, available);
}

openagc_result openagc_gl_destroy_query(openagc_gl_query *query)
{
    openagc_result result;

    if (query == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_frontend_query_pool_destroy(query->pool);
    if (result != OPENAGC_OK) {
        return result;
    }
    query->context->child_count--;
    free(query);
    return OPENAGC_OK;
}

openagc_result openagc_gl_dispatch_compute(openagc_gl_context *context, uint32_t groups_x,
                                           uint32_t groups_y, uint32_t groups_z)
{
    if (context == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (context->bound_program == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    {
        const openagc_frontend_buffer *buffers[8];
        const openagc_frontend_image *images[8];
        uint64_t offsets[8];
        uint64_t sizes[8];

        openagc_gl_recorded_objects(context, buffers, offsets, sizes, images);
        {
            openagc_frontend_pipeline_layout *layout = NULL;

            if (openagc_frontend_pipeline_get_layout(context->bound_program->pipeline, &layout) !=
                    OPENAGC_OK ||
                openagc_frontend_layout_matches(context->bound_program->pipeline, layout) !=
                    OPENAGC_OK) {
                return OPENAGC_ERROR_BAD_STATE;
            }
        }
        if (openagc_frontend_recording_matches(
                context->bound_program->pipeline, buffers, offsets, sizes, images,
                context->bound_sampler == NULL ? NULL : context->bound_sampler->sampler) !=
            OPENAGC_OK) {
            return OPENAGC_ERROR_BAD_STATE;
        }
    }
    return openagc_frontend_dispatch(context->bound_program->pipeline, groups_x, groups_y,
                                    groups_z);
}

openagc_result openagc_gl_bind_default_framebuffer(openagc_gl_context *context)
{
    return context == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                           : OPENAGC_ERROR_UNSUPPORTED_OPERATION;
}
