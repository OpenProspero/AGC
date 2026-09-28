/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Step AN: the AGC-shaped raster draw with native ESGS ring item size.
 *
 * Step AD's NGG vehicle routed two of the AGC linker's three linkage
 * records through the context table: the user-VGPR enable (a uconfig
 * index) and VGT_PRIMITIVE_TYPE, whose write used the GFX7/GFX9 indexed
 * form. The public native runtime and Mesa say otherwise:
 *
 *   - src/platform/ps5_agc_native_runtime.c (release commit
 *     6cb291abea32281571c49705735046425cf000fd) has sceAgcLinkShaders
 *     write 34 context records and three uconfig records, then loads
 *     them with set_cx/set_uc; the uconfig records are GE_CNTL (0x25b),
 *     SPI_SHADER_USER_VGPR_EN (0x262) and VGT_PRIMITIVE_TYPE (0x242,
 *     value 4 for a triangle list in PS5_Vulkan's public C1 capture);
 *   - Mesa si_state_draw.cpp si_emit_draw_registers writes
 *     VGT_PRIMITIVE_TYPE with the *plain* uconfig packet for
 *     GFX_VERSION >= GFX10 and only uses the indexed form for GFX7-9.
 *
 * This payload submits the complete draw the shared rasterizer composes
 * (include/openagc/raster.h, src/openagc_raster.c): the proven scalar
 * state, the NGG vertex program and pixel program from the pinned
 * fixtures, the AGC-shaped uconfig table, the color bind and one
 * DRAW_INDEX_AUTO of three vertices, with the state of every register
 * read back into the arena. Mesa gfx10.json identifies context 0x2ab as
 * VGT_ESGS_RING_ITEMSIZE, where the successful PS5_Vulkan C1 capture has 1.
 * The pinned fixture has 0. The earlier probe read context 0x2d3 under the
 * wrong name; that is GE_NGG_SUBGRP_CNTL. This run changes just 0x2ab to 1
 * in the borrowed vertex table and reads the actual register before DRAW.
 * Acceptance is a pixel: the color window must
 * hold the pixel shader's export and nothing else, and nothing may land
 * outside the rect. One submit, one deadline, no retry.
 */

#include "openagc/pm4_ib_dump_fw940.h"
#include "openagc/pm4_agc_completion_fw940.h"
#include "openagc/raster.h"
#if OPENAGC_AGC_COMPLETION
#include "openagc/ps5_policy.h"
#endif
#if OPENAGC_GPU_BRIDGE
#include "openagc/ps5_gpu.h"
#include <openprospero/firmware.h>
extern int sceKernelGetProsperoSystemSwVersion(void *);
#endif

#include "ngg_smoke_tables.h"

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#if !OPENAGC_GPU_BRIDGE
#include <sys/ioctl.h>
#endif
#include <sys/mman.h>
#include <sys/time.h>
#if OPENAGC_APP_BRIDGE
#include <openprospero/udp_log.h>
#ifndef OPENAGC_LOG_HOST_IP
#error "The application UDP log requires an explicit receiver IPv4 address"
#endif
#define OPENAGC_UDP_LOG_PORT 9999u
#endif
#if OPENAGC_AGC_SUBMIT
#include <dlfcn.h>
#endif
#if OPENAGC_AGC_COMPLETION
extern uint32_t kernel_get_fw_version(void);
#endif

extern int sceKernelAllocateMainDirectMemory(size_t len, size_t alignment,
                                             int memory_type, off_t *physical);
extern int sceKernelMapNamedDirectMemory(void **address, size_t len,
                                         int protection, int flags,
                                         off_t physical, size_t alignment,
                                         const char *name);

#define OPENAGC_SUBMIT_16 0xC0108102u
#define OPENAGC_CONTEXT_QUERY 0xC004812Eu

#define OPENAGC_ARENA (256u * 1024u)
#define OPENAGC_VERT_CODE_OFF 0x0000u
#define OPENAGC_FRAG_CODE_OFF 0x0400u
#define OPENAGC_BASELINE_OFF 0x1000u
#define OPENAGC_PROBE_OFF 0x1100u
#define OPENAGC_NGG_PROBE_OFF 0x1200u
#define OPENAGC_CONTEXT_TABLE_OFF 0x1400u
#define OPENAGC_UCONFIG_TABLE_OFF 0x1a00u
#define OPENAGC_CB_PROBE_OFF 0x1b00u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x3000u
#define OPENAGC_IB_BYTES (OPENAGC_CB_OFF - OPENAGC_IB_OFF)
#if OPENAGC_IB_BYTES < 4096u
#error "the IB window must hold the largest draw the encoder composes"
#endif
#define OPENAGC_MARKER_OFF 0x3800u
/* 256-byte aligned linear color target: 32x32 RGBA8 at a 128-byte pitch. */
#define OPENAGC_COLOR_OFF 0x4000u
#define OPENAGC_COLOR_WIDTH 32u
#define OPENAGC_COLOR_HEIGHT 32u
/* The colour surface's row stride: 256 bytes, twice the 32-pixel row. */
#define OPENAGC_COLOR_PITCH 256u
#define OPENAGC_COLOR_STRIDE_WORDS (OPENAGC_COLOR_PITCH / 4u)
#ifndef OPENAGC_VIEW_X
#define OPENAGC_VIEW_X 8u
#endif
#ifndef OPENAGC_VIEW_Y
#define OPENAGC_VIEW_Y 8u
#endif
#ifndef OPENAGC_VIEW_W
#define OPENAGC_VIEW_W 8u
#endif
#ifndef OPENAGC_VIEW_H
#define OPENAGC_VIEW_H 8u
#endif
#define OPENAGC_VIEW_WORDS (OPENAGC_VIEW_W * OPENAGC_VIEW_H)
#define OPENAGC_EOP_SEQUENCE 1u
#ifndef OPENAGC_AGC_SUBMIT
#define OPENAGC_AGC_SUBMIT 0
#endif
#ifndef OPENAGC_AGC_COMPLETION
#define OPENAGC_AGC_COMPLETION 0
#endif
#ifndef OPENAGC_GPU_BRIDGE
#define OPENAGC_GPU_BRIDGE 0
#endif
#if OPENAGC_AGC_COMPLETION && !OPENAGC_AGC_SUBMIT
#error "AGC completion requires AGC submission"
#endif
#if OPENAGC_GPU_BRIDGE && !OPENAGC_AGC_COMPLETION
#error "the native GPU bridge requires AGC completion"
#endif
#ifndef OPENAGC_POINT_DRAW
#define OPENAGC_POINT_DRAW 0
#endif
#ifndef OPENAGC_GATE_MASK
#define OPENAGC_GATE_MASK OPENAGC_RASTER_GATE_ALL
#endif
#define OPENAGC_DEADLINE_SECONDS 30

#define OPENAGC_LOG_BYTES (OPENAGC_VIEW_WORDS * 9u + 3072u)

#define OPENAGC_PROT_READ 0x01
#define OPENAGC_PROT_WRITE 0x02
#define OPENAGC_PROT_GPU_READ 0x10
#define OPENAGC_PROT_GPU_WRITE 0x20
#define OPENAGC_MAP_NO_COALESCE 0x400000

struct openagc_submit {
    uint32_t queue_type;
    uint32_t num_cbs;
    uint64_t cb_array;
};

struct openagc_cb {
    uint64_t header;
    uint64_t ib_base;
};

#if OPENAGC_AGC_SUBMIT
struct openagc_agc_description {
    void *words;
    uint32_t word_count;
    uint8_t flag;
    uint8_t padding[3];
};
#endif

#ifndef OPENAGC_LOG_PATH
#if OPENAGC_GPU_BRIDGE
#define OPENAGC_LOG_PATH "/data/prosperoai/openagc-gpu-bridge-op-fw940.log"
#elif OPENAGC_AGC_COMPLETION
#define OPENAGC_LOG_PATH "/data/prosperoai/openagc-agc-completion-fw940.log"
#else
#define OPENAGC_LOG_PATH "/data/prosperoai/openagc-ib-dump-draw-agc-rows.log"
#endif
#endif
#if OPENAGC_APP_BRIDGE
static int openagc_log_bytes(const char *bytes, size_t length)
{
    return op_ps5_udp_log_write(bytes, length);
}
#else
static const char openagc_log_path[] = OPENAGC_LOG_PATH;

static int openagc_log_bytes(const char *bytes, size_t length)
{
    FILE *handle = fopen(openagc_log_path, "w");

    if (handle == NULL) {
        return -1;
    }
    if (fwrite(bytes, 1, length, handle) != length) {
        fclose(handle);
        return -1;
    }
    fclose(handle);
    return 0;
}
#endif

static int openagc_logf(const char *fmt, ...)
{
    char line[512];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(line)) {
        return -1;
    }
    return openagc_log_bytes(line, strlen(line));
}

static int openagc_write_report(int completed, uint64_t color_va,
                                uint32_t pixels, uint32_t outside,
                                uint32_t guard, uint32_t value,
                                int wait_seconds, int match, uint32_t ib_words,
                                uint32_t topology, uint32_t gs_out,
                                uint32_t target_nonzero, uint32_t target_expected,
                                uint32_t bbox_min_x, uint32_t bbox_min_y,
                                uint32_t bbox_max_x, uint32_t bbox_max_y,
                                uint32_t target_first, const uint32_t *target_rows,
                                const uint32_t *baseline, const uint32_t *probe,
                                const uint32_t *ngg, const uint32_t *cb_probe,
                                uint32_t marker_value, const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-agc-report: color_va=%016llx submit=%u gate=%u ib=%u "
                 "completed=%d pixels=%u outside=%u guard=%u value=%08x "
                 "wait=%ds match=%d\n",
                 (unsigned long long)color_va, (unsigned)OPENAGC_AGC_SUBMIT,
                 (unsigned)OPENAGC_GATE_MASK, ib_words, completed, pixels,
                 outside, guard, value, wait_seconds, match);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-agc-target: nonzero=%u expected=%u bbox=%u,%u..%u,%u "
                 "first=%08x topology=%u gs_out=%u\n",
                 target_nonzero, target_expected, bbox_min_x, bbox_min_y,
                 bbox_max_x, bbox_max_y, target_first, topology, gs_out);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-agc-rows:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_COLOR_HEIGHT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", target_rows[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    n = snprintf(buffer + used, sizeof(buffer) - used, "\nopenagc-agc-state:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", baseline[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    n = snprintf(buffer + used, sizeof(buffer) - used, " |");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_DRAW_PROBE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", probe[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    n = snprintf(buffer + used, sizeof(buffer) - used, " |");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_PM4_NGG_PROBE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", ngg[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "\nopenagc-agc-cb: marker=%u", marker_value);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_CB_BIND_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", cb_probe[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    n = snprintf(buffer + used, sizeof(buffer) - used, "\nopenagc-agc-window:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_VIEW_WORDS; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", window[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    if (used + 1u >= sizeof(buffer)) {
        return -1;
    }
    buffer[used++] = '\n';
    buffer[used] = '\0';
    return openagc_log_bytes(buffer, used);
}

static int openagc_write_draw_dump(uint32_t target_nonzero,
                                   uint32_t target_expected, uint32_t bbox_min_x,
                                   uint32_t bbox_min_y, uint32_t bbox_max_x,
                                   uint32_t bbox_max_y, uint32_t target_first,
                                   int completed, uint64_t color_va,
                                   uint32_t pixels, uint32_t outside,
                                   uint32_t guard, uint32_t value,
                                   int wait_seconds, int match,
                                   uint32_t word_count, uint32_t draw_topology,
                                   uint32_t draw_gs_out, const uint32_t *baseline,
                                   const uint32_t *probe, const uint32_t *ngg,
                                   const uint32_t *target_rows,
                                   const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-draw-raster-owned: color_va=%016llx rect=%u,%u,%ux%u "
                 "pixels=%u outside=%u guard=%u value=%08x gate=%u wait=%ds "
                 "match=%d\n",
                 (unsigned long long)color_va, OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                 OPENAGC_VIEW_W, OPENAGC_VIEW_H, pixels, outside, guard, value,
                 (unsigned)OPENAGC_GATE_MASK, wait_seconds, match);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-baseline:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", baseline[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    n = snprintf(buffer + used, sizeof(buffer) - used, "\nopenagc-probe:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_DRAW_PROBE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", probe[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    /* VGT_SHADER_STAGES_EN (context), GE_CNTL (uconfig aperture), ES PGM
     * LO (SH), the GS user-data dword carrying ngg_lds_layout,
     * VGT_PRIMITIVE_TYPE (uconfig aperture), GE_CNTL (context aperture),
     * SPI_SHADER_COL_FORMAT, SPI_PS_INPUT_ENA, VGT_ESGS_RING_ITEMSIZE. */
    n = snprintf(buffer + used, sizeof(buffer) - used, "\nopenagc-ngg:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_PM4_NGG_PROBE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", ngg[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "\nopenagc-raster-fixture: topology=%u gs_out=%u slots=%u,%u\n",
                 (unsigned)draw_topology, (unsigned)draw_gs_out,
                 (unsigned)OPENAGC_NGG_USER_DATA_LAYOUT_SLOT,
                 (unsigned)OPENAGC_NGG_USER_DATA_LAYOUT);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-raster-target: nonzero=%u expected=%u "
                 "bbox=%u,%u..%u,%u first=%08x\n",
                 target_nonzero, target_expected, bbox_min_x, bbox_min_y,
                 bbox_max_x, bbox_max_y, target_first);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-raster-rows: %08x %08x %08x %08x %08x %08x %08x %08x "
                 "%08x %08x %08x %08x %08x %08x %08x %08x\n",
                 target_rows[0], target_rows[1], target_rows[2], target_rows[3],
                 target_rows[4], target_rows[5], target_rows[6], target_rows[7],
                 target_rows[8], target_rows[9], target_rows[10], target_rows[11],
                 target_rows[12], target_rows[13], target_rows[14], target_rows[15]);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\n",
                 OPENAGC_IB_DUMP_TAG_DRAW_RASTER, OPENAGC_IB_DUMP_FW940_ID,
                 completed, word_count);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < word_count; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", window[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    if (used + 1u >= sizeof(buffer)) {
        return -1;
    }
    buffer[used++] = '\n';
    buffer[used] = '\0';
    return openagc_log_bytes(buffer, used);
}

static int openagc_elapsed_seconds(const struct timeval *start)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (int)(now.tv_sec - start->tv_sec);
}

#if OPENAGC_AGC_COMPLETION
static void openagc_flush_arena(const void *address, size_t bytes)
{
    uintptr_t line = (uintptr_t)address & ~(uintptr_t)63u;
    uintptr_t end = (uintptr_t)address + bytes;

    for (; line < end; line += 64u) {
        __asm__ volatile("clflush (%0)" : : "r"(line) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
}
#endif

int main(void)
{
#if OPENAGC_APP_BRIDGE
    if (op_ps5_udp_log_open(OPENAGC_LOG_HOST_IP, OPENAGC_UDP_LOG_PORT) != 0)
        return 3;
    if (openagc_logf("openagc-app: entered main; UDP receiver=9999\n") != 0)
        return 3;
#endif
    uint8_t *arena = NULL;
    uint8_t *color = NULL;
    uint32_t *ib = NULL;
    uint32_t *baseline = NULL;
    uint32_t *probe = NULL;
    uint32_t *ngg = NULL;
    uint32_t *context_table = NULL;
    uint32_t *uconfig_table = NULL;
    uint32_t *cb_probe = NULL;
#if !OPENAGC_GPU_BRIDGE
    struct openagc_cb *cb = NULL;
#endif
    volatile uint32_t *marker = NULL;
#if !OPENAGC_GPU_BRIDGE
    struct openagc_submit submit;
#endif
    struct timeval start;
    uint64_t color_va, marker_va, vert_code_va, frag_code_va;
#if !OPENAGC_GPU_BRIDGE
    uint64_t ib_va, cb_va;
#endif
    uint64_t baseline_va, probe_va, ngg_probe_va;
    uint64_t context_table_va, uconfig_table_va, cb_probe_va;
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t gs_out;
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t window[OPENAGC_VIEW_WORDS];
    uint32_t vertex_context_values[OPENAGC_NGG_VERTEX_CONTEXT_COUNT];
    uint32_t word_count;
    uint32_t pixels = 0u;
    uint32_t target_nonzero = 0u;
    uint32_t target_expected = 0u;
    uint32_t target_min_x = 0u;
    uint32_t target_max_x = 0u;
    uint32_t target_min_y = 0u;
    uint32_t target_max_y = 0u;
    uint32_t target_first = 0u;
    uint32_t target_rows[OPENAGC_COLOR_HEIGHT];
    uint32_t outside = 0u;
    uint32_t guard = 0u;
    uint32_t value = 0u;
    uint32_t i;
    int gc_fd = -1;
    int completed = 0;
    int match = 0;
    off_t physical = 0;

#if OPENAGC_AGC_COMPLETION
    {
#if OPENAGC_GPU_BRIDGE
        uint32_t fw = op_ps5_system_firmware_version();
#else
        uint32_t fw = kernel_get_fw_version();
#endif
        if (fw != OPENAGC_PS5_POLICY_FW940_ID) {
            (void)openagc_logf(
                "openagc-draw-raster: firmware refused got=%08x expected=%08x\n",
                fw, OPENAGC_PS5_POLICY_FW940_ID);
            return 1;
        }
#if OPENAGC_APP_BRIDGE
        (void)openagc_logf("openagc-app: runtime firmware validated\n");
#endif
    }
#endif

#if OPENAGC_APP_BRIDGE
    (void)openagc_logf("openagc-app: allocating direct GPU memory\n");
#endif
    {
        int rc = sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                                   &physical);

        if (rc != 0) {
            (void)openagc_logf("openagc-draw-raster: allocate failed rc=%d\n",
                               rc);
            return 1;
        }
    }
    {
        int rc = sceKernelMapNamedDirectMemory(
            (void **)&arena, OPENAGC_ARENA,
            OPENAGC_PROT_READ | OPENAGC_PROT_WRITE | OPENAGC_PROT_GPU_READ |
                OPENAGC_PROT_GPU_WRITE,
            OPENAGC_MAP_NO_COALESCE, physical, OPENAGC_ARENA,
            "openagc-draw-raster");

        if (rc != 0) {
            (void)openagc_logf("openagc-draw-raster: map failed rc=%d\n", rc);
            return 1;
        }
    }

    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    baseline = (uint32_t *)(arena + OPENAGC_BASELINE_OFF);
    probe = (uint32_t *)(arena + OPENAGC_PROBE_OFF);
    ngg = (uint32_t *)(arena + OPENAGC_NGG_PROBE_OFF);
    context_table = (uint32_t *)(arena + OPENAGC_CONTEXT_TABLE_OFF);
    uconfig_table = (uint32_t *)(arena + OPENAGC_UCONFIG_TABLE_OFF);
    cb_probe = (uint32_t *)(arena + OPENAGC_CB_PROBE_OFF);
#if !OPENAGC_GPU_BRIDGE
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
#endif
    marker = (volatile uint32_t *)(arena + OPENAGC_MARKER_OFF);
    color = arena + OPENAGC_COLOR_OFF;
#if !OPENAGC_GPU_BRIDGE
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
#endif
    color_va = (uint64_t)(uintptr_t)color;
    marker_va = (uint64_t)(uintptr_t)marker;
    baseline_va = (uint64_t)(uintptr_t)baseline;
    probe_va = (uint64_t)(uintptr_t)probe;
    ngg_probe_va = (uint64_t)(uintptr_t)ngg;
    context_table_va = (uint64_t)(uintptr_t)context_table;
    uconfig_table_va = (uint64_t)(uintptr_t)uconfig_table;
    cb_probe_va = (uint64_t)(uintptr_t)cb_probe;
    vert_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_VERT_CODE_OFF);
    frag_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_FRAG_CODE_OFF);

    memset(arena, 0x00, OPENAGC_ARENA);
    memcpy(arena + OPENAGC_VERT_CODE_OFF, openagc_ngg_vert_code,
           sizeof(openagc_ngg_vert_code));
    memcpy(arena + OPENAGC_FRAG_CODE_OFF, openagc_ngg_frag_code,
           sizeof(openagc_ngg_frag_code));
    *marker = 0u;

    if ((color_va & 0xffull) != 0ull || (vert_code_va & 0xffull) != 0ull ||
        (frag_code_va & 0xffull) != 0ull) {
        (void)openagc_logf("openagc-draw-raster: VA not 256B aligned\n");
        return 1;
    }

    memset(&program, 0, sizeof(program));
    memcpy(vertex_context_values, openagc_ngg_vertex_context_values,
           sizeof(vertex_context_values));
    for (i = 0u; i < OPENAGC_NGG_VERTEX_CONTEXT_COUNT; ++i) {
        if (openagc_ngg_vertex_context_offsets[i] ==
            OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE) {
            vertex_context_values[i] = 1u;
            break;
        }
    }
    if (i == OPENAGC_NGG_VERTEX_CONTEXT_COUNT) {
        (void)openagc_logf("openagc-draw-raster: no ESGS ring record\n");
        return 1;
    }
    program.vertex_context.count = OPENAGC_NGG_VERTEX_CONTEXT_COUNT;
    program.vertex_context.offsets = openagc_ngg_vertex_context_offsets;
    program.vertex_context.values = vertex_context_values;
    program.vertex_shader.count = OPENAGC_NGG_VERTEX_SHADER_COUNT;
    program.vertex_shader.offsets = openagc_ngg_vertex_shader_offsets;
    program.vertex_shader.values = openagc_ngg_vertex_shader_values;
    program.linkage.count = OPENAGC_NGG_LINKAGE_COUNT;
    program.linkage.offsets = openagc_ngg_linkage_offsets;
    program.linkage.values = openagc_ngg_linkage_values;
    program.fragment_context.count = OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT;
    program.fragment_context.offsets = openagc_ngg_fragment_context_offsets;
    program.fragment_context.values = openagc_ngg_fragment_context_values;
    program.fragment_shader.count = OPENAGC_NGG_FRAGMENT_SHADER_COUNT;
    program.fragment_shader.offsets = openagc_ngg_fragment_shader_offsets;
    program.fragment_shader.values = openagc_ngg_fragment_shader_values;
    program.vertex_pgm_lo_slot = 0u;
    program.vertex_pgm_hi_slot = 1u;
    program.fragment_pgm_lo_slot = 0u;
    program.fragment_pgm_hi_slot = 1u;
    program.user_data.count = OPENAGC_NGG_USER_DATA_COUNT;
    program.user_data.offsets = openagc_ngg_user_data_offsets;
    program.user_data.values = openagc_ngg_user_data_values;
    program.user_data_layout_slot = OPENAGC_NGG_USER_DATA_LAYOUT_SLOT;
    program.user_data_layout = OPENAGC_NGG_USER_DATA_LAYOUT;
    program.di_primitive = OPENAGC_GFX10_DI_PT_TRILIST;

    memset(&draw, 0, sizeof(draw));
    draw.struct_size = (uint32_t)sizeof(draw);
    draw.api_version = OPENAGC_RASTER_API_VERSION;
    draw.color_va = color_va;
    draw.color_width = OPENAGC_COLOR_WIDTH;
    draw.color_height = OPENAGC_COLOR_HEIGHT;
    draw.color_pitch_bytes = OPENAGC_COLOR_WIDTH * 4u;
    draw.color_bgra = 0u;
    draw.viewport_x = OPENAGC_VIEW_X;
    draw.viewport_y = OPENAGC_VIEW_Y;
    draw.viewport_width = OPENAGC_VIEW_W;
    draw.viewport_height = OPENAGC_VIEW_H;
    draw.viewport_y_down = 1u;
    /* smoke.tri.vert covers the viewport with one triangle: three
     * vertices, rasterized as a strip (VGT_GS_OUT_PRIM_TYPE 2). */
#if OPENAGC_POINT_DRAW
    draw.topology = OPENAGC_RASTER_TOPOLOGY_POINT_LIST;
    draw.vertex_count = 1u;
#else
    draw.topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;
    draw.vertex_count = 3u;
#endif
    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw.program = &program;
    draw.vertex_code_va = vert_code_va;
    draw.fragment_code_va = frag_code_va;
    draw.baseline_va = baseline_va;
    draw.probe_va = probe_va;
    draw.ngg_probe_va = ngg_probe_va;
    draw.context_table_va = context_table_va;
    draw.context_table = context_table;
    draw.uconfig_table_va = uconfig_table_va;
    draw.uconfig_table = uconfig_table;
    draw.cb_probe_va = cb_probe_va;
    draw.gate_mask = OPENAGC_GATE_MASK;
    draw.append_eop = 0u;
    draw.sequence = OPENAGC_EOP_SEQUENCE;
    draw.marker_va = marker_va;
    gs_out = draw.topology == OPENAGC_RASTER_TOPOLOGY_POINT_LIST
                 ? OPENAGC_GFX10_GS_OUT_POINTLIST
                 : OPENAGC_GFX10_GS_OUT_TRISTRIP;

    word_count = openagc_raster_encode_draw(&draw, words,
                                            OPENAGC_RASTER_MAX_WORDS);
    if (word_count == 0u || word_count > OPENAGC_RASTER_MAX_WORDS) {
        fprintf(stderr,
                "encode refused: words=%u viewport=%u,%u,%ux%u color_va=%llx "
                "pitch=%u tables=%p/%p vc=%u vsh=%u link=%u fc=%u fsh=%u ud=%u\n",
                word_count, draw.viewport_x, draw.viewport_y,
                draw.viewport_width, draw.viewport_height,
                (unsigned long long)draw.color_va, draw.color_pitch_bytes,
                (void *)draw.context_table, (void *)draw.uconfig_table,
                program.vertex_context.count, program.vertex_shader.count,
                program.linkage.count, program.fragment_context.count,
                program.fragment_shader.count, program.user_data.count);
        (void)openagc_logf("openagc-draw-raster: encode refused\n");
        return 1;
    }
    if ((uint64_t)word_count * 4u > OPENAGC_CB_OFF - OPENAGC_IB_OFF) {
        (void)openagc_logf("openagc-draw-raster: IB overruns its window\n");
        return 1;
    }
#if OPENAGC_AGC_COMPLETION && !OPENAGC_GPU_BRIDGE
    if (word_count > OPENAGC_RASTER_MAX_WORDS - OPENAGC_PM4_AGC_COMPLETION_WORDS ||
        word_count + OPENAGC_PM4_AGC_COMPLETION_WORDS >
            (OPENAGC_CB_OFF - OPENAGC_IB_OFF) / 4u) {
        (void)openagc_logf("openagc-draw-raster: completion overruns IB\n");
        return 1;
    }
    openagc_pm4_encode_agc_completion(marker_va, OPENAGC_EOP_SEQUENCE,
                                      words + word_count);
    word_count += OPENAGC_PM4_AGC_COMPLETION_WORDS;
#endif
#if !OPENAGC_GPU_BRIDGE
    memcpy(ib, words, word_count * 4u);
#if OPENAGC_AGC_COMPLETION
    openagc_flush_arena(arena, OPENAGC_ARENA);
#endif
#endif

#if OPENAGC_GPU_BRIDGE
    {
        openagc_ps5_gpu *gpu = NULL;
        openagc_ps5_gpu_draw request = OPENAGC_PS5_GPU_DRAW_INIT;
        openagc_ps5_gpu_submission receipt = OPENAGC_PS5_GPU_SUBMISSION_INIT;
        int32_t platform_error = 0;
        openagc_result result;

        request.draw = &draw;
        request.words = ib;
        request.word_capacity = (OPENAGC_CB_OFF - OPENAGC_IB_OFF) / 4u;
        request.marker = marker;
        request.vertex_code = arena + OPENAGC_VERT_CODE_OFF;
        request.vertex_code_bytes = sizeof(openagc_ngg_vert_code);
        request.fragment_code = arena + OPENAGC_FRAG_CODE_OFF;
        request.fragment_code_bytes = sizeof(openagc_ngg_frag_code);
        request.color = color;
        request.color_bytes = OPENAGC_COLOR_PITCH * OPENAGC_COLOR_HEIGHT;

        openagc_flush_arena(arena, OPENAGC_ARENA);
        result = openagc_ps5_gpu_create(&gpu, &platform_error);
        if (result != OPENAGC_OK) {
            const char *loader_error = dlerror();
            struct {
                uint64_t reserved_00;
                char text[0x1c];
                uint32_t packed;
                uint64_t reserved_28;
            } raw_version = {0};
            int version_rc = sceKernelGetProsperoSystemSwVersion(&raw_version);
            uint32_t system_fw = op_ps5_system_firmware_version();
            (void)openagc_logf(
                "openagc-gpu-bridge: create refused result=%d platform=%d "
                "system=%08x raw_rc=%d raw_packed=%08x raw_text=%.24s "
                "sdk=%08x loader=%.120s\n",
                result, platform_error, system_fw,
                version_rc, raw_version.packed, raw_version.text,
                kernel_get_fw_version(),
                loader_error != NULL ? loader_error : "none");
            return 1;
        }
        gettimeofday(&start, NULL);
        result = openagc_ps5_gpu_submit_draw(gpu, &request,
                                             OPENAGC_PS5_GPU_MAX_WAIT_MS,
                                             &receipt);
        if (result != OPENAGC_OK || !receipt.completed) {
            (void)openagc_logf(
                "openagc-gpu-bridge: draw refused result=%d platform=%d "
                "attempted=%u completed=%u words=%u sequence=%u\n",
                result, receipt.platform_error, receipt.submitted,
                receipt.completed, receipt.word_count, receipt.sequence);
            return 1;
        }
        word_count = receipt.word_count;
        completed = 1;
        result = openagc_ps5_gpu_destroy(gpu);
        if (result != OPENAGC_OK) {
            (void)openagc_logf(
                "openagc-gpu-bridge: destroy refused result=%d\n", result);
            return 1;
        }
        openagc_flush_arena(arena, OPENAGC_ARENA);
    }
#elif OPENAGC_AGC_SUBMIT
    {
        void *agc_module = dlopen("libSceAgc.sprx", RTLD_NOW | RTLD_LOCAL);
        void *driver_module = dlopen("libSceAgcDriver.sprx", RTLD_NOW | RTLD_LOCAL);
        int32_t (*agc_init)(uint32_t);
        int32_t (*submit_dcb)(void *);
        int32_t (*suspend_point)(void);
        struct openagc_agc_description description;
        int32_t rc;

        if (agc_module == NULL || driver_module == NULL) {
            (void)openagc_logf("openagc-draw-raster: agc module missing\n");
            return 1;
        }
        *(void **)(&agc_init) = dlsym(agc_module, "sceAgcInit");
        *(void **)(&suspend_point) = dlsym(agc_module, "sceAgcSuspendPoint");
        *(void **)(&submit_dcb) = dlsym(driver_module, "sceAgcDriverSubmitDcb");
        if (agc_init == NULL || submit_dcb == NULL || suspend_point == NULL) {
            (void)openagc_logf(
                "openagc-draw-raster: agc symbols missing init=%d submit=%d "
                "suspend=%d\n",
                agc_init != NULL, submit_dcb != NULL, suspend_point != NULL);
            return 1;
        }
        rc = agc_init(8u);
        if (rc != 0) {
            (void)openagc_logf("openagc-draw-raster: agc init refused rc=%d\n",
                               rc);
            return 1;
        }
        description.words = ib;
        description.word_count = word_count;
        description.flag = 0u;
        description.padding[0] = 0u;
        description.padding[1] = 0u;
        description.padding[2] = 0u;
        gettimeofday(&start, NULL);
        rc = submit_dcb(&description);
        if (rc != 0) {
            (void)openagc_logf(
                "openagc-draw-raster: agc submit refused rc=%d\n", rc);
            return 1;
        }
        (void)submit;
        (void)ib_va;
        (void)cb_va;
        rc = suspend_point();
        if (rc != 0) {
            (void)openagc_logf("openagc-draw-raster: agc suspend refused rc=%d\n",
                               rc);
        }
    }
#else
    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)word_count << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        (void)openagc_logf("openagc-draw-raster: /dev/gc unavailable\n");
        return 1;
    }
    {
        int rc = ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit);

        if (rc != 0) {
            close(gc_fd);
            (void)openagc_logf(
                "openagc-draw-raster: context query refused rc=%d\n", rc);
            return 1;
        }
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    {
        int rc = ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit);

        if (rc != 0) {
            close(gc_fd);
            (void)openagc_logf("openagc-draw-raster: submit refused rc=%d\n",
                               rc);
            return 1;
        }
    }
#endif

#if !OPENAGC_GPU_BRIDGE
    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
#if OPENAGC_AGC_COMPLETION
        openagc_flush_arena((const void *)marker, sizeof(*marker));
#endif
        if (*marker == OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
#endif
    if (gc_fd >= 0) {
        close(gc_fd);
    }
#if OPENAGC_AGC_COMPLETION
    if (!completed) {
        (void)openagc_logf("openagc-draw-raster: AGC completion timed out\n");
        return 1;
    }
    openagc_flush_arena(color,
                        OPENAGC_COLOR_STRIDE_WORDS * OPENAGC_COLOR_HEIGHT * 4u);
#endif

    {
        const uint32_t *target = (const uint32_t *)(const void *)color;
        uint32_t x;
        uint32_t y;

        pixels = 0u;
        outside = 0u;
        value = 0u;
        for (y = 0u; y < OPENAGC_COLOR_HEIGHT; ++y) {
            for (x = 0u; x < OPENAGC_COLOR_WIDTH; ++x) {
                uint32_t word = target[y * OPENAGC_COLOR_STRIDE_WORDS + x];
                uint32_t in_window = (x >= OPENAGC_VIEW_X && x < OPENAGC_VIEW_X + OPENAGC_VIEW_W &&
                                      y >= OPENAGC_VIEW_Y && y < OPENAGC_VIEW_Y + OPENAGC_VIEW_H);

                if (in_window) {
                    window[(y - OPENAGC_VIEW_Y) * OPENAGC_VIEW_W + (x - OPENAGC_VIEW_X)] = word;
                }
                if (word == 0u) {
                    continue;
                }
                if (in_window) {
                    ++pixels;
                    if (value == 0u) {
                        value = word;
                    }
                } else {
                    ++outside;
                }
            }
        }
    }
    for (i = (OPENAGC_COLOR_OFF +
              OPENAGC_COLOR_STRIDE_WORDS * OPENAGC_COLOR_HEIGHT * 4u) / 4u;
         i < OPENAGC_ARENA / 4u; ++i) {
        if (((const uint32_t *)(const void *)arena)[i] != 0u) {
            ++guard;
        }
    }

    {
        const uint32_t *target = (const uint32_t *)(const void *)color;
        uint32_t first_value = 0u;

        target_nonzero = 0u;
        target_expected = 0u;
        target_min_x = 0xffffffffu;
        target_max_x = 0u;
        target_min_y = 0xffffffffu;
        target_max_y = 0u;
        for (i = 0u; i < OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT; ++i) {
            uint32_t word =
                target[(i / OPENAGC_COLOR_WIDTH) * OPENAGC_COLOR_STRIDE_WORDS +
                       (i % OPENAGC_COLOR_WIDTH)];

            if (word == 0u) {
                continue;
            }
            ++target_nonzero;
            if (word == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
                ++target_expected;
            }
            if (first_value == 0u) {
                first_value = word;
            }
            if (i % OPENAGC_COLOR_WIDTH < target_min_x) target_min_x = i % OPENAGC_COLOR_WIDTH;
            if (i % OPENAGC_COLOR_WIDTH > target_max_x) target_max_x = i % OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH < target_min_y) target_min_y = i / OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH > target_max_y) target_max_y = i / OPENAGC_COLOR_WIDTH;
        }
        target_first = first_value;
        for (i = 0u; i < OPENAGC_COLOR_HEIGHT; ++i) {
            uint32_t mask = 0u;
            uint32_t x;

            for (x = 0u; x < OPENAGC_COLOR_WIDTH; ++x) {
                if (target[i * OPENAGC_COLOR_STRIDE_WORDS + x] != 0u) {
                    mask |= 1u << x;
                }
            }
            target_rows[i] = mask;
        }
    }

    /* The original Step AQ accepted pixels without a completion packet.
     * OPENAGC_AGC_COMPLETION requires a separate marker before this scan. */
    match = (target_nonzero != 0u && target_expected == target_nonzero &&
             outside == 0u && guard == 0u)
                ? 1
                : 0;
    for (i = 0u; i < OPENAGC_VIEW_WORDS; ++i) {
        if (window[i] != 0u && window[i] != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
            match = 0;
        }
    }

    /* Whole-target summary: where the writes landed and what they hold. */
    {
        const uint32_t *target = (const uint32_t *)(const void *)color;
        uint32_t min_x = 0xffffffffu, max_x = 0u, min_y = 0xffffffffu, max_y = 0u;
        uint32_t first_value = 0u;

        for (i = 0u; i < OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT; ++i) {
            uint32_t word =
                target[(i / OPENAGC_COLOR_WIDTH) * OPENAGC_COLOR_STRIDE_WORDS +
                       (i % OPENAGC_COLOR_WIDTH)];

            if (word == 0u) {
                continue;
            }
            if (first_value == 0u) {
                first_value = word;
            }
            if (i % OPENAGC_COLOR_WIDTH < min_x) min_x = i % OPENAGC_COLOR_WIDTH;
            if (i % OPENAGC_COLOR_WIDTH > max_x) max_x = i % OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH < min_y) min_y = i / OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH > max_y) max_y = i / OPENAGC_COLOR_WIDTH;
        }
        target_first = first_value;
        for (i = 0u; i < OPENAGC_COLOR_HEIGHT; ++i) {
            uint32_t mask = 0u;
            uint32_t x;

            for (x = 0u; x < OPENAGC_COLOR_WIDTH; ++x) {
                if (target[i * OPENAGC_COLOR_STRIDE_WORDS + x] != 0u) {
                    mask |= 1u << x;
                }
            }
            target_rows[i] = mask;
        }
    }

    /* Only the opt-in completion probe requires a marker before readback. */
    match = (target_nonzero != 0u && target_expected == target_nonzero &&
             outside == 0u && guard == 0u)
                ? 1
                : 0;
    if (pixels != 0u && value != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
        match = 0;
    }
#if OPENAGC_AGC_COMPLETION
    if (target_nonzero != OPENAGC_VIEW_WORDS ||
        target_expected != OPENAGC_VIEW_WORDS ||
        pixels != OPENAGC_VIEW_WORDS || !completed ||
        target_min_x != OPENAGC_VIEW_X ||
        target_min_y != OPENAGC_VIEW_Y ||
        target_max_x != OPENAGC_VIEW_X + OPENAGC_VIEW_W - 1u ||
        target_max_y != OPENAGC_VIEW_Y + OPENAGC_VIEW_H - 1u) {
        match = 0;
    }
    for (i = 0u; i < OPENAGC_VIEW_WORDS; ++i) {
        if (window[i] != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
            match = 0;
        }
    }
#endif

    if (openagc_write_report(
            completed, color_va, pixels, outside, guard, value,
            openagc_elapsed_seconds(&start), match, word_count, draw.topology,
            gs_out, target_nonzero, target_expected, target_min_x, target_min_y,
            target_max_x, target_max_y, target_first, target_rows, baseline,
            probe, ngg, cb_probe, (uint32_t)*marker, window) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
