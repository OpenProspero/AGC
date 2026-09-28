/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PS5_GPU_H
#define OPENAGC_PS5_GPU_H

#include "openagc/pm4_agc_completion_fw940.h"
#include "openagc/raster.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_PS5_GPU_API_VERSION 1u
#define OPENAGC_PS5_GPU_MAX_WAIT_MS 2000u
#define OPENAGC_PS5_GPU_MAX_WORDS \
    (OPENAGC_RASTER_MAX_WORDS + OPENAGC_PM4_AGC_COMPLETION_WORDS)

typedef struct openagc_ps5_gpu openagc_ps5_gpu;

typedef struct openagc_ps5_gpu_draw {
    uint32_t struct_size;
    uint32_t api_version;
    const openagc_raster_gpu_draw *draw;
    /* Caller-owned, GPU-readable direct memory. Keep it, the shaders,
     * target and both register tables mapped until completion. */
    uint32_t *words;
    uint32_t word_capacity;
    /* A separate, GPU-writable direct-memory dword; its VA is its address. */
    volatile uint32_t *marker;
    /* CPU mappings of the GPU-visible VA spans referenced by draw. The
     * backend flushes them before submission and invalidates the target
     * after marker completion. No allocation or shader validation occurs. */
    const void *vertex_code;
    uint32_t vertex_code_bytes;
    const void *fragment_code;
    uint32_t fragment_code_bytes;
    void *color;
    uint32_t color_bytes;
} openagc_ps5_gpu_draw;

typedef struct openagc_ps5_gpu_submission {
    uint32_t struct_size;
    uint32_t word_count;
    uint32_t sequence;
    /* An attempted driver submission, not proof that the GPU accepted it. */
    uint32_t submitted;
    uint32_t completed;
    int32_t platform_error;
} openagc_ps5_gpu_submission;

#define OPENAGC_PS5_GPU_DRAW_INIT \
    { (uint32_t)sizeof(openagc_ps5_gpu_draw), OPENAGC_PS5_GPU_API_VERSION, \
      (const openagc_raster_gpu_draw *)0, (uint32_t *)0, 0u, \
      (volatile uint32_t *)0, (const void *)0, 0u, (const void *)0, \
      0u, (void *)0, 0u }
#define OPENAGC_PS5_GPU_SUBMISSION_INIT \
    { (uint32_t)sizeof(openagc_ps5_gpu_submission), 0u, 0u, 0u, 0u, 0 }

/*
 * Separate from OpenAGC's fail-closed host/PS5 policy ABI. This native
 * target admits only the console-observed FW identity and the qualified
 * DRAW capability. It does not make arbitrary shaders, Vulkan/OpenGL or
 * VideoOut executable; the caller owns GPU-visible memory and code.
 * One bridge instance may be active per process. Calls must be serialized.
 * AGC modules remain loaded after initialization until process exit:
 * unloading the initialized driver was refused in the FW9.40 console test.
 */
openagc_result openagc_ps5_gpu_create(openagc_ps5_gpu **out_gpu,
                                      int32_t *out_platform_error);
/* Once submission is attempted, even a driver error may leave GPU work
 * in flight. Never free or overwrite GPU memory until wait succeeds;
 * destroy and a second submission return BUSY while completion is unknown. */
openagc_result openagc_ps5_gpu_submit_draw(
    openagc_ps5_gpu *gpu, const openagc_ps5_gpu_draw *desc,
    uint32_t timeout_ms, openagc_ps5_gpu_submission *submission);
openagc_result openagc_ps5_gpu_wait(openagc_ps5_gpu *gpu, uint32_t timeout_ms,
                                    openagc_ps5_gpu_submission *submission);
openagc_result openagc_ps5_gpu_destroy(openagc_ps5_gpu *gpu);

#ifdef __cplusplus
}
#endif

#endif
