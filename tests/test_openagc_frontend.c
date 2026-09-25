/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/frontend.h"
#include "openagc/shader.h"
#include "openagc/store_const_code.h"
#include "openagc/store_span_code.h"
#include "openagc/pm4_compute_fw940.h"
#include "openagc/pm4_fw940.h"
#include "openagc/pm4_write_fw940.h"
#include "openagc_sha256.h"

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

static int test_translation_tables(void)
{
    openagc_graphics_format format = 0u;
    openagc_graphics_usage usage = 0u;
    openagc_graphics_image_state state = OPENAGC_GRAPHICS_STATE_PRESENT;
    openagc_graphics_owner owner = OPENAGC_GRAPHICS_OWNER_COPY;
    openagc_gpu_buffer_usage buffer_usage = 0u;
    uint32_t native = 0u;

    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 0u, &native),
           OPENAGC_OK);
    CHECK(native == OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, native, &format),
           OPENAGC_OK);
    CHECK(format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 1u, &native),
           OPENAGC_OK);
    CHECK(native == OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, native, &format),
           OPENAGC_OK);
    CHECK(format == OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 2u, &native),
           OPENAGC_OK);
    CHECK(native == OPENAGC_FRONTEND_VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 3u, &native),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_OPENGL, 0u, &native),
           OPENAGC_OK);
    CHECK(native == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_OPENGL, native, &format),
           OPENAGC_OK);
    CHECK(format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_OPENGL, 1u, &native),
           OPENAGC_OK);
    CHECK(native == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_OPENGL, 2u, &native),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_native_format_at(99u, 0u, &native),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_native_format_at(OPENAGC_FRONTEND_VULKAN, 0u, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    /* Undefined, sRGB and R8 stay outside the host-linear formats. D24S8 is accepted. */
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 0u, &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 43u, &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 9u, &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 129u, &format),
           OPENAGC_OK);
    CHECK(format == OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 130u, &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_DEPTH24_STENCIL8,
               &format),
           OPENAGC_OK);
    CHECK(format == OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_OPENGL, 0x8051u, &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_OPENGL,
                                             OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
                                             &format),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_format(99u, 37u, &format),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_translate_format(OPENAGC_FRONTEND_VULKAN, 37u, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    EXPECT(openagc_frontend_translate_image_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                   OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT |
                   OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   OPENAGC_FRONTEND_VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
               &usage),
           OPENAGC_OK);
    CHECK(usage == (OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT |
                    OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT));
    EXPECT(openagc_frontend_translate_image_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &usage),
           OPENAGC_OK);
    CHECK(usage == OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT);
    EXPECT(openagc_frontend_translate_image_usage(
               OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
               &usage),
           OPENAGC_OK);
    CHECK(usage == OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT);
    EXPECT(openagc_frontend_translate_image_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &usage),
           OPENAGC_OK);
    CHECK(usage == OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT);
    /* Transfer-only, empty, storage and unknown usage have no backend meaning. */
    EXPECT(openagc_frontend_translate_image_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                   OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT,
               &usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_VULKAN, 0u, &usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_VULKAN, 0x00000008u,
                                                  &usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_VULKAN, 0x00001010u,
                                                  &usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_OPENGL,
                                                  OPENAGC_FRONTEND_GL_TEXTURE_2D, &usage),
           OPENAGC_OK);
    CHECK(usage == (OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT |
                    OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT));
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_OPENGL,
                                                  OPENAGC_FRONTEND_GL_RENDERBUFFER, &usage),
           OPENAGC_OK);
    CHECK(usage == OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_OPENGL, 0x8CD5u,
                                                  &usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_usage(99u, 4u, &usage),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_translate_image_usage(OPENAGC_FRONTEND_VULKAN, 4u, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED,
               &state, &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &state,
               &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &state,
               &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_DEPTH_TARGET &&
          owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &state, &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
          owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &state, &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE &&
          owner == OPENAGC_GRAPHICS_OWNER_COPY);
    EXPECT(openagc_frontend_translate_image_layout(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &state, &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION &&
          owner == OPENAGC_GRAPHICS_OWNER_COPY);
    /* GENERAL, depth read-only, preinitialized, present and GL layouts. */
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 1u, &state,
                                                   &owner),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 4u, &state,
                                                   &owner),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 8u, &state,
                                                   &owner),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 1000001002u,
                                                   &state, &owner),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_OPENGL, 0u, &state,
                                                   &owner),
           OPENAGC_OK);
    CHECK(state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_OPENGL, 2u, &state,
                                                   &owner),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_image_layout(99u, 0u, &state, &owner),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 0u, NULL,
                                                   &owner),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_translate_image_layout(OPENAGC_FRONTEND_VULKAN, 0u, &state,
                                                   NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT, &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN,
               OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                   OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == (OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                           OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT |
                           OPENAGC_GPU_BUFFER_SHADER_READ_BIT));
    /* Storage buffers have no backend meaning. */
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_INDIRECT_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(OPENAGC_FRONTEND_VULKAN, 0x0020u,
                                                   &buffer_usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_INDEX_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_VERTEX_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(OPENAGC_FRONTEND_VULKAN, 0u,
                                                   &buffer_usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == (OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                           OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_PIXEL_UNPACK_BUFFER,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == (OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                           OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_UNIFORM_BUFFER,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_ARRAY_BUFFER, &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == (OPENAGC_GPU_BUFFER_VERTEX_BIT |
                           OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));
    EXPECT(openagc_frontend_translate_buffer_usage(
               OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_ELEMENT_ARRAY_BUFFER,
               &buffer_usage),
           OPENAGC_OK);
    CHECK(buffer_usage == (OPENAGC_GPU_BUFFER_INDEX_BIT |
                           OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));
    EXPECT(openagc_frontend_translate_buffer_usage(OPENAGC_FRONTEND_OPENGL, 0x8894u,
                                                   &buffer_usage),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_translate_buffer_usage(99u, 1u, &buffer_usage),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_translate_buffer_usage(OPENAGC_FRONTEND_VULKAN, 1u, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    return 0;
}

static int test_device_capabilities_and_lifetime(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_capabilities caps = OPENAGC_FRONTEND_CAPABILITIES_INIT;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_device *short_lived = NULL;

    CHECK(make_device(1048576u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(NULL, &desc, &frontend),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_device_create(device, NULL, &frontend),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_device_create(device, &desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.struct_size--;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.struct_size++;
    desc.api_version++;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.api_version = OPENAGC_FRONTEND_API_VERSION;
    desc.staging_bytes = 0u;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.staging_bytes = 2048u;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.staging_bytes = 4097u;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.staging_bytes = OPENAGC_FRONTEND_MAX_STAGING_BYTES + 4u;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.staging_bytes = 4096u;
    EXPECT(openagc_frontend_device_create(device, &desc, &short_lived), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(short_lived), OPENAGC_OK);
    CHECK(frontend == NULL);

    desc.staging_bytes = OPENAGC_FRONTEND_DEFAULT_STAGING_BYTES;
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend), OPENAGC_OK);
    caps.struct_size--;
    EXPECT(openagc_frontend_device_get_capabilities(frontend, &caps),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    caps.struct_size++;
    EXPECT(openagc_frontend_device_get_capabilities(frontend, &caps), OPENAGC_OK);
    CHECK(caps.supported_kind_mask == 3u && caps.host_translation == 1u);
    CHECK(caps.gpu_execution == 0u && caps.rasterization == 0u);
    CHECK(caps.presentation == 0u);
    CHECK(caps.supported_format_mask == 7u && caps.supported_usage_mask == 11u);
    CHECK(caps.max_width == 4096u && caps.max_height == 4096u);
    CHECK(caps.max_image_bytes == 16777216u);
    CHECK(caps.staging_bytes == OPENAGC_FRONTEND_DEFAULT_STAGING_BYTES);
    EXPECT(openagc_frontend_device_get_capabilities(NULL, &caps),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_device_get_capabilities(frontend, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_device_destroy(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);

    /* The frontend holds its backend device and cannot outlive it. */
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_image_create_and_refusals(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_image_desc desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 8u, 4u, 0u);
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_image *image = NULL;
    openagc_frontend_image *candidate = NULL;
    openagc_frontend_image *gl_image = NULL;
    openagc_frontend_image *bgra_image = NULL;
    openagc_frontend_device *frontend = NULL;

    CHECK(make_device(1048576u, &context, &device) == 0);
    CHECK(frontend_desc.staging_bytes == OPENAGC_FRONTEND_DEFAULT_STAGING_BYTES);
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);

    EXPECT(openagc_frontend_image_create(NULL, &desc, &image),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_create(frontend, NULL, &image),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_create(frontend, &desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.struct_size--;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &image),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.struct_size++;
    desc.api_version++;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &image),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.api_version = OPENAGC_FRONTEND_API_VERSION;
    desc.native_format = OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.native_format = OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM;
    desc.native_layout = OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.native_layout = OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED;
    desc.width = 0u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 4097u;
    desc.row_pitch_bytes = 16388u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 8u;
    desc.row_pitch_bytes = 16u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.row_pitch_bytes = 33u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.row_pitch_bytes = 0u;
    desc.height = 0u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 4096u;
    desc.height = 4096u;
    desc.row_pitch_bytes = 16384u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.width = 8u;
    desc.height = 4u;
    desc.row_pitch_bytes = 0u;
    CHECK(candidate == NULL);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_frontend_image_create(frontend, &desc, &image), OPENAGC_OK);
    info.struct_size--;
    EXPECT(openagc_frontend_image_get_info(image, &info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    info.struct_size++;
    EXPECT(openagc_frontend_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_FRONTEND_VULKAN);
    CHECK(info.native_format == OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    CHECK(info.native_usage == OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT);
    CHECK(info.format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    CHECK(info.usage == OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    CHECK(info.width == 8u && info.height == 4u && info.row_pitch_bytes == 32u);
    CHECK(info.footprint_bytes == 128u);
    EXPECT(openagc_frontend_image_get_info(NULL, &info),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_get_info(image, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_destroy(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);

    /* A sampled-only image can never enter the color-target state. */
    EXPECT(openagc_frontend_image_transition(image, OPENAGC_GRAPHICS_STATE_COLOR_TARGET,
                                             OPENAGC_GRAPHICS_OWNER_GRAPHICS),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_image_transition(image, OPENAGC_GRAPHICS_STATE_SHADER_READ,
                                             OPENAGC_GRAPHICS_OWNER_GRAPHICS),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_transition(image, OPENAGC_GRAPHICS_STATE_SHADER_READ,
                                             OPENAGC_GRAPHICS_OWNER_GRAPHICS),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_transition(image, OPENAGC_GRAPHICS_STATE_HOST_READ,
                                             OPENAGC_GRAPHICS_OWNER_GRAPHICS),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);

    /* The initial layout may be a color target when the usage declares it. */
    desc.native_usage = OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    desc.native_layout = OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &bgra_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(bgra_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    CHECK(info.usage == OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT);
    EXPECT(openagc_frontend_image_destroy(bgra_image), OPENAGC_OK);
    bgra_image = NULL;

    desc.kind = OPENAGC_FRONTEND_OPENGL;
    desc.native_format = OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8;
    desc.native_usage = OPENAGC_FRONTEND_GL_TEXTURE_2D;
    desc.native_layout = 2u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.native_layout = 0u;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &gl_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(gl_image, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_FRONTEND_OPENGL);
    CHECK(info.native_format == OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8);
    CHECK(info.usage == (OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT |
                         OPENAGC_GRAPHICS_USAGE_COLOR_TARGET_BIT));
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    CHECK(candidate == NULL);

    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(gl_image), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_upload_readback_roundtrip(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_image_desc desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 8u, 4u, 0u);
    openagc_frontend_image_desc big_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 64u, 64u, 0u);
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_image *image = NULL;
    openagc_frontend_image *big = NULL;
    openagc_frontend_image *copy_image = NULL;
    uint8_t pattern[128];
    uint8_t patch[16];
    uint8_t readback[128];
    uint32_t i;

    CHECK(make_device(1048576u, &context, &device) == 0);
    frontend_desc.staging_bytes = 4096u;
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &desc, &image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_frontend_image_create(frontend, &big_desc, &big), OPENAGC_OK);

    for (i = 0u; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)(0x10u + i);
    }
    memset(patch, 0xa5, sizeof(patch));
    EXPECT(openagc_frontend_image_upload(NULL, 0u, pattern, sizeof(pattern)),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_upload(image, 0u, NULL, sizeof(pattern)),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_upload(image, 0u, pattern, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_upload(image, 1u, pattern, 16u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_upload(image, 0u, pattern, 6u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_upload(image, 112u, pattern, 32u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_readback(image, 128u, readback, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_readback(NULL, 0u, readback, 4u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_image_readback(image, 0u, NULL, 4u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    /* The staging window, not the image, bounds one transfer. */
    EXPECT(openagc_frontend_image_upload(big, 0u, pattern, 8192u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_destroy(big), OPENAGC_OK);
    big = NULL;

    EXPECT(openagc_frontend_image_upload(image, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    memset(readback, 0, sizeof(readback));
    EXPECT(openagc_frontend_image_readback(image, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_frontend_image_get_info(image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);

    EXPECT(openagc_frontend_image_upload(image, 16u, patch, sizeof(patch)), OPENAGC_OK);
    EXPECT(openagc_frontend_image_readback(image, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, 16u) == 0);
    CHECK(memcmp(&readback[16], patch, sizeof(patch)) == 0);
    CHECK(memcmp(&readback[32], &pattern[32], 96u) == 0);

    /* A transfer-destination image keeps its copy ownership across I/O. */
    desc.native_usage = OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                        OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT;
    desc.native_layout = OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    EXPECT(openagc_frontend_image_create(frontend, &desc, &copy_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_upload(copy_image, 0u, pattern, sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(copy_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION &&
          info.owner == OPENAGC_GRAPHICS_OWNER_COPY);
    EXPECT(openagc_frontend_image_readback(copy_image, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_frontend_image_get_info(copy_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION &&
          info.owner == OPENAGC_GRAPHICS_OWNER_COPY);
    EXPECT(openagc_frontend_image_transition(copy_image,
                                             OPENAGC_GRAPHICS_STATE_SHADER_READ,
                                             OPENAGC_GRAPHICS_OWNER_GRAPHICS),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_upload(copy_image, 0u, patch, sizeof(patch)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(copy_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_SHADER_READ);

    EXPECT(openagc_frontend_image_destroy(copy_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_buffer_objects(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_buffer_desc desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        256u);
    openagc_frontend_buffer_desc gl_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_PIXEL_UNPACK_BUFFER, 128u);
    openagc_frontend_buffer_desc gl_pack_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER, 128u);
    openagc_frontend_image_desc image_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 8u, 4u, 0u);
    openagc_frontend_buffer_info info = OPENAGC_FRONTEND_BUFFER_INFO_INIT;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_buffer *buffer = NULL;
    openagc_frontend_buffer *gl_buffer = NULL;
    openagc_frontend_buffer *gl_pack = NULL;
    openagc_frontend_buffer *candidate = NULL;
    openagc_frontend_image *image = NULL;
    uint8_t pattern[256];
    uint8_t small[64];
    uint8_t readback[256];
    uint32_t i;

    CHECK(make_device(1048576u, &context, &device) == 0);
    frontend_desc.staging_bytes = 4096u;
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);

    EXPECT(openagc_frontend_buffer_create(NULL, &desc, &buffer),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_create(frontend, NULL, &buffer),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.struct_size--;
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &buffer),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.struct_size++;
    desc.api_version++;
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &buffer),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.api_version = OPENAGC_FRONTEND_API_VERSION;
    desc.native_usage = OPENAGC_FRONTEND_GL_PIXEL_PACK_BUFFER;
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.native_usage = 0x0020u;
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.native_usage = OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    desc.size_bytes = 2u;
    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.size_bytes = 256u;
    CHECK(candidate == NULL);

    EXPECT(openagc_frontend_buffer_create(frontend, &desc, &buffer), OPENAGC_OK);
    info.struct_size--;
    EXPECT(openagc_frontend_buffer_get_info(buffer, &info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    info.struct_size++;
    EXPECT(openagc_frontend_buffer_get_info(buffer, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_FRONTEND_VULKAN && info.size_bytes == 256u);
    CHECK(info.native_usage == (OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    CHECK(info.usage == (OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                         OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));
    EXPECT(openagc_frontend_buffer_get_info(NULL, &info),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_get_info(buffer, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_destroy(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_create(frontend, &gl_desc, &gl_buffer), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_get_info(gl_buffer, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_FRONTEND_OPENGL && info.size_bytes == 128u);
    CHECK(info.usage == (OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                         OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT));

    /* A buffer object and an image share one staging window. */
    EXPECT(openagc_frontend_image_create(frontend, &image_desc, &image), OPENAGC_OK);
    for (i = 0u; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)(0x60u + i);
    }
    memset(small, 0x3c, sizeof(small));
    EXPECT(openagc_frontend_buffer_upload(buffer, 0u, pattern, sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_upload(image, 0u, small, sizeof(small)), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_readback(buffer, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(pattern)) == 0);
    memset(readback, 0, sizeof(readback));
    EXPECT(openagc_frontend_image_readback(image, 0u, readback, sizeof(small)),
           OPENAGC_OK);
    CHECK(memcmp(readback, small, sizeof(small)) == 0);

    EXPECT(openagc_frontend_buffer_upload(buffer, 0u, pattern, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_upload(buffer, 2u, pattern, 16u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_upload(buffer, 0u, pattern, 6u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_upload(buffer, 240u, pattern, 64u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_readback(buffer, 256u, readback, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_upload(gl_buffer, 0u, pattern, 4096u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_upload(buffer, 0u, NULL, 16u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_readback(buffer, 0u, NULL, 16u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_buffer_upload(gl_buffer, 0u, pattern, sizeof(small)),
           OPENAGC_OK);
    memset(readback, 0, sizeof(readback));
    EXPECT(openagc_frontend_buffer_readback(gl_buffer, 0u, readback, sizeof(small)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(small)) == 0);
    EXPECT(openagc_frontend_buffer_create(frontend, &gl_pack_desc, &gl_pack), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_upload(gl_pack, 0u, pattern, sizeof(small)),
           OPENAGC_OK);
    memset(readback, 0xff, sizeof(readback));
    EXPECT(openagc_frontend_buffer_readback(gl_pack, 64u, readback, sizeof(small)),
           OPENAGC_OK);
    for (i = 0u; i < sizeof(small); ++i) {
        CHECK(readback[i] == 0u);
    }

    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(gl_buffer), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(gl_pack), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_two_kinds_share_one_backend(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_image_desc vulkan_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 4u, 4u, 0u);
    openagc_frontend_image_desc gl_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_OPENGL, OPENAGC_FRONTEND_GL_INTERNAL_FORMAT_RGBA8,
        OPENAGC_FRONTEND_GL_TEXTURE_2D, 0u, 4u, 4u, 0u);
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_image *vulkan_image = NULL;
    openagc_frontend_image *gl_image = NULL;
    uint8_t vulkan_bytes[64];
    uint8_t gl_bytes[64];
    uint8_t readback[64];
    uint32_t i;

    CHECK(make_device(1048576u, &context, &device) == 0);
    frontend_desc.staging_bytes = 8192u;
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &vulkan_desc, &vulkan_image),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &gl_desc, &gl_image), OPENAGC_OK);
    for (i = 0u; i < sizeof(vulkan_bytes); ++i) {
        vulkan_bytes[i] = (uint8_t)(0x30u + i);
        gl_bytes[i] = (uint8_t)(0x90u + i);
    }

    EXPECT(openagc_frontend_image_upload(vulkan_image, 0u, vulkan_bytes,
                                         sizeof(vulkan_bytes)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_upload(gl_image, 0u, gl_bytes, sizeof(gl_bytes)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_readback(gl_image, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, gl_bytes, sizeof(gl_bytes)) == 0);
    EXPECT(openagc_frontend_image_readback(vulkan_image, 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, vulkan_bytes, sizeof(vulkan_bytes)) == 0);
    EXPECT(openagc_frontend_image_get_info(vulkan_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET &&
          info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_frontend_image_get_info(gl_image, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);

    /* Both kinds are children of one backend device and one frontend device. */
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_image_destroy(vulkan_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(gl_image), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_shared_heap_timeline_and_plan(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_capabilities caps = OPENAGC_FRONTEND_CAPABILITIES_INIT;
    openagc_frontend_buffer_desc buffer_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                     OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        256u);
    openagc_frontend_timeline_info timeline_info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_frontend_pipeline_info pipeline_info = OPENAGC_FRONTEND_PIPELINE_INFO_INIT;
    openagc_shader_artifact_desc artifact_desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_pipeline_desc plan_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    static const uint8_t code_hash[32] = {
        0x9fu, 0x86u, 0xd0u, 0x81u, 0x88u, 0x4cu, 0x7du, 0x65u,
        0x9au, 0x2fu, 0xeau, 0xa0u, 0xc5u, 0x5au, 0xd0u, 0x15u,
        0xa3u, 0xbfu, 0x4fu, 0x1bu, 0x2bu, 0x0bu, 0x82u, 0x2cu,
        0xd1u, 0x5du, 0x6cu, 0x15u, 0xb0u, 0xf0u, 0x0au, 0x08u
    };
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_buffer *buffers[65];
    openagc_frontend_timeline *timeline = NULL;
    openagc_frontend_pipeline *pipeline = NULL;
    openagc_shader_artifact *artifact = NULL;
    uint32_t i;
    uint8_t pattern[4] = { 1u, 2u, 3u, 4u };
    uint8_t readback[4] = { 0u, 0u, 0u, 0u };

    memset(buffers, 0, sizeof(buffers));
    CHECK(make_device(1048576u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_device_get_capabilities(frontend, &caps), OPENAGC_OK);
    CHECK(caps.host_suballocation == 1u && caps.host_timeline == 1u);
    CHECK(caps.host_pipeline_plans == 1u && caps.gpu_execution == 0u);
    CHECK(caps.heap_bytes == 262144u);
    for (i = 0u; i < 65u; ++i) {
        EXPECT(openagc_frontend_buffer_create(frontend, &buffer_desc, &buffers[i]),
               OPENAGC_OK);
    }
    EXPECT(openagc_frontend_buffer_upload(buffers[0], 0u, pattern, sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_readback(buffers[0], 0u, readback, sizeof(readback)),
           OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_frontend_buffer_destroy(buffers[1]), OPENAGC_OK);
    buffers[1] = NULL;
    EXPECT(openagc_frontend_buffer_create(frontend, &buffer_desc, &buffers[1]), OPENAGC_OK);

    EXPECT(openagc_frontend_timeline_create(NULL, &timeline), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_timeline_create(frontend, &timeline), OPENAGC_OK);
    timeline_info.struct_size--;
    EXPECT(openagc_frontend_timeline_poll(timeline, 1u, &timeline_info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    timeline_info.struct_size++;
    EXPECT(openagc_frontend_timeline_poll(timeline, 0u, &timeline_info),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_timeline_poll(timeline, 1u, &timeline_info),
           OPENAGC_ERROR_NOT_READY);
    CHECK(timeline_info.signaled == 0u && timeline_info.value == 0u);
    EXPECT(openagc_frontend_timeline_signal(timeline), OPENAGC_OK);
    EXPECT(openagc_frontend_timeline_signal(timeline), OPENAGC_OK);
    EXPECT(openagc_frontend_timeline_poll(timeline, 2u, &timeline_info), OPENAGC_OK);
    CHECK(timeline_info.signaled == 1u && timeline_info.value == 2u);
    EXPECT(openagc_frontend_timeline_poll(timeline, 3u, &timeline_info),
           OPENAGC_ERROR_NOT_READY);

    artifact_desc.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact_desc.code = code;
    artifact_desc.code_size = 4u;
    artifact_desc.workgroup_x = 8u;
    artifact_desc.workgroup_y = 1u;
    artifact_desc.workgroup_z = 1u;
    memcpy(artifact_desc.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_shader_artifact_intake_host(device, &artifact_desc, &artifact),
           OPENAGC_OK);
    plan_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    plan_desc.compute = artifact;
    EXPECT(openagc_frontend_pipeline_create(frontend, NULL, &pipeline),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_frontend_pipeline_create(frontend, &plan_desc, &pipeline), OPENAGC_OK);
    EXPECT(openagc_frontend_pipeline_get_info(pipeline, &pipeline_info), OPENAGC_OK);
    CHECK(pipeline_info.kind == OPENAGC_SHADER_PIPELINE_COMPUTE);
    CHECK(pipeline_info.compiler_verified == 0u && pipeline_info.gpu_executable == 0u);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_pipeline_destroy(pipeline), OPENAGC_OK);
    EXPECT(openagc_frontend_timeline_destroy(timeline), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    for (i = 0u; i < 65u; ++i) {
        if (buffers[i] != NULL) {
            EXPECT(openagc_frontend_buffer_destroy(buffers[i]), OPENAGC_OK);
        }
    }
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_explicit_memory(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device_desc desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_buffer_desc buffer_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN,
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        64u);
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_memory *memory = NULL;
    openagc_frontend_buffer *first = NULL;
    openagc_frontend_buffer *second = NULL;
    openagc_frontend_buffer *dedicated = NULL;
    uint8_t bytes[4] = { 0xabu, 0xcdu, 0xefu, 0x01u };
    uint8_t readback[4] = { 0u, 0u, 0u, 0u };

    CHECK(make_device(16u * 1024u * 1024u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_memory_allocate(frontend, 0u, &memory), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_memory_allocate(frontend, 512u, &memory), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_create_unbound(frontend, &buffer_desc, &first), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_upload(first, 0u, bytes, sizeof(bytes)),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_buffer_bind_memory(first, memory, 4u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_bind_memory(first, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_bind_memory(first, memory, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_buffer_create_unbound(frontend, &buffer_desc, &second), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_bind_memory(second, memory, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_buffer_bind_memory(second, memory, 256u), OPENAGC_OK);
    EXPECT(openagc_frontend_memory_destroy(memory), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_frontend_buffer_upload(first, 0u, bytes, sizeof(bytes)), OPENAGC_OK);
    EXPECT(openagc_frontend_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(readback, bytes, sizeof(bytes)) == 0);
    EXPECT(openagc_frontend_buffer_create(frontend, &buffer_desc, &dedicated), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_bind_memory(dedicated, memory, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_buffer_destroy(dedicated), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(first), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(second), OPENAGC_OK);
    EXPECT(openagc_frontend_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_fill_and_image_copy(void)
{
    uint8_t source_pixels[64];
    uint8_t destination_pixels[64];
    uint8_t words[64];
    openagc_context *context = NULL;
    openagc_context *other_context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_device *other_device = NULL;
    openagc_frontend_device_desc desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_capabilities capabilities = OPENAGC_FRONTEND_CAPABILITIES_INIT;
    openagc_frontend_buffer_desc fillable_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN,
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        512u);
    openagc_frontend_buffer_desc source_only_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 64u);
    openagc_frontend_image_desc image_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u, 0u);
    openagc_frontend_image_desc other_format_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u, 0u);
    openagc_frontend_image_info info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_device *other_frontend = NULL;
    openagc_frontend_buffer *fillable = NULL;
    openagc_frontend_buffer *source_only = NULL;
    openagc_frontend_buffer *unbound = NULL;
    openagc_frontend_buffer *other_buffer = NULL;
    openagc_frontend_image *source = NULL;
    openagc_frontend_image *destination = NULL;
    openagc_frontend_image *other_format = NULL;
    openagc_frontend_image *unbound_image = NULL;
    uint32_t value = 0x11223344u;
    uint32_t index;

    for (index = 0u; index < sizeof(source_pixels); ++index) {
        source_pixels[index] = (uint8_t)(index + 1u);
    }
    memset(destination_pixels, 0, sizeof(destination_pixels));
    CHECK(make_device(32u * 1024u * 1024u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(device, &desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_device_get_capabilities(frontend, &capabilities), OPENAGC_OK);
    CHECK(capabilities.host_image_copy == 1u && capabilities.host_buffer_fill == 1u);
    CHECK(capabilities.gpu_execution == 0u && capabilities.presentation == 0u);

    EXPECT(openagc_frontend_buffer_create(frontend, &fillable_desc, &fillable), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_create(frontend, &source_only_desc, &source_only), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_create_unbound(frontend, &fillable_desc, &unbound), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_fill(fillable, 2u, 4u, value), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_fill(fillable, 0u, 0u, value), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_fill(fillable, 508u, 8u, value), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_buffer_fill(unbound, 0u, 4u, value), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_buffer_fill(source_only, 0u, 4u, value),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    {
        openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        uint32_t dword = 0u;

        EXPECT(openagc_frontend_buffer_fill(fillable, 0u, 4u, value), OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_readback(fillable, 0u, &dword, sizeof(dword)),
               OPENAGC_OK);
        CHECK(dword == value);
        EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
        CHECK(write_view.gpu_submitted == 0u);
        CHECK(write_view.word_count == OPENAGC_PM4_WRITE_DATA_EOP_WORDS);
        CHECK(write_view.words[0] == 0xc0033700u);
        CHECK(write_view.words[4] == value);
        CHECK(write_view.words[5] == OPENAGC_PM4_EOP_HEADER);
    }
    EXPECT(openagc_frontend_buffer_fill(fillable, 0u, sizeof(words), value), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_readback(fillable, 0u, words, sizeof(words)), OPENAGC_OK);
    for (index = 0u; index < (uint32_t)(sizeof(words) / 4u); ++index) {
        uint32_t observed;

        memcpy(&observed, words + index * 4u, 4u);
        CHECK(observed == value);
    }
    {
        /* Step G: 128-byte contiguous fill = two 16-dword WRITE_DATA rows. */
        uint8_t wide[128];
        openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        uint32_t observed;
        uint32_t i;

        memset(wide, 0, sizeof(wide));
        EXPECT(openagc_frontend_buffer_fill(fillable, 0u, sizeof(wide), value), OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_readback(fillable, 0u, wide, sizeof(wide)), OPENAGC_OK);
        for (i = 0u; i < (uint32_t)(sizeof(wide) / 4u); ++i) {
            memcpy(&observed, wide + i * 4u, 4u);
            CHECK(observed == value);
        }
        EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
        CHECK(write_view.gpu_submitted == 0u);
        CHECK(write_view.word_count == OPENAGC_PM4_WRITE_DATA_STEP_G_EOP_WORDS);
        CHECK(write_view.words[0] == 0xc0123700u);
        CHECK(write_view.words[4] == value);
        CHECK(write_view.words[20] == 0xc0123700u);
        CHECK(write_view.words[40] == OPENAGC_PM4_EOP_HEADER);
    }
    {
        /* 80 bytes: one full 16-dword row then an 4-dword remainder packet. */
        uint8_t wide[80];
        openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        uint32_t observed;
        uint32_t i;

        memset(wide, 0, sizeof(wide));
        EXPECT(openagc_frontend_buffer_fill(fillable, 0u, sizeof(wide), value), OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_readback(fillable, 0u, wide, sizeof(wide)), OPENAGC_OK);
        for (i = 0u; i < (uint32_t)(sizeof(wide) / 4u); ++i) {
            memcpy(&observed, wide + i * 4u, 4u);
            CHECK(observed == value);
        }
        EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
        CHECK(write_view.gpu_submitted == 0u);
        CHECK(write_view.word_count ==
              OPENAGC_PM4_WRITE_DATA_WORDS(4u) + OPENAGC_PM4_EOP_WITH_NOP_WORDS);
        CHECK(write_view.words[0] == 0xc0063700u);
        CHECK(write_view.words[4] == value);
        CHECK(write_view.words[8] == OPENAGC_PM4_EOP_HEADER);
    }
    {
        /* Step H: copy 64 bytes then WRITE_DATA 16-byte head. */
        openagc_frontend_buffer_desc src_desc = OPENAGC_FRONTEND_BUFFER_DESC_INIT(
            OPENAGC_FRONTEND_VULKAN,
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            64u);
        openagc_frontend_buffer *copy_source = NULL;
        uint8_t pattern[64];
        uint8_t out[64];
        openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
        uint32_t i;
        uint32_t dma_word = 0x11111111u;
        uint32_t observed;

        for (i = 0u; i < 64u; i += 4u) {
            memcpy(pattern + i, &dma_word, 4u);
        }
        EXPECT(openagc_frontend_buffer_create(frontend, &src_desc, &copy_source), OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_upload(copy_source, 0u, pattern, sizeof(pattern)),
               OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_copy_then_fill(copy_source, 0u, fillable, 0u, 64u, value,
                                                      16u),
               OPENAGC_OK);
        EXPECT(openagc_frontend_buffer_readback(fillable, 0u, out, sizeof(out)), OPENAGC_OK);
        for (i = 0u; i < 4u; ++i) {
            memcpy(&observed, out + i * 4u, 4u);
            CHECK(observed == value);
        }
        for (i = 4u; i < 16u; ++i) {
            memcpy(&observed, out + i * 4u, 4u);
            CHECK(observed == dma_word);
        }
        EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
        CHECK(write_view.gpu_submitted == 0u);
        CHECK(write_view.word_count == OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS);
        CHECK(write_view.words[0] == OPENAGC_PM4_DMA_HEADER);
        CHECK(write_view.words[7] == 0xc0063700u);
        CHECK(write_view.words[11] == value);
        CHECK(write_view.words[15] == OPENAGC_PM4_EOP_HEADER);
        EXPECT(openagc_frontend_buffer_destroy(copy_source), OPENAGC_OK);
    }

    EXPECT(openagc_frontend_image_create(frontend, &image_desc, &source), OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &image_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &other_format_desc, &other_format),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_create_unbound(frontend, &image_desc, &unbound_image),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_upload(source, 0u, source_pixels, sizeof(source_pixels)),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_copy_rect(source, 0u, 0u, source, 0u, 0u, 2u, 2u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_image_copy_rect(source, 0u, 0u, other_format, 0u, 0u, 2u, 2u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_image_copy_rect(unbound_image, 0u, 0u, destination, 0u, 0u, 2u, 2u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_image_copy_rect(source, 3u, 0u, destination, 0u, 0u, 2u, 2u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_copy_rect(source, 0u, 0u, destination, 3u, 0u, 2u, 1u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_copy_rect(source, 0u, 0u, destination, 0u, 0u, 0u, 2u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_image_copy_rect(source, 0u, 0u, destination, 1u, 1u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_frontend_image_get_info(source, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_frontend_image_get_info(destination, &info), OPENAGC_OK);
    CHECK(info.state == OPENAGC_GRAPHICS_STATE_UNDEFINED &&
          info.owner == OPENAGC_GRAPHICS_OWNER_HOST);
    EXPECT(openagc_frontend_image_readback(destination, 0u, destination_pixels,
                                           sizeof(destination_pixels)),
           OPENAGC_OK);
    CHECK(destination_pixels[20] == 1u && destination_pixels[27] == 8u);
    CHECK(destination_pixels[36] == 17u && destination_pixels[43] == 24u);
    CHECK(destination_pixels[19] == 0u && destination_pixels[28] == 0u);
    CHECK(destination_pixels[35] == 0u && destination_pixels[44] == 0u);

    /* A second backend device is a second ownership domain, not a second copy. */
    CHECK(make_device(32u * 1024u * 1024u, &other_context, &other_device) == 0);
    EXPECT(openagc_frontend_device_create(other_device, &desc, &other_frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_create(other_frontend, &fillable_desc, &other_buffer),
           OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_copy(fillable, 0u, other_buffer, 0u, 4u),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_frontend_buffer_fill(other_buffer, 0u, 4u, value), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(other_buffer), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(other_frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(other_device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(other_context), OPENAGC_OK);

    EXPECT(openagc_frontend_image_destroy(unbound_image), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(other_format), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(destination), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(source), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(source_only), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(unbound), OPENAGC_OK);
    EXPECT(openagc_frontend_buffer_destroy(fillable), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_store_const_dispatch(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(16u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(16u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *buffer = NULL;
    openagc_shader_artifact_desc artifact_desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_binding_decl binding = { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 4u };
    openagc_shader_artifact *artifact = NULL;
    openagc_shader_pipeline_desc plan_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_resource_binding resource;
    openagc_frontend_pipeline *pipeline = NULL;
    uint8_t code_hash[32] = {
        0x48, 0x47, 0x46, 0xd3, 0x32, 0x1b, 0x61, 0x85, 0x50, 0xe2, 0x6e, 0x5a, 0xb5, 0xa2,
        0xde, 0x56, 0x4d, 0x76, 0xf8, 0x6a, 0x61, 0x15, 0x72, 0x0b, 0x20, 0x1a, 0x39, 0x05,
        0xe8, 0x27, 0xd2, 0xe5
    };
    uint32_t word = 0u;

    CHECK(make_device(1048576u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer, memory, 0u), OPENAGC_OK);

    artifact_desc.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact_desc.code = openagc_store_const_code;
    artifact_desc.code_size = OPENAGC_STORE_CONST_CODE_SIZE;
    artifact_desc.bindings = &binding;
    artifact_desc.binding_count = 1u;
    artifact_desc.workgroup_x = 1u;
    artifact_desc.workgroup_y = 1u;
    artifact_desc.workgroup_z = 1u;
    memcpy(artifact_desc.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_shader_artifact_intake_host(device, &artifact_desc, &artifact), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_get_info(artifact, &artifact_info), OPENAGC_OK);
    CHECK(artifact_info.host_store_const == 1u);
    CHECK(artifact_info.gpu_executable == 0u && artifact_info.compiler_verified == 0u);

    memset(&resource, 0, sizeof(resource));
    resource.binding = 0u;
    resource.buffer = buffer;
    resource.offset = 0u;
    resource.size_bytes = 16u;
    plan_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    plan_desc.compute = artifact;
    plan_desc.resources = &resource;
    plan_desc.resource_count = 1u;
    EXPECT(openagc_frontend_pipeline_create(frontend, &plan_desc, &pipeline), OPENAGC_OK);

    EXPECT(openagc_frontend_dispatch(pipeline, 2u, 1u, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_frontend_dispatch(pipeline, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, &word, sizeof(word)), OPENAGC_OK);
    CHECK(word == OPENAGC_STORE_CONST_VALUE);
    {
        openagc_gpu_submission_view compute = OPENAGC_GPU_SUBMISSION_VIEW_INIT;

        EXPECT(openagc_gpu_device_get_last_compute(device, &compute), OPENAGC_OK);
        CHECK(compute.gpu_submitted == 0u);
        CHECK(compute.word_count == OPENAGC_PM4_COMPUTE_STORE_WORDS);
        CHECK(compute.submission_id == 1u);
        CHECK(compute.words != NULL);
        CHECK(compute.words[26] == OPENAGC_PM4_DISPATCH_INITIATOR);
    }

    EXPECT(openagc_frontend_pipeline_destroy(pipeline), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_store_span_dispatch(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(256u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(256u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *buffer = NULL;
    openagc_shader_artifact_desc artifact_desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_binding_decl binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, OPENAGC_STORE_SPAN_BYTES
    };
    openagc_shader_artifact *artifact = NULL;
    openagc_shader_pipeline_desc plan_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_resource_binding resource;
    openagc_frontend_pipeline *pipeline = NULL;
    uint8_t code_hash[32] = {
        0xa7, 0x0a, 0xe4, 0xfd, 0xe2, 0x4b, 0xe0, 0x34, 0x0a, 0x81, 0x8b, 0xee, 0x67, 0x40,
        0x27, 0x2e, 0x57, 0xb7, 0xc9, 0x56, 0x07, 0xf1, 0x24, 0x43, 0x7e, 0x1c, 0x94, 0x39,
        0x81, 0x9b, 0xa2, 0xc4
    };
    uint32_t words[64];
    uint32_t i;

    CHECK(make_device(1048576u, &context, &device) == 0);
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer, memory, 0u), OPENAGC_OK);

    artifact_desc.stage = OPENAGC_SHADER_STAGE_COMPUTE;
    artifact_desc.code = openagc_store_span_code;
    artifact_desc.code_size = OPENAGC_STORE_SPAN_CODE_SIZE;
    artifact_desc.bindings = &binding;
    artifact_desc.binding_count = 1u;
    artifact_desc.workgroup_x = 8u;
    artifact_desc.workgroup_y = 1u;
    artifact_desc.workgroup_z = 1u;
    memcpy(artifact_desc.code_sha256, code_hash, sizeof(code_hash));
    EXPECT(openagc_shader_artifact_intake_host(device, &artifact_desc, &artifact), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_get_info(artifact, &artifact_info), OPENAGC_OK);
    CHECK(artifact_info.host_store_span == 1u);
    CHECK(artifact_info.gpu_executable == 0u && artifact_info.compiler_verified == 0u);

    memset(&resource, 0, sizeof(resource));
    resource.binding = 0u;
    resource.buffer = buffer;
    resource.offset = 0u;
    resource.size_bytes = OPENAGC_STORE_SPAN_BYTES;
    plan_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    plan_desc.compute = artifact;
    plan_desc.resources = &resource;
    plan_desc.resource_count = 1u;
    EXPECT(openagc_frontend_pipeline_create(frontend, &plan_desc, &pipeline), OPENAGC_OK);
    EXPECT(openagc_frontend_dispatch(pipeline, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, words, OPENAGC_STORE_SPAN_BYTES), OPENAGC_OK);
    for (i = 0u; i < OPENAGC_STORE_SPAN_LANES; ++i) {
        CHECK(words[i] == OPENAGC_STORE_SPAN_VALUE);
    }
    EXPECT(openagc_frontend_pipeline_destroy(pipeline), OPENAGC_OK);

    resource.size_bytes = 128u;
    EXPECT(openagc_frontend_pipeline_create(frontend, &plan_desc, &pipeline), OPENAGC_OK);
    EXPECT(openagc_frontend_dispatch(pipeline, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, words, 128u), OPENAGC_OK);
    for (i = 0u; i < 32u; ++i) {
        CHECK(words[i] == OPENAGC_STORE_SPAN_VALUE);
    }
    {
        openagc_gpu_submission_view compute = OPENAGC_GPU_SUBMISSION_VIEW_INIT;

        EXPECT(openagc_gpu_device_get_last_compute(device, &compute), OPENAGC_OK);
        CHECK(compute.gpu_submitted == 0u);
        CHECK(compute.word_count == OPENAGC_PM4_COMPUTE_STORE_SPAN4_WORDS);
    }
    EXPECT(openagc_frontend_pipeline_destroy(pipeline), OPENAGC_OK);

    resource.size_bytes = 256u;
    EXPECT(openagc_frontend_pipeline_create(frontend, &plan_desc, &pipeline), OPENAGC_OK);
    EXPECT(openagc_frontend_dispatch(pipeline, 1u, 1u, 1u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, words, 256u), OPENAGC_OK);
    for (i = 0u; i < 64u; ++i) {
        CHECK(words[i] == OPENAGC_STORE_SPAN_VALUE);
    }
    {
        openagc_gpu_submission_view compute = OPENAGC_GPU_SUBMISSION_VIEW_INIT;

        EXPECT(openagc_gpu_device_get_last_compute(device, &compute), OPENAGC_OK);
        CHECK(compute.word_count == OPENAGC_PM4_COMPUTE_STORE_SPAN_MAX_WORDS);
    }

    EXPECT(openagc_frontend_pipeline_destroy(pipeline), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_cb_capture_bind_path(void)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_frontend_device_desc frontend_desc = OPENAGC_FRONTEND_DEVICE_DESC_INIT;
    openagc_frontend_image_desc image_desc = OPENAGC_FRONTEND_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VULKAN, OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 8u, 4u, 32u);
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_frontend_device *frontend = NULL;
    openagc_frontend_image *color = NULL;
    openagc_frontend_render_pass *pass = NULL;
    openagc_cb_capture_manifest manifest = OPENAGC_CB_CAPTURE_MANIFEST_INIT;
    openagc_cb_capture_info info = OPENAGC_CB_CAPTURE_INFO_INIT;
    openagc_gpu_submission_view write_view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    static const uint32_t fixture_words[] = { OPENAGC_PM4_NOP_HEADER, 0u,
                                              OPENAGC_PM4_NOP_HEADER, 0u };

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_frontend_device_create(device, &frontend_desc, &frontend), OPENAGC_OK);
    EXPECT(openagc_frontend_image_create(frontend, &image_desc, &color), OPENAGC_OK);
    EXPECT(openagc_frontend_render_pass_create(frontend, color, &pass), OPENAGC_OK);

    manifest.kind = OPENAGC_CB_CAPTURE_KIND_CB_BIND;
    manifest.firmware_id = OPENAGC_CB_CAPTURE_FW940_ID;
    manifest.word_count = 4u;
    openagc_sha256((const uint8_t *)fixture_words, sizeof(fixture_words),
                   manifest.words_sha256);

    /* Not begun yet. */
    EXPECT(openagc_frontend_render_pass_bind_cb_capture(pass, &manifest, fixture_words),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frontend_render_pass_begin(pass), OPENAGC_OK);
    EXPECT(openagc_frontend_render_pass_bind_cb_capture(pass, &manifest, fixture_words),
           OPENAGC_OK);
    EXPECT(openagc_frontend_device_get_cb_capture_info(frontend, &info), OPENAGC_OK);
    CHECK(info.capture_verified == 1u);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.gpu_submitted == 0u);
    EXPECT(openagc_gpu_device_get_last_write(device, &write_view), OPENAGC_OK);
    CHECK(write_view.gpu_submitted == 0u && write_view.word_count == 4u);

    /* Invent remains refused; draw without pipeline stays refused. */
    EXPECT(openagc_cb_capture_encode_invent(OPENAGC_CB_CAPTURE_KIND_CB_BIND, NULL, 0u, NULL),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_frontend_render_pass_draw(pass, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_frontend_render_pass_end(pass), OPENAGC_OK);
    EXPECT(openagc_frontend_render_pass_destroy(pass), OPENAGC_OK);
    EXPECT(openagc_frontend_image_destroy(color), OPENAGC_OK);
    EXPECT(openagc_frontend_device_destroy(frontend), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_translation_tables() != 0 ||
        test_device_capabilities_and_lifetime() != 0 ||
        test_image_create_and_refusals() != 0 ||
        test_upload_readback_roundtrip() != 0 ||
        test_buffer_objects() != 0 ||
        test_two_kinds_share_one_backend() != 0 ||
        test_shared_heap_timeline_and_plan() != 0 ||
        test_explicit_memory() != 0 ||
        test_fill_and_image_copy() != 0 ||
        test_host_store_const_dispatch() != 0 ||
        test_host_store_span_dispatch() != 0 ||
        test_cb_capture_bind_path() != 0) {
        return 1;
    }
    puts("OpenAGC shared frontend tests passed");
    return 0;
}
