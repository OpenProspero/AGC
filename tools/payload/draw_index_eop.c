/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Step AG: the same draw through an index buffer instead of auto-index.
 *
 * Every OpenAGC draw so far used DRAW_INDEX_AUTO. This one writes
 * VGT_INDEX_TYPE and submits DRAW_INDEX_2 with a three-entry 16-bit index
 * buffer, and sets the user-data dword the shader reads as its indexed
 * flag, which is what a driver does for an indexed draw.
 */

#include "openagc/pm4_ib_dump_fw940.h"
#include "openagc/raster.h"

#include "ngg_smoke_tables.h"

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
#define OPENAGC_INDEX_OFF 0x1c00u
#define OPENAGC_INDEX_COUNT 3u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_IB_BYTES 0x0c00u
#define OPENAGC_CB_OFF 0x2c00u
#define OPENAGC_MARKER_OFF 0x3000u
#define OPENAGC_COLOR_OFF 0x4000u
#define OPENAGC_COLOR_WIDTH 32u
#define OPENAGC_COLOR_HEIGHT 32u
#define OPENAGC_COLOR_PITCH 128u
#define OPENAGC_VIEW_X 8u
#define OPENAGC_VIEW_Y 8u
#define OPENAGC_VIEW_W 8u
#define OPENAGC_VIEW_H 8u
#define OPENAGC_VIEW_WORDS (OPENAGC_VIEW_W * OPENAGC_VIEW_H)
#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#define OPENAGC_IS_INDEXED_DWORD 1u

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
    "/data/prosperoai/openagc-ib-dump-draw-index.log";

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

static int openagc_write_draw_dump(int completed, uint64_t color_va,
                                   uint64_t index_va, uint32_t pixels,
                                   uint32_t outside, uint32_t guard,
                                   uint32_t value, int wait_seconds, int match,
                                   uint32_t ib_words, const uint32_t *baseline,
                                   const uint32_t *probe, const uint32_t *ngg,
                                   const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-draw-index-owned: color_va=%016llx index_va=%016llx "
                 "indices=%u rect=%u,%u,%ux%u pixels=%u outside=%u guard=%u "
                 "value=%08x ib=%u wait=%ds match=%d\n",
                 (unsigned long long)color_va, (unsigned long long)index_va,
                 OPENAGC_INDEX_COUNT, OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                 OPENAGC_VIEW_W, OPENAGC_VIEW_H, pixels, outside, guard, value,
                 ib_words, wait_seconds, match);
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
                 "\nopenagc-draw-index-fixture: is_indexed_dword=%u value=1\n",
                 (unsigned)OPENAGC_IS_INDEXED_DWORD);
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;

    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "openagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\n",
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
    uint16_t *indices = NULL;
    uint32_t *ib = NULL;
    uint32_t *baseline = NULL;
    uint32_t *probe = NULL;
    uint32_t *ngg = NULL;
    uint32_t *context_table = NULL;
    uint32_t *uconfig_table = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t ib_va, cb_va, color_va, index_va, marker_va;
    uint64_t vert_code_va, frag_code_va;
    uint64_t baseline_va, probe_va, ngg_probe_va;
    uint64_t context_table_va, uconfig_table_va;
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t user_data_values[OPENAGC_NGG_USER_DATA_COUNT];
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
            return openagc_logf("openagc-draw-index: allocate failed rc=%d\n",
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
            "openagc-draw-index");

        if (rc != 0) {
            return openagc_logf("openagc-draw-index: map failed rc=%d\n", rc) == 0
                       ? 0
                       : 1;
        }
    }

    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    baseline = (uint32_t *)(arena + OPENAGC_BASELINE_OFF);
    probe = (uint32_t *)(arena + OPENAGC_PROBE_OFF);
    ngg = (uint32_t *)(arena + OPENAGC_NGG_PROBE_OFF);
    context_table = (uint32_t *)(arena + OPENAGC_CONTEXT_TABLE_OFF);
    uconfig_table = (uint32_t *)(arena + OPENAGC_UCONFIG_TABLE_OFF);
    indices = (uint16_t *)(arena + OPENAGC_INDEX_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    color = arena + OPENAGC_COLOR_OFF;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
    color_va = (uint64_t)(uintptr_t)color;
    index_va = (uint64_t)(uintptr_t)indices;
    marker_va = (uint64_t)(uintptr_t)marker;
    baseline_va = (uint64_t)(uintptr_t)baseline;
    probe_va = (uint64_t)(uintptr_t)probe;
    ngg_probe_va = (uint64_t)(uintptr_t)ngg;
    context_table_va = (uint64_t)(uintptr_t)context_table;
    uconfig_table_va = (uint64_t)(uintptr_t)uconfig_table;
    vert_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_VERT_CODE_OFF);
    frag_code_va = (uint64_t)(uintptr_t)(arena + OPENAGC_FRAG_CODE_OFF);

    memset(arena, 0x00, OPENAGC_ARENA);
    memcpy(arena + OPENAGC_VERT_CODE_OFF, openagc_ngg_vert_code,
           sizeof(openagc_ngg_vert_code));
    memcpy(arena + OPENAGC_FRAG_CODE_OFF, openagc_ngg_frag_code,
           sizeof(openagc_ngg_frag_code));
    for (i = 0u; i < OPENAGC_INDEX_COUNT; ++i) {
        indices[i] = (uint16_t)i;
    }
    *marker = 0u;

    if ((color_va & 0xffull) != 0ull || (vert_code_va & 0xffull) != 0ull ||
        (frag_code_va & 0xffull) != 0ull || (index_va & 1ull) != 0ull) {
        return openagc_logf("openagc-draw-index: VA not aligned\n") == 0 ? 0 : 1;
    }

    memset(&program, 0, sizeof(program));
    program.vertex_context.count = OPENAGC_NGG_VERTEX_CONTEXT_COUNT;
    program.vertex_context.offsets = openagc_ngg_vertex_context_offsets;
    program.vertex_context.values = openagc_ngg_vertex_context_values;
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
    for (i = 0u; i < OPENAGC_NGG_USER_DATA_COUNT; ++i) {
        user_data_values[i] = openagc_ngg_user_data_values[i];
    }
    user_data_values[OPENAGC_IS_INDEXED_DWORD] = 1u;
    program.user_data.count = OPENAGC_NGG_USER_DATA_COUNT;
    program.user_data.offsets = openagc_ngg_user_data_offsets;
    program.user_data.values = user_data_values;
    program.user_data_layout_slot = OPENAGC_NGG_USER_DATA_LAYOUT_SLOT;
    program.user_data_layout = OPENAGC_NGG_USER_DATA_LAYOUT;
    program.di_primitive = OPENAGC_GFX10_DI_PT_TRILIST;

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
    draw.topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;
    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw.vertex_count = 3u;
    draw.index_va = index_va;
    draw.index_count = OPENAGC_INDEX_COUNT;
    draw.index_size = 2u;
    draw.index_max_size = OPENAGC_INDEX_COUNT;
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
    draw.append_eop = 1u;
    draw.sequence = OPENAGC_EOP_SEQUENCE;
    draw.marker_va = marker_va;

    word_count = openagc_raster_encode_draw(&draw, words,
                                            OPENAGC_RASTER_MAX_WORDS);
    if (word_count == 0u || (uint64_t)word_count * 4u > OPENAGC_IB_BYTES) {
        return openagc_logf("openagc-draw-index: encode refused\n") == 0 ? 0 : 1;
    }
    memcpy(ib, words, word_count * 4u);

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)word_count << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-draw-index: /dev/gc unavailable\n") == 0 ? 0 : 1;
    }
    {
        int rc = ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit);

        if (rc != 0) {
            close(gc_fd);
            return openagc_logf("openagc-draw-index: context query refused rc=%d\n",
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
            return openagc_logf("openagc-draw-index: submit refused rc=%d\n",
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
        return openagc_logf("openagc-draw-index: scan refused\n") == 0 ? 0 : 1;
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

    if (openagc_write_draw_dump(completed, color_va, index_va, pixels, outside,
                                guard, value, openagc_elapsed_seconds(&start),
                                match, word_count, baseline, probe, ngg,
                                window) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
