/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/driver.h"
#include "openagc/pm4_compute_fw940.h"
#include "openagc/pm4_copy_data_fw940.h"
#include "openagc/pm4_context_regs_gfx10.h"
#include "openagc/pm4_draw_fw940.h"
#include "openagc/pm4_ngg_draw_fw940.h"
#include "openagc/pm4_graphics_fw940.h"
#include "openagc/pm4_write_fw940.h"
#include "openagc/store_const_code.h"
#include "openagc/store_span_code.h"
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

static int make_gpu(uint64_t budget, openagc_context **context,
                    openagc_gpu_device **device)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc gpu_desc = OPENAGC_GPU_DEVICE_DESC_INIT;

    gpu_desc.memory_budget_bytes = budget;
    EXPECT(openagc_context_create(&context_desc, context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(*context, &gpu_desc, device), OPENAGC_OK);
    return 0;
}

static int test_device_and_memory(void)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc gpu_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_gpu_capabilities capabilities = OPENAGC_GPU_CAPABILITIES_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_memory *other = NULL;
    openagc_gpu_memory *candidate = NULL;
    uint8_t bytes[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
    uint8_t readback[8] = { 0u };

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    gpu_desc.struct_size--;
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, &device),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    gpu_desc.struct_size++;
    gpu_desc.api_version++;
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, &device),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    gpu_desc.api_version = OPENAGC_GPU_API_VERSION;
    gpu_desc.memory_budget_bytes = 0u;
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, &device),
           OPENAGC_ERROR_OUT_OF_RANGE);
    gpu_desc.memory_budget_bytes = 67108868u;
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, &device),
           OPENAGC_ERROR_OUT_OF_RANGE);
    gpu_desc.memory_budget_bytes = 128u;
    EXPECT(openagc_gpu_device_create(context, &gpu_desc, &device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_ERROR_BAD_STATE);

    capabilities.struct_size--;
    EXPECT(openagc_gpu_device_get_capabilities(device, &capabilities),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    capabilities.struct_size++;
    EXPECT(openagc_gpu_device_get_capabilities(device, &capabilities), OPENAGC_OK);
    CHECK(capabilities.backend == OPENAGC_BACKEND_HOST_REFERENCE);
    CHECK(capabilities.gpu_execution == 0u && capabilities.video_output == 0u);
    CHECK(capabilities.host_copy_simulation == 1u);
    CHECK(capabilities.reference_pm4_encoding == 1u);
    CHECK(capabilities.memory_budget_bytes == 128u);
    CHECK(capabilities.max_allocation_bytes == 16777216u);
    CHECK(capabilities.max_allocations == 64u);
    CHECK(capabilities.max_copy_bytes == 0x1ffffcu);
    CHECK(capabilities.max_queues == 1u);

    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    memory_desc.struct_size--;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    memory_desc.struct_size++;
    memory_desc.memory_class = OPENAGC_GPU_MEMORY_DEVICE_LOCAL;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    memory_desc.memory_class = OPENAGC_GPU_MEMORY_HOST_REFERENCE;
    memory_desc.size_bytes = 0u;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory),
           OPENAGC_ERROR_OUT_OF_RANGE);
    memory_desc.size_bytes = 16777220u;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory),
           OPENAGC_ERROR_OUT_OF_RANGE);
    memory_desc.size_bytes = 64u;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &other), OPENAGC_OK);
    CHECK(memory != NULL && other != NULL);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &candidate),
           OPENAGC_ERROR_CAPACITY);
    CHECK(candidate == NULL);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_memory_read(memory, 0u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(readback[0] == 0u && readback[7] == 0u);
    EXPECT(openagc_gpu_memory_write(memory, 60u, bytes, sizeof(bytes)),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_memory_write(memory, UINT64_MAX, bytes, sizeof(bytes)),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_memory_write(memory, 0u, bytes, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_memory_write(memory, 4u, bytes, sizeof(bytes)), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(memory, 4u, readback, sizeof(readback)), OPENAGC_OK);
    CHECK(memcmp(readback, bytes, sizeof(bytes)) == 0);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_memory_destroy(other), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_ownership_and_binding(void)
{
    openagc_context *context_a = NULL;
    openagc_context *context_b = NULL;
    openagc_gpu_device *device_a = NULL;
    openagc_gpu_device *device_b = NULL;
    openagc_gpu_memory *memory_a = NULL;
    openagc_gpu_memory *memory_b = NULL;
    openagc_gpu_buffer *buffer_a = NULL;
    openagc_gpu_buffer *buffer_b = NULL;
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(128u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_gpu_queue *queue = NULL;
    openagc_gpu_queue *other_queue = NULL;

    CHECK(make_gpu(256u, &context_a, &device_a) == 0);
    CHECK(make_gpu(256u, &context_b, &device_b) == 0);
    EXPECT(openagc_gpu_memory_allocate(device_a, &memory_desc, &memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device_b, &memory_desc, &memory_b), OPENAGC_OK);
    buffer_desc.usage = 0u;
    EXPECT(openagc_gpu_buffer_create(device_a, &buffer_desc, &buffer_a),
           OPENAGC_ERROR_OUT_OF_RANGE);
    buffer_desc.usage = 4u;
    EXPECT(openagc_gpu_buffer_create(device_a, &buffer_desc, &buffer_a),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    buffer_desc.usage = OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT;
    EXPECT(openagc_gpu_buffer_create(device_a, &buffer_desc, &buffer_a), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device_b, &buffer_desc, &buffer_b), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_b, 0u),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_a, 1u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_a, 68u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_a, 64u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_a, 0u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_buffer_unbind_memory(buffer_a), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_unbind_memory(buffer_a), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer_a, memory_a, 64u), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_a), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_device_destroy(device_a), OPENAGC_ERROR_BUSY);

    queue_desc.family = OPENAGC_GPU_QUEUE_GRAPHICS;
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &queue),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    queue_desc.family = OPENAGC_GPU_QUEUE_COMPUTE;
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &queue),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    queue_desc.family = 99u;
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &queue),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    queue_desc.family = OPENAGC_GPU_QUEUE_COPY;
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &queue), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &other_queue),
           OPENAGC_ERROR_CAPACITY);
    CHECK(other_queue == NULL);

    EXPECT(openagc_gpu_queue_destroy(queue), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer_a), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer_b), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_b), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_a), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_b), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_a), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_b), OPENAGC_OK);
    return 0;
}

static int test_packet_words_and_fence(void)
{
    /* Console-observed FW9.40 layout: 7 DMA + 8 EOP + 16 NOP = 31 words. */
    const uint32_t expected_dma[7] = {
        0xc0055002u, 0x8c00c000u, 0x00000000u, 0x00000001u,
        0x00001000u, 0x00000001u, 0x00000010u
    };
    const uint32_t expected_submission[31] = {
        0xc0055002u, 0x8c00c000u, 0x00000000u, 0x00000001u,
        0x00001000u, 0x00000001u, 0x00000010u,
        0xc0064900u, 0x06703514u, 0x20000000u, 0x00002000u,
        0x00000001u, 0x00000001u, 0u, 0u,
        0xc0001000u, 0u, 0xc0001000u, 0u, 0xc0001000u, 0u, 0xc0001000u, 0u,
        0xc0001000u, 0u, 0xc0001000u, 0u, 0xc0001000u, 0u, 0xc0001000u, 0u
    };
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_buffer_desc source_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_buffer_desc destination_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_command_buffer_desc command_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(2u, 14u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_gpu_memory *source_memory = NULL;
    openagc_gpu_memory *destination_memory = NULL;
    openagc_gpu_buffer *source = NULL;
    openagc_gpu_buffer *destination = NULL;
    openagc_gpu_command_buffer *command_buffer = NULL;
    openagc_gpu_queue *queue = NULL;
    openagc_gpu_fence *fence = NULL;
    openagc_gpu_recording_view recording = OPENAGC_GPU_RECORDING_VIEW_INIT;
    openagc_gpu_submission_view submission = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    uint8_t data[16];
    uint8_t copied[16] = { 0u };
    uint32_t i;

    CHECK(make_gpu(256u, &context, &device) == 0);
    for (i = 0u; i < 16u; ++i) {
        data[i] = (uint8_t)(i + 1u);
    }
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &source_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &destination_memory),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &source_desc, &source), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &destination_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(source, source_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(destination, destination_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_write(source_memory, 0u, data, sizeof(data)), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(source, 0u, copied, sizeof(data)), OPENAGC_OK);
    CHECK(memcmp(copied, data, sizeof(data)) == 0);
    EXPECT(openagc_gpu_buffer_read(source, 64u, copied, 4u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_queue_create(device, &queue_desc, &queue), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_fence_create(device, &fence), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_ERROR_NOT_READY);
    CHECK(fence_info.signaled == 0u && fence_info.submission_id == 0u);
    EXPECT(openagc_gpu_fence_reset(fence), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission),
           OPENAGC_ERROR_BAD_STATE);

    EXPECT(openagc_gpu_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &recording),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 0u,
                                          destination, 0u, sizeof(data)), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(destination_memory, 0u, copied, sizeof(copied)),
           OPENAGC_OK);
    CHECK(copied[0] == 0u && copied[15] == 0u);
    EXPECT(openagc_gpu_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &recording),
           OPENAGC_OK);
    CHECK(recording.command_count == 1u && recording.word_count == 7u);
    CHECK(memcmp(recording.words, expected_dma, sizeof(expected_dma)) == 0);
    EXPECT(openagc_gpu_buffer_destroy(source), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_memory_destroy(source_memory), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_gpu_queue_submit(queue, command_buffer, fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    CHECK(submission.submission_id == 1u && submission.gpu_submitted == 0u);
    CHECK(submission.word_count == 31u);
    CHECK(memcmp(submission.words, expected_submission, sizeof(expected_submission)) == 0);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_OK);
    CHECK(fence_info.signaled == 1u && fence_info.submission_id == 1u);
    CHECK(fence_info.expected_value == 1u && fence_info.observed_value == 1u);
    EXPECT(openagc_gpu_memory_read(destination_memory, 0u, copied, sizeof(copied)),
           OPENAGC_OK);
    CHECK(memcmp(copied, data, sizeof(data)) == 0);
    EXPECT(openagc_gpu_queue_submit(queue, command_buffer, fence), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    CHECK(memcmp(submission.words, expected_submission, sizeof(expected_submission)) == 0);

    EXPECT(openagc_gpu_fence_reset(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_poll(fence, &fence_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gpu_queue_submit(queue, command_buffer, fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission), OPENAGC_OK);
    CHECK(submission.submission_id == 2u && submission.word_count == 31u);
    for (i = 0u; i < submission.word_count; ++i) {
        CHECK(submission.words[i] == (i == 12u ? 2u : expected_submission[i]));
    }
    EXPECT(openagc_gpu_command_buffer_begin(command_buffer), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &recording),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_buffer_destroy(source), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(destination), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(source_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(destination_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_destroy(fence), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_destroy(queue), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_rejected_commands_and_aliasing(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(256u);
    openagc_gpu_buffer_desc source_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_buffer_desc destination_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_buffer_desc shared_desc = OPENAGC_GPU_BUFFER_DESC_INIT(
        128u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT | OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_command_buffer_desc command_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(1u, 7u);
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *source = NULL;
    openagc_gpu_buffer *destination = NULL;
    openagc_gpu_buffer *shared = NULL;
    openagc_gpu_buffer *unbound = NULL;
    openagc_gpu_command_buffer *command_buffer = NULL;
    openagc_gpu_recording_view view = OPENAGC_GPU_RECORDING_VIEW_INIT;

    CHECK(make_gpu(512u, &context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &source_desc, &source), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &destination_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &shared_desc, &shared), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &source_desc, &unbound), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(source, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(destination, memory, 64u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(shared, memory, 0u), OPENAGC_OK);
    command_desc.max_words = 6u;
    EXPECT(openagc_gpu_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_ERROR_OUT_OF_RANGE);
    command_desc.max_words = 7u;
    EXPECT(openagc_gpu_command_buffer_create(device, &command_desc, &command_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 0u,
                                          destination, 0u, 16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 0u,
                                          destination, 0u, 0u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 1u,
                                          destination, 0u, 16u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, UINT64_MAX,
                                          destination, 0u, 16u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 60u,
                                          destination, 0u, 8u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 0u,
                                          destination, 0u, 0x200000u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, destination, 0u,
                                          source, 0u, 16u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, unbound, 0u,
                                          destination, 0u, 16u),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, shared, 0u,
                                          shared, 4u, 8u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 0u,
                                          destination, 0u, 16u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, source, 16u,
                                          destination, 16u, 16u),
           OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_gpu_command_buffer_end(command_buffer), OPENAGC_OK);
    view.struct_size--;
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &view),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    view.struct_size++;
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &view), OPENAGC_OK);
    CHECK(view.command_count == 1u && view.word_count == 7u);
    EXPECT(openagc_gpu_buffer_destroy(source), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(source), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(destination), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(shared), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(unbound), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_cross_device_and_alias_copy(void)
{
    openagc_context *context_a = NULL;
    openagc_context *context_b = NULL;
    openagc_gpu_device *device_a = NULL;
    openagc_gpu_device *device_b = NULL;
    openagc_gpu_memory_desc memory_a_desc = OPENAGC_GPU_MEMORY_DESC_INIT(256u);
    openagc_gpu_memory_desc memory_b_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_buffer_desc shared_desc = OPENAGC_GPU_BUFFER_DESC_INIT(
        128u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT | OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_buffer_desc other_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_command_buffer_desc command_desc =
        OPENAGC_GPU_COMMAND_BUFFER_DESC_INIT(2u, 7u);
    openagc_gpu_queue_desc queue_desc = OPENAGC_GPU_QUEUE_DESC_INIT;
    openagc_gpu_memory *memory_a = NULL;
    openagc_gpu_memory *memory_b = NULL;
    openagc_gpu_buffer *shared = NULL;
    openagc_gpu_buffer *other = NULL;
    openagc_gpu_command_buffer *command_buffer = NULL;
    openagc_gpu_queue *queue = NULL;
    openagc_gpu_fence *fence_a = NULL;
    openagc_gpu_fence *fence_b = NULL;
    openagc_gpu_recording_view recording = OPENAGC_GPU_RECORDING_VIEW_INIT;
    openagc_gpu_submission_view submission = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_gpu_fence_info fence_info = OPENAGC_GPU_FENCE_INFO_INIT;
    uint8_t source[16];
    uint8_t destination[16] = { 0u };
    uint32_t i;

    CHECK(make_gpu(512u, &context_a, &device_a) == 0);
    CHECK(make_gpu(128u, &context_b, &device_b) == 0);
    EXPECT(openagc_gpu_memory_allocate(device_a, &memory_a_desc, &memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device_b, &memory_b_desc, &memory_b), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device_a, &shared_desc, &shared), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device_b, &other_desc, &other), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(shared, memory_a, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(other, memory_b, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_create(device_a, &command_desc, &command_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_queue_create(device_a, &queue_desc, &queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_create(device_a, &fence_a), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_create(device_b, &fence_b), OPENAGC_OK);
    for (i = 0u; i < 16u; ++i) {
        source[i] = (uint8_t)(0xa0u + i);
    }
    EXPECT(openagc_gpu_memory_write(memory_a, 0u, source, sizeof(source)), OPENAGC_OK);

    EXPECT(openagc_gpu_command_buffer_begin(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, other, 0u,
                                          shared, 64u, 16u), OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, shared, 0u,
                                          shared, 4u, 8u), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, shared, 0u,
                                          shared, 64u, 16u), OPENAGC_OK);
    EXPECT(openagc_gpu_command_copy_buffer(command_buffer, shared, 16u,
                                          shared, 80u, 16u), OPENAGC_ERROR_CAPACITY);
    EXPECT(openagc_gpu_command_buffer_end(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_get_recording(command_buffer, &recording),
           OPENAGC_OK);
    CHECK(recording.command_count == 1u && recording.word_count == 7u);

    EXPECT(openagc_gpu_queue_submit(queue, command_buffer, fence_b),
           OPENAGC_ERROR_OWNERSHIP);
    EXPECT(openagc_gpu_queue_get_last_submission(queue, &submission),
           OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_memory_read(memory_a, 64u, destination, sizeof(destination)),
           OPENAGC_OK);
    CHECK(destination[0] == 0u && destination[15] == 0u);
    EXPECT(openagc_gpu_fence_poll(fence_b, &fence_info), OPENAGC_ERROR_NOT_READY);
    EXPECT(openagc_gpu_queue_submit(queue, command_buffer, fence_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_read(memory_a, 64u, destination, sizeof(destination)),
           OPENAGC_OK);
    CHECK(memcmp(source, destination, sizeof(source)) == 0);
    EXPECT(openagc_gpu_buffer_destroy(shared), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_command_buffer_reset(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(shared), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(other), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_a), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory_b), OPENAGC_OK);
    EXPECT(openagc_gpu_command_buffer_destroy(command_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_queue_destroy(queue), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_destroy(fence_a), OPENAGC_OK);
    EXPECT(openagc_gpu_fence_destroy(fence_b), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_a), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device_b), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_a), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context_b), OPENAGC_OK);
    return 0;
}

static int test_write_data_words(void)
{
    uint32_t one[OPENAGC_PM4_WRITE_DATA_ONE_WORDS];
    uint32_t eop[OPENAGC_PM4_WRITE_DATA_EOP_WORDS];
    uint32_t burst[OPENAGC_PM4_WRITE_DATA_WORDS(3u)];
    const uint32_t payload[3] = { 0xa5a5a5a5u, 0x11111111u, 0x22222222u };
    const uint64_t destination = UINT64_C(0x100001004); /* low bits cleared */
    const uint64_t marker = UINT64_C(0x100002000);

    openagc_pm4_encode_write_data_one(destination, payload[0], one);
    CHECK(one[0] == 0xc0033700u);
    CHECK(one[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
    CHECK(OPENAGC_PM4_WRITE_DATA_CONTROL == 0x00100500u);
    CHECK(one[2] == (uint32_t)(destination & ~UINT64_C(3)));
    CHECK(one[3] == (uint32_t)(destination >> 32));
    CHECK(one[4] == payload[0]);
    CHECK(OPENAGC_PM4_WRITE_DATA_ONE_WORDS == 5u);

    openagc_pm4_encode_write_data(destination, payload, 3u, burst);
    CHECK(burst[0] == 0xc0053700u);
    CHECK(burst[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
    CHECK(burst[2] == one[2] && burst[3] == one[3]);
    CHECK(burst[4] == payload[0] && burst[5] == payload[1] && burst[6] == payload[2]);
    CHECK(OPENAGC_PM4_WRITE_DATA_WORDS(3u) == 7u);

    openagc_pm4_encode_write_data_eop(destination, payload[0], 1u, marker, eop);
    CHECK(eop[0] == 0xc0033700u && eop[4] == payload[0]);
    CHECK(eop[5] == OPENAGC_PM4_EOP_HEADER);
    CHECK(eop[8] == (uint32_t)marker);
    CHECK(eop[10] == 1u);
    CHECK(eop[13] == OPENAGC_PM4_NOP_HEADER);
    CHECK(eop[28] == 0u);
    CHECK(OPENAGC_PM4_WRITE_DATA_EOP_WORDS == 29u);

    {
        uint32_t rows[OPENAGC_PM4_WRITE_DATA_STEP_F_EOP_WORDS];
        const uint64_t base = UINT64_C(0x100001000);
        const uint64_t marker = UINT64_C(0x100002000);
        const uint32_t pitch = 64u;

        openagc_pm4_encode_write_data_rows_eop(base, pitch, payload[0], 8u, 2u, 1u, marker,
                                               rows);
        CHECK(OPENAGC_PM4_WRITE_DATA_STEP_F_EOP_WORDS == 48u);
        CHECK(rows[0] == 0xc00a3700u);
        CHECK(rows[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
        CHECK(rows[2] == (uint32_t)base);
        CHECK(rows[4] == payload[0] && rows[11] == payload[0]);
        CHECK(rows[12] == 0xc00a3700u);
        CHECK(rows[14] == (uint32_t)(base + pitch));
        CHECK(rows[24] == OPENAGC_PM4_EOP_HEADER);
        CHECK(rows[27] == (uint32_t)marker);
        CHECK(rows[29] == 1u);
    }
    {
        uint32_t wide[OPENAGC_PM4_WRITE_DATA_STEP_G_EOP_WORDS];
        const uint64_t base = UINT64_C(0x100001000);
        const uint64_t marker = UINT64_C(0x100002000);
        const uint32_t pitch = 128u;

        openagc_pm4_encode_write_data_rows_eop(base, pitch, payload[0], 16u, 2u, 1u, marker,
                                               wide);
        CHECK(OPENAGC_PM4_WRITE_DATA_STEP_G_EOP_WORDS == 64u);
        CHECK(wide[0] == 0xc0123700u);
        CHECK(wide[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
        CHECK(wide[2] == (uint32_t)base);
        CHECK(wide[4] == payload[0] && wide[19] == payload[0]);
        CHECK(wide[20] == 0xc0123700u);
        CHECK(wide[22] == (uint32_t)(base + pitch));
        CHECK(wide[40] == OPENAGC_PM4_EOP_HEADER);
        CHECK(wide[43] == (uint32_t)marker);
        CHECK(wide[45] == 1u);
    }
    {
        uint32_t max_rows[OPENAGC_PM4_WRITE_DATA_STEP_M_EOP_WORDS];
        const uint64_t base = UINT64_C(0x100001000);
        const uint64_t marker = UINT64_C(0x100002000);
        const uint32_t pitch = 64u;
        uint32_t last_row;

        openagc_pm4_encode_write_data_rows_eop(
            base, pitch, payload[0], OPENAGC_PM4_WRITE_DATA_STEP_M_ROW_DWORDS,
            OPENAGC_PM4_WRITE_DATA_STEP_M_ROWS, 1u, marker, max_rows);
        CHECK(OPENAGC_PM4_WRITE_DATA_STEP_M_EOP_WORDS == 184u);
        CHECK(max_rows[0] == 0xc0123700u);
        CHECK(max_rows[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
        CHECK(max_rows[2] == (uint32_t)base);
        last_row = 7u * OPENAGC_PM4_WRITE_DATA_WORDS(16u);
        CHECK(max_rows[last_row] == 0xc0123700u);
        CHECK(max_rows[last_row + 2u] == (uint32_t)(base + 7u * pitch));
        CHECK(max_rows[160] == OPENAGC_PM4_EOP_HEADER);
        CHECK(max_rows[163] == (uint32_t)marker);
        CHECK(max_rows[165] == 1u);
    }
    {
        uint32_t grid[OPENAGC_PM4_WRITE_DATA_STEP_N_EOP_WORDS];
        const uint64_t base = UINT64_C(0x100001000);
        const uint64_t marker = UINT64_C(0x100002000);
        const uint32_t pitch = 128u;
        uint32_t second;

        openagc_pm4_encode_write_data_grid_eop(
            base, pitch, payload[0], OPENAGC_PM4_WRITE_DATA_STEP_N_COL_DWORDS,
            OPENAGC_PM4_WRITE_DATA_STEP_N_COLS, OPENAGC_PM4_WRITE_DATA_STEP_N_ROWS, 1u,
            marker, grid);
        CHECK(OPENAGC_PM4_WRITE_DATA_STEP_N_EOP_WORDS == 344u);
        CHECK(grid[0] == 0xc0123700u);
        CHECK(grid[2] == (uint32_t)base);
        second = OPENAGC_PM4_WRITE_DATA_WORDS(16u);
        CHECK(grid[second] == 0xc0123700u);
        CHECK(grid[second + 2u] == (uint32_t)(base + 64u));
        CHECK(grid[320] == OPENAGC_PM4_EOP_HEADER);
        CHECK(grid[323] == (uint32_t)marker);
        CHECK(grid[325] == 1u);
    }
    {
        uint32_t composite[OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS];
        const uint64_t source = UINT64_C(0x100000000);
        const uint64_t destination = UINT64_C(0x100001000);
        const uint64_t marker = UINT64_C(0x100002000);

        openagc_pm4_encode_dma_write_data_eop(source, destination, 64u, payload[0], 4u, 1u,
                                              marker, composite);
        CHECK(OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS == 39u);
        CHECK(composite[0] == OPENAGC_PM4_DMA_HEADER);
        CHECK(composite[2] == (uint32_t)source);
        CHECK(composite[4] == (uint32_t)destination);
        CHECK(composite[6] == 64u);
        CHECK(composite[7] == 0xc0063700u);
        CHECK(composite[8] == OPENAGC_PM4_WRITE_DATA_CONTROL);
        CHECK(composite[11] == payload[0] && composite[14] == payload[0]);
        CHECK(composite[15] == OPENAGC_PM4_EOP_HEADER);
        CHECK(composite[18] == (uint32_t)marker);
        CHECK(composite[20] == 1u);
    }
    return 0;
}

static int test_compute_store_const_words(void)
{
    uint32_t words[OPENAGC_PM4_COMPUTE_STORE_WORDS];
    const uint64_t code = UINT64_C(0x100000000);
    const uint64_t destination = UINT64_C(0x100001000);
    const uint64_t marker = UINT64_C(0x100002000);

    openagc_pm4_encode_compute_store_const(code, destination, 1u, marker, words);
    CHECK(words[0] == 0xc0027601u);
    CHECK(words[1] == OPENAGC_SH_COMPUTE_PGM_LO);
    CHECK(words[2] == (uint32_t)(code >> 8));
    CHECK(words[3] == (uint32_t)(code >> 40));
    CHECK(words[4] == 0xc0027601u);
    CHECK(words[5] == OPENAGC_SH_COMPUTE_PGM_RSRC1);
    CHECK(words[6] == OPENAGC_COMPUTE_RSRC1_W32);
    CHECK(words[7] == OPENAGC_COMPUTE_RSRC2_USER4);
    CHECK(words[8] == 0xc0017601u);
    CHECK(words[9] == OPENAGC_SH_COMPUTE_PGM_RSRC3);
    CHECK(words[10] == 0u);
    CHECK(words[11] == 0xc0037601u);
    CHECK(words[12] == OPENAGC_SH_COMPUTE_NUM_THREAD_X);
    CHECK(words[13] == 1u && words[14] == 1u && words[15] == 1u);
    CHECK(words[16] == 0xc0047601u);
    CHECK(words[17] == OPENAGC_SH_COMPUTE_USER_DATA_0);
    CHECK(words[18] == 0u && words[19] == 0u);
    CHECK(words[20] == (uint32_t)destination);
    CHECK(words[21] == (uint32_t)(destination >> 32));
    CHECK(words[22] == 0xc0031501u);
    CHECK(words[23] == 1u && words[24] == 1u && words[25] == 1u);
    CHECK(words[26] == OPENAGC_PM4_DISPATCH_INITIATOR);
    CHECK(words[27] == OPENAGC_PM4_EOP_HEADER);
    CHECK(words[30] == (uint32_t)marker);
    CHECK(words[32] == 1u);
    CHECK(words[35] == OPENAGC_PM4_NOP_HEADER);
    CHECK(words[50] == 0u);
    {
        uint32_t span[OPENAGC_PM4_COMPUTE_STORE_WORDS];

        openagc_pm4_encode_compute_store_span(code, destination, 1u, marker, span);
        CHECK(span[0] == words[0]);
        CHECK(span[12] == OPENAGC_SH_COMPUTE_NUM_THREAD_X);
        CHECK(span[13] == 8u && span[14] == 1u && span[15] == 1u);
        CHECK(span[26] == OPENAGC_PM4_DISPATCH_INITIATOR);
        CHECK(span[27] == OPENAGC_PM4_EOP_HEADER);
    }
    return 0;
}

static int test_host_store_const_apply(void)
{
    openagc_context_desc context_desc = OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(16u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(16u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_gpu_buffer_desc copy_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(16u, OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *buffer = NULL;
    openagc_gpu_buffer *copy_only = NULL;
    openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    uint32_t word = 0u;

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &copy_desc, &copy_only), OPENAGC_OK);
    EXPECT(openagc_gpu_host_store_const(device, buffer, 0u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(copy_only, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_host_store_const(device, copy_only, 0u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gpu_host_store_const(device, buffer, 2u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_host_store_const(NULL, buffer, 0u), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_host_store_const(device, buffer, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, &word, sizeof(word)), OPENAGC_OK);
    CHECK(word == OPENAGC_STORE_CONST_VALUE);
    EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_OK);
    CHECK(view.gpu_submitted == 0u && view.word_count == OPENAGC_PM4_COMPUTE_STORE_WORDS);
    CHECK(view.submission_id == 1u);
    CHECK(view.words[26] == OPENAGC_PM4_DISPATCH_INITIATOR);
    CHECK(view.words[27] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_host_store_const(device, buffer, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_OK);
    CHECK(view.submission_id == 2u);

    EXPECT(openagc_gpu_host_store_span(device, buffer, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
    {
        openagc_gpu_memory_desc span_mem = OPENAGC_GPU_MEMORY_DESC_INIT(32u);
        openagc_gpu_buffer_desc span_desc =
            OPENAGC_GPU_BUFFER_DESC_INIT(32u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
        openagc_gpu_memory *span_memory = NULL;
        openagc_gpu_buffer *span_buffer = NULL;
        uint32_t span_words[8];
        uint32_t i;

        EXPECT(openagc_gpu_memory_allocate(device, &span_mem, &span_memory), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_create(device, &span_desc, &span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_bind_memory(span_buffer, span_memory, 0u), OPENAGC_OK);
        EXPECT(openagc_gpu_host_store_span(device, span_buffer, 0u), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_read(span_buffer, 0u, span_words, sizeof(span_words)),
               OPENAGC_OK);
        for (i = 0u; i < 8u; ++i) {
            CHECK(span_words[i] == OPENAGC_STORE_SPAN_VALUE);
        }
        EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_OK);
        CHECK(view.words[13] == 8u);
        EXPECT(openagc_gpu_host_store_span2(device, span_buffer, 0u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_gpu_buffer_destroy(span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_memory_destroy(span_memory), OPENAGC_OK);
    }
    {
        openagc_gpu_memory_desc span_mem = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
        openagc_gpu_buffer_desc span_desc =
            OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
        openagc_gpu_memory *span_memory = NULL;
        openagc_gpu_buffer *span_buffer = NULL;
        uint32_t span_words[16];
        uint32_t i;

        EXPECT(openagc_gpu_memory_allocate(device, &span_mem, &span_memory), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_create(device, &span_desc, &span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_bind_memory(span_buffer, span_memory, 0u), OPENAGC_OK);
        EXPECT(openagc_gpu_host_store_span2(device, span_buffer, 0u), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_read(span_buffer, 0u, span_words, sizeof(span_words)),
               OPENAGC_OK);
        for (i = 0u; i < 16u; ++i) {
            CHECK(span_words[i] == OPENAGC_STORE_SPAN_VALUE);
        }
        EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_OK);
        CHECK(view.word_count == OPENAGC_PM4_COMPUTE_STORE_SPAN2_WORDS);
        CHECK(view.words[13] == 8u);
        EXPECT(openagc_gpu_buffer_destroy(span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_memory_destroy(span_memory), OPENAGC_OK);
    }
    {
        openagc_gpu_memory_desc span_mem = OPENAGC_GPU_MEMORY_DESC_INIT(128u);
        openagc_gpu_buffer_desc span_desc =
            OPENAGC_GPU_BUFFER_DESC_INIT(128u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
        openagc_gpu_memory *span_memory = NULL;
        openagc_gpu_buffer *span_buffer = NULL;
        uint32_t span_words[32];
        uint32_t i;

        EXPECT(openagc_gpu_memory_allocate(device, &span_mem, &span_memory), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_create(device, &span_desc, &span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_bind_memory(span_buffer, span_memory, 0u), OPENAGC_OK);
        EXPECT(openagc_gpu_host_store_span_n(device, span_buffer, 0u, 0u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_gpu_host_store_span_n(device, span_buffer, 0u, 9u),
               OPENAGC_ERROR_OUT_OF_RANGE);
        EXPECT(openagc_gpu_host_store_span_n(device, span_buffer, 0u, 4u), OPENAGC_OK);
        EXPECT(openagc_gpu_buffer_read(span_buffer, 0u, span_words, sizeof(span_words)),
               OPENAGC_OK);
        for (i = 0u; i < 32u; ++i) {
            CHECK(span_words[i] == OPENAGC_STORE_SPAN_VALUE);
        }
        EXPECT(openagc_gpu_device_get_last_compute(device, &view), OPENAGC_OK);
        CHECK(view.word_count == OPENAGC_PM4_COMPUTE_STORE_SPAN4_WORDS);
        CHECK(view.words[13] == 8u);
        EXPECT(openagc_gpu_buffer_destroy(span_buffer), OPENAGC_OK);
        EXPECT(openagc_gpu_memory_destroy(span_memory), OPENAGC_OK);
    }

    EXPECT(openagc_gpu_buffer_destroy(copy_only), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_write_data_apply(void)
{
    openagc_context_desc context_desc = OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(16u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(16u, OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_gpu_buffer_desc shader_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(16u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *buffer = NULL;
    openagc_gpu_buffer *shader_only = NULL;
    openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    uint32_t word = 0u;
    const uint32_t pattern = 0xa5a5a5a5u;

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &shader_desc, &shader_only), OPENAGC_OK);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 0u, pattern, 1u), OPENAGC_ERROR_BAD_STATE);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(shader_only, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_host_write_data(device, shader_only, 0u, pattern, 1u),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 2u, pattern, 1u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 0u, pattern, 0u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 0u, pattern, 17u), OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_host_write_data(NULL, buffer, 0u, pattern, 1u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 0u, pattern, 1u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(buffer, 0u, &word, sizeof(word)), OPENAGC_OK);
    CHECK(word == pattern);
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_OK);
    CHECK(view.gpu_submitted == 0u && view.word_count == OPENAGC_PM4_WRITE_DATA_EOP_WORDS);
    CHECK(view.submission_id == 1u);
    CHECK(view.words[0] == 0xc0033700u);
    CHECK(view.words[1] == OPENAGC_PM4_WRITE_DATA_CONTROL);
    CHECK(view.words[4] == pattern);
    CHECK(view.words[5] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_host_write_data(device, buffer, 0u, pattern, 4u), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_OK);
    CHECK(view.word_count == OPENAGC_PM4_WRITE_DATA_WORDS(4u) + OPENAGC_PM4_EOP_WITH_NOP_WORDS);
    CHECK(view.words[0] == 0xc0063700u);
    CHECK(view.words[4] == pattern && view.words[7] == pattern);
    CHECK(view.words[8] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_buffer_destroy(shader_only), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_dma_write_data_apply(void)
{
    openagc_context_desc context_desc = OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(128u);
    openagc_gpu_buffer_desc source_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_gpu_buffer_desc dest_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT);
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory *memory = NULL;
    openagc_gpu_buffer *source = NULL;
    openagc_gpu_buffer *destination = NULL;
    openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    uint32_t words[16];
    uint32_t i;
    const uint32_t dma_value = 0x11111111u;
    const uint32_t write_value = 0xa5a5a5a5u;

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &source_desc, &source), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &dest_desc, &destination), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(source, memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(destination, memory, 64u), OPENAGC_OK);
    for (i = 0u; i < 16u; ++i) {
        words[i] = dma_value;
    }
    EXPECT(openagc_gpu_buffer_write(source, 0u, words, 64u), OPENAGC_OK);
    EXPECT(openagc_gpu_host_dma_write_data(device, source, 0u, destination, 0u, 64u, write_value,
                                           4u),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_read(destination, 0u, words, 64u), OPENAGC_OK);
    for (i = 0u; i < 4u; ++i) {
        CHECK(words[i] == write_value);
    }
    for (i = 4u; i < 16u; ++i) {
        CHECK(words[i] == dma_value);
    }
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_OK);
    CHECK(view.gpu_submitted == 0u);
    CHECK(view.word_count == OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS);
    CHECK(view.words[0] == OPENAGC_PM4_DMA_HEADER);
    CHECK(view.words[7] == 0xc0063700u);
    CHECK(view.words[11] == write_value);
    CHECK(view.words[15] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_host_dma_write_data(device, source, 0u, destination, 0u, 64u, write_value,
                                           17u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_gpu_host_dma_write_data(NULL, source, 0u, destination, 0u, 64u, write_value, 4u),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_buffer_destroy(destination), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(source), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_host_graphics_register_eop(void)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    static const uint32_t offsets[] = { 72u, 73u };
    static const uint32_t values[] = { 0x200010u, 0u };
    uint32_t register_words[6];
    uint32_t count;

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    count = openagc_pm4_encode_psbc_shader_pairs(offsets, values, 2u, register_words);
    CHECK(count == 6u);
    EXPECT(openagc_gpu_host_graphics_register_eop(device, register_words, count), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_OK);
    CHECK(view.gpu_submitted == 0u);
    CHECK(view.word_count == count + OPENAGC_PM4_EOP_WITH_NOP_WORDS);
    CHECK(view.words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
    CHECK(view.words[count] == OPENAGC_PM4_EOP_HEADER);
    EXPECT(openagc_gpu_host_graphics_register_eop(NULL, register_words, count),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_host_graphics_register_eop(device, NULL, count),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static void test_fill_cb_manifest(openagc_cb_capture_manifest *manifest,
                                  openagc_cb_capture_kind kind,
                                  const uint32_t *words, uint32_t word_count)
{
    *manifest = OPENAGC_CB_CAPTURE_MANIFEST_INIT;
    manifest->kind = kind;
    manifest->firmware_id = OPENAGC_CB_CAPTURE_FW940_ID;
    manifest->word_count = word_count;
    openagc_sha256((const uint8_t *)words, (size_t)word_count * sizeof(uint32_t),
                   manifest->words_sha256);
}

static int test_cb_capture_refuse_and_accept(void)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_submission_view view = OPENAGC_GPU_SUBMISSION_VIEW_INIT;
    openagc_cb_capture_info info = OPENAGC_CB_CAPTURE_INFO_INIT;
    openagc_cb_capture_manifest manifest = OPENAGC_CB_CAPTURE_MANIFEST_INIT;
    /* Structural fixture only — not a real FW9.40 CB/DB IB cite. */
    static const uint32_t fixture_words[] = { OPENAGC_PM4_NOP_HEADER, 0u,
                                              OPENAGC_PM4_NOP_HEADER, 0u };
    uint32_t invent_words[8];
    uint32_t invent_count = 99u;
    uint32_t i;

    CHECK(OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT == 0u);
    EXPECT(openagc_cb_capture_encode_invent(OPENAGC_CB_CAPTURE_KIND_CB_BIND, invent_words,
                                            8u, &invent_count),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(invent_count == 0u);
    EXPECT(openagc_cb_capture_encode_invent(OPENAGC_CB_CAPTURE_KIND_DRAW, invent_words, 8u,
                                            &invent_count),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_CB_BIND, fixture_words, 4u);
    EXPECT(openagc_cb_capture_verify(&manifest, fixture_words), OPENAGC_OK);
    CHECK(openagc_cb_capture_evidence_qualified(&manifest) == 0u);

    manifest.words_sha256[0] ^= 0xffu;
    EXPECT(openagc_cb_capture_verify(&manifest, fixture_words), OPENAGC_ERROR_INTEGRITY);
    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_CB_BIND, fixture_words, 4u);
    manifest.firmware_id = 0u;
    EXPECT(openagc_cb_capture_verify(&manifest, fixture_words),
           OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_NONE, fixture_words, 4u);
    EXPECT(openagc_cb_capture_verify(&manifest, fixture_words),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    EXPECT(openagc_context_create(&context_desc, &context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(context, &device_desc, &device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_cb_capture_info(device, &info), OPENAGC_ERROR_NOT_READY);

    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_DRAW, fixture_words, 4u);
    EXPECT(openagc_gpu_host_cb_bind_from_capture(device, &manifest, fixture_words),
           OPENAGC_ERROR_NOT_READY);

    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_CB_BIND, fixture_words, 4u);
    EXPECT(openagc_gpu_host_cb_bind_from_capture(device, &manifest, fixture_words),
           OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_last_write(device, &view), OPENAGC_OK);
    CHECK(view.gpu_submitted == 0u);
    CHECK(view.word_count == 4u);
    for (i = 0u; i < 4u; ++i) {
        CHECK(view.words[i] == fixture_words[i]);
    }
    EXPECT(openagc_gpu_device_get_cb_capture_info(device, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_CB_CAPTURE_KIND_CB_BIND);
    CHECK(info.word_count == 4u);
    CHECK(info.capture_verified == 1u);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.gpu_submitted == 0u);

    test_fill_cb_manifest(&manifest, OPENAGC_CB_CAPTURE_KIND_DB_BIND, fixture_words, 4u);
    EXPECT(openagc_gpu_host_cb_bind_from_capture(device, &manifest, fixture_words),
           OPENAGC_OK);
    EXPECT(openagc_gpu_device_get_cb_capture_info(device, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_CB_CAPTURE_KIND_DB_BIND);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.gpu_submitted == 0u);

    EXPECT(openagc_gpu_host_cb_bind_from_capture(NULL, &manifest, fixture_words),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_ib_dump_parse_and_refuse_contracts(void)
{
    openagc_ib_dump_info info = OPENAGC_IB_DUMP_INFO_INIT;
    uint32_t words[8];
    uint32_t probe_words[OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS];
    uint32_t rt_words[OPENAGC_PM4_CTXREG_RT_EOP_WORDS];
    uint32_t bind_words[OPENAGC_PM4_CTXREG_CB_BIND_EOP_WORDS];
    uint32_t full_words[OPENAGC_PM4_CTXREG_CB_BIND_FULL_EOP_WORDS];
    uint32_t expected_bind[OPENAGC_GFX10_CB_BIND_COUNT];
    uint32_t full_dump_words[OPENAGC_GFX10_CB_BIND_COUNT];
    uint32_t mmio_probe_words[OPENAGC_PM4_MMIO_TILEMODE_PROBE_EOP_WORDS];
    uint32_t mmio_words[OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT];
    uint32_t probe_count;
    uint32_t rt_count;
    uint32_t bind_count;
    uint32_t full_count;
    uint32_t mmio_count;
    uint32_t word_index;
    uint32_t tile_index;
    uint32_t expected_base_lo;
    uint32_t expected_base_ext;
    const openagc_gfx10_reg_name *named;
    static const uint64_t owned_color_va = 0x123456789000ull;
    static const char dump_text[] =
        "openagc-ib-dump: tag=step-u fw=0x9400008 completed=1 words=4\n"
        "ib c0001000 00000000 c0001000 00000000\n";
    static const char ctxreg_text[] =
        "openagc-ib-dump: tag=ctxreg-cb fw=0x9400008 completed=1 words=8\n"
        "ib 11111111 22222222 33333333 44444444 55555555 66666666 77777777 0000000f\n";
    static const char ctxreg_abs_text[] =
        "openagc-ib-dump: tag=ctxreg-abs fw=0x9400008 completed=0 words=8\n"
        "ib cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc\n";
    static const char ctxreg_rt_text[] =
        "openagc-ib-dump: tag=ctxreg-rt fw=0x9400008 completed=1 words=6\n"
        "ib 00000009 00000080 00000080 00008000 00000010 0000000f\n";
    /* owned_color_va>>8 = 0x1234567890 → BASE=0x34567890 BASE_EXT=0x12 */
    static const char ctxreg_cb_bind_text[] =
        "openagc-ib-dump: tag=ctxreg-cb-bind fw=0x9400008 completed=1 words=8\n"
        "ib 34567890 00000012 00000000 00000000 00000000 00000000 ffffffff 0000000f\n";
    static const char ctxreg_cb_bind_zero[] =
        "openagc-ib-dump: tag=ctxreg-cb-bind fw=0x9400008 completed=1 words=8\n"
        "ib 00000000 00000000 00000000 00000000 00000000 00000000 ffffffff 0000000f\n";
    /* Step-Z payload shape: owned-expect line precedes the dump header. */
    static const char ctxreg_cb_bind_owned_text[] =
        "openagc-cb-bind-owned: color_va=00000001234567890000 base_lo=34567890 "
        "base_ext=00000012 shader_mask=0000000f\n"
        "openagc-ib-dump: tag=ctxreg-cb-bind fw=0x9400008 completed=1 words=8\n"
        "ib 34567890 00000012 00000000 00000000 00000000 00000000 ffffffff 0000000f\n";
    static const char no_header[] =
        "openagc-cb-bind-owned: color_va=00000001234567890000 base_lo=34567890\n"
        "ib 34567890 00000012\n";
    /* Step-AB dump: nine composed words for owned_color_va (32x32 RGBA8). */
    static const char cb_bind_full_text[] =
        "openagc-cb-bind-full-owned: color_va=00000012345678900000 base_lo=34567890 "
        "base_ext=00000012 info=00028028 attrib2=0007c01f attrib3=01000000\n"
        "openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=1 words=9\n"
        "ib 34567890 00000012 00000000 00028028 00000000 0007c01f 01000000 "
        "0000000f 0000000f\n";
    static const char cb_bind_full_incomplete[] =
        "openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=0 words=9\n"
        "ib cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc "
        "cccccccc cccccccc\n";
    static const char cb_bind_full_zero_base[] =
        "openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=1 words=9\n"
        "ib 00000000 00000012 00000000 00028028 00000000 0007c01f 01000000 "
        "0000000f 0000000f\n";
    static const char cb_bind_full_bad_info[] =
        "openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=1 words=9\n"
        "ib 34567890 00000012 00000000 0002802c 00000000 0007c01f 01000000 "
        "0000000f 0000000f\n";
    /*
     * Exact Step-AB console log shape (fw 0x9400008, arena color VA
     * 0x0000000200024000, 32x32 RGBA8). The host composition for that VA
     * must reproduce these nine words exactly.
     */
    static const char cb_bind_full_console_text[] =
        "openagc-cb-bind-full-owned: base_lo=02000240 base_ext=00000000 "
        "view=00000000 info=00028028 attrib=00000000 attrib2=0007c01f "
        "attrib3=01000000 target_mask=0000000f shader_mask=0000000f match=1\n"
        "openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=1 words=9\n"
        "ib 02000240 00000000 00000000 00028028 00000000 0007c01f 01000000 "
        "0000000f 0000000f\n";
    /* Exact Step-Z console log shape (fw 0x9400008, arena VA 0x200024000). */
    static const char ctxreg_cb_bind_console_text[] =
        "openagc-cb-bind-owned: color_va=0000000200024000 base_lo=02000240 "
        "base_ext=00000000 shader_mask=0000000f\n"
        "openagc-ib-dump: tag=ctxreg-cb-bind fw=0x9400008 completed=1 words=8\n"
        "ib 02000240 00000000 00000000 00000000 00000000 00000000 ffffffff 0000000f\n";
    static const char bad_tag[] =
        "openagc-ib-dump: tag=cb-invent fw=0x9400008 completed=1 words=1\n"
        "ib deadbeef\n";
    /* Step-AA tile-mode dump: addr config + GB_TILE_MODE0..31. */
    static const char mmio_tilemode_text[] =
        "openagc-ib-dump: tag=mmio-tilemode fw=0x9400008 completed=1 words=33\n"
        "ib 04100009 00000000 00000010 00000000 00000000 00000000 00800004 00000000\n"
        "00000000 00000000 00400008 00000000 00000000 00000000 00000000 00000000\n"
        "00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n"
        "00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n"
        "00000000\n";
    static const char mmio_tilemode_incomplete[] =
        "openagc-ib-dump: tag=mmio-tilemode fw=0x9400008 completed=0 words=33\n"
        "ib 04100009 00000000 00000010 00000000 00000000 00000000 00800004 00000000\n"
        "00000000 00000000 00400008 00000000 00000000 00000000 00000000 00000000\n"
        "00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n"
        "00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000\n"
        "00000000\n";

    CHECK(OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT == 0u);
    CHECK(OPENAGC_NATIVE_TILING_SUPPORTED == 0u);
    CHECK(OPENAGC_SCANOUT_USAGE_SUPPORTED == 0u);
    CHECK(OPENAGC_PRESENTATION_SUPPORTED == 0u);
    CHECK(OPENAGC_VIDEOOUT_EVIDENCE_PIN_COUNT == 0u);

    /* Smoke context offsets map to public Mesa/amdgpu names; COLOR_BASE absent. */
    named = openagc_gfx10_lookup_reg(433u, 1u);
    CHECK(named != NULL && strcmp(named->name, "SPI_VS_OUT_CONFIG") == 0);
    named = openagc_gfx10_lookup_reg(143u, 1u);
    CHECK(named != NULL && strcmp(named->name, "CB_SHADER_MASK") == 0 &&
          named->smoke_owned == 1u);
    named = openagc_gfx10_lookup_reg(792u, 1u);
    CHECK(named != NULL && strcmp(named->name, "CB_COLOR0_BASE") == 0 &&
          named->smoke_owned == 0u);
    named = openagc_gfx10_lookup_reg(793u, 1u);
    CHECK(named != NULL && strcmp(named->name, "CB_COLOR0_PITCH") == 0);
    CHECK(OPENAGC_GFX10_CB_COLOR0_BASE_EXT == 912u);
    CHECK(OPENAGC_GFX10_CB_COLOR0_ATTRIB2 == 944u);
    CHECK(OPENAGC_GFX10_CB_COLOR0_ATTRIB3 == 952u);
    CHECK(OPENAGC_GFX10_CB_COLOR0_SLICE == 794u);
    named = openagc_gfx10_lookup_reg(OPENAGC_GFX10_CB_COLOR0_ATTRIB3, 1u);
    CHECK(named != NULL && strcmp(named->name, "CB_COLOR0_ATTRIB3") == 0);
    named = openagc_gfx10_lookup_reg(72u, 0u);
    CHECK(named != NULL && strcmp(named->name, "SPI_SHADER_PGM_LO_VS") == 0);
    CHECK(openagc_gfx10_cb_probe_offsets[0] == OPENAGC_GFX10_CB_COLOR0_BASE);
    CHECK(openagc_gfx10_cb_probe_offsets[1] == OPENAGC_GFX10_CB_COLOR0_PITCH);
    CHECK(openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_COLOR0_BASE) ==
          (OPENAGC_PM4_CONTEXT_REG_START + OPENAGC_GFX10_CB_COLOR0_BASE));
    CHECK(OPENAGC_GFX10_CTXREG_RT_COUNT == 6u);
    CHECK(openagc_gfx10_ctxreg_rt_offsets[5] == OPENAGC_GFX10_CB_SHADER_MASK);
    CHECK(openagc_gfx10_ctxreg_rt_values[5] == OPENAGC_GFX10_CB_SHADER_MASK_OWNED);
    CHECK(openagc_pm4_cb_color0_base_lo(owned_color_va) == 0x34567890u);
    CHECK(openagc_pm4_cb_color0_base_ext(owned_color_va) == 0x00000012u);

    probe_count = openagc_pm4_encode_copy_data_cb_probe_eop(0x1000u, 1u, 0x2000u,
                                                            probe_words);
    CHECK(probe_count == OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS);
    CHECK(probe_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(probe_words[1] == OPENAGC_PM4_COPY_DATA_CONTROL);
    CHECK(probe_words[2] == OPENAGC_GFX10_CB_COLOR0_BASE);

    probe_count = openagc_pm4_encode_copy_data_cb_probe_abs_eop(0x1000u, 1u, 0x2000u,
                                                                probe_words);
    CHECK(probe_count == OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS);
    CHECK(probe_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(probe_words[1] == OPENAGC_PM4_COPY_DATA_CONTROL);
    CHECK(probe_words[2] ==
          openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_COLOR0_BASE));
    CHECK(probe_words[2] != OPENAGC_GFX10_CB_COLOR0_BASE);

    rt_count = openagc_pm4_encode_ctxreg_rt_abs_eop(0x1000u, 1u, 0x2000u, rt_words);
    CHECK(rt_count == OPENAGC_PM4_CTXREG_RT_EOP_WORDS);
    CHECK(rt_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(rt_words[1] == OPENAGC_GFX10_SPI_SHADER_COL_FORMAT);
    CHECK(rt_words[2] == 9u);
    CHECK(rt_words[OPENAGC_PM4_CTXREG_RT_SET_WORDS] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(rt_words[OPENAGC_PM4_CTXREG_RT_SET_WORDS + 1u] == OPENAGC_PM4_COPY_DATA_CONTROL);
    CHECK(rt_words[OPENAGC_PM4_CTXREG_RT_SET_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_SPI_SHADER_COL_FORMAT));

    expected_base_lo = openagc_pm4_cb_color0_base_lo(owned_color_va);
    expected_base_ext = openagc_pm4_cb_color0_base_ext(owned_color_va);
    bind_count = openagc_pm4_encode_ctxreg_cb_bind_abs_eop(
        owned_color_va, 0x1000u, 1u, 0x2000u, bind_words);
    CHECK(bind_count == OPENAGC_PM4_CTXREG_CB_BIND_EOP_WORDS);
    CHECK(bind_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(bind_words[1] == OPENAGC_GFX10_CB_COLOR0_BASE);
    CHECK(bind_words[2] == expected_base_lo);
    /* Byte-identical to the pushed Step-Z IB: second SET target was 793. */
    CHECK(bind_words[4] == OPENAGC_GFX10_CB_COLOR0_PITCH);
    CHECK(bind_words[5] == expected_base_ext);
    CHECK(bind_words[7] == OPENAGC_GFX10_CB_SHADER_MASK);
    CHECK(bind_words[8] == OPENAGC_GFX10_CB_SHADER_MASK_OWNED);
    CHECK(bind_words[OPENAGC_PM4_CTXREG_CB_BIND_SET_WORDS] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(bind_words[OPENAGC_PM4_CTXREG_CB_BIND_SET_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_COLOR0_BASE));

    /* Step AB: nine-register linear color bind composition + encode lock. */
    CHECK(OPENAGC_GFX10_CB_BIND_COUNT == 9u);
    CHECK(openagc_gfx10_cb_bind_offsets[0] == OPENAGC_GFX10_CB_COLOR0_BASE);
    CHECK(openagc_gfx10_cb_bind_offsets[1] == OPENAGC_GFX10_CB_COLOR0_BASE_EXT);
    CHECK(openagc_gfx10_cb_bind_offsets[2] == OPENAGC_GFX10_CB_COLOR0_VIEW);
    CHECK(openagc_gfx10_cb_bind_offsets[3] == OPENAGC_GFX10_CB_COLOR0_INFO);
    CHECK(openagc_gfx10_cb_bind_offsets[4] == OPENAGC_GFX10_CB_COLOR0_ATTRIB);
    CHECK(openagc_gfx10_cb_bind_offsets[5] == OPENAGC_GFX10_CB_COLOR0_ATTRIB2);
    CHECK(openagc_gfx10_cb_bind_offsets[6] == OPENAGC_GFX10_CB_COLOR0_ATTRIB3);
    CHECK(openagc_gfx10_cb_bind_offsets[7] == OPENAGC_GFX10_CB_TARGET_MASK);
    CHECK(openagc_gfx10_cb_bind_offsets[8] == OPENAGC_GFX10_CB_SHADER_MASK);
    /* Cited field values: FORMAT(COLOR_8_8_8_8=10)<<2 | BLEND_CLAMP |
     * SIMPLE_FLOAT | COMP_SWAP(STD=0); BGRA8 differs only in COMP_SWAP. */
    CHECK(openagc_gfx10_cb_color0_info_8888(OPENAGC_GFX10_CB_COMP_SWAP_RGBA8) ==
          0x00028028u);
    CHECK(openagc_gfx10_cb_color0_info_8888(OPENAGC_GFX10_CB_COMP_SWAP_BGRA8) ==
          0x00028828u);
    CHECK(openagc_gfx10_cb_color0_attrib_single_sample() == 0u);
    CHECK(openagc_gfx10_cb_color0_view_2d() == 0u);
    CHECK(openagc_gfx10_cb_color0_attrib2(32u, 32u) == 0x0007C01Fu);
    CHECK(openagc_gfx10_cb_color0_attrib2(4096u, 1u) == 0x03FFC000u);
    CHECK(openagc_gfx10_cb_color0_attrib3_linear_2d() == 0x01000000u);
    CHECK(openagc_gfx10_cb_target_mask_rgba0() == 0x0000000Fu);

    openagc_gfx10_cb_bind_linear_8888_words(
        owned_color_va, 32u, 32u, OPENAGC_GFX10_CB_COMP_SWAP_RGBA8,
        expected_bind);
    CHECK(expected_bind[OPENAGC_GFX10_CB_BIND_IDX_BASE] == expected_base_lo);
    CHECK(expected_bind[OPENAGC_GFX10_CB_BIND_IDX_BASE_EXT] == expected_base_ext);
    CHECK(expected_bind[OPENAGC_GFX10_CB_BIND_IDX_SHADER_MASK] ==
          OPENAGC_GFX10_CB_SHADER_MASK_OWNED);

    full_count = openagc_pm4_encode_ctxreg_cb_bind_full_abs_eop(
        owned_color_va, 32u, 32u, 0x1000u, 1u, 0x2000u, full_words);
    CHECK(full_count == OPENAGC_PM4_CTXREG_CB_BIND_FULL_EOP_WORDS);
    CHECK(OPENAGC_PM4_CTXREG_CB_BIND_FULL_EOP_WORDS ==
          9u * 3u + 9u * OPENAGC_PM4_COPY_DATA_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS);
    CHECK(full_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(full_words[1] == OPENAGC_GFX10_CB_COLOR0_BASE);
    CHECK(full_words[2] == expected_bind[0]);
    CHECK(full_words[4] == OPENAGC_GFX10_CB_COLOR0_BASE_EXT);
    CHECK(full_words[5] == expected_bind[1]);
    CHECK(full_words[7] == OPENAGC_GFX10_CB_COLOR0_VIEW);
    CHECK(full_words[10] == OPENAGC_GFX10_CB_COLOR0_INFO);
    CHECK(full_words[13] == OPENAGC_GFX10_CB_COLOR0_ATTRIB);
    CHECK(full_words[16] == OPENAGC_GFX10_CB_COLOR0_ATTRIB2);
    CHECK(full_words[19] == OPENAGC_GFX10_CB_COLOR0_ATTRIB3);
    CHECK(full_words[22] == OPENAGC_GFX10_CB_TARGET_MASK);
    CHECK(full_words[25] == OPENAGC_GFX10_CB_SHADER_MASK);
    CHECK(full_words[OPENAGC_PM4_CTXREG_CB_BIND_FULL_SET_WORDS] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(full_words[OPENAGC_PM4_CTXREG_CB_BIND_FULL_SET_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_COLOR0_BASE));
    CHECK(full_words[OPENAGC_PM4_CTXREG_CB_BIND_FULL_SET_WORDS +
                     8u * OPENAGC_PM4_COPY_DATA_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_SHADER_MASK));
    CHECK(full_words[OPENAGC_PM4_CTXREG_CB_BIND_FULL_EOP_WORDS -
                     OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);

    mmio_count = openagc_pm4_encode_mmio_tilemode_probe_eop(0x1000u, 1u, 0x2000u,
                                                            mmio_probe_words);
    CHECK(mmio_count == OPENAGC_PM4_MMIO_TILEMODE_PROBE_EOP_WORDS);
    CHECK(OPENAGC_PM4_MMIO_TILEMODE_PROBE_EOP_WORDS ==
          33u * OPENAGC_PM4_COPY_DATA_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS);
    CHECK(mmio_probe_words[0] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(mmio_probe_words[2] == OPENAGC_GFX10_MMIO_GB_ADDR_CONFIG);
    CHECK(mmio_probe_words[OPENAGC_PM4_COPY_DATA_WORDS + 2u] ==
          OPENAGC_GFX10_MMIO_GB_TILE_MODE_BASE);
    CHECK(openagc_gfx10_mmio_gb_tile_mode_offset(31u) == 0x1403u);
    CHECK(mmio_probe_words[32u * OPENAGC_PM4_COPY_DATA_WORDS + 2u] ==
          openagc_gfx10_mmio_gb_tile_mode_offset(31u));
    CHECK(mmio_probe_words[OPENAGC_PM4_MMIO_TILEMODE_PROBE_WORDS] ==
          OPENAGC_PM4_EOP_HEADER);

    EXPECT(openagc_ib_dump_parse(NULL, words, 8u, &info), OPENAGC_ERROR_INVALID_ARGUMENT);
    EXPECT(openagc_ib_dump_parse(dump_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.dump_parsed == 1u);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_REGISTER_EOP);
    CHECK(info.firmware_id == OPENAGC_IB_DUMP_FW940_ID);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 4u);
    CHECK(words[0] == 0xc0001000u && words[1] == 0u);
    CHECK(words[2] == 0xc0001000u && words[3] == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.word_count == 8u);
    CHECK(words[0] == 0x11111111u && words[7] == 0x0000000fu);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_abs_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_ABS);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.completed == 0u);
    CHECK(info.word_count == 8u);
    CHECK(words[0] == 0xccccccccu && words[7] == 0xccccccccu);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_rt_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_RT);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 6u);
    CHECK(words[0] == 0x00000009u && words[5] == 0x0000000fu);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND);
    CHECK(info.evidence_qualified == 0u);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 8u);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, expected_base_lo,
              OPENAGC_GFX10_CB_SHADER_MASK_OWNED) == 1u);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, 0u, OPENAGC_GFX10_CB_SHADER_MASK_OWNED) == 0u);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, expected_base_lo, 0u) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_zero, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, expected_base_lo,
              OPENAGC_GFX10_CB_SHADER_MASK_OWNED) == 0u);

    /* Leading owned-expect line is skipped; the dump itself still matches. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_owned_text, words, 8u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND);
    CHECK(info.dump_parsed == 1u);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 8u);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, expected_base_lo,
              OPENAGC_GFX10_CB_SHADER_MASK_OWNED) == 1u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(no_header, words, 8u, &info),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(info.dump_parsed == 0u);

    /* Console Step-Z log: the legacy matcher scores BASE + shader mask. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_console_text, words, 8u, &info),
           OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND);
    CHECK(info.completed == 1u);
    CHECK(openagc_ib_dump_cb_bind_legacy_base_match(
              &info, words, openagc_pm4_cb_color0_base_lo(0x200024000ull),
              OPENAGC_GFX10_CB_SHADER_MASK_OWNED) == 1u);
    CHECK(words[OPENAGC_GFX10_CB_PROBE_IDX_BASE] == 0x02000240u);
    CHECK(words[OPENAGC_GFX10_CB_PROBE_IDX_PITCH] == 0u);
    CHECK(words[OPENAGC_GFX10_CB_PROBE_IDX_SHADER_MASK] == 15u);
    CHECK(info.evidence_qualified == 0u);

    /*
     * Step-AB dump: the full nine-word set matches only against the exact
     * composed expectation; completed=0, a zero BASE, a single wrong field,
     * or the wrong kind all refuse.
     */
    openagc_gfx10_cb_bind_linear_8888_words(
        owned_color_va, 32u, 32u, OPENAGC_GFX10_CB_COMP_SWAP_RGBA8,
        expected_bind);
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_text, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND_FULL);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == OPENAGC_GFX10_CB_BIND_COUNT);
    CHECK(info.evidence_qualified == 0u);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 1u);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words, NULL) == 0u);
    CHECK(openagc_ib_dump_cb_bind_full_match(NULL, full_dump_words,
                                             expected_bind) == 0u);
    /* Wrong kind (legacy probe layout) never scores as a full bind. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_text, words, 8u, &info), OPENAGC_OK);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, words, expected_bind) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_incomplete, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(info.completed == 0u);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_zero_base, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_bad_info, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_INFO] == 0x0002802Cu);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 0u);

    /* Every single-field mismatch refuses, not just INFO. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_text, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 1u);
    for (word_index = 0u; word_index < OPENAGC_GFX10_CB_BIND_COUNT; ++word_index) {
        uint32_t saved = full_dump_words[word_index];

        full_dump_words[word_index] = saved ^ 1u;
        CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                                 expected_bind) == 0u);
        full_dump_words[word_index] = saved;
    }
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 1u);

    /*
     * Console Step-AB log, verbatim: the words the console read back equal
     * this host's composition for the same arena VA and 32x32 RGBA8 view.
     */
    openagc_gfx10_cb_bind_linear_8888_words(
        0x200024000ull, 32u, 32u, OPENAGC_GFX10_CB_COMP_SWAP_RGBA8,
        expected_bind);
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(cb_bind_full_console_text, full_dump_words,
                                 OPENAGC_GFX10_CB_BIND_COUNT, &info),
           OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND_FULL);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == OPENAGC_GFX10_CB_BIND_COUNT);
    CHECK(openagc_ib_dump_cb_bind_full_match(&info, full_dump_words,
                                             expected_bind) == 1u);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_BASE] == 0x02000240u);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_BASE_EXT] == 0u);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_INFO] == 0x00028028u);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_ATTRIB2] == 0x0007C01Fu);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_ATTRIB3] == 0x01000000u);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_TARGET_MASK] == 0x0000000Fu);
    CHECK(full_dump_words[OPENAGC_GFX10_CB_BIND_IDX_SHADER_MASK] == 0x0000000Fu);
    CHECK(info.evidence_qualified == 0u);
    CHECK(OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(bad_tag, words, 8u, &info),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(info.dump_parsed == 0u);
    CHECK(info.evidence_qualified == 0u);

    /* Step-AA tile-mode table: owned words drive a fail-closed lookup. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(mmio_tilemode_text, mmio_words,
                                 OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT, &info),
           OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_MMIO_TILEMODE);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT);
    CHECK(info.evidence_qualified == 0u);
    CHECK(mmio_words[0] == 0x04100009u);
    CHECK(openagc_gfx10_gb_tile_mode_array_mode(mmio_words[1]) == 0u);
    CHECK(openagc_gfx10_gb_tile_mode_array_mode(mmio_words[6]) == 1u);
    CHECK(openagc_gfx10_gb_tile_mode_micro_tile_mode_new(mmio_words[6]) == 2u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 0u, 0u,
                                               &tile_index) == 1u);
    CHECK(tile_index == 0u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 1u, 2u,
                                               &tile_index) == 1u);
    CHECK(tile_index == 5u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 2u, 1u,
                                               &tile_index) == 1u);
    CHECK(tile_index == 9u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 3u, 3u,
                                               &tile_index) == 0u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 1u, 2u, NULL) == 0u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(NULL, mmio_words, 1u, 2u,
                                               &tile_index) == 0u);

    /* completed=0 refuses: a partial readback never resolves an index. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(mmio_tilemode_incomplete, mmio_words,
                                 OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT, &info),
           OPENAGC_OK);
    CHECK(info.completed == 0u);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 1u, 2u,
                                               &tile_index) == 0u);

    /* Wrong kind refuses even when the words look like a tile-mode table. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(ctxreg_cb_bind_text, words, 8u, &info), OPENAGC_OK);
    CHECK(openagc_ib_dump_mmio_tilemode_lookup(&info, mmio_words, 1u, 2u,
                                               &tile_index) == 0u);
    return 0;
}

static int test_draw_point_ib_and_pixels(void)
{
    openagc_pm4_draw_point_state state;
    openagc_ib_dump_info info = OPENAGC_IB_DUMP_INFO_INIT;
    uint32_t words[OPENAGC_PM4_DRAW_POINT_EOP_WORDS];
    uint32_t offsets[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t values[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t viewport[6];
    uint32_t guardband[4];
    uint32_t pixel_words[4];
    uint32_t pixels = 0u;
    uint32_t count;
    uint32_t offset;
    uint32_t i;
    static const uint64_t color_va = 0x200024000ull;
    static const char draw_text[] =
        "openagc-draw-point-owned: color_va=0000000200024000 pixels=1 "
        "outside=0 value=ff0040ff match=1\n"
        "openagc-ib-dump: tag=draw-point-eop fw=0x9400008 completed=1 words=4\n"
        "ib 00000000 ff0040ff 00000000 00000000\n";
    static const char draw_empty[] =
        "openagc-ib-dump: tag=draw-point-eop fw=0x9400008 completed=1 words=4\n"
        "ib 00000000 00000000 00000000 00000000\n";
    static const char draw_wrong[] =
        "openagc-ib-dump: tag=draw-point-eop fw=0x9400008 completed=1 words=4\n"
        "ib 00000000 ff4000ff 00000000 00000000\n";
    static const char draw_incomplete[] =
        "openagc-ib-dump: tag=draw-point-eop fw=0x9400008 completed=0 words=4\n"
        "ib 00000000 ff0040ff 00000000 00000000\n";
    static const char other_kind[] =
        "openagc-ib-dump: tag=ctxreg-rt fw=0x9400008 completed=1 words=4\n"
        "ib 00000009 00000080 00000080 00008000\n";

    /* Field-level locks, all from the cited compositions. */
    CHECK(openagc_gfx10_draw_point_size_px(1u) == 0x00080008u);
    CHECK(openagc_gfx10_draw_point_minmax_px(1u) == 0x00080008u);
    CHECK(openagc_gfx10_draw_point_size_8px() == 0x00400040u);
    CHECK(openagc_gfx10_draw_point_minmax_8px() == 0x00400040u);
    CHECK(openagc_gfx10_draw_mode_cntl_0() == 0x00000023u);
    CHECK(openagc_gfx10_draw_sc_mode_cntl() == 0u);
    CHECK(openagc_gfx10_draw_clip_cntl() == 0x01000000u);
    CHECK(openagc_gfx10_draw_edgerule() ==
          (0x0000000Au | (0x6u << 4) | (0xAu << 8) | (0x19u << 12) |
           (0x25u << 18) | (0xAu << 24) | (0xAu << 28)));
    CHECK(openagc_gfx10_draw_interp_control_0() == 0x00000869u);
    CHECK(openagc_gfx10_draw_vtx_cntl() == 0x0000002Du);
    CHECK(openagc_gfx10_draw_db_z_info_unbound() == 0u);
    CHECK(openagc_gfx10_draw_db_stencil_info_unbound() == 0u);
    CHECK(openagc_gfx10_screen_scissor_tl(8u, 8u) == 0x00080008u);
    CHECK(openagc_gfx10_screen_scissor_br(24u, 24u) == 0x00180018u);
    CHECK(openagc_gfx10_window_scissor_tl() == 0x80000000u);
    CHECK(openagc_gfx10_window_scissor_br(32u, 32u) == 0x00200020u);
    CHECK(OPENAGC_GFX10_CLIPRECT_RULE_DISABLED == 0xffffu);
    CHECK(OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE == 578u);
    CHECK((OPENAGC_GFX10_UCONFIG_REG_VGT_PRIMITIVE_TYPE -
           OPENAGC_PM4_UCONFIG_REG_OFFSET) >> 2 == 578u);
    CHECK(OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX == 2u);
    CHECK(OPENAGC_GFX10_DI_PT_POINTLIST == 1u);
    CHECK(OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8 == 0xFF0040FFu);

    /* Viewport + guardband for the 8x8 rect at (8,8) of a 32x32 target. */
    openagc_gfx10_viewport_gl(8u, 8u, 8u, 8u, viewport);
    CHECK(viewport[0] == openagc_gfx10_float_bits(4.0f));
    CHECK(viewport[1] == openagc_gfx10_float_bits(12.0f));
    CHECK(viewport[2] == openagc_gfx10_float_bits(-4.0f));
    CHECK(viewport[3] == openagc_gfx10_float_bits(12.0f));
    CHECK(viewport[4] == openagc_gfx10_float_bits(0.5f));
    CHECK(viewport[5] == openagc_gfx10_float_bits(0.5f));
    openagc_gfx10_guardband_gl(8u, 8u, 8u, 8u, guardband);
    CHECK(guardband[0] == openagc_gfx10_float_bits(8189.0f));
    CHECK(guardband[1] == openagc_gfx10_float_bits(1.0f));
    CHECK(guardband[2] == openagc_gfx10_float_bits(8189.0f));
    CHECK(guardband[3] == openagc_gfx10_float_bits(1.0f));
    CHECK(openagc_gfx10_vport_scissor_tl(8u, 8u) == 0x80080008u);
    CHECK(openagc_gfx10_vport_scissor_br(16u, 16u) == 0x00100010u);
    CHECK(OPENAGC_GFX10_PA_SC_TILE_STEERING_OVERRIDE == 215u);
    CHECK(OPENAGC_GFX10_DRAW_BASELINE_COUNT == 22u);
    CHECK(openagc_gfx10_draw_baseline_offsets[10] ==
          OPENAGC_GFX10_PA_SC_TILE_STEERING_OVERRIDE);
    CHECK(openagc_gfx10_draw_baseline_offsets[16] ==
          OPENAGC_GFX10_PA_SC_MODE_CNTL_1);

    memset(&state, 0, sizeof(state));
    state.color_va = color_va;
    state.color_width = 32u;
    state.color_height = 32u;
    state.viewport_x = 8u;
    state.viewport_y = 8u;
    state.viewport_width = 8u;
    state.viewport_height = 8u;
    state.vertex_count = 1u;

    count = openagc_pm4_draw_point_state_pairs(&state, offsets, values);
    CHECK(count == OPENAGC_PM4_DRAW_POINT_STATE_COUNT);
    CHECK(offsets[0] == OPENAGC_GFX10_DB_Z_INFO && values[0] == 0u);
    CHECK(offsets[1] == OPENAGC_GFX10_DB_STENCIL_INFO);
    CHECK(offsets[2] == OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_TL);
    CHECK(values[2] == 0x00080008u);
    CHECK(offsets[3] == OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_BR);
    CHECK(values[3] == 0x00100010u);
    CHECK(offsets[4] == OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_TL);
    CHECK(offsets[5] == OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_BR);
    CHECK(values[5] == 0x00200020u);
    CHECK(offsets[6] == OPENAGC_GFX10_PA_SC_CLIPRECT_RULE);
    CHECK(values[6] == 0xffffu);
    CHECK(offsets[7] == OPENAGC_GFX10_PA_SC_EDGERULE);
    CHECK(offsets[8] == OPENAGC_GFX10_PA_SU_HARDWARE_SCREEN_OFFSET &&
          values[8] == 0u);
    CHECK(offsets[9] == OPENAGC_GFX10_SPI_INTERP_CONTROL_0);
    CHECK(offsets[10] == OPENAGC_GFX10_PA_CL_CLIP_CNTL);
    CHECK(offsets[11] == OPENAGC_GFX10_PA_SU_SC_MODE_CNTL);
    CHECK(offsets[12] == OPENAGC_GFX10_PA_SU_POINT_SIZE);
    CHECK(values[12] == 0x00400040u);
    CHECK(offsets[13] == OPENAGC_GFX10_PA_SU_POINT_MINMAX);
    CHECK(values[13] == 0x00400040u);
    CHECK(offsets[14] == OPENAGC_GFX10_PA_SC_MODE_CNTL_0);
    CHECK(values[14] == 0x00000023u);
    CHECK(offsets[15] == OPENAGC_GFX10_CB_COLOR_CONTROL);
    CHECK(values[15] == 0x00CC0011u);
    CHECK(offsets[16] == OPENAGC_GFX10_CB_BLEND0_CONTROL && values[16] == 0u);
    CHECK(offsets[17] == OPENAGC_GFX10_PA_SC_AA_CONFIG &&
          values[17] == 0x0000C000u);
    CHECK(offsets[18] == OPENAGC_GFX10_DB_EQAA && values[18] == 0x00310000u);
    CHECK(offsets[19] == OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X0Y0);
    CHECK(offsets[22] == OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X1Y1);
    CHECK(offsets[23] == OPENAGC_GFX10_PA_SC_CENTROID_PRIORITY_0 &&
          values[23] == 0x32103210u);
    CHECK(offsets[25] == OPENAGC_GFX10_PA_SC_AA_MASK_X0Y0_X1Y0 &&
          values[25] == 0xFFFFFFFFu);
    CHECK(offsets[27] == OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE && values[27] == 0u);
    CHECK(openagc_gfx10_draw_baseline_offsets[20] ==
          OPENAGC_GFX10_CB_COLOR_CONTROL);
    CHECK(openagc_gfx10_draw_baseline_offsets[21] ==
          OPENAGC_GFX10_CB_BLEND0_CONTROL);
    CHECK(openagc_gfx10_draw_ia_multi_vgt_param() == 0x0010007Fu);
    CHECK(openagc_gfx10_draw_cb_color_control_normal() == 0x00CC0011u);
    CHECK(OPENAGC_GFX10_AA_CONFIG_1X == 0x0000C000u);
    CHECK(OPENAGC_GFX10_DB_EQAA_1X == 0x00310000u);
    CHECK(OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X == 0x622AE6AEu);
    CHECK(OPENAGC_GFX10_AA_CENTROID_PRIORITY == 0x32103210u);
    CHECK(OPENAGC_GFX10_DRAW_PROBE_COUNT == 8u);
    CHECK(OPENAGC_GFX10_CB_COLOR_CONTROL == 514u);
    CHECK(OPENAGC_GFX10_CB_BLEND0_CONTROL == 480u);
    CHECK(OPENAGC_GFX10_CB_MODE_DISABLE == 0u);
    CHECK(OPENAGC_GFX10_CB_MODE_NORMAL == 1u);

    /* Refusals: no pairs, a zero viewport, or a viewport outside the target. */
    CHECK(openagc_pm4_draw_point_state_pairs(NULL, offsets, values) == 0u);
    CHECK(openagc_pm4_draw_point_state_pairs(&state, NULL, values) == 0u);
    memset(&state, 0, sizeof(state));
    state.color_va = color_va;
    state.color_width = 32u;
    state.color_height = 32u;
    CHECK(openagc_pm4_draw_point_state_pairs(&state, offsets, values) == 0u);
    state.viewport_x = 16u;
    state.viewport_y = 8u;
    state.viewport_width = 32u;
    state.viewport_height = 8u;
    CHECK(openagc_pm4_draw_point_state_pairs(&state, offsets, values) == 0u);
    state.viewport_x = 8u;
    state.viewport_y = 8u;
    state.viewport_width = 8u;
    state.viewport_height = 8u;
    state.vertex_count = 1u;

    /*
     * The full draw IB: the baseline readback, the scalar state, the
     * viewport/guardband/vport-scissor sequences, the shader program, the
     * colour bind, the topology, one draw, and the EOP trailer. No
     * CLEAR_STATE: the console's own clear-state data is already loaded and
     * a bare CLEAR_STATE of ours would reset it.
     */
    count = openagc_pm4_encode_draw_point_eop(
        &state, 0x200021000ull, 0x200021100ull, 0x200021200ull,
        0x200021300ull, 0x200020000ull, 0x200020100ull, 1u, 0x200023000ull,
        words);
    CHECK(count == OPENAGC_PM4_DRAW_POINT_EOP_WORDS);
    CHECK(OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS ==
          OPENAGC_GFX10_DRAW_BASELINE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS);
    CHECK(words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(words[2] == openagc_pm4_copy_data_src_context_abs(
                          OPENAGC_GFX10_DB_Z_INFO));
    CHECK(words[4] == (uint32_t)0x200021000ull);
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS -
           2u * OPENAGC_PM4_COPY_DATA_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(
              OPENAGC_GFX10_CB_COLOR_CONTROL));
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS -
           OPENAGC_PM4_COPY_DATA_WORDS + 2u] ==
          openagc_pm4_copy_data_src_context_abs(
              OPENAGC_GFX10_CB_BLEND0_CONTROL));
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS + 1u] ==
          OPENAGC_GFX10_DB_Z_INFO);
    /* The viewport sequence follows the scalar pairs and the indexed
     * IA_MULTI_VGT_PARAM packet. */
    offset = OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +
             (OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 1u) * 3u;
    CHECK(words[offset] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 8u, 0u));
    CHECK(words[offset + 1u] == OPENAGC_GFX10_PA_CL_VPORT_XSCALE);
    CHECK(words[offset + 2u] == openagc_gfx10_float_bits(4.0f));
    CHECK(words[offset + 3u] == openagc_gfx10_float_bits(12.0f));
    /* Then PA_SU_VTX_CNTL + the four guardband values, as one sequence. */
    offset += OPENAGC_PM4_SET_CONTEXT_WORDS(6u);
    CHECK(words[offset] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 7u, 0u));
    CHECK(words[offset + 1u] == OPENAGC_GFX10_PA_SU_VTX_CNTL);
    CHECK(words[offset + 2u] == openagc_gfx10_draw_vtx_cntl());
    CHECK(words[offset + 3u] == openagc_gfx10_float_bits(8189.0f));
    /* Then the per-viewport scissor pair. */
    offset += OPENAGC_PM4_SET_CONTEXT_WORDS(5u);
    CHECK(words[offset] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 4u, 0u));
    CHECK(words[offset + 1u] == OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_TL);
    CHECK(words[offset + 2u] == 0x80080008u);
    CHECK(words[offset + 3u] == 0x00100010u);
    /* Each uconfig form is written, then read back, and the draw follows. */
    CHECK(OPENAGC_PM4_RESET_FILTER_CAM == 0x4u);
    CHECK(OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT == 1u);
    CHECK(openagc_pm4_uconfig_vgt_primitive_type_indices[0] == 0u);
    CHECK(OPENAGC_PM4_SH_REG_START == 0x2C00u);
    CHECK(OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 == 140u);
    CHECK(OPENAGC_GFX10_SPI_SHADER_USER_DATA_VS_0 == 76u);
    CHECK(OPENAGC_GFX10_SPI_SHADER_USER_DATA_PS_0 == 12u);
    CHECK(OPENAGC_PM4_DRAW_USER_DATA_COUNT == 4u);
    CHECK(OPENAGC_PM4_DRAW_USER_DATA_PROBE_BASE == 0xA5A5A500u);
    offset = count - OPENAGC_PM4_EOP_WITH_NOP_WORDS - 5u /* instances + draw */ -
             OPENAGC_PM4_DRAW_USER_DATA_COUNT *
                 (3u + OPENAGC_PM4_COPY_DATA_WORDS) /* user data + reads */ -
             5u - OPENAGC_PM4_COPY_DATA_WORDS /* uconfig table load + read */ -
             OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT *
                 (3u + OPENAGC_PM4_COPY_DATA_WORDS);
    CHECK(count == OPENAGC_PM4_DRAW_POINT_EOP_WORDS);
    for (i = 0u; i < OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT; ++i) {
        uint32_t base = offset + i * (3u + OPENAGC_PM4_COPY_DATA_WORDS);

        CHECK(words[base] ==
              (openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
               OPENAGC_PM4_RESET_FILTER_CAM));
        CHECK(words[base + 1u] ==
              (578u | (openagc_pm4_uconfig_vgt_primitive_type_indices[i] << 28)));
        CHECK(words[base + 2u] == OPENAGC_GFX10_DI_PT_POINTLIST);
        CHECK(words[base + 3u] ==
              openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
        CHECK(words[base + 5u] == 0xC242u);
        CHECK(words[base + 7u] == (uint32_t)(0x200021200ull + i * 4u));
    }
    offset += OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT *
              (3u + OPENAGC_PM4_COPY_DATA_WORDS);
    /* Sony's uconfig register-table load, then its readback, then the draw. */
    CHECK(words[offset] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD, 5u, 0u));
    CHECK(words[offset + 1u] == (uint32_t)0x200021300ull);
    CHECK(words[offset + 2u] == 2u);
    CHECK(words[offset + 3u] == OPENAGC_PM4_TABLE_LOAD_CONTROL);
    CHECK(words[offset + 4u] == OPENAGC_PM4_DRAW_UCONFIG_TABLE_RECORDS);
    CHECK(OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD == 0x64u);
    CHECK(OPENAGC_PM4_TABLE_LOAD_CONTROL == 0x80000000u);
    CHECK(OPENAGC_PM4_TABLE_RECORD_BYTES == 8u);
    CHECK(OPENAGC_PM4_DRAW_UCONFIG_TABLE_RECORDS == 1u);
    CHECK(words[offset + 5u] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(words[offset + 7u] == 0xC242u);
    CHECK(words[offset + 9u] ==
          (uint32_t)(0x200021200ull + OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT * 4u));
    offset += 5u + OPENAGC_PM4_COPY_DATA_WORDS;
    /* The NGG vertex stage's user data, then its SH-aperture readback. */
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        uint32_t base = offset + i * 3u;

        CHECK(words[base] ==
              openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(words[base + 1u] == 140u + i);
        CHECK(words[base + 2u] == 0xA5A5A500u + i);
    }
    offset += OPENAGC_PM4_DRAW_USER_DATA_COUNT * 3u;
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        uint32_t base = offset + i * OPENAGC_PM4_COPY_DATA_WORDS;

        CHECK(words[base] ==
              openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
        CHECK(words[base + 2u] == 0x2C00u + 140u + i);
        CHECK(words[base + 4u] ==
              (uint32_t)(0x200021200ull +
                         (OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT + 1u + i) * 4u));
    }
    offset += OPENAGC_PM4_DRAW_USER_DATA_COUNT * OPENAGC_PM4_COPY_DATA_WORDS;
    CHECK(words[offset] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_NUM_INSTANCES, 2u, 0u));
    CHECK(words[offset + 1u] == OPENAGC_PM4_DRAW_INSTANCE_COUNT);
    CHECK(words[offset + 2u] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_DRAW_INDEX_AUTO, 3u, 0u));
    CHECK(words[offset + 3u] == 1u);
    CHECK(words[offset + 4u] == OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX);
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    CHECK(words[count - 1u] == 0u);
    CHECK(OPENAGC_PM4_OP_NUM_INSTANCES == 0x2Fu);
    CHECK(OPENAGC_PM4_DRAW_INSTANCE_COUNT == 1u);
    /* No CLEAR_STATE packet anywhere: it would reset the console's own
     * clear-state data. */
    for (i = 0u; i < count; ++i) {
        CHECK(words[i] != openagc_pm4_header3(OPENAGC_PM4_OP_CLEAR_STATE, 2u, 0u));
    }
    /* The pre-draw probe re-reads the same registers after the writes. */
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +
                OPENAGC_PM4_DRAW_POINT_STATE_COUNT * 3u] ==
          openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +
                OPENAGC_PM4_DRAW_POINT_STATE_COUNT * 3u + 1u] ==
          (OPENAGC_GFX10_IA_MULTI_VGT_PARAM | (1u << 28)));
    CHECK(words[OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +
                OPENAGC_PM4_DRAW_POINT_STATE_COUNT * 3u + 2u] == 0x0010007Fu);
    /* Viewport sequence, then guardband, then vport scissor. */
    offset = OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +
             (OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 1u) * 3u +
             OPENAGC_PM4_SET_CONTEXT_WORDS(6u) +
             OPENAGC_PM4_SET_CONTEXT_WORDS(5u) +
             OPENAGC_PM4_SET_CONTEXT_WORDS(2u);
    CHECK(words[offset] == openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA, 6u, 0u));
    CHECK(words[offset + 2u] == openagc_pm4_copy_data_src_context_abs(
                                    OPENAGC_GFX10_CB_COLOR_CONTROL));
    CHECK(words[offset + 4u] == (uint32_t)0x200021100ull);
    /* The vertex PGM patch lands in the first SET_SH pair (offset 72, 73). */
    offset += OPENAGC_GFX10_DRAW_PROBE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS +
              3u * 3u;
    CHECK(words[offset] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
    CHECK(words[offset + 1u] == 72u);
    CHECK(words[offset + 2u] == (uint32_t)(0x200020000ull >> 8));
    CHECK(words[offset + 3u] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
    CHECK(words[offset + 4u] == 73u);
    CHECK(words[offset + 5u] == (uint32_t)(0x200020000ull >> 40));

    /* Unaligned code addresses are refused, not encoded. */
    CHECK(openagc_pm4_encode_draw_point_eop(&state, 0x200021000ull,
                                            0x200021100ull, 0x200021200ull,
                                            0x200021300ull, 0x200020001ull,
                                            0x200020100ull, 1u, 0x2000u,
                                            words) == 0u);
    CHECK(openagc_pm4_encode_draw_point_eop(NULL, 0x200021000ull,
                                            0x200021100ull, 0x200021200ull,
                                            0x200021300ull, 0x200020000ull,
                                            0x200020100ull, 1u, 0x2000u,
                                            words) == 0u);

    /* Pixel window: the shader's color may appear, nothing else may. */
    pixels = 0u;
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(draw_text, pixel_words, 4u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_DRAW_POINT);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 4u);
    CHECK(info.evidence_qualified == 0u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
              &pixels) == 1u);
    CHECK(pixels == 1u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
              NULL) == 1u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(&info, pixel_words, 0u,
                                                  &pixels) == 0u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(&info, pixel_words,
                                                  0xFF4000FFu, &pixels) == 0u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(&info, NULL,
                                                  OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
                                                  &pixels) == 0u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(NULL, pixel_words,
                                                  OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
                                                  &pixels) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(draw_empty, pixel_words, 4u, &info), OPENAGC_OK);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
              &pixels) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(draw_wrong, pixel_words, 4u, &info), OPENAGC_OK);
    CHECK(pixel_words[1] == 0xFF4000FFu);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
              &pixels) == 0u);

    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(draw_incomplete, pixel_words, 4u, &info),
           OPENAGC_OK);
    CHECK(info.completed == 0u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8,
              &pixels) == 0u);

    /* Wrong kind refuses even when the words look like pixels. */
    info = OPENAGC_IB_DUMP_INFO_INIT;
    EXPECT(openagc_ib_dump_parse(other_kind, pixel_words, 4u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_CTXREG_RT);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, pixel_words, 0x00000009u, &pixels) == 0u);
    return 0;
}

/*
 * The Step-AC acceptance scan is shared code, so its bounds are testable:
 * it must never read outside the target it is given, and it must report
 * both the drawn pixels and any write outside the drawn rect.
 */
static int test_draw_point_scan_bounds(void)
{
    uint32_t target[64u];
    uint32_t window[16u];
    uint32_t window_full[64u];
    uint32_t pixels = 0u;
    uint32_t outside = 0u;
    uint32_t value = 0u;
    uint32_t i;

    memset(target, 0, sizeof(target));
    target[5u * 8u + 2u] = OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8; /* (2,5) */
    target[7u * 8u + 6u] = OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8; /* (6,7) */

    /* 4x4 window at (1,4) covers the first write only; the second is
     * outside it and must be counted as such. */
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 1u, 4u, 4u, 4u, window,
                                      16u, &pixels, &outside, &value) == 1u);
    CHECK(pixels == 1u);
    CHECK(outside == 1u);
    CHECK(value == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8);
    CHECK(window[(5u - 4u) * 4u + (2u - 1u)] ==
          OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8);
    for (i = 0u; i < 16u; ++i) {
        if (i != 5u) {
            CHECK(window[i] == 0u);
        }
    }

    /* The whole target as the window: everything is inside it. */
    memset(window_full, 0xEE, sizeof(window_full));
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 0u, 8u, 8u,
                                      window_full, 64u, &pixels, &outside,
                                      &value) == 1u);
    CHECK(pixels == 2u);
    CHECK(outside == 0u);
    CHECK(window_full[5u * 8u + 2u] == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8);
    CHECK(window_full[7u * 8u + 6u] == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8);
    CHECK(window_full[0] == 0u);

    /* Refusals: a window that leaves the target, a zero dimension, no
     * outputs, and a window buffer that is too small. */
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 6u, 0u, 4u, 4u, window,
                                      16u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 6u, 4u, 4u, window,
                                      16u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 0u, 0u, 4u, window,
                                      16u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 0u, 8u, 0u, 0u, 4u, 4u, window,
                                      16u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(NULL, 8u, 8u, 0u, 0u, 4u, 4u, window,
                                      16u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 0u, 4u, 4u, window,
                                      16u, &pixels, &outside, NULL) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 0u, 4u, 4u, window,
                                      15u, &pixels, &outside, &value) == 0u);
    CHECK(openagc_pm4_draw_point_scan(target, 8u, 8u, 0u, 0u, 4u, 4u, NULL,
                                      16u, &pixels, &outside, &value) == 0u);
    return 0;
}

/*
 * The payload writes the scissored window dump (now 8x8) plus a baseline
 * line; the host must be able to parse and accept exactly that shape before
 * another console run is attempted.
 */
static int test_draw_point_window_dump_shape(void)
{
    openagc_ib_dump_info info = OPENAGC_IB_DUMP_INFO_INIT;
    uint32_t words[64u];
    uint32_t pixels = 0u;
    static char text[4096];
    static const char header[] =
        "openagc-draw-point-owned: color_va=0000000200024000 rect=8,8,8x8 "
        "pixels=1 outside=0 value=ff0040ff match=1\n"
        "openagc-baseline: 00000022 00000000 00000000 00000000 00000000\n"
        "openagc-ib-dump: tag=draw-point-eop fw=0x9400008 completed=1 words=64\n"
        "ib";
    size_t used = sizeof(header) - 1u;
    uint32_t i;

    memcpy(text, header, used);
    for (i = 0u; i < 64u; ++i) {
        int n = snprintf(text + used, sizeof(text) - used, " %08x",
                         i == 5u ? OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8 : 0u);

        CHECK(n == 9);
        used += (size_t)n;
    }
    CHECK(used + 2u < sizeof(text));
    text[used++] = '\n';
    text[used] = '\0';

    EXPECT(openagc_ib_dump_parse(text, words, 64u, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_IB_DUMP_KIND_DRAW_POINT);
    CHECK(info.completed == 1u);
    CHECK(info.word_count == 64u);
    CHECK(openagc_ib_dump_draw_point_pixels_match(
              &info, words, OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8, &pixels) == 1u);
    CHECK(pixels == 1u);
    return 0;
}

static int test_ngg_draw_ib_contracts(void)
{
    openagc_pm4_draw_point_state state;
    openagc_pm4_ngg_program program;
    static const uint32_t vertex_context_offsets[] = { 433u, 451u };
    static const uint32_t vertex_context_values[] = { 0u, 4u };
    static const uint32_t vertex_shader_offsets[] = { 200u, 201u, 138u, 139u };
    static const uint32_t vertex_shader_values[] = { 0u, 0u, 0x422c0003u,
                                                     0x00100006u };
    static const uint32_t linkage_offsets[] = { 603u, 725u, 610u };
    static const uint32_t linkage_values[] = { 0x00010080u, 0x00012010u, 0u };
    static const uint32_t fragment_context_offsets[] = { 452u };
    static const uint32_t fragment_context_values[] = { 0u };
    static const uint32_t fragment_shader_offsets[] = { 8u, 9u, 10u, 11u };
    static const uint32_t fragment_shader_values[] = { 0u, 0u, 0x022c0001u, 4u };
    static const uint32_t user_data_offsets[] = { 140u, 141u, 142u, 143u };
    static const uint32_t user_data_values[] = { 0u, 0u, 0u, 0u };
    uint32_t words[OPENAGC_PM4_NGG_DRAW_EOP_WORDS];
    uint32_t table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t count;
    uint32_t i;
    int saw_layout = 0;
    int saw_stages = 0;
    int saw_draw = 0;

    memset(&state, 0, sizeof(state));
    state.color_va = 0x200024000ull;
    state.color_width = 32u;
    state.color_height = 32u;
    state.viewport_x = 8u;
    state.viewport_y = 8u;
    state.viewport_width = 8u;
    state.viewport_height = 8u;
    state.gs_out_prim_type = OPENAGC_GFX10_GS_OUT_TRISTRIP;
    state.vertex_count = 3u;

    memset(&program, 0, sizeof(program));
    program.vertex_context.count = 2u;
    program.vertex_context.offsets = vertex_context_offsets;
    program.vertex_context.values = vertex_context_values;
    program.vertex_shader.count = 4u;
    program.vertex_shader.offsets = vertex_shader_offsets;
    program.vertex_shader.values = vertex_shader_values;
    program.linkage.count = 3u;
    program.linkage.offsets = linkage_offsets;
    program.linkage.values = linkage_values;
    program.fragment_context.count = 1u;
    program.fragment_context.offsets = fragment_context_offsets;
    program.fragment_context.values = fragment_context_values;
    program.fragment_shader.count = 4u;
    program.fragment_shader.offsets = fragment_shader_offsets;
    program.fragment_shader.values = fragment_shader_values;
    program.vertex_pgm_lo_slot = 0u;
    program.vertex_pgm_hi_slot = 1u;
    program.fragment_pgm_lo_slot = 0u;
    program.fragment_pgm_hi_slot = 1u;
    program.user_data.count = 4u;
    program.user_data.offsets = user_data_offsets;
    program.user_data.values = user_data_values;
    program.user_data_layout_slot = 2u;
    program.user_data_layout = 0x00000200u;
    program.di_primitive = OPENAGC_GFX10_DI_PT_TRILIST;

    count = openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                            0x200011000ull, 0x200012000ull,
                                            0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                            0x200013000ull, words);
    CHECK(count != 0u);
    CHECK(count <= OPENAGC_PM4_NGG_DRAW_EOP_WORDS);
    for (i = 0u; i < count; ++i) {
        if (words[i] == 0x00000200u) {
            saw_layout = 1;
        }
    }
    for (i = 0u; i + 2u < count; ++i) {
        if (words[i] == openagc_pm4_header3(OPENAGC_PM4_OP_DRAW_INDEX_AUTO, 3u,
                                            0u) &&
            words[i + 1u] == state.vertex_count &&
            words[i + 2u] == OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX) {
            saw_draw = 1;
        }
    }
    CHECK(saw_layout == 1);
    CHECK(saw_draw == 1);
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    /* GE_CNTL is a uconfig register, so it must not be in the context table;
     * VGT_SHADER_STAGES_EN is context and must be. Both uconfig writes carry
     * the CAM flag their decoded stream uses. */
    saw_draw = 0;
    saw_stages = 0;
    for (i = 0u; i + 1u < OPENAGC_PM4_NGG_TABLE_WORDS; i += 2u) {
        if (table[i] == OPENAGC_GFX10_UCONFIG_GE_CNTL) {
            saw_draw = 1;
        }
        if (table[i] == 725u && table[i + 1u] == 0x00012010u) {
            saw_stages = 1;
        }
    }
    CHECK(saw_draw == 0);
    CHECK(saw_stages == 1);
    saw_draw = 0;
    saw_layout = 0;
    for (i = 0u; i + 2u < count; ++i) {
        if (words[i] ==
            (openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
             OPENAGC_PM4_RESET_FILTER_CAM)) {
            if (words[i + 1u] == OPENAGC_GFX10_UCONFIG_GE_CNTL &&
                words[i + 2u] == 0x00010080u) {
                saw_draw = 1;
            }
        }
        /* The input topology goes through the indexed form radv uses: the
         * plain opcode is what the GFX10 CAM skips. */
        if (words[i] ==
            (openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG_INDEX, 3u, 0u) |
             OPENAGC_PM4_RESET_FILTER_CAM)) {
            if (words[i + 1u] ==
                    (OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE |
                     (OPENAGC_PM4_UCONFIG_PRIMITIVE_TYPE_INDEX << 28)) &&
                words[i + 2u] == OPENAGC_GFX10_DI_PT_TRILIST) {
                saw_layout = 1;
            }
        }
    }
    CHECK(saw_draw == 1);
    CHECK(saw_layout == 1);

    /* A triangle-list program must refuse the one-vertex packet that made
     * the first NGG capture incapable of assembling a triangle. */
    state.vertex_count = 1u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    state.vertex_count = 3u;

    /* Refusals, one at a time: a NULL program, an empty or oversized table, a
     * short user-data block, a bad layout slot, a bad PGM slot and a
     * misaligned code address. */
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, NULL, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020080ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    program.linkage.count = 0u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    program.linkage.count = OPENAGC_PM4_NGG_MAX_WRITES + 1u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    program.linkage.count = 3u;
    program.user_data.count = 3u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    program.user_data.count = 4u;
    program.user_data_layout_slot = 4u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    program.user_data_layout_slot = 2u;
    program.vertex_pgm_hi_slot = 4u;
    CHECK(openagc_pm4_encode_draw_ngg_eop(&state, &program, 0x200010000ull,
                                          0x200011000ull, 0x200012000ull,
                                          0x200020000ull, 0x200020400ull, table,
                                          0x200014000ull, 1u,
                                          0x200013000ull, words) == 0u);
    return 0;
}

int main(void)
{
    if (test_device_and_memory() != 0 ||
        test_ownership_and_binding() != 0 ||
        test_packet_words_and_fence() != 0 ||
        test_rejected_commands_and_aliasing() != 0 ||
        test_cross_device_and_alias_copy() != 0 ||
        test_write_data_words() != 0 ||
        test_compute_store_const_words() != 0 ||
        test_host_store_const_apply() != 0 ||
        test_host_write_data_apply() != 0 ||
        test_host_dma_write_data_apply() != 0 ||
        test_host_graphics_register_eop() != 0 ||
        test_cb_capture_refuse_and_accept() != 0 ||
        test_ib_dump_parse_and_refuse_contracts() != 0 ||
        test_draw_point_ib_and_pixels() != 0 ||
        test_draw_point_scan_bounds() != 0 ||
        test_draw_point_window_dump_shape() != 0 ||
        test_ngg_draw_ib_contracts() != 0) {
        return 1;
    }
    puts("OpenAGC GPU foundation tests passed");
    return 0;
}
