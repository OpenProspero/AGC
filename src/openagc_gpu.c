/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc_gpu_internal.h"
#include "openagc_graphics_internal.h"
#include "openagc_shader_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define OPENAGC_GPU_MAX_BUDGET_BYTES 67108864u
#define OPENAGC_GPU_MAX_ALLOCATIONS 64u
#define OPENAGC_GPU_MAX_BUFFERS 128u
#define OPENAGC_GPU_MAX_COMMAND_BUFFERS 32u
#define OPENAGC_GPU_MAX_COMMANDS 128u
#define OPENAGC_GPU_MAX_WORDS 4096u
#define OPENAGC_GPU_MAX_COPY_BYTES 0x1ffffcu
#define OPENAGC_GPU_MAX_QUEUES 1u
#define OPENAGC_GPU_MAX_FENCES 64u
#define OPENAGC_GPU_DMA_WORDS 7u
#define OPENAGC_GPU_EOP_WORDS 10u
#define OPENAGC_GPU_VA_PAGE UINT64_C(4096)
#define OPENAGC_GPU_VA_LIMIT (UINT64_C(1) << 48)
#define OPENAGC_GPU_VA_START (UINT64_C(1) << 32)

typedef enum openagc_gpu_command_state {
    OPENAGC_GPU_COMMAND_INITIAL,
    OPENAGC_GPU_COMMAND_RECORDING,
    OPENAGC_GPU_COMMAND_EXECUTABLE
} openagc_gpu_command_state;

typedef struct openagc_gpu_copy_record {
    openagc_gpu_buffer *source;
    openagc_gpu_buffer *destination;
    uint64_t source_memory_offset;
    uint64_t destination_memory_offset;
    uint64_t size_bytes;
} openagc_gpu_copy_record;

struct openagc_gpu_buffer {
    openagc_gpu_device *device;
    openagc_gpu_memory *memory;
    uint64_t size_bytes;
    uint64_t memory_offset;
    openagc_gpu_buffer_usage usage;
    uint32_t command_references;
    uint32_t pipeline_references;
};

struct openagc_gpu_command_buffer {
    openagc_gpu_device *device;
    openagc_gpu_copy_record *records;
    uint32_t *words;
    uint32_t max_commands;
    uint32_t max_words;
    uint32_t command_count;
    uint32_t word_count;
    openagc_gpu_command_state state;
};

struct openagc_gpu_queue {
    openagc_gpu_device *device;
    uint32_t *last_words;
    uint32_t last_word_count;
    uint32_t sequence;
};

struct openagc_gpu_fence {
    openagc_gpu_device *device;
    uint64_t marker_va;
    uint64_t submission_id;
    uint32_t expected_value;
    uint32_t observed_value;
    uint32_t signaled;
};

static uint32_t openagc_gpu_pm4_header(uint32_t opcode, uint32_t words,
                                      uint32_t low_bits)
{
    return (3u << 30) | ((words - 2u) << 16) | (opcode << 8) | low_bits;
}

static openagc_result openagc_gpu_reserve_va(const openagc_gpu_device *device,
                                            uint64_t bytes, uint64_t *address,
                                            uint64_t *next_address)
{
    uint64_t span;

    if (bytes == 0u || bytes > UINT64_MAX - (OPENAGC_GPU_VA_PAGE - 1u)) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    span = (bytes + OPENAGC_GPU_VA_PAGE - 1u) & ~(OPENAGC_GPU_VA_PAGE - 1u);
    if (device->next_synthetic_va >= OPENAGC_GPU_VA_LIMIT ||
        span > OPENAGC_GPU_VA_LIMIT - device->next_synthetic_va) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    *address = device->next_synthetic_va;
    *next_address = device->next_synthetic_va + span;
    return OPENAGC_OK;
}

static openagc_result openagc_gpu_encode_dma(uint64_t source, uint64_t destination,
                                            uint64_t bytes, uint32_t words[7])
{
    if (source == 0u || destination == 0u || bytes == 0u ||
        bytes > OPENAGC_GPU_MAX_COPY_BYTES ||
        ((source | destination | bytes) & 3u) != 0u ||
        source >= OPENAGC_GPU_VA_LIMIT || destination >= OPENAGC_GPU_VA_LIMIT ||
        bytes > OPENAGC_GPU_VA_LIMIT - source ||
        bytes > OPENAGC_GPU_VA_LIMIT - destination) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    words[0] = openagc_gpu_pm4_header(0x50u, OPENAGC_GPU_DMA_WORDS, 2u);
    words[1] = (2u << 13) | (1u << 15) | (2u << 25) | (1u << 27) | (1u << 31);
    words[2] = (uint32_t)source;
    words[3] = (uint32_t)(source >> 32);
    words[4] = (uint32_t)destination;
    words[5] = (uint32_t)(destination >> 32);
    words[6] = (uint32_t)bytes;
    return OPENAGC_OK;
}

static openagc_result openagc_gpu_encode_eop(uint64_t address, uint32_t value,
                                            uint32_t words[10])
{
    if (address == 0u || (address & 3u) != 0u ||
        address >= OPENAGC_GPU_VA_LIMIT || value == 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }

    words[0] = openagc_gpu_pm4_header(0x49u, 8u, 0u);
    words[1] = 0x14u | (5u << 8) | (0x703u << 12) | (3u << 25);
    words[2] = 1u << 29;
    words[3] = (uint32_t)address;
    words[4] = (uint32_t)(address >> 32);
    words[5] = value;
    words[6] = 0u;
    words[7] = 0u;
    words[8] = openagc_gpu_pm4_header(0x10u, 2u, 0u);
    words[9] = 0u;
    return OPENAGC_OK;
}

static openagc_result openagc_gpu_memory_range(const openagc_gpu_memory *memory,
                                              uint64_t offset, uint64_t bytes)
{
    if (bytes == 0u || offset > memory->size_bytes ||
        bytes > memory->size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return OPENAGC_OK;
}

static void openagc_gpu_release_records(openagc_gpu_command_buffer *command_buffer)
{
    uint32_t i;

    for (i = 0u; i < command_buffer->command_count; ++i) {
        command_buffer->records[i].source->command_references--;
        command_buffer->records[i].destination->command_references--;
    }
    command_buffer->command_count = 0u;
    command_buffer->word_count = 0u;
}

openagc_result openagc_gpu_device_create(openagc_context *context,
                                         const openagc_gpu_device_desc *desc,
                                         openagc_gpu_device **out_device)
{
    openagc_gpu_device *device;

    if (out_device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_device = NULL;
    if (context == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_GPU_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (context->backend != OPENAGC_BACKEND_HOST_REFERENCE) {
        return OPENAGC_ERROR_UNSUPPORTED_FIRMWARE;
    }
    if (desc->memory_budget_bytes < 4u ||
        desc->memory_budget_bytes > OPENAGC_GPU_MAX_BUDGET_BYTES ||
        (desc->memory_budget_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (context->device_count == (size_t)-1) {
        return OPENAGC_ERROR_OVERFLOW;
    }

    device = (openagc_gpu_device *)calloc(1u, sizeof(*device));
    if (device == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    device->context = context;
    device->memory_budget_bytes = desc->memory_budget_bytes;
    device->next_synthetic_va = OPENAGC_GPU_VA_START;
    context->device_count++;
    *out_device = device;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_device_destroy(openagc_gpu_device *device)
{
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->allocation_count != 0u || device->buffer_count != 0u ||
        device->command_buffer_count != 0u || device->queue_count != 0u ||
        device->fence_count != 0u || device->image_count != 0u ||
        device->graphics_command_buffer_count != 0u ||
        device->shader_artifact_count != 0u ||
        device->shader_pipeline_plan_count != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    device->context->device_count--;
    free(device);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_device_get_capabilities(
    const openagc_gpu_device *device, openagc_gpu_capabilities *capabilities)
{
    if (device == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->backend = OPENAGC_BACKEND_HOST_REFERENCE;
    capabilities->gpu_execution = 0u;
    capabilities->video_output = 0u;
    capabilities->host_copy_simulation = 1u;
    capabilities->reference_pm4_encoding = 1u;
    capabilities->memory_budget_bytes = device->memory_budget_bytes;
    capabilities->max_allocation_bytes = OPENAGC_GPU_MAX_ALLOCATION_BYTES;
    capabilities->max_allocations = OPENAGC_GPU_MAX_ALLOCATIONS;
    capabilities->max_buffers = OPENAGC_GPU_MAX_BUFFERS;
    capabilities->max_command_buffers = OPENAGC_GPU_MAX_COMMAND_BUFFERS;
    capabilities->max_commands_per_buffer = OPENAGC_GPU_MAX_COMMANDS;
    capabilities->max_command_words = OPENAGC_GPU_MAX_WORDS;
    capabilities->max_copy_bytes = OPENAGC_GPU_MAX_COPY_BYTES;
    capabilities->max_queues = OPENAGC_GPU_MAX_QUEUES;
    capabilities->max_fences = OPENAGC_GPU_MAX_FENCES;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_memory_allocate(openagc_gpu_device *device,
                                           const openagc_gpu_memory_desc *desc,
                                           openagc_gpu_memory **out_memory)
{
    openagc_gpu_memory *memory;
    uint64_t address;
    uint64_t next_address;
    openagc_result result;

    if (out_memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->memory_class != OPENAGC_GPU_MEMORY_HOST_REFERENCE) {
        return desc->memory_class == OPENAGC_GPU_MEMORY_DEVICE_LOCAL
                   ? OPENAGC_ERROR_UNSUPPORTED_OPERATION
                   : OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->size_bytes < 4u ||
        desc->size_bytes > OPENAGC_GPU_MAX_ALLOCATION_BYTES ||
        (desc->size_bytes & 3u) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (device->allocation_count >= OPENAGC_GPU_MAX_ALLOCATIONS ||
        desc->size_bytes > device->memory_budget_bytes - device->allocated_bytes) {
        return OPENAGC_ERROR_CAPACITY;
    }
    result = openagc_gpu_reserve_va(device, desc->size_bytes, &address, &next_address);
    if (result != OPENAGC_OK) {
        return result;
    }

    memory = (openagc_gpu_memory *)calloc(1u, sizeof(*memory));
    if (memory == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    memory->bytes = (uint8_t *)calloc((size_t)desc->size_bytes, 1u);
    if (memory->bytes == NULL) {
        free(memory);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    memory->device = device;
    memory->size_bytes = desc->size_bytes;
    memory->synthetic_va = address;
    device->next_synthetic_va = next_address;
    device->allocated_bytes += desc->size_bytes;
    device->allocation_count++;
    *out_memory = memory;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_memory_destroy(openagc_gpu_memory *memory)
{
    openagc_gpu_device *device;

    if (memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (memory->bound_buffers != 0u || memory->bound_images != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    device = memory->device;
    device->allocated_bytes -= memory->size_bytes;
    device->allocation_count--;
    free(memory->bytes);
    free(memory);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_memory_write(openagc_gpu_memory *memory,
                                       uint64_t offset, const void *data,
                                       uint64_t size_bytes)
{
    openagc_result result;

    if (memory == NULL || data == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_gpu_memory_range(memory, offset, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    memcpy(memory->bytes + (size_t)offset, data, (size_t)size_bytes);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_memory_read(const openagc_gpu_memory *memory,
                                      uint64_t offset, void *data,
                                      uint64_t size_bytes)
{
    openagc_result result;

    if (memory == NULL || data == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_gpu_memory_range(memory, offset, size_bytes);
    if (result != OPENAGC_OK) {
        return result;
    }
    memcpy(data, memory->bytes + (size_t)offset, (size_t)size_bytes);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_buffer_read(const openagc_gpu_buffer *buffer,
                                       uint64_t offset, void *data,
                                       uint64_t size_bytes)
{
    if (buffer == NULL || data == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (size_bytes == 0u || offset > buffer->size_bytes ||
        size_bytes > buffer->size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    return openagc_gpu_memory_read(buffer->memory, buffer->memory_offset + offset, data,
                                   size_bytes);
}

openagc_result openagc_gpu_buffer_create(openagc_gpu_device *device,
                                         const openagc_gpu_buffer_desc *desc,
                                         openagc_gpu_buffer **out_buffer)
{
    openagc_gpu_buffer *buffer;

    if (out_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_buffer = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->size_bytes < 4u ||
        desc->size_bytes > OPENAGC_GPU_MAX_ALLOCATION_BYTES ||
        (desc->size_bytes & 3u) != 0u || desc->usage == 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if ((desc->usage & ~(OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT |
                         OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT |
                         OPENAGC_GPU_BUFFER_SHADER_READ_BIT |
                         OPENAGC_GPU_BUFFER_VERTEX_BIT |
                         OPENAGC_GPU_BUFFER_INDEX_BIT |
                         OPENAGC_GPU_BUFFER_INDIRECT_BIT)) != 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (device->buffer_count >= OPENAGC_GPU_MAX_BUFFERS) {
        return OPENAGC_ERROR_CAPACITY;
    }

    buffer = (openagc_gpu_buffer *)calloc(1u, sizeof(*buffer));
    if (buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    buffer->device = device;
    buffer->size_bytes = desc->size_bytes;
    buffer->usage = desc->usage;
    device->buffer_count++;
    *out_buffer = buffer;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_buffer_bind_memory(openagc_gpu_buffer *buffer,
                                              openagc_gpu_memory *memory,
                                              uint64_t memory_offset)
{
    if (buffer == NULL || memory == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->device != memory->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (buffer->memory != NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((memory_offset & 3u) != 0u || memory_offset > memory->size_bytes ||
        buffer->size_bytes > memory->size_bytes - memory_offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    buffer->memory = memory;
    buffer->memory_offset = memory_offset;
    memory->bound_buffers++;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_buffer_unbind_memory(openagc_gpu_buffer *buffer)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (buffer->command_references != 0u || buffer->pipeline_references != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    buffer->memory->bound_buffers--;
    buffer->memory = NULL;
    buffer->memory_offset = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_buffer_destroy(openagc_gpu_buffer *buffer)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->command_references != 0u ||
        buffer->pipeline_references != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (buffer->memory != NULL) {
        buffer->memory->bound_buffers--;
    }
    buffer->device->buffer_count--;
    free(buffer);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_create(
    openagc_gpu_device *device, const openagc_gpu_command_buffer_desc *desc,
    openagc_gpu_command_buffer **out_command_buffer)
{
    openagc_gpu_command_buffer *command_buffer;

    if (out_command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_command_buffer = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->max_commands == 0u || desc->max_commands > OPENAGC_GPU_MAX_COMMANDS ||
        desc->max_words < OPENAGC_GPU_DMA_WORDS ||
        desc->max_words > OPENAGC_GPU_MAX_WORDS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (device->command_buffer_count >= OPENAGC_GPU_MAX_COMMAND_BUFFERS) {
        return OPENAGC_ERROR_CAPACITY;
    }

    command_buffer = (openagc_gpu_command_buffer *)calloc(1u, sizeof(*command_buffer));
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    command_buffer->records = (openagc_gpu_copy_record *)calloc(
        (size_t)desc->max_commands, sizeof(*command_buffer->records));
    command_buffer->words = (uint32_t *)calloc(
        (size_t)desc->max_words, sizeof(*command_buffer->words));
    if (command_buffer->records == NULL || command_buffer->words == NULL) {
        free(command_buffer->words);
        free(command_buffer->records);
        free(command_buffer);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    command_buffer->device = device;
    command_buffer->max_commands = desc->max_commands;
    command_buffer->max_words = desc->max_words;
    device->command_buffer_count++;
    *out_command_buffer = command_buffer;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_destroy(openagc_gpu_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    openagc_gpu_release_records(command_buffer);
    command_buffer->device->command_buffer_count--;
    free(command_buffer->words);
    free(command_buffer->records);
    free(command_buffer);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_begin(openagc_gpu_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GPU_COMMAND_INITIAL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->state = OPENAGC_GPU_COMMAND_RECORDING;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_copy_buffer(
    openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_buffer *source, uint64_t source_offset,
    openagc_gpu_buffer *destination, uint64_t destination_offset,
    uint64_t size_bytes)
{
    uint64_t source_memory_offset;
    uint64_t destination_memory_offset;
    uint32_t packet[OPENAGC_GPU_DMA_WORDS];
    openagc_gpu_copy_record *record;
    openagc_result result;

    if (command_buffer == NULL || source == NULL || destination == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GPU_COMMAND_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (source->device != command_buffer->device ||
        destination->device != command_buffer->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (source->memory == NULL || destination->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((source->usage & OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT) == 0u ||
        (destination->usage & OPENAGC_GPU_BUFFER_COPY_DESTINATION_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (size_bytes == 0u || size_bytes > OPENAGC_GPU_MAX_COPY_BYTES ||
        ((source_offset | destination_offset | size_bytes) & 3u) != 0u ||
        source_offset > source->size_bytes ||
        size_bytes > source->size_bytes - source_offset ||
        destination_offset > destination->size_bytes ||
        size_bytes > destination->size_bytes - destination_offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    source_memory_offset = source->memory_offset + source_offset;
    destination_memory_offset = destination->memory_offset + destination_offset;
    if (source->memory == destination->memory &&
        source_memory_offset < destination_memory_offset + size_bytes &&
        destination_memory_offset < source_memory_offset + size_bytes) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_graphics_image_copy_access(
        command_buffer->device, source->memory, source_memory_offset, size_bytes,
        OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_graphics_image_copy_access(
        command_buffer->device, destination->memory, destination_memory_offset,
        size_bytes, OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (command_buffer->command_count == command_buffer->max_commands ||
        command_buffer->word_count > command_buffer->max_words - OPENAGC_GPU_DMA_WORDS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    if (source->command_references == UINT32_MAX ||
        destination->command_references >=
            (source == destination ? UINT32_MAX - 1u : UINT32_MAX)) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    result = openagc_gpu_encode_dma(source->memory->synthetic_va + source_memory_offset,
                                   destination->memory->synthetic_va +
                                       destination_memory_offset,
                                   size_bytes, packet);
    if (result != OPENAGC_OK) {
        return result;
    }

    record = &command_buffer->records[command_buffer->command_count];
    record->source = source;
    record->destination = destination;
    record->source_memory_offset = source_memory_offset;
    record->destination_memory_offset = destination_memory_offset;
    record->size_bytes = size_bytes;
    memcpy(&command_buffer->words[command_buffer->word_count],
           packet, sizeof(packet));
    source->command_references++;
    destination->command_references++;
    command_buffer->command_count++;
    command_buffer->word_count += OPENAGC_GPU_DMA_WORDS;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_end(openagc_gpu_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state != OPENAGC_GPU_COMMAND_RECORDING) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    command_buffer->state = OPENAGC_GPU_COMMAND_EXECUTABLE;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_reset(openagc_gpu_command_buffer *command_buffer)
{
    if (command_buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (command_buffer->state == OPENAGC_GPU_COMMAND_INITIAL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    openagc_gpu_release_records(command_buffer);
    command_buffer->state = OPENAGC_GPU_COMMAND_INITIAL;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_command_buffer_get_recording(
    const openagc_gpu_command_buffer *command_buffer,
    openagc_gpu_recording_view *view)
{
    if (command_buffer == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (view->struct_size != sizeof(*view)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (command_buffer->state != OPENAGC_GPU_COMMAND_EXECUTABLE) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    view->command_count = command_buffer->command_count;
    view->word_count = command_buffer->word_count;
    view->words = command_buffer->words;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_queue_create(openagc_gpu_device *device,
                                        const openagc_gpu_queue_desc *desc,
                                        openagc_gpu_queue **out_queue)
{
    openagc_gpu_queue *queue;

    if (out_queue == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_queue = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (desc->family != OPENAGC_GPU_QUEUE_COPY) {
        return desc->family == OPENAGC_GPU_QUEUE_GRAPHICS ||
                       desc->family == OPENAGC_GPU_QUEUE_COMPUTE
                   ? OPENAGC_ERROR_UNSUPPORTED_OPERATION
                   : OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->queue_count >= OPENAGC_GPU_MAX_QUEUES) {
        return OPENAGC_ERROR_CAPACITY;
    }
    queue = (openagc_gpu_queue *)calloc(1u, sizeof(*queue));
    if (queue == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    queue->last_words = (uint32_t *)calloc(
        OPENAGC_GPU_MAX_WORDS + OPENAGC_GPU_EOP_WORDS, sizeof(*queue->last_words));
    if (queue->last_words == NULL) {
        free(queue);
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    queue->device = device;
    device->queue_count++;
    *out_queue = queue;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_queue_destroy(openagc_gpu_queue *queue)
{
    if (queue == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    queue->device->queue_count--;
    free(queue->last_words);
    free(queue);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_queue_submit(openagc_gpu_queue *queue,
                                       openagc_gpu_command_buffer *command_buffer,
                                       openagc_gpu_fence *fence)
{
    uint32_t eop[OPENAGC_GPU_EOP_WORDS];
    uint32_t sequence;
    uint32_t i;
    openagc_result result;

    if (queue == NULL || command_buffer == NULL || fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (queue->device != command_buffer->device ||
        queue->device != fence->device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (command_buffer->state != OPENAGC_GPU_COMMAND_EXECUTABLE ||
        command_buffer->command_count == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if (fence->signaled != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    if (queue->sequence == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    if (command_buffer->word_count > OPENAGC_GPU_MAX_WORDS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    sequence = queue->sequence + 1u;
    result = openagc_gpu_encode_eop(fence->marker_va, sequence, eop);
    if (result != OPENAGC_OK) {
        return result;
    }
    for (i = 0u; i < command_buffer->command_count; ++i) {
        const openagc_gpu_copy_record *record = &command_buffer->records[i];

        result = openagc_graphics_image_copy_access(
            queue->device, record->source->memory, record->source_memory_offset,
            record->size_bytes, OPENAGC_GRAPHICS_STATE_TRANSFER_SOURCE);
        if (result != OPENAGC_OK) {
            return result;
        }
        result = openagc_graphics_image_copy_access(
            queue->device, record->destination->memory,
            record->destination_memory_offset, record->size_bytes,
            OPENAGC_GRAPHICS_STATE_TRANSFER_DESTINATION);
        if (result != OPENAGC_OK) {
            return result;
        }
    }

    memcpy(queue->last_words, command_buffer->words,
           (size_t)command_buffer->word_count * sizeof(uint32_t));
    memcpy(&queue->last_words[command_buffer->word_count], eop, sizeof(eop));
    for (i = 0u; i < command_buffer->command_count; ++i) {
        const openagc_gpu_copy_record *record = &command_buffer->records[i];
        memcpy(record->destination->memory->bytes +
                   (size_t)record->destination_memory_offset,
               record->source->memory->bytes +
                   (size_t)record->source_memory_offset,
               (size_t)record->size_bytes);
    }
    queue->last_word_count = command_buffer->word_count + OPENAGC_GPU_EOP_WORDS;
    queue->sequence = sequence;
    fence->submission_id = sequence;
    fence->expected_value = sequence;
    fence->observed_value = sequence;
    fence->signaled = 1u;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_queue_get_last_submission(
    const openagc_gpu_queue *queue, openagc_gpu_submission_view *view)
{
    if (queue == NULL || view == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (view->struct_size != sizeof(*view)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (queue->sequence == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    view->word_count = queue->last_word_count;
    view->submission_id = queue->sequence;
    view->gpu_submitted = 0u;
    view->words = queue->last_words;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_fence_create(openagc_gpu_device *device,
                                       openagc_gpu_fence **out_fence)
{
    openagc_gpu_fence *fence;
    uint64_t address;
    uint64_t next_address;
    openagc_result result;

    if (out_fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_fence = NULL;
    if (device == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->fence_count >= OPENAGC_GPU_MAX_FENCES) {
        return OPENAGC_ERROR_CAPACITY;
    }
    result = openagc_gpu_reserve_va(device, 4u, &address, &next_address);
    if (result != OPENAGC_OK) {
        return result;
    }
    fence = (openagc_gpu_fence *)calloc(1u, sizeof(*fence));
    if (fence == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    fence->device = device;
    fence->marker_va = address;
    device->next_synthetic_va = next_address;
    device->fence_count++;
    *out_fence = fence;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_fence_destroy(openagc_gpu_fence *fence)
{
    if (fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    fence->device->fence_count--;
    free(fence);
    return OPENAGC_OK;
}

openagc_result openagc_gpu_fence_poll(const openagc_gpu_fence *fence,
                                     openagc_gpu_fence_info *info)
{
    if (fence == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->signaled = fence->signaled;
    info->expected_value = fence->expected_value;
    info->observed_value = fence->observed_value;
    info->submission_id = fence->submission_id;
    return fence->signaled != 0u ? OPENAGC_OK : OPENAGC_ERROR_NOT_READY;
}

openagc_result openagc_gpu_fence_reset(openagc_gpu_fence *fence)
{
    if (fence == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (fence->signaled == 0u) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    fence->signaled = 0u;
    fence->expected_value = 0u;
    fence->observed_value = 0u;
    fence->submission_id = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_gpu_shader_buffer_validate(
    const openagc_gpu_device *device, const openagc_gpu_buffer *buffer,
    uint64_t offset, uint64_t size_bytes, uint32_t minimum_bytes)
{
    if (buffer == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (buffer->device != device) {
        return OPENAGC_ERROR_OWNERSHIP;
    }
    if (buffer->memory == NULL) {
        return OPENAGC_ERROR_BAD_STATE;
    }
    if ((buffer->usage & OPENAGC_GPU_BUFFER_SHADER_READ_BIT) == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (size_bytes == 0u || size_bytes < minimum_bytes ||
        ((offset | size_bytes) & 3u) != 0u ||
        offset > buffer->size_bytes ||
        size_bytes > buffer->size_bytes - offset) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    if (buffer->pipeline_references == UINT32_MAX) {
        return OPENAGC_ERROR_OVERFLOW;
    }
    return OPENAGC_OK;
}

void openagc_gpu_shader_buffer_retain(openagc_gpu_buffer *buffer)
{
    buffer->pipeline_references++;
}

void openagc_gpu_shader_buffer_release(openagc_gpu_buffer *buffer)
{
    buffer->pipeline_references--;
}
