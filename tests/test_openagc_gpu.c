/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/driver.h"

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
    /* Field-derived from the FW9.40 notes and public PM4 layout, not HW capture. */
    const uint32_t expected_dma[7] = {
        0xc0055002u, 0x8c00c000u, 0x00000000u, 0x00000001u,
        0x00001000u, 0x00000001u, 0x00000010u
    };
    const uint32_t expected_submission[17] = {
        0xc0055002u, 0x8c00c000u, 0x00000000u, 0x00000001u,
        0x00001000u, 0x00000001u, 0x00000010u,
        0xc0064900u, 0x06703514u, 0x20000000u, 0x00002000u,
        0x00000001u, 0x00000001u, 0u, 0u, 0xc0001000u, 0u
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
    CHECK(submission.word_count == 17u);
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
    CHECK(submission.submission_id == 2u && submission.word_count == 17u);
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

int main(void)
{
    if (test_device_and_memory() != 0 ||
        test_ownership_and_binding() != 0 ||
        test_packet_words_and_fence() != 0 ||
        test_rejected_commands_and_aliasing() != 0 ||
        test_cross_device_and_alias_copy() != 0) {
        return 1;
    }
    puts("OpenAGC GPU foundation tests passed");
    return 0;
}
