/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_OPENGL_H
#define OPENAGC_OPENGL_H

#include "openagc/frontend.h"
#include "openagc/shader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_GL_API_VERSION 1u

#define OPENAGC_GL_TEXTURE_2D OPENAGC_FRONTEND_GL_TEXTURE_2D
#define OPENAGC_GL_RENDERBUFFER OPENAGC_FRONTEND_GL_RENDERBUFFER
#define OPENAGC_GL_RGBA8 OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8
#define OPENAGC_GL_FRAMEBUFFER 0x8D40u
#define OPENAGC_GL_COLOR_ATTACHMENT0 0x8CE0u
#define OPENAGC_GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008u
#define OPENAGC_GL_PIXEL_PACK_BUFFER OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER
#define OPENAGC_GL_PIXEL_UNPACK_BUFFER OPENAGC_FRONTEND_GL_PIXEL_UNPACK_BUFFER
#define OPENAGC_GL_UNIFORM_BUFFER OPENAGC_FRONTEND_GL_UNIFORM_BUFFER
#define OPENAGC_GL_ARRAY_BUFFER OPENAGC_FRONTEND_GL_ARRAY_BUFFER
#define OPENAGC_GL_ELEMENT_ARRAY_BUFFER OPENAGC_FRONTEND_GL_ELEMENT_ARRAY_BUFFER
#define OPENAGC_GL_DRAW_INDIRECT_BUFFER 0x8F3Fu
#define OPENAGC_GL_UNSIGNED_SHORT 0x1403u
#define OPENAGC_GL_UNSIGNED_INT 0x1405u

typedef struct openagc_gl_context openagc_gl_context;
typedef struct openagc_gl_query openagc_gl_query;
typedef struct openagc_gl_texture openagc_gl_texture;
typedef struct openagc_gl_renderbuffer openagc_gl_renderbuffer;
typedef struct openagc_gl_framebuffer openagc_gl_framebuffer;
typedef struct openagc_gl_buffer openagc_gl_buffer;
typedef struct openagc_gl_memory openagc_gl_memory;
typedef struct openagc_gl_program openagc_gl_program;
typedef struct openagc_gl_sampler openagc_gl_sampler;

typedef struct openagc_gl_context_desc {
    uint32_t struct_size;
    uint32_t api_version;
} openagc_gl_context_desc;

typedef struct openagc_gl_image_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t internal_format;
    uint32_t width;
    uint32_t height;
} openagc_gl_image_desc;

typedef struct openagc_gl_capabilities {
    uint32_t struct_size;
    uint32_t gpu_execution;
    uint32_t presentation;
    uint32_t default_framebuffer;
} openagc_gl_capabilities;

#define OPENAGC_GL_CONTEXT_DESC_INIT \
    { (uint32_t)sizeof(openagc_gl_context_desc), OPENAGC_GL_API_VERSION }
#define OPENAGC_GL_IMAGE_DESC_INIT(format_value, width_value, height_value) \
    { (uint32_t)sizeof(openagc_gl_image_desc), OPENAGC_GL_API_VERSION, \
      (format_value), (width_value), (height_value) }
#define OPENAGC_GL_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_gl_capabilities), 0u, 0u, 0u }

openagc_result openagc_gl_context_create(const openagc_gl_context_desc *desc,
                                         openagc_gl_context **out_context);
openagc_result openagc_gl_context_destroy(openagc_gl_context *context);
/* Host-only last WRITE_DATA / Step-U register+EOP snapshot (gpu_submitted=0). */
openagc_result openagc_gl_context_get_last_write(const openagc_gl_context *context,
                                                  openagc_gpu_submission_view *view);
openagc_result openagc_gl_get_capabilities(const openagc_gl_context *context,
                                           openagc_gl_capabilities *capabilities);
openagc_result openagc_gl_get_format(const openagc_gl_context *context, uint32_t index,
                                     uint32_t *native_format,
                                     openagc_graphics_format *backend_format);

openagc_result openagc_gl_create_texture(openagc_gl_context *context,
                                         const openagc_gl_image_desc *desc,
                                         openagc_gl_texture **out_texture);
openagc_result openagc_gl_tex_sub_image(openagc_gl_texture *texture, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes);
openagc_result openagc_gl_tex_sub_image_from_buffer(openagc_gl_texture *texture,
                                                    openagc_gl_buffer *buffer,
                                                    uint64_t buffer_offset,
                                                    uint64_t image_offset,
                                                    uint64_t size_bytes);
openagc_result openagc_gl_get_tex_image(openagc_gl_texture *texture, uint64_t offset,
                                        void *bytes, uint64_t size_bytes);
openagc_result openagc_gl_get_tex_image_to_buffer(openagc_gl_texture *texture,
                                                  uint64_t image_offset,
                                                  openagc_gl_buffer *buffer,
                                                  uint64_t buffer_offset,
                                                  uint64_t size_bytes);
/* Reads the framebuffer's color attachment, the way CopyTexSubImage2D does. No
   rasterization, blending, or depth test is involved; both images keep the
   logical state they already had. */
openagc_result openagc_gl_copy_tex_sub_image(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_texture *texture, uint32_t xoffset,
                                             uint32_t yoffset, uint32_t x, uint32_t y,
                                             uint32_t width, uint32_t height);
/* Sampling derives SHADER_READ/GRAPHICS. */
openagc_result openagc_gl_create_sampler(openagc_gl_context *context, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_gl_sampler **out_sampler);
openagc_result openagc_gl_bind_sampler(openagc_gl_context *context,
                                       openagc_gl_sampler *sampler);
openagc_result openagc_gl_destroy_sampler(openagc_gl_sampler *sampler);
openagc_result openagc_gl_bind_texture_unit(openagc_gl_context *context, uint32_t unit,
                                          openagc_gl_texture *texture);
openagc_result openagc_gl_bind_texture_for_sampling(openagc_gl_context *context,
                                                    openagc_gl_texture *texture);
openagc_result openagc_gl_texture_get_info(const openagc_gl_texture *texture,
                                           openagc_frontend_image_info *info);
openagc_result openagc_gl_destroy_texture(openagc_gl_texture *texture);

openagc_result openagc_gl_create_renderbuffer(openagc_gl_context *context,
                                              const openagc_gl_image_desc *desc,
                                              openagc_gl_renderbuffer **out_renderbuffer);
openagc_result openagc_gl_renderbuffer_get_info(const openagc_gl_renderbuffer *renderbuffer,
                                                openagc_frontend_image_info *info);
openagc_result openagc_gl_renderbuffer_read(openagc_gl_renderbuffer *renderbuffer,
                                            uint64_t offset, void *bytes, uint64_t size_bytes);
openagc_result openagc_gl_destroy_renderbuffer(openagc_gl_renderbuffer *renderbuffer);

openagc_result openagc_gl_create_framebuffer(openagc_gl_context *context,
                                             openagc_gl_framebuffer **out_framebuffer);
/* Attachment derives COLOR_TARGET/GRAPHICS. The default framebuffer is absent. */
openagc_result openagc_gl_framebuffer_renderbuffer(openagc_gl_framebuffer *framebuffer,
                                                   openagc_gl_renderbuffer *renderbuffer);
/* NULL detaches. Derives DEPTH_TARGET/GRAPHICS. No depth test runs. */
openagc_result openagc_gl_framebuffer_depth_renderbuffer(
    openagc_gl_framebuffer *framebuffer, openagc_gl_renderbuffer *renderbuffer);
openagc_result openagc_gl_framebuffer_texture(openagc_gl_framebuffer *framebuffer,
                                              openagc_gl_texture *texture);
openagc_result openagc_gl_viewport(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height);
openagc_result openagc_gl_bind_program(openagc_gl_framebuffer *framebuffer,
                                        openagc_gl_program *program);
openagc_result openagc_gl_bind_index_buffer(openagc_gl_framebuffer *framebuffer,
                                            openagc_gl_buffer *buffer, uint64_t offset);
openagc_result openagc_gl_bind_vertex_buffer(openagc_gl_framebuffer *framebuffer,
                                             openagc_gl_buffer *buffer, uint64_t offset);
openagc_result openagc_gl_scissor(openagc_gl_framebuffer *framebuffer, uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height);
openagc_result openagc_gl_clear(openagc_gl_framebuffer *framebuffer, openagc_color color);
openagc_result openagc_gl_clear_scissor(openagc_gl_framebuffer *framebuffer, openagc_color color);
/* depth is 0..1. Requires a depth renderbuffer and writes D24S8 inside the scissor. */
openagc_result openagc_gl_clear_depth(openagc_gl_framebuffer *framebuffer, float depth,
                                     uint32_t stencil);
openagc_result openagc_gl_framebuffer_begin(openagc_gl_framebuffer *framebuffer,
                                          openagc_frontend_load_op load_op, openagc_color color);
openagc_result openagc_gl_framebuffer_begin_with_depth(
    openagc_gl_framebuffer *framebuffer, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil);
openagc_result openagc_gl_read_pixels(openagc_gl_framebuffer *framebuffer, uint64_t offset,
                                      void *bytes, uint64_t size_bytes);
openagc_result openagc_gl_destroy_framebuffer(openagc_gl_framebuffer *framebuffer);

/* The bound sampled texture moves to SHADER_READ/GRAPHICS. */
openagc_result openagc_gl_memory_barrier(openagc_gl_context *context, uint32_t barriers);
/* Publishes the synchronous timeline point. Does not block. */
openagc_result openagc_gl_finish(openagc_gl_context *context);
/* Does not block. A value the timeline has not reached is NOT_READY. */
openagc_result openagc_gl_client_wait(const openagc_gl_context *context, uint64_t value,
                                      openagc_frontend_timeline_info *info);
openagc_result openagc_gl_fence_poll(const openagc_gl_context *context, uint64_t value,
                                     openagc_frontend_timeline_info *info);

openagc_result openagc_gl_allocate_memory(openagc_gl_context *context, uint64_t size_bytes,
                                          openagc_gl_memory **out_memory);
openagc_result openagc_gl_free_memory(openagc_gl_memory *memory);
openagc_result openagc_gl_create_buffer(openagc_gl_context *context, uint32_t target,
                                        uint64_t size_bytes, openagc_gl_buffer **out_buffer);
openagc_result openagc_gl_bind_uniform_base(openagc_gl_context *context, uint32_t index,
                                          openagc_gl_buffer *buffer);
/* The range must match the plan. A zero size is OUT_OF_RANGE. */
openagc_result openagc_gl_bind_uniform_range(openagc_gl_context *context, uint32_t index,
                                            openagc_gl_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes);
openagc_result openagc_gl_bind_buffer(openagc_gl_context *context, uint32_t target,
                                      openagc_gl_buffer *buffer);
/* Host write into unpack/uniform/array/element/indirect storage. */
openagc_result openagc_gl_buffer_data(openagc_gl_buffer *buffer, uint64_t offset,
                                      const void *bytes, uint64_t size_bytes);
/* Same upload path as buffer_data; named for glBufferSubData parity with
   vkCmdUpdateBuffer (Vulkan defers the write to queue submit). */
openagc_result openagc_gl_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                          const void *bytes, uint64_t size_bytes);
openagc_result openagc_gl_get_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                              void *bytes, uint64_t size_bytes);
/* Repeating 32-bit pattern over a copy-destination range. A uniform buffer holds
   no copy-destination usage and is refused. */
openagc_result openagc_gl_clear_buffer_sub_data(openagc_gl_buffer *buffer, uint64_t offset,
                                                uint64_t size_bytes, uint32_t value);
/* Same shared copy as vkCmdCopyBuffer. Both buffers must belong to one context
   and carry copy-source / copy-destination usage (pixel pack/unpack). */
openagc_result openagc_gl_copy_buffer_sub_data(openagc_gl_buffer *source, uint64_t source_offset,
                                               openagc_gl_buffer *destination,
                                               uint64_t destination_offset, uint64_t size_bytes);
/* Step H composite: DMA copy then WRITE_DATA fill of the destination head. */
openagc_result openagc_gl_copy_buffer_then_clear_sub_data(
    openagc_gl_buffer *source, uint64_t source_offset, openagc_gl_buffer *destination,
    uint64_t destination_offset, uint64_t size_bytes, uint32_t value, uint64_t fill_bytes);
openagc_result openagc_gl_buffer_get_info(const openagc_gl_buffer *buffer,
                                          openagc_frontend_buffer_info *info);
openagc_result openagc_gl_destroy_buffer(openagc_gl_buffer *buffer);
openagc_result openagc_gl_create_unbound_buffer(openagc_gl_context *context, uint32_t target,
                                                uint64_t size_bytes,
                                                openagc_gl_buffer **out_buffer);
openagc_result openagc_gl_bind_buffer_memory(openagc_gl_buffer *buffer,
                                             openagc_gl_memory *memory, uint64_t offset);

/* The plan is host-only. A compiler-verified or GPU-executable plan is refused. */
openagc_result openagc_gl_create_graphics_program(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_program **out_program);
openagc_result openagc_gl_create_program(openagc_gl_context *context,
                                         const openagc_shader_artifact_desc *desc,
                                         openagc_gl_program **out_program);
openagc_result openagc_gl_create_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program);
/* One entry per reflected slot, in increasing binding order. */
openagc_result openagc_gl_create_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *desc,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program);
openagc_result openagc_gl_create_graphics_program_with_bindings(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    openagc_gl_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_gl_texture *sampled, uint32_t texture_binding,
    openagc_gl_program **out_program);
/* One entry per reflected slot, in increasing binding order. */
openagc_result openagc_gl_create_graphics_program_with_resources(
    openagc_gl_context *context, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_gl_renderbuffer *color_target,
    const openagc_gl_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_gl_texture *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_gl_program **out_program);
openagc_result openagc_gl_program_set_sampler(openagc_gl_program *program,
                                              openagc_gl_sampler *sampler);
openagc_result openagc_gl_program_set_vertex_stride(openagc_gl_program *program, uint32_t stride);
openagc_result openagc_gl_program_set_vertex_input(openagc_gl_program *program, uint32_t stride,
                                                  uint32_t attribute_offset,
                                                  uint32_t attribute_bytes);
openagc_result openagc_gl_program_get_vertex_attribute(const openagc_gl_program *program,
                                                       uint32_t index, uint32_t *offset,
                                                       uint32_t *bytes);
openagc_result openagc_gl_program_set_vertex_attributes(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count);
/* components are 1..4 and type is GL_FLOAT. One type applies to every attribute. */
/* divisors are 0 or 1. A larger divisor is refused. */
openagc_result openagc_gl_program_set_primitive(openagc_gl_program *program, uint32_t mode);
openagc_result openagc_gl_program_set_blend(openagc_gl_program *program, uint32_t enable,
                                            uint32_t src_factor, uint32_t dst_factor);
openagc_result openagc_gl_program_get_blend(const openagc_gl_program *program, uint32_t *enable,
                                            openagc_frontend_blend_factor *src,
                                            openagc_frontend_blend_factor *dst);
openagc_result openagc_gl_program_uniform(openagc_gl_program *program, uint32_t offset,
                                          const void *bytes, uint32_t size);
openagc_result openagc_gl_program_read_uniform(const openagc_gl_program *program, uint32_t offset,
                                               void *bytes, uint32_t size);
openagc_result openagc_gl_program_get_primitive(const openagc_gl_program *program,
                                                openagc_frontend_primitive *out_primitive);
openagc_result openagc_gl_program_set_vertex_divisors(openagc_gl_program *program,
                                                     const uint32_t *divisors, uint32_t count);
openagc_result openagc_gl_program_set_vertex_formats(
    openagc_gl_program *program, uint32_t stride, const uint32_t *offsets,
    const uint32_t *components, uint32_t type, uint32_t count);
openagc_result openagc_gl_program_get_vertex_format(const openagc_gl_program *program,
                                                   uint32_t index, uint32_t *offset,
                                                   openagc_frontend_vertex_format *format,
                                                   uint32_t *bytes);
openagc_result openagc_gl_program_accepts_layout(
    const openagc_gl_program *program, const openagc_frontend_pipeline_layout *layout);
openagc_result openagc_gl_program_get_info(const openagc_gl_program *program,
                                           openagc_frontend_pipeline_info *info);
/* Host-only PSBC register-program snapshot; shared frontend path. */
openagc_result openagc_gl_program_set_psbc_register_snapshot(
    openagc_gl_program *program, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size);
openagc_result openagc_gl_program_get_host_register_program(
    const openagc_gl_program *program, uint32_t *words, uint32_t max_words,
    uint32_t *out_count);
openagc_result openagc_gl_program_patch_psbc_pgm_vas(openagc_gl_program *program,
                                                     uint64_t vertex_code_va,
                                                     uint64_t pixel_code_va);
openagc_result openagc_gl_program_record_psbc_register_eop(openagc_gl_program *program);
openagc_result openagc_gl_program_get_psbc_code_vas(const openagc_gl_program *program,
                                                    uint64_t *vertex_code_va,
                                                    uint64_t *pixel_code_va);
openagc_result openagc_gl_program_bind_psbc_code(openagc_gl_program *program);
openagc_result openagc_gl_use_program(openagc_gl_context *context,
                                      openagc_gl_program *program);
openagc_result openagc_gl_destroy_program(openagc_gl_program *program);

openagc_result openagc_gl_bind_framebuffer(openagc_gl_context *context,
                                           openagc_gl_framebuffer *framebuffer);
openagc_result openagc_gl_draw_arrays_indirect(openagc_gl_context *context,
                                              openagc_gl_buffer *buffer, uint64_t offset);
openagc_result openagc_gl_draw_arrays(openagc_gl_context *context, uint32_t first,
                                      uint32_t count);
/* UNSIGNED_SHORT or UNSIGNED_INT. The byte offset selects the first index.
   An unbound index buffer is BAD_STATE. */
openagc_result openagc_gl_draw_elements(openagc_gl_context *context, uint32_t count,
                                        uint32_t type, uint64_t byte_offset);
openagc_result openagc_gl_create_query(openagc_gl_context *context, openagc_frontend_query_kind kind,
                                       uint32_t count, openagc_gl_query **out_query);
openagc_result openagc_gl_begin_query(openagc_gl_context *context, openagc_gl_query *query,
                                     uint32_t index);
openagc_result openagc_gl_end_query(openagc_gl_context *context);
openagc_result openagc_gl_get_query(const openagc_gl_query *query, uint32_t index,
                                   uint32_t *available);
openagc_result openagc_gl_destroy_query(openagc_gl_query *query);
openagc_result openagc_gl_dispatch_compute(openagc_gl_context *context, uint32_t groups_x,
                                           uint32_t groups_y, uint32_t groups_z);
openagc_result openagc_gl_bind_default_framebuffer(openagc_gl_context *context);

#ifdef __cplusplus
}
#endif

#endif
