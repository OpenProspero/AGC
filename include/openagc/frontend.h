/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_FRONTEND_H
#define OPENAGC_FRONTEND_H

#include "openagc/shader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_FRONTEND_API_VERSION 1u
/* Vulkan's minimum maxComputeWorkGroupCount on each axis. */
#define OPENAGC_FRONTEND_MAX_DISPATCH_GROUPS 65535u
/* Same alignment the heap uses for a dedicated placement. */
#define OPENAGC_FRONTEND_MEMORY_ALIGN 256u
#define OPENAGC_FRONTEND_DEFAULT_STAGING_BYTES 65536u
#define OPENAGC_FRONTEND_MAX_STAGING_BYTES 1048576u

typedef struct openagc_frontend_device openagc_frontend_device;
typedef struct openagc_frontend_image openagc_frontend_image;
typedef struct openagc_frontend_buffer openagc_frontend_buffer;
typedef struct openagc_frontend_memory openagc_frontend_memory;
typedef struct openagc_frontend_sampler openagc_frontend_sampler;
typedef struct openagc_frontend_render_pass openagc_frontend_render_pass;
typedef struct openagc_frontend_query_pool openagc_frontend_query_pool;
typedef struct openagc_frontend_timeline openagc_frontend_timeline;
typedef struct openagc_frontend_pipeline openagc_frontend_pipeline;
typedef struct openagc_frontend_pipeline_layout openagc_frontend_pipeline_layout;

typedef uint32_t openagc_frontend_kind;
enum {
    /* native_* values are published Vulkan 1.0 enumerants. */
    OPENAGC_FRONTEND_VULKAN = 1u,
    /* native_* values are published OpenGL enumerants. */
    OPENAGC_FRONTEND_OPENGL = 2u
};

/* Published Vulkan 1.0 enumerants this host backend accepts. */
#define OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM 37u
#define OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM 44u
#define OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT 129u
#define OPENAGC_FRONTEND_VK_FORMAT_R32_SFLOAT 100u
#define OPENAGC_FRONTEND_VK_FORMAT_R32G32_SFLOAT 103u
#define OPENAGC_FRONTEND_VK_FORMAT_R32G32B32_SFLOAT 106u
#define OPENAGC_FRONTEND_VK_FORMAT_R32G32B32A32_SFLOAT 109u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT 0x00000020u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_SRC_BIT 0x00000001u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT 0x00000002u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT 0x00000004u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 0x00000010u
#define OPENAGC_FRONTEND_VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT 0x00000080u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED 0u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 2u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL 3u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 5u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 6u
#define OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 7u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT 0x0001u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT 0x0002u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 0x0010u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 0x0080u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDEX_BUFFER_BIT 0x0040u
#define OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT 0x0100u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_POINT_LIST 0u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_LINE_LIST 1u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_LINE_STRIP 2u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST 3u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP 4u
#define OPENAGC_FRONTEND_VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN 5u
#define OPENAGC_FRONTEND_GL_POINTS 0x0000u
#define OPENAGC_FRONTEND_GL_LINES 0x0001u
#define OPENAGC_FRONTEND_GL_LINE_STRIP 0x0003u
#define OPENAGC_FRONTEND_GL_TRIANGLES 0x0004u
#define OPENAGC_FRONTEND_GL_TRIANGLE_STRIP 0x0005u
#define OPENAGC_FRONTEND_GL_TRIANGLE_FAN 0x0006u
#define OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT16 0u
#define OPENAGC_FRONTEND_VK_INDEX_TYPE_UINT32 1u
#define OPENAGC_FRONTEND_GL_UNSIGNED_SHORT 0x1403u
#define OPENAGC_FRONTEND_GL_UNSIGNED_INT 0x1405u
#define OPENAGC_FRONTEND_VK_FILTER_NEAREST 0u
#define OPENAGC_FRONTEND_VK_FILTER_LINEAR 1u
#define OPENAGC_FRONTEND_VK_SAMPLER_ADDRESS_CLAMP_TO_EDGE 2u
#define OPENAGC_FRONTEND_GL_NEAREST 0x2600u
#define OPENAGC_FRONTEND_GL_LINEAR 0x2601u
#define OPENAGC_FRONTEND_GL_CLAMP_TO_EDGE 0x812Fu

/* Published OpenGL enumerants this host backend accepts. */
#define OPENAGC_FRONTEND_GL_FLOAT 0x1406u
#define OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8 0x8058u

typedef uint32_t openagc_frontend_vertex_format;
enum {
    OPENAGC_FRONTEND_VERTEX_R32_SFLOAT = 1u,
    OPENAGC_FRONTEND_VERTEX_R32G32_SFLOAT = 2u,
    OPENAGC_FRONTEND_VERTEX_R32G32B32_SFLOAT = 3u,
    OPENAGC_FRONTEND_VERTEX_R32G32B32A32_SFLOAT = 4u
};

typedef uint32_t openagc_frontend_primitive;
enum {
    OPENAGC_FRONTEND_PRIMITIVE_POINT_LIST = 1u,
    OPENAGC_FRONTEND_PRIMITIVE_LINE_LIST = 2u,
    OPENAGC_FRONTEND_PRIMITIVE_LINE_STRIP = 3u,
    OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_LIST = 4u,
    OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_STRIP = 5u,
    OPENAGC_FRONTEND_PRIMITIVE_TRIANGLE_FAN = 6u
};

typedef uint32_t openagc_frontend_blend_factor;
enum {
    OPENAGC_FRONTEND_BLEND_ZERO = 1u,
    OPENAGC_FRONTEND_BLEND_ONE = 2u,
    OPENAGC_FRONTEND_BLEND_SRC_ALPHA = 3u,
    OPENAGC_FRONTEND_BLEND_ONE_MINUS_SRC_ALPHA = 4u
};

#define OPENAGC_FRONTEND_VK_BLEND_FACTOR_ZERO 0u
#define OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE 1u
#define OPENAGC_FRONTEND_VK_BLEND_FACTOR_SRC_ALPHA 6u
#define OPENAGC_FRONTEND_VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA 7u
#define OPENAGC_FRONTEND_GL_ZERO 0u
#define OPENAGC_FRONTEND_GL_ONE 1u
#define OPENAGC_FRONTEND_GL_SRC_ALPHA 0x0302u
#define OPENAGC_FRONTEND_GL_ONE_MINUS_SRC_ALPHA 0x0303u
#define OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8 0x88F0u
#define OPENAGC_FRONTEND_GL_TEXTURE_2D 0x0DE1u
#define OPENAGC_FRONTEND_GL_RENDERBUFFER 0x8D41u
#define OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER 0x88EBu
#define OPENAGC_FRONTEND_GL_PIXEL_UNPACK_BUFFER 0x88ECu
#define OPENAGC_FRONTEND_GL_UNIFORM_BUFFER 0x8A11u
#define OPENAGC_FRONTEND_GL_ARRAY_BUFFER 0x8892u
#define OPENAGC_FRONTEND_GL_ELEMENT_ARRAY_BUFFER 0x8893u
#define OPENAGC_FRONTEND_GL_DRAW_INDIRECT_BUFFER 0x8F3Fu

typedef struct openagc_frontend_capabilities {
    uint32_t struct_size;
    uint32_t supported_kind_mask;
    uint32_t host_translation;
    uint32_t gpu_execution;
    uint32_t rasterization;
    uint32_t presentation;
    uint32_t supported_format_mask;
    uint32_t supported_usage_mask;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_image_bytes;
    uint32_t staging_bytes;
    uint32_t host_suballocation;
    uint32_t host_timeline;
    uint32_t host_pipeline_plans;
    uint32_t host_image_copy;
    uint32_t host_buffer_fill;
    uint64_t heap_bytes;
} openagc_frontend_capabilities;

typedef struct openagc_frontend_device_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t staging_bytes;
} openagc_frontend_device_desc;

typedef struct openagc_frontend_image_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_frontend_kind kind;
    uint32_t native_format;
    uint32_t native_usage;
    /* Requested initial layout; zero leaves the image host-owned in both kinds. */
    uint32_t native_layout;
    uint32_t width;
    uint32_t height;
    /* Zero selects a tightly packed width times four byte pitch. */
    uint32_t row_pitch_bytes;
} openagc_frontend_image_desc;

typedef struct openagc_frontend_image_info {
    uint32_t struct_size;
    openagc_frontend_kind kind;
    uint32_t native_format;
    uint32_t native_usage;
    openagc_graphics_format format;
    openagc_graphics_usage usage;
    openagc_graphics_image_state state;
    openagc_graphics_owner owner;
    uint32_t width;
    uint32_t height;
    uint32_t row_pitch_bytes;
    uint64_t footprint_bytes;
} openagc_frontend_image_info;

typedef struct openagc_frontend_buffer_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_frontend_kind kind;
    uint32_t native_usage;
    uint64_t size_bytes;
} openagc_frontend_buffer_desc;

typedef struct openagc_frontend_buffer_info {
    uint32_t struct_size;
    openagc_frontend_kind kind;
    uint32_t native_usage;
    openagc_gpu_buffer_usage usage;
    uint64_t size_bytes;
} openagc_frontend_buffer_info;

typedef struct openagc_frontend_timeline_info {
    uint32_t struct_size;
    uint32_t signaled;
    uint64_t value;
} openagc_frontend_timeline_info;

typedef struct openagc_frontend_pipeline_info {
    uint32_t struct_size;
    openagc_shader_pipeline_kind kind;
    uint32_t compiler_verified;
    uint32_t gpu_executable;
    uint32_t resource_count;
    uint32_t texture_count;
    /* Host-only SET_CONTEXT_REG/SET_SH_REG dword count; 0 until PSBC snapshot. */
    uint32_t host_register_program_dwords;
    /* 1 when SPI_SHADER_PGM_LO/HI were patched from host code VAs (still not gpu_executable). */
    uint32_t psbc_pgm_patched;
} openagc_frontend_pipeline_info;

#define OPENAGC_FRONTEND_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_frontend_capabilities), 0u, 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_FRONTEND_DEVICE_DESC_INIT \
    { (uint32_t)sizeof(openagc_frontend_device_desc), OPENAGC_FRONTEND_API_VERSION, \
      OPENAGC_FRONTEND_DEFAULT_STAGING_BYTES }
#define OPENAGC_FRONTEND_IMAGE_DESC_INIT(kind_value, format_value, usage_value, \
                                         layout_value, width_value, height_value, \
                                         pitch_value) \
    { (uint32_t)sizeof(openagc_frontend_image_desc), OPENAGC_FRONTEND_API_VERSION, \
      (kind_value), (format_value), (usage_value), (layout_value), (width_value), \
      (height_value), (pitch_value) }
#define OPENAGC_FRONTEND_IMAGE_INFO_INIT \
    { (uint32_t)sizeof(openagc_frontend_image_info), 0u, 0u, 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u }
#define OPENAGC_FRONTEND_BUFFER_DESC_INIT(kind_value, usage_value, size_value) \
    { (uint32_t)sizeof(openagc_frontend_buffer_desc), OPENAGC_FRONTEND_API_VERSION, \
      (kind_value), (usage_value), (size_value) }
#define OPENAGC_FRONTEND_BUFFER_INFO_INIT \
    { (uint32_t)sizeof(openagc_frontend_buffer_info), 0u, 0u, 0u, 0u }
#define OPENAGC_FRONTEND_TIMELINE_INFO_INIT \
    { (uint32_t)sizeof(openagc_frontend_timeline_info), 0u, 0u }
#define OPENAGC_FRONTEND_PIPELINE_INFO_INIT \
    { (uint32_t)sizeof(openagc_frontend_pipeline_info), 0u, 0u, 0u, 0u, 0u, 0u, 0u }

/* Translation of native frontend enumerants onto the shared host backend. */
openagc_result openagc_frontend_native_format_at(openagc_frontend_kind kind,
                                                 uint32_t index,
                                                 uint32_t *out_native_format);
/* Vulkan passes a VkFormat and components 0. OpenGL passes GL_FLOAT and a component count. */
openagc_result openagc_frontend_translate_vertex_format(
    openagc_frontend_kind kind, uint32_t native, uint32_t components,
    openagc_frontend_vertex_format *out_format, uint32_t *out_bytes);
openagc_result openagc_frontend_translate_format(openagc_frontend_kind kind,
                                                 uint32_t native_format,
                                                 openagc_graphics_format *out_format);
openagc_result openagc_frontend_translate_image_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_graphics_usage *out_usage);
/* Vulkan layouts are explicit; a GL caller passes zero and derives states
   from the commands it records through the shared image entry points. */
openagc_result openagc_frontend_translate_image_layout(
    openagc_frontend_kind kind, uint32_t native_layout,
    openagc_graphics_image_state *out_state, openagc_graphics_owner *out_owner);
openagc_result openagc_frontend_translate_buffer_usage(
    openagc_frontend_kind kind, uint32_t native_usage,
    openagc_gpu_buffer_usage *out_usage);

/* One staging allocation, one copy queue, and one transition recorder. */
openagc_result openagc_frontend_device_create(
    openagc_gpu_device *device, const openagc_frontend_device_desc *desc,
    openagc_frontend_device **out_frontend);
openagc_result openagc_frontend_device_get_capabilities(
    const openagc_frontend_device *frontend,
    openagc_frontend_capabilities *capabilities);
openagc_result openagc_frontend_device_destroy(openagc_frontend_device *frontend);

openagc_result openagc_frontend_image_create(openagc_frontend_device *frontend,
                                             const openagc_frontend_image_desc *desc,
                                             openagc_frontend_image **out_image);
/* Same image, with no heap placement until bind. */
openagc_result openagc_frontend_image_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_image_desc *desc,
    openagc_frontend_image **out_image);
openagc_result openagc_frontend_image_get_info(const openagc_frontend_image *image,
                                               openagc_frontend_image_info *info);
/* Records and commits one logical state/owner change and preserves it across I/O. */
openagc_result openagc_frontend_image_transition(
    openagc_frontend_image *image, openagc_graphics_image_state state,
    openagc_graphics_owner owner);
/* CPU staging copies; both preserve the image's logical state and owner. */
openagc_result openagc_frontend_image_upload(openagc_frontend_image *image,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes);
openagc_result openagc_frontend_image_readback(openagc_frontend_image *image,
                                               uint64_t offset, void *bytes,
                                               uint64_t size_bytes);
/* CPU clear of a color target. Does not change the logical state or owner. */
openagc_result openagc_frontend_image_clear(openagc_frontend_image *image,
                                            openagc_color color);
/* Rect copy between two same-format images of one frontend device. Each image is
   moved to its copy-owned state for the copy and restored afterwards. A self
   copy, a format mismatch, and a rect that leaves either image are refused. */
openagc_result openagc_frontend_image_copy_rect(openagc_frontend_image *source,
                                                uint32_t source_x, uint32_t source_y,
                                                openagc_frontend_image *destination,
                                                uint32_t destination_x, uint32_t destination_y,
                                                uint32_t width, uint32_t height);
openagc_result openagc_frontend_image_destroy(openagc_frontend_image *image);

openagc_result openagc_frontend_buffer_create(openagc_frontend_device *frontend,
                                              const openagc_frontend_buffer_desc *desc,
                                              openagc_frontend_buffer **out_buffer);
/* Same buffer, with no heap placement until bind. */
openagc_result openagc_frontend_buffer_create_unbound(
    openagc_frontend_device *frontend, const openagc_frontend_buffer_desc *desc,
    openagc_frontend_buffer **out_buffer);

/* One host-visible slice of the shared heap. Device-local memory stays refused. */
openagc_result openagc_frontend_memory_allocate(openagc_frontend_device *frontend,
                                                uint64_t size_bytes,
                                                openagc_frontend_memory **out_memory);
openagc_result openagc_frontend_memory_write(openagc_frontend_memory *memory,
                                             uint64_t offset, const void *bytes,
                                             uint64_t size_bytes);
openagc_result openagc_frontend_memory_read(const openagc_frontend_memory *memory,
                                            uint64_t offset, void *bytes,
                                            uint64_t size_bytes);
openagc_result openagc_frontend_memory_destroy(openagc_frontend_memory *memory);
/* Offset is OPENAGC_FRONTEND_MEMORY_ALIGN. Overlap and a second bind are refused. */
openagc_result openagc_frontend_buffer_bind_memory(openagc_frontend_buffer *buffer,
                                                   openagc_frontend_memory *memory,
                                                   uint64_t offset);
openagc_result openagc_frontend_image_bind_memory(openagc_frontend_image *image,
                                                  openagc_frontend_memory *memory,
                                                  uint64_t offset);
openagc_result openagc_frontend_buffer_get_info(const openagc_frontend_buffer *buffer,
                                                openagc_frontend_buffer_info *info);
/* CPU staging copies through the same path the image entry points use. */
openagc_result openagc_frontend_buffer_upload(openagc_frontend_buffer *buffer,
                                              uint64_t offset, const void *bytes,
                                              uint64_t size_bytes);
openagc_result openagc_frontend_buffer_readback(openagc_frontend_buffer *buffer,
                                                uint64_t offset, void *bytes,
                                                uint64_t size_bytes);
/* Repeating four-byte pattern over a copy-destination range. Offset and size are
   multiples of 4; a buffer without the copy-destination bit is refused. */
openagc_result openagc_frontend_buffer_fill(openagc_frontend_buffer *buffer, uint64_t offset,
                                            uint64_t size_bytes, uint32_t value);
/* CPU copy. The source must be a copy source and the destination a copy
   destination; both belong to the same frontend device. */
openagc_result openagc_frontend_buffer_copy(openagc_frontend_buffer *source,
                                            uint64_t source_offset,
                                            openagc_frontend_buffer *destination,
                                            uint64_t destination_offset,
                                            uint64_t size_bytes);
/*
 * Console Step H: one DMA + WRITE_DATA fill of the destination head in a
 * single PM4 snapshot. fill_bytes must be a multiple of 4, ≤64, and ≤
 * size_bytes. Falls back to copy then fill when the composite shape does
 * not apply.
 */
openagc_result openagc_frontend_buffer_copy_then_fill(openagc_frontend_buffer *source,
                                                      uint64_t source_offset,
                                                      openagc_frontend_buffer *destination,
                                                      uint64_t destination_offset,
                                                      uint64_t size_bytes, uint32_t value,
                                                      uint64_t fill_bytes);
/* If the image is not already a transfer destination, it is moved there for
   the copy and then restored. */
openagc_result openagc_frontend_copy_buffer_to_image(openagc_frontend_buffer *source,
                                                     uint64_t source_offset,
                                                     openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     uint64_t size_bytes);
/* If the image is not already a transfer source, it is moved there for the
   copy and then restored. The destination must be a copy destination. */
openagc_result openagc_frontend_copy_image_to_buffer(openagc_frontend_image *image,
                                                     uint64_t image_offset,
                                                     openagc_frontend_buffer *destination,
                                                     uint64_t destination_offset,
                                                     uint64_t size_bytes);
openagc_result openagc_frontend_buffer_destroy(openagc_frontend_buffer *buffer);

typedef struct openagc_frontend_sampler_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_frontend_kind kind;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t address_mode;
} openagc_frontend_sampler_desc;

typedef struct openagc_frontend_sampler_info {
    uint32_t struct_size;
    openagc_frontend_kind kind;
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t address_mode;
} openagc_frontend_sampler_info;

#define OPENAGC_FRONTEND_SAMPLER_DESC_INIT(kind_value, mag_value, min_value, address_value) \
    { (uint32_t)sizeof(openagc_frontend_sampler_desc), OPENAGC_FRONTEND_API_VERSION, \
      (kind_value), (mag_value), (min_value), (address_value) }
#define OPENAGC_FRONTEND_SAMPLER_INFO_INIT \
    { (uint32_t)sizeof(openagc_frontend_sampler_info), 0u, 0u, 0u, 0u }

/* Nearest and clamp-to-edge only. No texel is fetched. */
openagc_result openagc_frontend_sampler_create(openagc_frontend_device *frontend,
                                               const openagc_frontend_sampler_desc *desc,
                                               openagc_frontend_sampler **out_sampler);
openagc_result openagc_frontend_sampler_get_info(const openagc_frontend_sampler *sampler,
                                                 openagc_frontend_sampler_info *info);
openagc_result openagc_frontend_sampler_destroy(openagc_frontend_sampler *sampler);

/* One color target plus an optional depth attachment. No depth test runs. */
openagc_result openagc_frontend_render_pass_create(
    openagc_frontend_device *frontend, openagc_frontend_image *color,
    openagc_frontend_render_pass **out_pass);
/* NULL detaches. The image must already be DEPTH_TARGET and match the color size. */
openagc_result openagc_frontend_render_pass_attach_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_image *depth);
typedef uint32_t openagc_frontend_load_op;
enum {
    OPENAGC_FRONTEND_LOAD_OP_LOAD = 1u,
    OPENAGC_FRONTEND_LOAD_OP_CLEAR = 2u
};

openagc_result openagc_frontend_render_pass_begin(openagc_frontend_render_pass *pass);
/* LOAD leaves pixels. CLEAR writes the color, then the pass is begun. Depth is left untouched. */
openagc_result openagc_frontend_render_pass_begin_with_load(openagc_frontend_render_pass *pass,
                                                           openagc_frontend_load_op load_op,
                                                           openagc_color color);
/* Depth CLEAR packs D24S8. LOAD leaves those bytes. No depth test runs. */
openagc_result openagc_frontend_render_pass_begin_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil);
/* Marks the pass begun without load-op pixel writes. Vulkan records loads for submit. */
openagc_result openagc_frontend_render_pass_begin_validate_with_depth(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil);
/* Applies recorded load ops. Used on Vulkan queue submit; does not require begun. */
openagc_result openagc_frontend_render_pass_apply_loads(
    openagc_frontend_render_pass *pass, openagc_frontend_load_op color_op, openagc_color color,
    openagc_frontend_load_op depth_op, float depth, uint32_t stencil);
/* Scissor-bounded clears for deferred Vulkan submit (begun not required). */
openagc_result openagc_frontend_render_pass_clear_rect(openagc_frontend_render_pass *pass,
                                                       openagc_color color, uint32_t x, uint32_t y,
                                                       uint32_t width, uint32_t height);
openagc_result openagc_frontend_render_pass_clear_depth_rect(
    openagc_frontend_render_pass *pass, float depth, uint32_t stencil, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height);
openagc_result openagc_frontend_render_pass_set_viewport(openagc_frontend_render_pass *pass,
                                                         uint32_t x, uint32_t y,
                                                         uint32_t width, uint32_t height);
openagc_result openagc_frontend_render_pass_bind_pipeline(
    openagc_frontend_render_pass *pass, openagc_frontend_pipeline *pipeline);
openagc_result openagc_frontend_render_pass_bind_index(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_buffer *buffer,
                                                        uint64_t offset);
/* width is 2 or 4. The bound index stream is read with that element size. */
openagc_result openagc_frontend_render_pass_set_index_width(openagc_frontend_render_pass *pass,
                                                            uint32_t width);
openagc_result openagc_frontend_translate_index_width(openagc_frontend_kind kind, uint32_t native,
                                                      uint32_t *out_width);
openagc_result openagc_frontend_render_pass_bind_vertex(openagc_frontend_render_pass *pass,
                                                         openagc_frontend_buffer *buffer,
                                                         uint64_t offset);
openagc_result openagc_frontend_render_pass_set_scissor(openagc_frontend_render_pass *pass,
                                                        uint32_t x, uint32_t y,
                                                        uint32_t width, uint32_t height);
openagc_result openagc_frontend_render_pass_end(openagc_frontend_render_pass *pass);
/* Inside a begun pass. A scissor limits the write; without one the whole target is cleared. */
openagc_result openagc_frontend_render_pass_clear(openagc_frontend_render_pass *pass,
                                                  openagc_color color);
/* Same scissor rule as the color clear. depth is 0..1 and stencil is 0..255. */
openagc_result openagc_frontend_render_pass_clear_depth(openagc_frontend_render_pass *pass,
                                                       float depth, uint32_t stencil);
/* Snapshot the clear rectangle (scissor or full color target). Requires begun.
   require_depth nonzero also needs a depth attachment. */
openagc_result openagc_frontend_render_pass_clear_bounds(
    const openagc_frontend_render_pass *pass, uint32_t require_depth, uint32_t *out_x,
    uint32_t *out_y, uint32_t *out_width, uint32_t *out_height);

typedef uint32_t openagc_frontend_query_kind;
enum {
    OPENAGC_FRONTEND_QUERY_OCCLUSION = 1u,
    OPENAGC_FRONTEND_QUERY_TIMESTAMP = 2u
};

/* Records a query slot. Results stay unavailable: gpu_execution is 0. */
openagc_result openagc_frontend_query_pool_create(openagc_frontend_device *frontend,
                                                  openagc_frontend_query_kind kind, uint32_t count,
                                                  openagc_frontend_query_pool **out_pool);
openagc_result openagc_frontend_render_pass_begin_query(openagc_frontend_render_pass *pass,
                                                        openagc_frontend_query_pool *pool,
                                                        uint32_t index);
openagc_result openagc_frontend_render_pass_end_query(openagc_frontend_render_pass *pass);
openagc_result openagc_frontend_query_write_timestamp(openagc_frontend_query_pool *pool,
                                                     uint32_t index);
/* Writes 0 and returns NOT_READY. An open query is BAD_STATE. */
openagc_result openagc_frontend_query_get(const openagc_frontend_query_pool *pool, uint32_t index,
                                          uint32_t *available);
openagc_result openagc_frontend_query_pool_destroy(openagc_frontend_query_pool *pool);
/* Does not rasterize. Zero vertices or instances complete with no launch.
   A positive count whose plan is not executable is NOT_READY.
   When vertex_input_mask is zero (attribute-less / VertexID shaders), no
   vertex buffer or attributes are required. */
/* One 16-byte record: vertex count, instance count, first vertex, first instance. */
openagc_result openagc_frontend_render_pass_draw_indirect(
    const openagc_frontend_render_pass *pass, openagc_frontend_buffer *buffer, uint64_t offset);
openagc_result openagc_frontend_render_pass_draw(const openagc_frontend_render_pass *pass,
                                                 uint32_t vertex_count, uint32_t instance_count,
                                                 uint32_t first_vertex, uint32_t first_instance);
/* Requires a bound index buffer. Attribute-less plans skip vertex fetch. */
openagc_result openagc_frontend_render_pass_draw_indexed(
    const openagc_frontend_render_pass *pass, uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
openagc_result openagc_frontend_render_pass_destroy(openagc_frontend_render_pass *pass);

/* Host-linear first-fit heap shared by images and buffers. One backend
   allocation, not one per object. */
openagc_result openagc_frontend_timeline_create(
    openagc_frontend_device *frontend, openagc_frontend_timeline **out_timeline);
/* Publishes the next synchronous point. Copies have already completed, so
   this does not wait and does not invent an in-flight GPU submit. */
openagc_result openagc_frontend_timeline_signal(openagc_frontend_timeline *timeline);
openagc_result openagc_frontend_timeline_poll(const openagc_frontend_timeline *timeline,
                                              uint64_t value,
                                              openagc_frontend_timeline_info *info);
openagc_result openagc_frontend_timeline_destroy(openagc_frontend_timeline *timeline);

/* Both frontends build pipeline plans here. A plan that claims a compiler
   or GPU execution is refused. */
/* Host graphics plan. The color target must already be COLOR_TARGET/GRAPHICS.
   The plan is refused if it claims compiler verification or GPU execution. */
openagc_result openagc_frontend_graphics_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_pipeline **out_pipeline);
/* Fills set 0 at the given slots. Counts stay zero when both are absent.
   The plan copies the bindings. A slot at or above 8 is OUT_OF_RANGE. */
openagc_result openagc_frontend_apply_reflection(
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_shader_resource_binding *resource, openagc_shader_texture_binding *texture,
    openagc_shader_pipeline_desc *plan);
/* Same set-0 copy for every reflected slot. Bindings must increase.
   A count or slot at or above 8 is OUT_OF_RANGE. */
openagc_result openagc_frontend_apply_reflection_set(
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_shader_resource_binding *resources,
    openagc_shader_texture_binding *textures, openagc_shader_pipeline_desc *plan);
openagc_result openagc_frontend_graphics_pipeline_create_with_bindings(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    openagc_frontend_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_frontend_image *sampled, uint32_t texture_binding,
    openagc_frontend_pipeline **out_pipeline);
/* One entry per reflected slot, in increasing binding order. */
openagc_result openagc_frontend_graphics_pipeline_create_with_resources(
    openagc_frontend_device *frontend, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_frontend_image *color_target,
    const openagc_frontend_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_frontend_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_frontend_pipeline **out_pipeline);
openagc_result openagc_frontend_pipeline_create(
    openagc_frontend_device *frontend, const openagc_shader_pipeline_desc *desc,
    openagc_frontend_pipeline **out_pipeline);
openagc_result openagc_frontend_pipeline_get_layout(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout);
/* A pipeline with reflected slots accepts only its own layout. An empty plan accepts NULL. */
openagc_result openagc_frontend_layout_matches(const openagc_frontend_pipeline *pipeline,
                                               const openagc_frontend_pipeline_layout *layout);
openagc_result openagc_frontend_pipeline_layout_retain(openagc_frontend_pipeline_layout *layout);
openagc_result openagc_frontend_pipeline_layout_release(openagc_frontend_pipeline_layout *layout);
openagc_result openagc_frontend_pipeline_get_info(
    const openagc_frontend_pipeline *pipeline, openagc_frontend_pipeline_info *info);
/*
 * Host-only: parse pinned-shape PSBC metadata for the bound graphics
 * vertex+pixel stages and store SET_CONTEXT_REG/SET_SH_REG words on the
 * plan (including vertex linkage context pairs when present). Graphics
 * create auto-attaches this when both stages are psbc_envelope with
 * retained metadata. Does not set compiler_verified or gpu_executable
 * and never emits DRAW. machine_code_size in each metadata object must
 * match the stage artifact code size.
 */
openagc_result openagc_frontend_pipeline_set_psbc_register_snapshot(
    openagc_frontend_pipeline *pipeline, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size);
openagc_result openagc_frontend_pipeline_get_host_register_program(
    const openagc_frontend_pipeline *pipeline, uint32_t *words, uint32_t max_words,
    uint32_t *out_count);
/*
 * Patch SPI_SHADER_PGM_LO/HI in the stored snapshot from 256-byte-aligned
 * host code VAs (same >>8 / >>40 encoding as console-proven compute).
 * Re-encodes from retained stage metadata. Does not set gpu_executable.
 */
openagc_result openagc_frontend_pipeline_patch_psbc_pgm_vas(
    openagc_frontend_pipeline *pipeline, uint64_t vertex_code_va, uint64_t pixel_code_va);
/* Host-only: register program + shared EOP into the device write snapshot. No DRAW. */
openagc_result openagc_frontend_pipeline_record_psbc_register_eop(
    openagc_frontend_pipeline *pipeline);
openagc_result openagc_frontend_pipeline_get_psbc_code_vas(
    const openagc_frontend_pipeline *pipeline, uint64_t *vertex_code_va,
    uint64_t *pixel_code_va);
/*
 * Host-only: place vertex+pixel PSBC machine code on the shared heap
 * (256-byte blocks), patch SPI_SHADER_PGM_LO/HI from those VAs, and retain
 * the spans on the plan. Requires an attached register snapshot. Does not
 * set gpu_executable and never emits DRAW.
 */
openagc_result openagc_frontend_pipeline_bind_psbc_code(openagc_frontend_pipeline *pipeline);
openagc_result openagc_frontend_pipeline_destroy(openagc_frontend_pipeline *pipeline);
/* Graphics only. Stride must be a non-zero multiple of 4. Required before
   a draw that fetches vertices (vertex_input_mask != 0). */
/* Names the sampler a sampled image must use. NULL clears it. A pipeline
   with no texture refuses a sampler. Destroy of a named sampler is BUSY. */
openagc_result openagc_frontend_pipeline_set_sampler(openagc_frontend_pipeline *pipeline,
                                                    openagc_frontend_sampler *sampler);
openagc_result openagc_frontend_pipeline_set_vertex_stride(
    openagc_frontend_pipeline *pipeline, uint32_t stride);
/* One attribute. Its bytes must fit in the stride. Both are multiples of 4. */
openagc_result openagc_frontend_pipeline_set_vertex_input(
    openagc_frontend_pipeline *pipeline, uint32_t stride, uint32_t attribute_offset,
    uint32_t attribute_bytes);
/* Several attributes. Their union must cover every shader input location.
   A gap leaves the previous input unchanged. */
openagc_result openagc_frontend_pipeline_set_vertex_attributes(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *bytes, uint32_t count);
openagc_result openagc_frontend_pipeline_get_vertex_attribute(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset, uint32_t *bytes);
/* formats are shared vertex-format ids. A raw byte attribute stores format 0. */
/* rates are 0 per vertex and 1 per instance. count must match the stored attributes. */
openagc_result openagc_frontend_translate_primitive(openagc_frontend_kind kind, uint32_t native,
                                                    openagc_frontend_primitive *out_primitive);
openagc_result openagc_frontend_pipeline_set_primitive(openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive primitive);
openagc_result openagc_frontend_pipeline_get_primitive(const openagc_frontend_pipeline *pipeline,
                                                       openagc_frontend_primitive *out_primitive);
/* At most 128 bytes. offset and size are multiples of 4. */
openagc_result openagc_frontend_translate_blend_factor(openagc_frontend_kind kind, uint32_t native,
                                                       openagc_frontend_blend_factor *out_factor);
openagc_result openagc_frontend_pipeline_set_blend(openagc_frontend_pipeline *pipeline,
                                                   uint32_t enable, openagc_frontend_blend_factor src,
                                                   openagc_frontend_blend_factor dst);
openagc_result openagc_frontend_pipeline_get_blend(const openagc_frontend_pipeline *pipeline,
                                                   uint32_t *enable,
                                                   openagc_frontend_blend_factor *src,
                                                   openagc_frontend_blend_factor *dst);
openagc_result openagc_frontend_pipeline_write_push_constants(
    openagc_frontend_pipeline *pipeline, uint32_t offset, const void *bytes, uint32_t size);
openagc_result openagc_frontend_pipeline_read_push_constants(
    const openagc_frontend_pipeline *pipeline, uint32_t offset, void *bytes, uint32_t size);
openagc_result openagc_frontend_pipeline_set_vertex_rates(
    openagc_frontend_pipeline *pipeline, const uint32_t *rates, uint32_t count);
openagc_result openagc_frontend_pipeline_set_vertex_formats(
    openagc_frontend_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const openagc_frontend_vertex_format *formats, uint32_t count);
openagc_result openagc_frontend_pipeline_get_vertex_format(
    const openagc_frontend_pipeline *pipeline, uint32_t index, uint32_t *offset,
    openagc_frontend_vertex_format *format, uint32_t *bytes);
/* Does not run a workgroup unless the compute artifact is the console-proven
   store-const blob (host CPU write of 0xA5A5A5A5 into plan slot 0). Zero groups
   complete with no launch. Any other positive dispatch is NOT_READY. */
openagc_result openagc_frontend_dispatch(const openagc_frontend_pipeline *pipeline,
                                         uint32_t groups_x, uint32_t groups_y,
                                         uint32_t groups_z);
/* Same gates as dispatch without writing. Used to record a Vulkan dispatch
   command that runs on queue submit. */
openagc_result openagc_frontend_dispatch_validate(const openagc_frontend_pipeline *pipeline,
                                                  uint32_t groups_x, uint32_t groups_y,
                                                  uint32_t groups_z);
/* A resource group with no buffer, image, or sampler is not a descriptor set. */
openagc_result openagc_frontend_resources_present(uint32_t has_buffer, uint32_t has_image,
                                                  uint32_t has_sampler);
/* BAD_STATE when a reflected slot is missing from the recorded masks.
   Extra recorded slots are ignored. */
openagc_result openagc_frontend_recording_covers(const openagc_frontend_pipeline *pipeline,
                                                 uint32_t resource_mask, uint32_t texture_mask);
/* BAD_STATE unless each reflected slot holds the buffer or image the plan retained.
   A sampled image also needs the sampler named on the pipeline. A NULL list means every slot is empty.
   Extra slots are ignored. */
openagc_result openagc_frontend_recording_matches(
    const openagc_frontend_pipeline *pipeline, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler);
openagc_result openagc_frontend_render_pass_layout_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_pipeline_layout *layout);
openagc_result openagc_frontend_render_pass_matches(
    const openagc_frontend_render_pass *pass, const openagc_frontend_buffer *const *buffers,
    const uint64_t *offsets, const uint64_t *sizes, const openagc_frontend_image *const *images,
    const openagc_frontend_sampler *sampler);
openagc_result openagc_frontend_render_pass_covers(const openagc_frontend_render_pass *pass,
                                                   uint32_t resource_mask, uint32_t texture_mask);

#ifdef __cplusplus
}
#endif

#endif
