/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step AC: the first OpenAGC DRAW - one point into an owned linear target.
 *
 * The IB (include/openagc/pm4_draw_fw940.h) is CLEAR_STATE + the smoke
 * draw state + the console-proven smoke.vert/frag register program with the
 * two PGMs patched to the uploaded code + the Step-AB color bind + a
 * POINTLIST topology + one DRAW_INDEX_AUTO of one vertex + the shared
 * EOP+NOP trailer. The vertex shader emits a constant position
 * (vec4(0,0,0,1)), so the only primitive it can rasterize non-degenerately
 * is a point; the pixel shader exports vec4(1.0, 0.25, 0.0, 1.0), which is
 * the whole acceptance value below.
 *
 * Bounding: one submit, finite marker deadline, no retry. Every color write
 * is bounded by the screen scissor and the single viewport (a 16x16 rect
 * inside the 32x32 target), the target sits inside a 256 KiB arena so a
 * stray write stays in our own memory, and the payload scans the rest of
 * the arena afterwards to report any pixel outside that rect.
 *
 * Packet layout: include/openagc/pm4_draw_fw940.h
 * Dump format: include/openagc/pm4_ib_dump_fw940.h (tag=draw-point-eop)
 */

#include "openagc/pm4_draw_fw940.h"
#include "openagc/pm4_ib_dump_fw940.h"

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

#define OPENAGC_PAGE 0x4000u
#define OPENAGC_ARENA (256u * 1024u)
#define OPENAGC_VERT_CODE_OFF 0x0000u
#define OPENAGC_FRAG_CODE_OFF 0x0100u
#define OPENAGC_BASELINE_OFF 0x1000u
#define OPENAGC_PROBE_OFF 0x1100u
/* The uconfig register table the AGC-style table load reads: one 8-byte
 * record, {offset u16 at bytes 0-1, value u32 at bytes 4-7}. */
#define OPENAGC_TABLE_OFF 0x1200u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u
/* 256-byte aligned linear color target: 32x32 RGBA8 at a 128-byte pitch. */
#define OPENAGC_COLOR_OFF 0x4000u
#define OPENAGC_COLOR_WIDTH 32u
#define OPENAGC_COLOR_HEIGHT 32u
#define OPENAGC_COLOR_BYTES (OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT * 4u)
/* The draw is scissored to this rect; it is also the dumped window. The
 * 8x8 point covers it exactly. */
#define OPENAGC_VIEW_X 8u
#define OPENAGC_VIEW_Y 8u
#define OPENAGC_VIEW_W 8u
#define OPENAGC_VIEW_H 8u
#define OPENAGC_VIEW_WORDS (OPENAGC_VIEW_W * OPENAGC_VIEW_H)

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30

/* Expect line + baseline + probe + header + one " %08x" per window word. */
#define OPENAGC_LOG_BYTES (OPENAGC_VIEW_WORDS * 9u + 1536u)

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
    "/data/prosperoai/openagc-ib-dump-draw-point.log";

/* tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin (44 bytes). */
static const uint8_t openagc_smoke_vert_code[] = {
    0x03, 0x00, 0x8f, 0xbf, 0x80, 0x02, 0x00, 0x7e, 0xf2, 0x02, 0x02, 0x7e,
    0xcf, 0x08, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x81, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf
};

/* tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin (48 bytes). */
static const uint8_t openagc_smoke_frag_code[] = {
    0xf2, 0x02, 0x00, 0x7e, 0xff, 0x02, 0x02, 0x7e, 0x00, 0x00, 0x80, 0x3e,
    0x80, 0x02, 0x04, 0x7e, 0x0f, 0x18, 0x00, 0xf8, 0x00, 0x01, 0x02, 0x00,
    0x00, 0x00, 0x81, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf
};

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

static int openagc_log(const char *line)
{
    return openagc_log_bytes(line, strlen(line));
}

static int openagc_logf(const char *fmt, ...)
{
    char line[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    return openagc_log(line);
}

static int openagc_write_draw_dump(int completed, uint64_t color_va,
                                   uint32_t pixels, uint32_t outside,
                                   uint32_t guard, uint32_t value, int match,
                                   const uint32_t *baseline,
                                   const uint32_t *probe,
                                   const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-draw-point-owned: color_va=%016llx rect=%u,%u,%ux%u "
                 "pixels=%u outside=%u guard=%u value=%08x match=%d\n",
                 (unsigned long long)color_va, OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                 OPENAGC_VIEW_W, OPENAGC_VIEW_H, pixels, outside, guard, value,
                 match);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;

    /* Inherited context state, read before this IB wrote anything. */
    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-baseline:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x",
                     baseline[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }

    /* Same registers after this IB wrote the draw state. */
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
    /* VGT_PRIMITIVE_TYPE read back through the uconfig aperture
     * (0xC000+578) after each write form this IB tried (plain, index 1,
     * index 2, index 4); a 1 means that form landed and left POINTLIST. */
    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "\nopenagc-uconfig: set_form=%08x table_load=%08x\n",
                 probe[OPENAGC_GFX10_DRAW_PROBE_COUNT + 0u],
                 probe[OPENAGC_GFX10_DRAW_PROBE_COUNT +
                       OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT]);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    if (used + 1u >= sizeof(buffer)) {
        return -1;
    }
    /* The NGG vertex stage's user data, read back through the SH aperture
     * (0x2C00 + SH offset 140, SPI_SHADER_USER_DATA_GS_0). */
    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-gs-user-data:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x",
                     probe[OPENAGC_GFX10_DRAW_PROBE_COUNT +
                           OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT + 1u + i]);
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
    if (used + 1u >= sizeof(buffer)) {
        return -1;
    }
    buffer[used++] = '\n';

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\nib",
                 OPENAGC_IB_DUMP_TAG_DRAW_POINT, OPENAGC_IB_DUMP_FW940_ID,
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
    /* The NGG vertex stage's user data, read back through the SH aperture
     * (0x2C00 + SH offset 140, SPI_SHADER_USER_DATA_GS_0). */
    n = snprintf(buffer + used, sizeof(buffer) - used, "openagc-gs-user-data:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x",
                     probe[OPENAGC_GFX10_DRAW_PROBE_COUNT +
                           OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT + 1u + i]);
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
    uint32_t *baseline = NULL;
    uint32_t *probe = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t ib_va, cb_va, color_va, marker_va, vert_code_va, frag_code_va;
    uint64_t baseline_va, probe_va, table_va;
    uint8_t *table = NULL;
    openagc_pm4_draw_point_state state;
    uint32_t words[OPENAGC_PM4_DRAW_POINT_EOP_WORDS];
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

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-draw-point: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-draw-point") != 0) {
        return openagc_logf("openagc-draw-point: map failed\n") == 0 ? 0 : 1;
    }

    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    baseline = (uint32_t *)(arena + OPENAGC_BASELINE_OFF);
    probe = (uint32_t *)(arena + OPENAGC_PROBE_OFF);
    table = arena + OPENAGC_TABLE_OFF;
    table_va = (uint64_t)(uintptr_t)table;
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    color = arena + OPENAGC_COLOR_OFF;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
    color_va = (uint64_t)(uintptr_t)color;
    marker_va = (uint64_t)(uintptr_t)marker;
    baseline_va = (uint64_t)(uintptr_t)baseline;
    probe_va = (uint64_t)(uintptr_t)probe;
    vert_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_VERT_CODE_OFF);
    frag_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_FRAG_CODE_OFF);

    memset(arena, 0x00, OPENAGC_ARENA);
    memcpy(arena + OPENAGC_VERT_CODE_OFF, openagc_smoke_vert_code,
           sizeof(openagc_smoke_vert_code));
    memcpy(arena + OPENAGC_FRAG_CODE_OFF, openagc_smoke_frag_code,
           sizeof(openagc_smoke_frag_code));
    *marker = 0u;
    /* VGT_PRIMITIVE_TYPE (uconfig offset 578) = DI_PT_POINTLIST (1). */
    table[0] = (uint8_t)(OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE & 0xffu);
    table[1] = (uint8_t)((OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE >> 8) & 0xffu);
    table[4] = (uint8_t)(OPENAGC_GFX10_DI_PT_POINTLIST & 0xffu);

    if ((color_va & 0xffull) != 0ull || (vert_code_va & 0xffull) != 0ull ||
        (frag_code_va & 0xffull) != 0ull) {
        return openagc_logf("openagc-draw-point: VA not 256B aligned\n") == 0 ? 0
                                                                             : 1;
    }

    memset(&state, 0, sizeof(state));
    state.color_va = color_va;
    state.color_width = OPENAGC_COLOR_WIDTH;
    state.color_height = OPENAGC_COLOR_HEIGHT;
    state.viewport_x = OPENAGC_VIEW_X;
    state.viewport_y = OPENAGC_VIEW_Y;
    state.viewport_width = OPENAGC_VIEW_W;
    state.viewport_height = OPENAGC_VIEW_H;
    state.vertex_count = 1u;

    word_count = openagc_pm4_encode_draw_point_eop(
        &state, baseline_va, probe_va,
        probe_va + (uint64_t)OPENAGC_GFX10_DRAW_PROBE_COUNT * 4u, table_va,
        vert_code_va, frag_code_va, OPENAGC_EOP_SEQUENCE, marker_va, words);
    if (word_count != OPENAGC_PM4_DRAW_POINT_EOP_WORDS) {
        return openagc_logf("openagc-draw-point: encode refused\n") == 0 ? 0 : 1;
    }
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_PM4_DRAW_POINT_EOP_WORDS << 32) |
                    ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-draw-point: /dev/gc unavailable\n") == 0 ? 0
                                                                            : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-draw-point: context query refused\n") == 0
                   ? 0
                   : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-draw-point: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    /* Read the scissored window back and report every other write, both
     * inside the target and in the guard region up to the arena end. The
     * scan is bounded by the target dimensions, so it cannot read past the
     * mapped arena. */
    if (openagc_pm4_draw_point_scan((const uint32_t *)(const void *)color,
                                    OPENAGC_COLOR_WIDTH, OPENAGC_COLOR_HEIGHT,
                                    OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                                    OPENAGC_VIEW_W, OPENAGC_VIEW_H, window,
                                    OPENAGC_VIEW_WORDS, &pixels, &outside,
                                    &value) == 0u) {
        return openagc_logf("openagc-draw-point: scan refused\n") == 0 ? 0 : 1;
    }
    for (i = (OPENAGC_COLOR_OFF + OPENAGC_COLOR_BYTES) / 4u;
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

    if (openagc_write_draw_dump(completed, color_va, pixels, outside, guard,
                                value, match, baseline, probe, window) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
