/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/openagc.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define EXPECT(expression, expected) do { \
    openagc_result actual_result = (expression); \
    if (actual_result != (expected)) { \
        fprintf(stderr, "%s:%d: %s returned %s; expected %s\n", \
                __FILE__, __LINE__, #expression, openagc_result_string(actual_result), \
                openagc_result_string(expected)); \
        return 1; \
    } \
} while (0)

static int make_device(uint32_t width, uint32_t height, uint32_t capacity,
                       openagc_context **context, openagc_device **device)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_device_desc device_desc = OPENAGC_DEVICE_DESC_INIT(width, height, capacity);

    EXPECT(openagc_context_create(&context_desc, context), OPENAGC_OK);
    EXPECT(openagc_device_create(*context, &device_desc, device), OPENAGC_OK);
    return 0;
}

static int test_lifecycle_and_limits(void)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_device_desc device_desc = OPENAGC_DEVICE_DESC_INIT(640u, 480u, 4u);
    openagc_context *context = NULL;
    openagc_device *first = NULL;
    openagc_device *second = NULL;

    CHECK(openagc_api_version() == OPENAGC_API_VERSION);
    CHECK(strcmp(openagc_result_string(OPENAGC_ERROR_UNSUPPORTED_FIRMWARE),
                 "firmware not qualified for PS5 access") == 0);
    CHECK(strcmp(openagc_result_string(999u), "unknown OpenAGC result code") == 0);
    EXPECT(openagc_context_create(NULL, &context), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_context_create(&context_desc, NULL), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_context_destroy(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_device_destroy(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);

    context_desc.struct_size--;
    EXPECT(openagc_context_create(&context_desc, &context),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    context_desc.struct_size++;
    context_desc.api_version++;
    EXPECT(openagc_context_create(&context_desc, &context),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    context_desc.api_version = OPENAGC_API_VERSION;
    context_desc.backend = 99u;
    EXPECT(openagc_context_create(&context_desc, &context),
           OPENAGC_ERROR_UNSUPPORTED_BACKEND);
    context_desc.backend = OPENAGC_BACKEND_HOST_REFERENCE;
    context_desc.firmware_major = 9u;
    EXPECT(openagc_context_create(&context_desc, &context),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    context_desc.firmware_major = 0u;
    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);

    EXPECT(openagc_device_create(context, &device_desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    device_desc.struct_size--;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    CHECK(first == NULL);
    device_desc.struct_size++;
    device_desc.width = 0u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    device_desc.width = 8193u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    device_desc.width = 8192u;
    device_desc.height = 8192u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    device_desc.width = 640u;
    device_desc.height = 480u;
    device_desc.height = 0u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    device_desc.height = 480u;
    device_desc.max_commands = 0u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    device_desc.max_commands = 4097u;
    EXPECT(openagc_device_create(context, &device_desc, &first),
           OPENAGC_ERROR_OUT_OF_RANGE);
    CHECK(first == NULL);

    device_desc.max_commands = 4u;
    EXPECT(openagc_device_create(context, &device_desc, &first), OPENAGC_OK);
    device_desc.width = 4096u;
    device_desc.height = 4096u;
    device_desc.max_commands = 4096u;
    EXPECT(openagc_device_create(context, &device_desc, &second), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_device_destroy(first), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_device_destroy(second), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_recording(void)
{
    openagc_context *context = NULL;
    openagc_device *device = NULL;
    openagc_capabilities capabilities = OPENAGC_CAPABILITIES_INIT;
    openagc_frame_view view = OPENAGC_FRAME_VIEW_INIT;
    openagc_color background = { 10u, 20u, 30u, 255u };
    openagc_rect rect = { 10.25f, 20.0f, 30.0f, 40.0f, { 1u, 2u, 3u, 128u } };
    openagc_command expected[2];

    CHECK(make_device(800u, 600u, 4u, &context, &device) == 0);
    capabilities.struct_size--;
    EXPECT(openagc_device_get_capabilities(device, &capabilities),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    capabilities.struct_size++;
    EXPECT(openagc_device_get_capabilities(device, &capabilities), OPENAGC_OK);
    CHECK(capabilities.backend == OPENAGC_BACKEND_HOST_REFERENCE);
    CHECK(capabilities.gpu_execution == 0u && capabilities.video_output == 0u);
    CHECK(capabilities.records_commands == 1u);
    CHECK(capabilities.max_width == 8192u && capabilities.max_height == 8192u);
    CHECK(capabilities.max_pixels == 16777216u && capabilities.max_commands == 4096u);

    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_clear(device, background), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_present(device), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_begin(device), OPENAGC_OK);
    EXPECT(openagc_frame_begin(device), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_clear(device, background), OPENAGC_OK);
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_OK);
    EXPECT(openagc_frame_present(device), OPENAGC_OK);
    EXPECT(openagc_frame_present(device), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_BAD_STATE);
    view.struct_size--;
    EXPECT(openagc_device_get_last_frame(device, &view),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    view.struct_size++;
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_OK);
    CHECK(view.width == 800u && view.height == 600u && view.frame_number == 1u);
    CHECK(view.command_count == 2u && view.commands != NULL);
    CHECK(view.commands[0].type == OPENAGC_COMMAND_CLEAR);
    CHECK(view.commands[0].data.clear.r == background.r);
    CHECK(view.commands[0].data.clear.a == background.a);
    CHECK(view.commands[1].type == OPENAGC_COMMAND_RECTANGLE);
    CHECK(view.commands[1].data.rectangle.x == rect.x);
    CHECK(view.commands[1].data.rectangle.height == rect.height);
    CHECK(view.commands[1].data.rectangle.color.a == rect.color.a);
    memcpy(expected, view.commands, sizeof(expected));

    EXPECT(openagc_frame_begin(device), OPENAGC_OK);
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_frame_clear(device, background), OPENAGC_OK);
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_OK);
    EXPECT(openagc_frame_present(device), OPENAGC_OK);
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_OK);
    CHECK(view.frame_number == 2u && view.command_count == 2u);
    CHECK(memcmp(expected, view.commands, sizeof(expected)) == 0);

    EXPECT(openagc_frame_begin(device), OPENAGC_OK);
    EXPECT(openagc_frame_present(device), OPENAGC_OK);
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_OK);
    CHECK(view.frame_number == 3u && view.command_count == 0u);
    EXPECT(openagc_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_geometry_and_capacity(void)
{
    openagc_context *context = NULL;
    openagc_device *device = NULL;
    openagc_frame_view view = OPENAGC_FRAME_VIEW_INIT;
    openagc_color color = { 255u, 0u, 0u, 255u };
    openagc_rect rect = { 0.0f, 0.0f, 10.0f, 10.0f, { 0u, 128u, 255u, 255u } };
    float zero = 0.0f;

    CHECK(make_device(100u, 50u, 2u, &context, &device) == 0);
    EXPECT(openagc_frame_begin(device), OPENAGC_OK);
    EXPECT(openagc_frame_rect(device, NULL), OPENAGC_ERROR_INVALID_ARGUMENT);
    rect.x = NAN;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = 0.0f;
    rect.y = INFINITY;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.y = 0.0f;
    rect.width = -INFINITY;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.width = 10.0f;
    rect.height = NAN;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.height = 10.0f;
    rect.x = -1.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = 0.0f;
    rect.width = 0.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.width = 10.0f;
    rect.height = -0.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.height = 10.0f;
    rect.x = 99.0f;
    rect.width = 2.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = FLT_MIN;
    rect.width = 100.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = 100.0f;
    rect.width = FLT_MIN;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = FLT_MAX;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = 90.0f;
    rect.width = 10.0f;
    rect.y = 49.0f;
    rect.height = 2.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);

    rect.x = -0.0f;
    rect.y = -0.0f;
    rect.width = 100.0f;
    rect.height = 50.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_OK);
    EXPECT(openagc_frame_clear(device, color), OPENAGC_OK);
    rect.x = NAN;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_OUT_OF_RANGE);
    rect.x = 0.0f;
    EXPECT(openagc_frame_rect(device, &rect), OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_frame_clear(device, color), OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_frame_present(device), OPENAGC_OK);
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_OK);
    CHECK(view.command_count == 2u);
    CHECK(view.commands[0].type == OPENAGC_COMMAND_RECTANGLE);
    CHECK(view.commands[1].type == OPENAGC_COMMAND_CLEAR);
    CHECK(memcmp(&view.commands[0].data.rectangle.x, &zero, sizeof(zero)) == 0);
    CHECK(memcmp(&view.commands[0].data.rectangle.y, &zero, sizeof(zero)) == 0);

    EXPECT(openagc_frame_begin(device), OPENAGC_OK);
    EXPECT(openagc_frame_clear(device, color), OPENAGC_OK);
    EXPECT(openagc_frame_present(device), OPENAGC_OK);
    EXPECT(openagc_device_get_last_frame(device, &view), OPENAGC_OK);
    CHECK(view.command_count == 1u && view.frame_number == 2u);
    EXPECT(openagc_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_unqualified_firmware(void)
{
    openagc_context_desc desc = OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_PS5);
    openagc_context *context = NULL;

    desc.firmware_major = 9u;
    desc.firmware_minor = 40u;
    EXPECT(openagc_context_create(&desc, &context),
           OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == NULL);
    desc.firmware_major = 0u;
    desc.firmware_minor = 0u;
    EXPECT(openagc_context_create(&desc, &context),
           OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == NULL);
    desc.firmware_major = 11u;
    desc.firmware_minor = 99u;
    EXPECT(openagc_context_create(&desc, &context),
           OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    CHECK(context == NULL);
    return 0;
}

int main(void)
{
    if (test_lifecycle_and_limits() != 0 || test_recording() != 0 ||
        test_geometry_and_capacity() != 0 || test_unqualified_firmware() != 0) {
        return 1;
    }
    puts("OpenAGC host tests passed");
    return 0;
}
