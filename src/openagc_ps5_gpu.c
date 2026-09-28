/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/ps5_gpu.h"
#include "openagc/ps5_policy.h"
#include "openagc/pm4_agc_completion_fw940.h"

#include <openprospero/firmware.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#if !defined(__x86_64__)
#error "The PS5 AGC bridge requires x86_64 cache instructions"
#endif

typedef struct openagc_agc_description {
    void *words;
    uint32_t word_count;
    uint8_t flag;
    uint8_t padding[3];
} openagc_agc_description;

struct openagc_ps5_gpu {
    void *agc_module;
    void *driver_module;
    int32_t (*submit)(void *);
    int32_t (*suspend_point)(void);
    volatile uint32_t *marker;
    void *color;
    uint32_t color_bytes;
    uint32_t sequence;
    uint32_t word_count;
    uint32_t pending;
    uint32_t initialized;
    uint32_t active;
};

static openagc_ps5_gpu gpu_instance;

static void openagc_ps5_gpu_flush(const void *data, size_t bytes)
{
    const uintptr_t first = (uintptr_t)data & ~(uintptr_t)63u;
    const uintptr_t last = (uintptr_t)data + bytes;
    uintptr_t address;

    for (address = first; address < last; address += 64u) {
        __asm__ volatile("clflush (%0)" : : "r"(address) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
}

static int32_t (*openagc_ps5_gpu_symbol(void *module, const char *name))(void)
{
    int32_t (*function)(void) = NULL;
    void *address = dlsym(module, name);

    if (sizeof(function) != sizeof(address)) return NULL;
    memcpy(&function, &address, sizeof(function));
    return function;
}

openagc_result openagc_ps5_gpu_create(openagc_ps5_gpu **out_gpu,
                                      int32_t *out_platform_error)
{
    openagc_ps5_gpu *gpu;
    int32_t (*init)(uint32_t) = NULL;
    int32_t rc;
    openagc_result result;

    if (out_gpu == NULL) return OPENAGC_ERROR_INVALID_ARGUMENT;
    *out_gpu = NULL;
    if (out_platform_error != NULL) *out_platform_error = 0;
    result = openagc_ps5_policy_require(op_ps5_system_firmware_version(),
                                       OPENAGC_PS5_CAP_DRAW);
    if (result != OPENAGC_OK) return result;
    gpu = &gpu_instance;
    if (gpu->active) return OPENAGC_ERROR_BUSY;
    if (gpu->initialized) {
        gpu->active = 1u;
        *out_gpu = gpu;
        return OPENAGC_OK;
    }
    memset(gpu, 0, sizeof(*gpu));
    gpu->agc_module = dlopen("libSceAgc.sprx", RTLD_NOW | RTLD_LOCAL);
    if (gpu->agc_module == NULL) {
        return OPENAGC_ERROR_NOT_READY;
    }
    gpu->driver_module = dlopen("libSceAgcDriver.sprx", RTLD_NOW | RTLD_LOCAL);
    if (gpu->driver_module == NULL) {
        (void)dlclose(gpu->agc_module);
        gpu->agc_module = NULL;
        return OPENAGC_ERROR_NOT_READY;
    }
    {
        int32_t (*function)(void) = openagc_ps5_gpu_symbol(gpu->agc_module,
                                                           "sceAgcInit");
        memcpy(&init, &function, sizeof(init));
    }
    {
        int32_t (*function)(void) = openagc_ps5_gpu_symbol(gpu->agc_module,
                                                           "sceAgcSuspendPoint");
        memcpy(&gpu->suspend_point, &function, sizeof(function));
    }
    {
        int32_t (*function)(void) = openagc_ps5_gpu_symbol(gpu->driver_module,
                                                           "sceAgcDriverSubmitDcb");
        memcpy(&gpu->submit, &function, sizeof(function));
    }
    if (init == NULL || gpu->submit == NULL || gpu->suspend_point == NULL) {
        (void)dlclose(gpu->driver_module);
        (void)dlclose(gpu->agc_module);
        memset(gpu, 0, sizeof(*gpu));
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    rc = init(8u);
    if (rc != 0) {
        if (out_platform_error != NULL) *out_platform_error = rc;
        (void)dlclose(gpu->driver_module);
        (void)dlclose(gpu->agc_module);
        memset(gpu, 0, sizeof(*gpu));
        return OPENAGC_ERROR_NOT_READY;
    }
    gpu->initialized = 1u;
    gpu->active = 1u;
    *out_gpu = gpu;
    return OPENAGC_OK;
}

openagc_result openagc_ps5_gpu_wait(openagc_ps5_gpu *gpu, uint32_t timeout_ms,
                                    openagc_ps5_gpu_submission *submission)
{
    uint32_t attempt;

    if (gpu == NULL || submission == NULL) return OPENAGC_ERROR_INVALID_ARGUMENT;
    if (gpu != &gpu_instance || !gpu->active) return OPENAGC_ERROR_BAD_STATE;
    if (submission->struct_size != sizeof(*submission)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (timeout_ms > OPENAGC_PS5_GPU_MAX_WAIT_MS) return OPENAGC_ERROR_OUT_OF_RANGE;
    if (!gpu->pending) return OPENAGC_ERROR_BAD_STATE;
    submission->word_count = gpu->word_count;
    submission->sequence = gpu->sequence;
    submission->submitted = 1u;
    submission->completed = 0u;

    for (attempt = 0u; attempt <= timeout_ms; ++attempt) {
        uint32_t value;

        openagc_ps5_gpu_flush((const void *)gpu->marker, sizeof(*gpu->marker));
        value = *gpu->marker;
        if (value == gpu->sequence) {
            openagc_ps5_gpu_flush(gpu->color, gpu->color_bytes);
            gpu->pending = 0u;
            gpu->marker = NULL;
            gpu->color = NULL;
            gpu->color_bytes = 0u;
            submission->completed = 1u;
            submission->platform_error = 0;
            return OPENAGC_OK;
        }
        if (value != 0u) return OPENAGC_ERROR_INTEGRITY;
        if (attempt != timeout_ms) usleep(1000u);
    }
    return OPENAGC_ERROR_NOT_READY;
}

openagc_result openagc_ps5_gpu_submit_draw(
    openagc_ps5_gpu *gpu, const openagc_ps5_gpu_draw *desc,
    uint32_t timeout_ms, openagc_ps5_gpu_submission *submission)
{
    openagc_raster_gpu_draw draw;
    openagc_agc_description description;
    uint32_t count;
    int32_t rc;

    if (gpu == NULL || desc == NULL || submission == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (gpu != &gpu_instance || !gpu->active) return OPENAGC_ERROR_BAD_STATE;
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_PS5_GPU_API_VERSION ||
        submission->struct_size != sizeof(*submission)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    if (gpu->pending) return OPENAGC_ERROR_BUSY;
    if (timeout_ms > OPENAGC_PS5_GPU_MAX_WAIT_MS) return OPENAGC_ERROR_OUT_OF_RANGE;
    submission->word_count = 0u;
    submission->sequence = 0u;
    submission->submitted = 0u;
    submission->completed = 0u;
    submission->platform_error = 0;
    if (desc->draw == NULL || desc->words == NULL || desc->marker == NULL ||
        desc->vertex_code == NULL || desc->fragment_code == NULL ||
        desc->color == NULL ||
        ((uintptr_t)desc->words & 3u) != 0u ||
        ((uintptr_t)desc->marker & 7u) != 0u ||
        desc->draw->struct_size != sizeof(draw) ||
        desc->word_capacity <= OPENAGC_PM4_AGC_COMPLETION_WORDS ||
        desc->word_capacity > OPENAGC_PS5_GPU_MAX_WORDS ||
        desc->draw->append_eop != 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (gpu->sequence == UINT32_MAX) return OPENAGC_ERROR_OVERFLOW;
    draw = *desc->draw;
    if ((uint64_t)(uintptr_t)desc->vertex_code != draw.vertex_code_va ||
        (uint64_t)(uintptr_t)desc->fragment_code != draw.fragment_code_va ||
        (uint64_t)(uintptr_t)desc->color != draw.color_va ||
        (uint64_t)(uintptr_t)draw.context_table != draw.context_table_va ||
        (uint64_t)(uintptr_t)draw.uconfig_table != draw.uconfig_table_va ||
        desc->vertex_code_bytes == 0u || desc->vertex_code_bytes > 65536u ||
        desc->fragment_code_bytes == 0u || desc->fragment_code_bytes > 65536u ||
        desc->color_bytes == 0u || desc->color_bytes > 64u * 1024u * 1024u ||
        (uint64_t)desc->color_bytes <
            ((uint64_t)draw.color_width * 4u + 255u) / 256u * 256u *
                draw.color_height) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    draw.marker_va = 0u;
    draw.sequence = 0u;
    count = openagc_raster_encode_draw(&draw, desc->words,
                desc->word_capacity - OPENAGC_PM4_AGC_COMPLETION_WORDS);
    if (count == 0u) return OPENAGC_ERROR_OUT_OF_RANGE;

    ++gpu->sequence;
    openagc_pm4_encode_agc_completion((uint64_t)(uintptr_t)desc->marker,
                                       gpu->sequence, desc->words + count);
    count += OPENAGC_PM4_AGC_COMPLETION_WORDS;
    *desc->marker = 0u;
    openagc_ps5_gpu_flush((const void *)desc->marker, sizeof(*desc->marker));
    openagc_ps5_gpu_flush(draw.context_table,
                         OPENAGC_PM4_NGG_TABLE_WORDS * sizeof(uint32_t));
    openagc_ps5_gpu_flush(draw.uconfig_table,
                         OPENAGC_RASTER_UCONFIG_TABLE_WORDS * sizeof(uint32_t));
    openagc_ps5_gpu_flush(desc->vertex_code, desc->vertex_code_bytes);
    openagc_ps5_gpu_flush(desc->fragment_code, desc->fragment_code_bytes);
    openagc_ps5_gpu_flush(desc->color, desc->color_bytes);
    openagc_ps5_gpu_flush(desc->words, (size_t)count * sizeof(uint32_t));
    description.words = desc->words;
    description.word_count = count;
    description.flag = 0u;
    memset(description.padding, 0, sizeof(description.padding));
    gpu->pending = 1u;
    gpu->marker = desc->marker;
    gpu->color = desc->color;
    gpu->color_bytes = desc->color_bytes;
    gpu->word_count = count;
    submission->word_count = count;
    submission->sequence = gpu->sequence;
    submission->submitted = 1u;
    rc = gpu->submit(&description);
    if (rc != 0) {
        submission->platform_error = rc;
        return OPENAGC_ERROR_NOT_READY;
    }
    rc = gpu->suspend_point();
    if (rc != 0) {
        submission->platform_error = rc;
        return OPENAGC_ERROR_NOT_READY;
    }
    return openagc_ps5_gpu_wait(gpu, timeout_ms, submission);
}

openagc_result openagc_ps5_gpu_destroy(openagc_ps5_gpu *gpu)
{
    if (gpu == NULL) return OPENAGC_ERROR_INVALID_ARGUMENT;
    if (gpu != &gpu_instance || !gpu->active) return OPENAGC_ERROR_BAD_STATE;
    if (gpu->pending) return OPENAGC_ERROR_BUSY;
    gpu->active = 0u;
    return OPENAGC_OK;
}
