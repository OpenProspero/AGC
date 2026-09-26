/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Legacy (non-NGG) vertex path with the viewport transform enabled. */

#include "openagc/pm4_ib_dump_fw940.h"
#include "openagc/raster.h"

#include "smoke_tables.h"

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

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
#define OPENAGC_UCONFIG_TABLE_OFF 0x1800u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_IB_BYTES 0x1000u
#define OPENAGC_CB_OFF 0x3000u
#define OPENAGC_MARKER_OFF 0x3800u
#define OPENAGC_COLOR_OFF 0x4000u
#define OPENAGC_COLOR_WIDTH 32u
#define OPENAGC_COLOR_HEIGHT 32u
#define OPENAGC_COLOR_PITCH 128u
#define OPENAGC_VIEW_X 8u
#define OPENAGC_VIEW_Y 8u
#ifndef OPENAGC_VIEW_W
#define OPENAGC_VIEW_W 1u
#endif
#ifndef OPENAGC_VIEW_H
#define OPENAGC_VIEW_H 1u
#endif
#define OPENAGC_VIEW_WORDS (OPENAGC_VIEW_W * OPENAGC_VIEW_H)
#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#ifndef OPENAGC_GATE_MASK
#define OPENAGC_GATE_MASK 0x703u
#endif
#define OPENAGC_LOG_BYTES (OPENAGC_VIEW_WORDS * 9u + 2048u)

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

static const char openagc_log_path[] =
    "/data/prosperoai/openagc-ib-dump-draw-legacy.log";

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

static int openagc_logf(const char *fmt, ...)
{
    char line[512];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (n < 0) {
        return -1;
    }
    return openagc_log_bytes(line, strlen(line));
}

static int openagc_write_dump(int completed, uint64_t color_va, uint32_t pixels,
                              uint32_t outside, uint32_t guard, uint32_t value,
                              int wait_seconds, int match, uint32_t ib_words,
                              uint32_t topology, uint32_t gs_out,
                              const uint32_t *probe, const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-draw-legacy-owned: color_va=%016llx rect=%u,%u,%ux%u "
                 "pixels=%u outside=%u guard=%u value=%08x topology=%u gs_out=%u "
                 "gate=%u ib=%u wait=%ds match=%d\n",
                 (unsigned long long)color_va, OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                 OPENAGC_VIEW_W, OPENAGC_VIEW_H, pixels, outside, guard, value,
                 topology, gs_out, (unsigned)OPENAGC_GATE_MASK, ib_words,
                 wait_seconds, match);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-probe:");
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

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "\nopenagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\n",
                 OPENAGC_IB_DUMP_TAG_DRAW_RASTER, OPENAGC_IB_DUMP_FW940_ID,
                 completed, OPENAGC_VIEW_WORDS);
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

static int openagc_elapsed_seconds(const struct timeval *start)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (int)(now.tv_sec - start->tv_sec);
}

int main(void)
{
    uint8_t *arena = NULL;
    uint8_t *color = NULL;
    uint32_t *ib = NULL;
    uint32_t *probe = NULL;
    uint32_t *context_table = NULL;
    uint32_t *uconfig_table = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t ib_va, cb_va, color_va, marker_va, vert_code_va, frag_code_va;
    uint64_t probe_va, context_table_va, uconfig_table_va;
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t user_data_values[OPENAGC_PM4_NGG_USER_DATA_COUNT];
    static const uint32_t user_data_offsets[OPENAGC_PM4_NGG_USER_DATA_COUNT] = {
        76u, 77u, 78u, 79u
    };
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t window[OPENAGC_VIEW_WORDS];
    uint32_t word_count;
    uint32_t pixels = 0u;
    uint32_t outside = 0u;
    uint32_t guard = 0u;
    uint32_t value = 0u;
    uint32_t i;
    int gc_fd;
    int completed = 0;
    int match = 0;
    off_t physical = 0;

    {
        int rc = sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                                   &physical);

        if (rc != 0) {
            return openagc_logf("openagc-draw-legacy: allocate failed rc=%d\n",
                                rc) == 0
                       ? 0
                       : 1;
        }
    }
    {
        int rc = sceKernelMapNamedDirectMemory(
            (void **)&arena, OPENAGC_ARENA,
            OPENAGC_PROT_READ | OPENAGC_PROT_WRITE | OPENAGC_PROT_GPU_READ |
                OPENAGC_PROT_GPU_WRITE,
            OPENAGC_MAP_NO_COALESCE, physical, OPENAGC_ARENA,
            "openagc-draw-legacy");

        if (rc != 0) {
            return openagc_logf("openagc-draw-legacy: map failed rc=%d\n", rc) == 0
                       ? 0
                       : 1;
        }
    }

    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    probe = (uint32_t *)(arena + OPENAGC_PROBE_OFF);
    context_table = (uint32_t *)(arena + OPENAGC_CONTEXT_TABLE_OFF);
    uconfig_table = (uint32_t *)(arena + OPENAGC_UCONFIG_TABLE_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    color = arena + OPENAGC_COLOR_OFF;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
    color_va = (uint64_t)(uintptr_t)color;
    marker_va = (uint64_t)(uintptr_t)marker;
    probe_va = (uint64_t)(uintptr_t)probe;
    context_table_va = (uint64_t)(uintptr_t)context_table;
    uconfig_table_va = (uint64_t)(uintptr_t)uconfig_table;
    vert_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_VERT_CODE_OFF);
    frag_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_FRAG_CODE_OFF);

    memset(arena, 0x00, OPENAGC_ARENA);
    memcpy(arena + OPENAGC_VERT_CODE_OFF, openagc_smoke_vert_code,
           sizeof(openagc_smoke_vert_code));
    memcpy(arena + OPENAGC_FRAG_CODE_OFF, openagc_smoke_frag_code,
           sizeof(openagc_smoke_frag_code));
    *marker = 0u;

    memset(&program, 0, sizeof(program));
    program.vertex_context.count = OPENAGC_PM4_SMOKE_VERT_CTX_COUNT;
    program.vertex_context.offsets = openagc_pm4_smoke_vert_ctx_offsets;
    program.vertex_context.values = openagc_pm4_smoke_vert_ctx_values;
    program.vertex_shader.count = OPENAGC_PM4_SMOKE_VERT_SH_COUNT;
    program.vertex_shader.offsets = openagc_pm4_smoke_vert_sh_offsets;
    program.vertex_shader.values = openagc_pm4_smoke_vert_sh_values;
    program.linkage.count = OPENAGC_PM4_SMOKE_LINK_COUNT;
    program.linkage.offsets = openagc_pm4_smoke_link_offsets;
    program.linkage.values = openagc_pm4_smoke_link_values;
    program.fragment_context.count = OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT;
    program.fragment_context.offsets = openagc_pm4_smoke_frag_ctx_offsets;
    program.fragment_context.values = openagc_pm4_smoke_frag_ctx_values;
    program.fragment_shader.count = OPENAGC_PM4_SMOKE_FRAG_SH_COUNT;
    program.fragment_shader.offsets = openagc_pm4_smoke_frag_sh_offsets;
    program.fragment_shader.values = openagc_pm4_smoke_frag_sh_values;
    program.vertex_pgm_lo_slot = 0u;
    program.vertex_pgm_hi_slot = 1u;
    program.fragment_pgm_lo_slot = 0u;
    program.fragment_pgm_hi_slot = 1u;
    for (i = 0u; i < OPENAGC_PM4_NGG_USER_DATA_COUNT; ++i) {
        user_data_values[i] = 0u;
    }
    program.user_data.count = OPENAGC_PM4_NGG_USER_DATA_COUNT;
    program.user_data.offsets = user_data_offsets;
    program.user_data.values = user_data_values;
    program.user_data_layout_slot = 0u;
    program.user_data_layout = 0u;
    program.di_primitive = OPENAGC_GFX10_DI_PT_POINTLIST;

    memset(&draw, 0, sizeof(draw));
    draw.struct_size = (uint32_t)sizeof(draw);
    draw.api_version = OPENAGC_RASTER_API_VERSION;
    draw.color_va = color_va;
    draw.color_width = OPENAGC_COLOR_WIDTH;
    draw.color_height = OPENAGC_COLOR_HEIGHT;
    draw.color_pitch_bytes = OPENAGC_COLOR_PITCH;
    draw.viewport_x = OPENAGC_VIEW_X;
    draw.viewport_y = OPENAGC_VIEW_Y;
    draw.viewport_width = OPENAGC_VIEW_W;
    draw.viewport_height = OPENAGC_VIEW_H;
    draw.topology = OPENAGC_RASTER_TOPOLOGY_POINT_LIST;
    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw.vertex_count = 1u;
    draw.program = &program;
    draw.vertex_code_va = vert_code_va;
    draw.fragment_code_va = frag_code_va;
    draw.probe_va = probe_va;
    draw.context_table_va = context_table_va;
    draw.context_table = context_table;
    draw.uconfig_table_va = uconfig_table_va;
    draw.uconfig_table = uconfig_table;
    draw.gate_mask = OPENAGC_GATE_MASK;
    draw.append_eop = 1u;
    draw.sequence = OPENAGC_EOP_SEQUENCE;
    draw.marker_va = marker_va;

    word_count = openagc_raster_encode_draw(&draw, words,
                                            OPENAGC_RASTER_MAX_WORDS);
    if (word_count == 0u || (uint64_t)word_count * 4u > OPENAGC_IB_BYTES) {
        return openagc_logf("openagc-draw-legacy: encode refused\n") == 0 ? 0 : 1;
    }
    memcpy(ib, words, word_count * 4u);

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)word_count << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-draw-legacy: /dev/gc unavailable\n") == 0 ? 0
                                                                              : 1;
    }
    {
        int rc = ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit);

        if (rc != 0) {
            close(gc_fd);
            return openagc_logf("openagc-draw-legacy: context query refused rc=%d\n",
                                rc) == 0
                       ? 0
                       : 1;
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
            return openagc_logf("openagc-draw-legacy: submit refused rc=%d\n",
                                rc) == 0
                       ? 0
                       : 1;
        }
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    if (openagc_pm4_draw_point_scan((const uint32_t *)(const void *)color,
                                    OPENAGC_COLOR_WIDTH, OPENAGC_COLOR_HEIGHT,
                                    OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                                    OPENAGC_VIEW_W, OPENAGC_VIEW_H, window,
                                    OPENAGC_VIEW_WORDS, &pixels, &outside,
                                    &value) == 0u) {
        return openagc_logf("openagc-draw-legacy: scan refused\n") == 0 ? 0 : 1;
    }
    for (i = (OPENAGC_COLOR_OFF + OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT *
                                       4u) / 4u;
         i < OPENAGC_ARENA / 4u; ++i) {
        if (((const uint32_t *)(const void *)arena)[i] != 0u) {
            ++guard;
        }
    }

    match = (completed && pixels != 0u && outside == 0u && guard == 0u &&
             value == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8)
                ? 1
                : 0;
    for (i = 0u; i < OPENAGC_VIEW_WORDS; ++i) {
        if (window[i] != 0u && window[i] != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
            match = 0;
        }
    }

    if (openagc_write_dump(completed, color_va, pixels, outside, guard, value,
                           openagc_elapsed_seconds(&start), match, word_count,
                           draw.topology, OPENAGC_GFX10_GS_OUT_POINTLIST, probe,
                           window) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
