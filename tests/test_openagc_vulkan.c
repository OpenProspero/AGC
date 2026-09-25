/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
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

static int test_enumeration_transfer_and_refusals(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_buffer_desc source_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        256u);
    openagc_vk_buffer_desc destination_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        256u);
    openagc_vk_buffer_desc uniform_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 256u);
    openagc_vk_image_desc image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            OPENAGC_FRONTEND_VK_IMAGE_USAGE_SAMPLED_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_vk_capabilities caps = OPENAGC_VK_CAPABILITIES_INIT;
    openagc_vk_queue_family family = OPENAGC_VK_QUEUE_FAMILY_INIT;
    openagc_frontend_timeline_info fence_info = OPENAGC_FRONTEND_TIMELINE_INFO_INIT;
    openagc_frontend_image_info image_info = OPENAGC_FRONTEND_IMAGE_INFO_INIT;
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_buffer *source = NULL;
    openagc_vk_buffer *destination = NULL;
    openagc_vk_buffer *uniform = NULL;
    openagc_vk_image *image = NULL;
    openagc_vk_image_view_desc view_desc = OPENAGC_VK_IMAGE_VIEW_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    openagc_vk_image_view_info view_info = OPENAGC_VK_IMAGE_VIEW_INFO_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *commands = NULL;
    openagc_vk_command_buffer *refused = NULL;
    openagc_vk_command_buffer *clears = NULL;
    openagc_vk_image_view *view = NULL;
    openagc_vk_image_view *rejected = NULL;
    openagc_vk_fence *fence = NULL;
    openagc_color clear = { 0x11u, 0x22u, 0x33u, 0x44u };
    uint8_t pixels[64];
    uint8_t pattern[16];
    uint8_t readback[16];
    uint32_t i;

    EXPECT(openagc_vk_instance_create(NULL, &instance), OPENAGC_ERROR_INVALID_ARGUMENT);
    instance_desc.api_version++;
    EXPECT(openagc_vk_instance_create(&instance_desc, &instance),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    instance_desc.api_version = OPENAGC_VK_API_VERSION;
    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    caps.struct_size--;
    EXPECT(openagc_vk_get_capabilities(instance, &caps), OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    caps.struct_size++;
    EXPECT(openagc_vk_get_capabilities(instance, &caps), OPENAGC_OK);
    CHECK(caps.physical_device_count == 1u && caps.copy_queue_count == 1u);
    CHECK(caps.graphics_queue_count == 0u && caps.compute_queue_count == 0u);
    CHECK(caps.gpu_execution == 0u && caps.presentation == 0u);
    EXPECT(openagc_vk_get_queue_family(instance, 0u, &family), OPENAGC_OK);
    CHECK(family.queue_flags == OPENAGC_VK_QUEUE_TRANSFER_BIT && family.queue_count == 1u);
    CHECK((family.queue_flags & OPENAGC_VK_QUEUE_GRAPHICS_BIT) == 0u);
    EXPECT(openagc_vk_get_queue_family(instance, 1u, &family), OPENAGC_ERROR_OUT_OF_RANGE);

    device_desc.queue_family_index = 1u;
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    device_desc.queue_family_index = 0u;
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_swapchain(device), OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_cmd_draw(NULL, 3u, 1u, 0u, 0u), OPENAGC_ERROR_INVALID_ARGUMENT);

    for (i = 0u; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)(0x40u + i);
    }
    EXPECT(openagc_vk_create_buffer(device, &source_desc, &source), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &destination_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &uniform_desc, &uniform), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(source, 0u, pattern, sizeof(pattern)), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &image_desc, &image), OPENAGC_OK);
    pool_desc.queue_family_index = 1u;
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    pool_desc.queue_family_index = 0u;
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer(commands, source, 0u, destination, 0u, sizeof(pattern)),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_draw(commands, 3u, 1u, 0u, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_cmd_image_barrier(commands, image, 99u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_cmd_copy_buffer(commands, source, 0u, destination, 0u, sizeof(pattern)),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_image_barrier(
               commands, image, OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);

    EXPECT(openagc_vk_create_fence(device, &fence), OPENAGC_OK);
    EXPECT(openagc_vk_fence_poll(fence, 1u, &fence_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, fence), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit(device, fence), OPENAGC_OK);
    EXPECT(openagc_vk_fence_poll(fence, 2u, &fence_info), OPENAGC_OK);
    CHECK(fence_info.value == 2u && fence_info.signaled == 1u);
    memset(readback, 0, sizeof(readback));
    EXPECT(openagc_vk_buffer_readback(destination, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(readback, pattern, sizeof(pattern)) == 0);
    EXPECT(openagc_vk_image_get_info(image, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);
    CHECK(image_info.owner == OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_vk_allocate_command_buffer(pool, &refused), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(refused), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer(refused, uniform, 0u, destination, 0u, 4u), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(refused), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, refused, NULL),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    EXPECT(openagc_vk_create_image_view(device, image, &view_desc, &view), OPENAGC_OK);
    EXPECT(openagc_vk_image_view_get_info(view, &view_info), OPENAGC_OK);
    CHECK(view_info.view_type == OPENAGC_VK_IMAGE_VIEW_TYPE_2D);
    CHECK(view_info.format == OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM);
    CHECK(view_info.width == 4u && view_info.height == 4u);
    view_desc.view_type = 2u;
    EXPECT(openagc_vk_create_image_view(device, image, &view_desc, &rejected),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(rejected == NULL);
    view_desc.view_type = OPENAGC_VK_IMAGE_VIEW_TYPE_2D;
    view_desc.format = OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM;
    EXPECT(openagc_vk_create_image_view(device, image, &view_desc, &rejected),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(rejected == NULL);
    EXPECT(openagc_vk_destroy_image(image), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_vk_allocate_command_buffer(pool, &clears), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(clears), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_clear_color(clears, image, clear), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(clears), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, clears, NULL), OPENAGC_OK);
    memset(pixels, 0, sizeof(pixels));
    EXPECT(openagc_vk_image_readback(image, 0u, pixels, sizeof(pixels)), OPENAGC_OK);
    for (i = 0u; i < 16u; ++i) {
        CHECK(pixels[i * 4u] == clear.r && pixels[i * 4u + 1u] == clear.g &&
              pixels[i * 4u + 2u] == clear.b && pixels[i * 4u + 3u] == clear.a);
    }
    EXPECT(openagc_vk_image_get_info(image, &image_info), OPENAGC_OK);
    CHECK(image_info.state == OPENAGC_GRAPHICS_STATE_COLOR_TARGET);

    EXPECT(openagc_vk_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_destroy_image_view(view), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(clears), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_pool_reset(pool), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_command_buffer_end(commands), OPENAGC_OK);
    EXPECT(openagc_vk_command_pool_reset(pool), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, commands, NULL), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_vk_destroy_command_buffer(commands), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(refused), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(image), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(source), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(destination), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(uniform), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_fence(fence), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

static int test_fill_update_and_image_copy(void)
{
    openagc_vk_instance_desc instance_desc = OPENAGC_VK_INSTANCE_DESC_INIT;
    openagc_vk_device_desc device_desc = OPENAGC_VK_DEVICE_DESC_INIT;
    openagc_vk_command_pool_desc pool_desc = OPENAGC_VK_COMMAND_POOL_DESC_INIT;
    openagc_vk_buffer_desc fillable_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        8192u);
    openagc_vk_buffer_desc source_only_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 64u);
    openagc_vk_buffer_desc input_desc = OPENAGC_VK_BUFFER_DESC_INIT(
        OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            OPENAGC_FRONTEND_VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        64u);
    openagc_vk_image_desc source_image_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_R8G8B8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_vk_image_desc other_format_desc = OPENAGC_VK_IMAGE_DESC_INIT(
        OPENAGC_FRONTEND_VK_FORMAT_B8G8R8A8_UNORM,
        OPENAGC_FRONTEND_VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        OPENAGC_FRONTEND_VK_IMAGE_LAYOUT_UNDEFINED, 4u, 4u);
    openagc_vk_instance *instance = NULL;
    openagc_vk_device *device = NULL;
    openagc_vk_command_pool *pool = NULL;
    openagc_vk_command_buffer *transfer = NULL;
    openagc_vk_command_buffer *self_copy = NULL;
    openagc_vk_command_buffer *out_of_range = NULL;
    openagc_vk_command_buffer *format_mismatch = NULL;
    openagc_vk_buffer *fillable = NULL;
    openagc_vk_buffer *source_only = NULL;
    openagc_vk_buffer *input = NULL;
    openagc_vk_image *source = NULL;
    openagc_vk_image *destination = NULL;
    openagc_vk_image *other_format = NULL;
    uint8_t payload[OPENAGC_VK_MAX_UPDATE_BYTES];
    uint8_t pixels[64];
    uint8_t readback[8192];
    uint32_t fill_value = 0xdeadbeefu;
    uint32_t index;
    uint32_t observed;

    for (index = 0u; index < sizeof(payload); ++index) {
        payload[index] = (uint8_t)(index + 1u);
    }
    for (index = 0u; index < sizeof(pixels); ++index) {
        pixels[index] = (uint8_t)(index + 1u);
    }
    EXPECT(openagc_vk_instance_create(&instance_desc, &instance), OPENAGC_OK);
    EXPECT(openagc_vk_device_create(instance, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_vk_create_command_pool(device, &pool_desc, &pool), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &fillable_desc, &fillable), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &source_only_desc, &source_only), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &source_image_desc, &source), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &source_image_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_vk_create_image(device, &other_format_desc, &other_format), OPENAGC_OK);
    EXPECT(openagc_vk_create_buffer(device, &input_desc, &input), OPENAGC_OK);
    EXPECT(openagc_vk_buffer_upload(input, 0u, pixels, sizeof(pixels)), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_fill_buffer(NULL, fillable, 0u, 4u, fill_value),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    EXPECT(openagc_vk_allocate_command_buffer(pool, &transfer), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, fillable, 0u, 4u, fill_value),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 0u, payload, 4u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_vk_command_buffer_begin(transfer), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, source_only, 0u, 4u, fill_value),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, fillable, 2u, 4u, fill_value),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, fillable, 0u, 0u, fill_value),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, fillable, 8188u, 8u, fill_value),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, source_only, 0u, payload, 4u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 2u, payload, 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 0u, NULL, 4u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 0u, payload,
                                        OPENAGC_VK_MAX_UPDATE_BYTES + 4u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 0u, payload, 16u), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_fill_buffer(transfer, fillable, 16u, 16u, fill_value), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_update_buffer(transfer, fillable, 4096u, payload,
                                        OPENAGC_VK_MAX_UPDATE_BYTES),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_buffer_to_image(transfer, input, 0u, source, 0u, sizeof(pixels)),
           OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image(transfer, source, 0u, 0u, destination, 1u, 1u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(transfer), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, transfer, NULL), OPENAGC_OK);

    memset(readback, 0, sizeof(readback));
    EXPECT(openagc_vk_buffer_readback(fillable, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(readback, payload, 16u) == 0);
    for (index = 0u; index < 4u; ++index) {
        memcpy(&observed, readback + 16u + index * 4u, 4u);
        CHECK(observed == fill_value);
    }
    CHECK(readback[32] == 0u);
    CHECK(memcmp(readback + 4096u, payload, OPENAGC_VK_MAX_UPDATE_BYTES) == 0);
    memset(pixels, 0, sizeof(pixels));
    EXPECT(openagc_vk_image_readback(destination, 0u, pixels, sizeof(pixels)), OPENAGC_OK);
    CHECK(pixels[20] == 1u && pixels[27] == 8u);
    CHECK(pixels[36] == 17u && pixels[43] == 24u);
    CHECK(pixels[19] == 0u && pixels[28] == 0u);

    /* A recording that cannot run is refused whole and leaves the buffer untouched. */
    EXPECT(openagc_vk_allocate_command_buffer(pool, &self_copy), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(self_copy), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image(self_copy, source, 0u, 0u, source, 0u, 0u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(self_copy), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, self_copy, NULL),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &out_of_range), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(out_of_range), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image(out_of_range, source, 3u, 0u, destination, 0u, 0u, 2u, 2u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(out_of_range), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, out_of_range, NULL),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_vk_allocate_command_buffer(pool, &format_mismatch), OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_begin(format_mismatch), OPENAGC_OK);
    EXPECT(openagc_vk_cmd_copy_image(format_mismatch, source, 0u, 0u, other_format, 0u, 0u, 2u,
                                     2u),
           OPENAGC_OK);
    EXPECT(openagc_vk_command_buffer_end(format_mismatch), OPENAGC_OK);
    EXPECT(openagc_vk_queue_submit_commands(device, format_mismatch, NULL),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    EXPECT(openagc_vk_destroy_command_buffer(format_mismatch), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(out_of_range), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(self_copy), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_buffer(transfer), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_command_pool(pool), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(other_format), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(destination), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_image(source), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(source_only), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(input), OPENAGC_OK);
    EXPECT(openagc_vk_destroy_buffer(fillable), OPENAGC_OK);
    EXPECT(openagc_vk_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_vk_instance_destroy(instance), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_enumeration_transfer_and_refusals() != 0 ||
        test_fill_update_and_image_copy() != 0) {
        return 1;
    }
    puts("OpenAGC Vulkan subset tests passed");
    return 0;
}
