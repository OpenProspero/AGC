/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_VULKAN_H
#define OPENAGC_VULKAN_H

#include "openagc/frontend.h"
#include "openagc/shader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_VK_API_VERSION 1u
/* Inline payload vkCmdUpdateBuffer may record. */
#define OPENAGC_VK_MAX_UPDATE_BYTES 4096u

typedef struct openagc_vk_instance openagc_vk_instance;
typedef struct openagc_vk_device openagc_vk_device;
typedef struct openagc_vk_buffer openagc_vk_buffer;
typedef struct openagc_vk_memory openagc_vk_memory;
typedef struct openagc_vk_descriptor_set openagc_vk_descriptor_set;
typedef struct openagc_vk_sampler openagc_vk_sampler;
typedef struct openagc_vk_image openagc_vk_image;
typedef struct openagc_vk_image_view openagc_vk_image_view;
typedef struct openagc_vk_command_pool openagc_vk_command_pool;
typedef struct openagc_vk_command_buffer openagc_vk_command_buffer;
typedef struct openagc_vk_query_pool openagc_vk_query_pool;
typedef struct openagc_vk_fence openagc_vk_fence;
typedef struct openagc_vk_semaphore openagc_vk_semaphore;
typedef struct openagc_vk_pipeline openagc_vk_pipeline;
typedef struct openagc_vk_render_pass openagc_vk_render_pass;

typedef struct openagc_vk_instance_desc {
    uint32_t struct_size;
    uint32_t api_version;
} openagc_vk_instance_desc;

typedef struct openagc_vk_queue_family {
    uint32_t struct_size;
    uint32_t queue_flags;
    uint32_t queue_count;
} openagc_vk_queue_family;

typedef struct openagc_vk_device_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t queue_family_index;
} openagc_vk_device_desc;

typedef struct openagc_vk_command_pool_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t queue_family_index;
} openagc_vk_command_pool_desc;

typedef struct openagc_vk_buffer_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t usage;
    uint64_t size_bytes;
} openagc_vk_buffer_desc;

typedef struct openagc_vk_image_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t format;
    uint32_t usage;
    uint32_t initial_layout;
    uint32_t width;
    uint32_t height;
} openagc_vk_image_desc;

typedef struct openagc_vk_image_view_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t view_type;
    uint32_t format;
    uint32_t aspect_mask;
    uint32_t base_mip_level;
    uint32_t level_count;
    uint32_t base_array_layer;
    uint32_t layer_count;
} openagc_vk_image_view_desc;

typedef struct openagc_vk_image_view_info {
    uint32_t struct_size;
    uint32_t view_type;
    uint32_t format;
    uint32_t aspect_mask;
    uint32_t width;
    uint32_t height;
} openagc_vk_image_view_info;

typedef struct openagc_vk_format_properties {
    uint32_t struct_size;
    uint32_t format;
    openagc_graphics_format backend_format;
} openagc_vk_format_properties;

typedef struct openagc_vk_capabilities {
    uint32_t struct_size;
    uint32_t physical_device_count;
    uint32_t copy_queue_count;
    uint32_t graphics_queue_count;
    uint32_t compute_queue_count;
    uint32_t gpu_execution;
    uint32_t presentation;
} openagc_vk_capabilities;

#define OPENAGC_VK_IMAGE_VIEW_TYPE_2D 1u
#define OPENAGC_VK_IMAGE_ASPECT_COLOR_BIT 0x00000001u

#define OPENAGC_VK_QUEUE_TRANSFER_BIT 0x00000004u
#define OPENAGC_VK_QUEUE_GRAPHICS_BIT 0x00000002u
#define OPENAGC_VK_QUEUE_COMPUTE_BIT 0x00000008u

#define OPENAGC_VK_INSTANCE_DESC_INIT \
    { (uint32_t)sizeof(openagc_vk_instance_desc), OPENAGC_VK_API_VERSION }
#define OPENAGC_VK_QUEUE_FAMILY_INIT \
    { (uint32_t)sizeof(openagc_vk_queue_family), 0u, 0u }
#define OPENAGC_VK_DEVICE_DESC_INIT \
    { (uint32_t)sizeof(openagc_vk_device_desc), OPENAGC_VK_API_VERSION, 0u }
#define OPENAGC_VK_COMMAND_POOL_DESC_INIT \
    { (uint32_t)sizeof(openagc_vk_command_pool_desc), OPENAGC_VK_API_VERSION, 0u }
#define OPENAGC_VK_BUFFER_DESC_INIT(usage_value, size_value) \
    { (uint32_t)sizeof(openagc_vk_buffer_desc), OPENAGC_VK_API_VERSION, \
      (usage_value), (size_value) }
#define OPENAGC_VK_IMAGE_DESC_INIT(format_value, usage_value, layout_value, \
                                   width_value, height_value) \
    { (uint32_t)sizeof(openagc_vk_image_desc), OPENAGC_VK_API_VERSION, \
      (format_value), (usage_value), (layout_value), (width_value), (height_value) }
#define OPENAGC_VK_IMAGE_VIEW_DESC_INIT(format_value) \
    { (uint32_t)sizeof(openagc_vk_image_view_desc), OPENAGC_VK_API_VERSION, \
      OPENAGC_VK_IMAGE_VIEW_TYPE_2D, (format_value), OPENAGC_VK_IMAGE_ASPECT_COLOR_BIT, \
      0u, 1u, 0u, 1u }
#define OPENAGC_VK_IMAGE_VIEW_INFO_INIT \
    { (uint32_t)sizeof(openagc_vk_image_view_info), 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_VK_FORMAT_PROPERTIES_INIT \
    { (uint32_t)sizeof(openagc_vk_format_properties), 0u, 0u }
#define OPENAGC_VK_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_vk_capabilities), 0u, 0u, 0u, 0u, 0u, 0u }

/* One host physical device. Graphics and compute families are absent. */
openagc_result openagc_vk_instance_create(const openagc_vk_instance_desc *desc,
                                          openagc_vk_instance **out_instance);
openagc_result openagc_vk_instance_destroy(openagc_vk_instance *instance);
openagc_result openagc_vk_get_capabilities(const openagc_vk_instance *instance,
                                           openagc_vk_capabilities *capabilities);
/* Index walks the shared format table. There is no second Vulkan list. */
openagc_result openagc_vk_get_format_properties(const openagc_vk_instance *instance,
                                                uint32_t index,
                                                openagc_vk_format_properties *properties);
openagc_result openagc_vk_get_queue_family(const openagc_vk_instance *instance,
                                           uint32_t index,
                                           openagc_vk_queue_family *family);
openagc_result openagc_vk_device_create(openagc_vk_instance *instance,
                                        const openagc_vk_device_desc *desc,
                                        openagc_vk_device **out_device);
openagc_result openagc_vk_device_destroy(openagc_vk_device *device);
/* Host-only last WRITE_DATA / Step-U register+EOP snapshot (gpu_submitted=0). */
openagc_result openagc_vk_device_get_last_write(const openagc_vk_device *device,
                                                 openagc_gpu_submission_view *view);

openagc_result openagc_vk_allocate_memory(openagc_vk_device *device, uint64_t size_bytes,
                                          openagc_vk_memory **out_memory);
openagc_result openagc_vk_memory_write(openagc_vk_memory *memory, uint64_t offset,
                                       const void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_memory_read(const openagc_vk_memory *memory, uint64_t offset,
                                      void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_free_memory(openagc_vk_memory *memory);
openagc_result openagc_vk_create_buffer(openagc_vk_device *device,
                                        const openagc_vk_buffer_desc *desc,
                                        openagc_vk_buffer **out_buffer);
openagc_result openagc_vk_buffer_upload(openagc_vk_buffer *buffer, uint64_t offset,
                                        const void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_buffer_readback(openagc_vk_buffer *buffer, uint64_t offset,
                                          void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_buffer_get_info(const openagc_vk_buffer *buffer,
                                          openagc_frontend_buffer_info *info);
openagc_result openagc_vk_destroy_buffer(openagc_vk_buffer *buffer);
openagc_result openagc_vk_create_unbound_buffer(openagc_vk_device *device,
                                                const openagc_vk_buffer_desc *desc,
                                                openagc_vk_buffer **out_buffer);
openagc_result openagc_vk_bind_buffer_memory(openagc_vk_buffer *buffer,
                                             openagc_vk_memory *memory, uint64_t offset);
openagc_result openagc_vk_create_unbound_image(openagc_vk_device *device,
                                               const openagc_vk_image_desc *desc,
                                               openagc_vk_image **out_image);
openagc_result openagc_vk_bind_image_memory(openagc_vk_image *image,
                                            openagc_vk_memory *memory, uint64_t offset);

/* Up to eight set-0 slots. Binding does not execute a shader. */
openagc_result openagc_vk_create_descriptor_set(openagc_vk_device *device,
                                                openagc_vk_descriptor_set **out_set);
openagc_result openagc_vk_create_descriptor_set_for_pipeline(
    openagc_vk_device *device, openagc_vk_pipeline *pipeline, openagc_vk_descriptor_set **out_set);
openagc_result openagc_vk_update_descriptor_set(openagc_vk_descriptor_set *set,
                                                openagc_vk_buffer *buffer);
openagc_result openagc_vk_update_descriptor_set_at(openagc_vk_descriptor_set *set,
                                                   openagc_vk_buffer *buffer, uint32_t binding);
/* The range must match the plan. A zero size is OUT_OF_RANGE. */
openagc_result openagc_vk_update_descriptor_buffer_range(openagc_vk_descriptor_set *set,
                                                         openagc_vk_buffer *buffer,
                                                         uint32_t binding, uint64_t offset,
                                                         uint64_t size_bytes);
/* The image must already be SHADER_READ/GRAPHICS. No shader runs. */
openagc_result openagc_vk_update_descriptor_image(openagc_vk_descriptor_set *set,
                                                  openagc_vk_image_view *view);
openagc_result openagc_vk_update_descriptor_image_at(openagc_vk_descriptor_set *set,
                                                     openagc_vk_image_view *view,
                                                     uint32_t binding);
openagc_result openagc_vk_create_sampler(openagc_vk_device *device, uint32_t mag_filter,
                                         uint32_t min_filter, uint32_t address_mode,
                                         openagc_vk_sampler **out_sampler);
openagc_result openagc_vk_update_descriptor_sampler(openagc_vk_descriptor_set *set,
                                                    openagc_vk_sampler *sampler);
openagc_result openagc_vk_destroy_sampler(openagc_vk_sampler *sampler);
openagc_result openagc_vk_destroy_descriptor_set(openagc_vk_descriptor_set *set);

openagc_result openagc_vk_create_image(openagc_vk_device *device,
                                       const openagc_vk_image_desc *desc,
                                       openagc_vk_image **out_image);
openagc_result openagc_vk_image_get_info(const openagc_vk_image *image,
                                         openagc_frontend_image_info *info);
openagc_result openagc_vk_image_readback(openagc_vk_image *image, uint64_t offset,
                                         void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_destroy_image(openagc_vk_image *image);

/* Identity 2D color view. One mip and one layer. No format reinterpretation. */
openagc_result openagc_vk_create_image_view(openagc_vk_device *device,
                                            openagc_vk_image *image,
                                            const openagc_vk_image_view_desc *desc,
                                            openagc_vk_image_view **out_view);
openagc_result openagc_vk_image_view_get_info(const openagc_vk_image_view *view,
                                              openagc_vk_image_view_info *info);
openagc_result openagc_vk_destroy_image_view(openagc_vk_image_view *view);

/* Only the transfer family can own a pool. Reset drops recorded commands. */
openagc_result openagc_vk_create_command_pool(openagc_vk_device *device,
                                              const openagc_vk_command_pool_desc *desc,
                                              openagc_vk_command_pool **out_pool);
openagc_result openagc_vk_command_pool_reset(openagc_vk_command_pool *pool);
openagc_result openagc_vk_destroy_command_pool(openagc_vk_command_pool *pool);

/* Recording only. Execution happens in queue submit, still on the CPU. */
openagc_result openagc_vk_allocate_command_buffer(openagc_vk_command_pool *pool,
                                                  openagc_vk_command_buffer **out_command_buffer);
openagc_result openagc_vk_command_buffer_begin(openagc_vk_command_buffer *command_buffer);
openagc_result openagc_vk_cmd_copy_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *source, uint64_t source_offset,
                                          openagc_vk_buffer *destination,
                                          uint64_t destination_offset, uint64_t size_bytes);
openagc_result openagc_vk_cmd_image_barrier(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_image *image, uint32_t layout);
openagc_result openagc_vk_cmd_clear_color(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_image *image, openagc_color color);
openagc_result openagc_vk_cmd_clear_attachments(openagc_vk_command_buffer *command_buffer,
                                               openagc_color color);
/* depth is 0..1. The write stays inside the scissor and does not test fragments. */
openagc_result openagc_vk_cmd_clear_depth(openagc_vk_command_buffer *command_buffer, float depth,
                                         uint32_t stencil);
openagc_result openagc_vk_cmd_copy_buffer_to_image(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_buffer *source,
                                                   uint64_t source_offset,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset, uint64_t size_bytes);
openagc_result openagc_vk_cmd_copy_image_to_buffer(openagc_vk_command_buffer *command_buffer,
                                                   openagc_vk_image *image,
                                                   uint64_t image_offset,
                                                   openagc_vk_buffer *destination,
                                                   uint64_t destination_offset,
                                                   uint64_t size_bytes);
/* Both images must share one format and one device. A self copy is refused. */
openagc_result openagc_vk_cmd_copy_image(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_image *source, uint32_t source_x,
                                         uint32_t source_y, openagc_vk_image *destination,
                                         uint32_t destination_x, uint32_t destination_y,
                                         uint32_t width, uint32_t height);
/* Repeating 32-bit value over a transfer-destination range. Offset and size are
   multiples of 4 and fit in the frontend staging allocation. */
openagc_result openagc_vk_cmd_fill_buffer(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset,
                                          uint64_t size_bytes, uint32_t value);
/* The bytes are copied into the recording, so the caller's memory may go away.
   At most OPENAGC_VK_MAX_UPDATE_BYTES, four-byte aligned, transfer-destination. */
openagc_result openagc_vk_cmd_update_buffer(openagc_vk_command_buffer *command_buffer,
                                            openagc_vk_buffer *buffer, uint64_t offset,
                                            const void *bytes, uint64_t size_bytes);
openagc_result openagc_vk_command_buffer_end(openagc_vk_command_buffer *command_buffer);
openagc_result openagc_vk_destroy_command_buffer(openagc_vk_command_buffer *command_buffer);
openagc_result openagc_vk_queue_submit_commands(openagc_vk_device *device,
                                                openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_fence *fence);

openagc_result openagc_vk_create_fence(openagc_vk_device *device,
                                       openagc_vk_fence **out_fence);
/* Publishes the synchronous timeline point. Does not block. */
openagc_result openagc_vk_queue_submit(openagc_vk_device *device,
                                       openagc_vk_fence *fence);
openagc_result openagc_vk_fence_poll(const openagc_vk_fence *fence, uint64_t value,
                                     openagc_frontend_timeline_info *info);
openagc_result openagc_vk_destroy_fence(openagc_vk_fence *fence);

/* Same timeline as a fence. A wait never blocks: a future value is NOT_READY. */
openagc_result openagc_vk_create_semaphore(openagc_vk_device *device,
                                           openagc_vk_semaphore **out_semaphore);
openagc_result openagc_vk_semaphore_poll(const openagc_vk_semaphore *semaphore, uint64_t value,
                                         openagc_frontend_timeline_info *info);
openagc_result openagc_vk_destroy_semaphore(openagc_vk_semaphore *semaphore);
openagc_result openagc_vk_queue_submit_wait(openagc_vk_device *device,
                                            openagc_vk_command_buffer *command_buffer,
                                            const openagc_vk_semaphore *wait,
                                            uint64_t wait_value,
                                            openagc_vk_semaphore *signal);

/* Host plan only. A compiler-verified or GPU-executable plan is refused. */
openagc_result openagc_vk_create_graphics_pipeline(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_pipeline **out_pipeline);
openagc_result openagc_vk_create_compute_pipeline(openagc_vk_device *device,
                                                  const openagc_shader_artifact_desc *desc,
                                                  openagc_vk_pipeline **out_pipeline);
/* Same plan as create_compute_pipeline, plus the reflected set-0 slots.
   A declared binding with no buffer is INVALID_ARGUMENT. */
openagc_result openagc_vk_create_compute_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline);
/* One entry per reflected slot, in increasing binding order. */
openagc_result openagc_vk_create_compute_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *desc,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets,
    const uint64_t *sizes, const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline);
openagc_result openagc_vk_create_graphics_pipeline_with_bindings(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    openagc_vk_buffer *uniform, uint64_t offset, uint64_t size_bytes,
    uint32_t resource_binding, openagc_vk_image *sampled, uint32_t texture_binding,
    openagc_vk_pipeline **out_pipeline);
/* One entry per reflected slot, in increasing binding order. */
openagc_result openagc_vk_create_graphics_pipeline_with_resources(
    openagc_vk_device *device, const openagc_shader_artifact_desc *vertex,
    const openagc_shader_artifact_desc *pixel, openagc_vk_image *color_target,
    const openagc_vk_buffer *const *uniforms, const uint64_t *offsets, const uint64_t *sizes,
    const uint32_t *resource_bindings, uint32_t resource_count,
    const openagc_vk_image *const *sampled, const uint32_t *texture_bindings,
    uint32_t texture_count, openagc_vk_pipeline **out_pipeline);
openagc_result openagc_vk_pipeline_set_sampler(openagc_vk_pipeline *pipeline,
                                             openagc_vk_sampler *sampler);
openagc_result openagc_vk_pipeline_set_vertex_stride(openagc_vk_pipeline *pipeline,
                                                    uint32_t stride);
openagc_result openagc_vk_pipeline_set_vertex_input(openagc_vk_pipeline *pipeline, uint32_t stride,
                                                    uint32_t attribute_offset,
                                                    uint32_t attribute_bytes);
openagc_result openagc_vk_pipeline_get_vertex_attribute(const openagc_vk_pipeline *pipeline,
                                                      uint32_t index, uint32_t *offset,
                                                      uint32_t *bytes);
openagc_result openagc_vk_pipeline_set_vertex_attributes(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets, const uint32_t *bytes,
    uint32_t count);
/* formats are VkFormat values. Only the four float vector formats are accepted. */
openagc_result openagc_vk_pipeline_set_topology(openagc_vk_pipeline *pipeline, uint32_t topology);
openagc_result openagc_vk_pipeline_set_blend(openagc_vk_pipeline *pipeline, uint32_t enable,
                                             uint32_t src_factor, uint32_t dst_factor);
openagc_result openagc_vk_pipeline_get_blend(const openagc_vk_pipeline *pipeline, uint32_t *enable,
                                             openagc_frontend_blend_factor *src,
                                             openagc_frontend_blend_factor *dst);
openagc_result openagc_vk_cmd_push_constants(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline, uint32_t offset,
                                             const void *bytes, uint32_t size);
openagc_result openagc_vk_pipeline_read_push_constants(const openagc_vk_pipeline *pipeline,
                                                       uint32_t offset, void *bytes,
                                                       uint32_t size);
openagc_result openagc_vk_pipeline_get_primitive(const openagc_vk_pipeline *pipeline,
                                                 openagc_frontend_primitive *out_primitive);
openagc_result openagc_vk_pipeline_set_vertex_rates(openagc_vk_pipeline *pipeline,
                                                 const uint32_t *rates, uint32_t count);
openagc_result openagc_vk_pipeline_set_vertex_formats(
    openagc_vk_pipeline *pipeline, uint32_t stride, const uint32_t *offsets,
    const uint32_t *formats, uint32_t count);
openagc_result openagc_vk_pipeline_get_vertex_format(const openagc_vk_pipeline *pipeline,
                                                    uint32_t index, uint32_t *offset,
                                                    openagc_frontend_vertex_format *format,
                                                    uint32_t *bytes);
openagc_result openagc_vk_pipeline_get_layout(
    const openagc_vk_pipeline *pipeline, openagc_frontend_pipeline_layout **out_layout);
openagc_result openagc_vk_pipeline_get_info(const openagc_vk_pipeline *pipeline,
                                            openagc_frontend_pipeline_info *info);
/* Host-only PSBC register-program snapshot; shared frontend path. */
openagc_result openagc_vk_pipeline_set_psbc_register_snapshot(
    openagc_vk_pipeline *pipeline, const uint8_t *vertex_metadata,
    uint32_t vertex_metadata_size, const uint8_t *pixel_metadata,
    uint32_t pixel_metadata_size);
openagc_result openagc_vk_pipeline_set_agc_linked_registers(
    openagc_vk_pipeline *pipeline,
    const openagc_frontend_agc_register *context_records, uint32_t context_count,
    const openagc_frontend_agc_register *uconfig_records, uint32_t uconfig_count);
openagc_result openagc_vk_pipeline_set_agc_target_registers(
    openagc_vk_pipeline *pipeline,
    const openagc_frontend_agc_register *target_records, uint32_t target_count);
openagc_result openagc_vk_pipeline_get_host_register_program(
    const openagc_vk_pipeline *pipeline, uint32_t *words, uint32_t max_words,
    uint32_t *out_count);
openagc_result openagc_vk_pipeline_patch_psbc_pgm_vas(openagc_vk_pipeline *pipeline,
                                                      uint64_t vertex_code_va,
                                                      uint64_t pixel_code_va);
openagc_result openagc_vk_pipeline_record_psbc_register_eop(openagc_vk_pipeline *pipeline);
openagc_result openagc_vk_pipeline_get_psbc_code_vas(const openagc_vk_pipeline *pipeline,
                                                     uint64_t *vertex_code_va,
                                                     uint64_t *pixel_code_va);
openagc_result openagc_vk_pipeline_bind_psbc_code(openagc_vk_pipeline *pipeline);
openagc_result openagc_vk_destroy_pipeline(openagc_vk_pipeline *pipeline);

/* Color only. The image must already be a color target. Begin does not draw. */
openagc_result openagc_vk_create_render_pass(openagc_vk_device *device,
                                             openagc_vk_image *color,
                                             openagc_vk_render_pass **out_pass);
/* NULL detaches. The image must already be a depth target of the same size. */
openagc_result openagc_vk_render_pass_attach_depth(openagc_vk_render_pass *pass,
                                                   openagc_vk_image *depth);
openagc_result openagc_vk_destroy_render_pass(openagc_vk_render_pass *pass);
openagc_result openagc_vk_cmd_begin_render_pass(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_render_pass *pass);
openagc_result openagc_vk_cmd_begin_render_pass_with_load(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op load_op, openagc_color color);
openagc_result openagc_vk_cmd_begin_render_pass_with_depth(
    openagc_vk_command_buffer *command_buffer, openagc_vk_render_pass *pass,
    openagc_frontend_load_op color_op, openagc_color color, openagc_frontend_load_op depth_op,
    float depth, uint32_t stencil);
openagc_result openagc_vk_cmd_set_viewport(openagc_vk_command_buffer *command_buffer,
                                           uint32_t x, uint32_t y, uint32_t width,
                                           uint32_t height);
openagc_result openagc_vk_cmd_bind_pipeline(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_pipeline *pipeline);
/* index_type is VK_INDEX_TYPE_UINT16 or UINT32. The offset must match that size. */
openagc_result openagc_vk_cmd_bind_index_buffer(openagc_vk_command_buffer *command_buffer,
                                                openagc_vk_buffer *buffer, uint64_t offset,
                                                uint32_t index_type);
openagc_result openagc_vk_cmd_bind_vertex_buffer(openagc_vk_command_buffer *command_buffer,
                                                 openagc_vk_buffer *buffer, uint64_t offset);
openagc_result openagc_vk_cmd_set_scissor(openagc_vk_command_buffer *command_buffer,
                                          uint32_t x, uint32_t y, uint32_t width,
                                          uint32_t height);
openagc_result openagc_vk_cmd_end_render_pass(openagc_vk_command_buffer *command_buffer);
openagc_result openagc_vk_create_query_pool(openagc_vk_device *device,
                                            openagc_frontend_query_kind kind, uint32_t count,
                                            openagc_vk_query_pool **out_pool);
openagc_result openagc_vk_cmd_begin_query(openagc_vk_command_buffer *command_buffer,
                                         openagc_vk_query_pool *pool, uint32_t index);
openagc_result openagc_vk_cmd_end_query(openagc_vk_command_buffer *command_buffer);
openagc_result openagc_vk_cmd_write_timestamp(openagc_vk_command_buffer *command_buffer,
                                             openagc_vk_query_pool *pool, uint32_t index);
openagc_result openagc_vk_get_query(const openagc_vk_query_pool *pool, uint32_t index,
                                   uint32_t *available);
openagc_result openagc_vk_destroy_query_pool(openagc_vk_query_pool *pool);
/* No draw, dispatch, or swapchain exists on this backend. */
openagc_result openagc_vk_cmd_draw_indirect(openagc_vk_command_buffer *command_buffer,
                                          openagc_vk_buffer *buffer, uint64_t offset);
openagc_result openagc_vk_cmd_draw(openagc_vk_command_buffer *command_buffer,
                                    uint32_t vertex_count, uint32_t instance_count,
                                    uint32_t first_vertex, uint32_t first_instance);
openagc_result openagc_vk_cmd_draw_indexed(openagc_vk_command_buffer *command_buffer,
                                           uint32_t index_count, uint32_t instance_count,
                                           uint32_t first_index, int32_t vertex_offset,
                                           uint32_t first_instance);
openagc_result openagc_vk_cmd_bind_descriptor_set(openagc_vk_command_buffer *command_buffer,
                                               openagc_vk_descriptor_set *set);
openagc_result openagc_vk_cmd_bind_compute_pipeline(openagc_vk_command_buffer *command_buffer,
                                                     openagc_vk_pipeline *pipeline);
openagc_result openagc_vk_cmd_dispatch(openagc_vk_command_buffer *command_buffer,
                                       uint32_t groups_x, uint32_t groups_y,
                                       uint32_t groups_z);
openagc_result openagc_vk_create_swapchain(openagc_vk_device *device);

#ifdef __cplusplus
}
#endif

#endif
