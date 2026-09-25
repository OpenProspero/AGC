/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/driver.h"
#include "openagc/pm4_compute_fw940.h"
#include "openagc/pm4_copy_data_fw940.h"
#include "openagc/pm4_context_regs_gfx10.h"
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
    uint32_t probe_count;
    const openagc_gfx10_reg_name *named;
    static const char dump_text[] =
        "openagc-ib-dump: tag=step-u fw=0x9400008 completed=1 words=4\n"
        "ib c0001000 00000000 c0001000 00000000\n";
    static const char ctxreg_text[] =
        "openagc-ib-dump: tag=ctxreg-cb fw=0x9400008 completed=1 words=8\n"
        "ib 11111111 22222222 33333333 44444444 55555555 66666666 77777777 0000000f\n";
    static const char ctxreg_abs_text[] =
        "openagc-ib-dump: tag=ctxreg-abs fw=0x9400008 completed=0 words=8\n"
        "ib cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc cccccccc\n";
    static const char bad_tag[] =
        "openagc-ib-dump: tag=cb-invent fw=0x9400008 completed=1 words=1\n"
        "ib deadbeef\n";

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
    named = openagc_gfx10_lookup_reg(72u, 0u);
    CHECK(named != NULL && strcmp(named->name, "SPI_SHADER_PGM_LO_VS") == 0);
    CHECK(openagc_gfx10_cb_probe_offsets[0] == OPENAGC_GFX10_CB_COLOR0_BASE);
    CHECK(openagc_pm4_copy_data_src_context_abs(OPENAGC_GFX10_CB_COLOR0_BASE) ==
          (OPENAGC_PM4_CONTEXT_REG_START + OPENAGC_GFX10_CB_COLOR0_BASE));

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
    EXPECT(openagc_ib_dump_parse(bad_tag, words, 8u, &info),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(info.dump_parsed == 0u);
    CHECK(info.evidence_qualified == 0u);
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
        test_ib_dump_parse_and_refuse_contracts() != 0) {
        return 1;
    }
    puts("OpenAGC GPU foundation tests passed");
    return 0;
}
