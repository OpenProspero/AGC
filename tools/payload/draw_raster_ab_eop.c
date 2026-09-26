/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Step AF: two targets in one IB, one draw each.
 *
 * Step AE proved the AGC-shaped program executes (completed=1) with every
 * register of the program readable, and still wrote no pixel. One
 * suspected structural difference from public drivers was the colour
 * bind: OpenAGC's nine-word set binds a *linear* 8_8_8_8 surface
 * (COLOR_SW_MODE 0), while the public native runtime
 * (blackbearreloaded/ps5-opengl, release commit 6cb291ab,
 * append_target_state) and PS5_Vulkan's C1 capture both write 16 COLOR0
 * records with COLOR_SW_MODE 27, FMASK_SW_MODE 24, RESOURCE_LEVEL 1,
 * CMASK/DCC pipe aligned and CB_COLOR0_DCC_CONTROL = 0x48. 0x31e is
 * DCC_CONTROL, not an ATTRIB alias (Mesa gfx10.json), so the nine-word
 * set never writes it at all.
 *
 * This payload draws the same viewport-covering triangle twice in one IB:
 *
 *   pass A  the nine-word linear bind into a 32x32 linear target
 *   pass B  the capture's 16-record bind into a 256B-pitch 64x256 target
 *
 * Both passes use the same program, topology and rasterizer state. A later
 * source audit found that this payload left draw.gate_mask at zero, so the
 * fragment-gate block did not run; zero pixels cannot exclude either bind
 * in combination with that omitted state. Neither target
 * can see a write outside itself: A is 4 KiB at 0x4000, B is 64 KiB at
 * 0x10000, both inside the payload's own 256 KiB arena, and the guard
 * counts nonzero dwords in the arena regions that hold no target. One
 * submit, one deadline, no retry.
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
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_IB_BYTES 0x1000u
#define OPENAGC_CB_OFF 0x3000u
#define OPENAGC_MARKER_OFF 0x3800u
/* Target A: 32x32 RGBA8, 128-byte pitch, 4 KiB, linear bind. */
#define OPENAGC_COLOR_A_OFF 0x4000u
#define OPENAGC_COLOR_A_WIDTH 32u
#define OPENAGC_COLOR_A_HEIGHT 32u
#define OPENAGC_COLOR_A_PITCH 128u
#define OPENAGC_COLOR_A_BYTES 4096u
/* Target B: 64x256 RGBA8, 256-byte pitch, 64 KiB, capture-shaped bind.
 * 64 KiB is the swizzle block the capture's COLOR_SW_MODE 27 belongs to,
 * and the arena offset is block aligned. */
#define OPENAGC_COLOR_B_OFF 0x10000u
#define OPENAGC_COLOR_B_WIDTH 64u
#define OPENAGC_COLOR_B_HEIGHT 256u
#define OPENAGC_COLOR_B_PITCH 256u
#define OPENAGC_COLOR_B_BYTES 65536u
#define OPENAGC_VIEW_X 8u
#define OPENAGC_VIEW_Y 8u
#define OPENAGC_VIEW_W 8u
#define OPENAGC_VIEW_H 8u
#define OPENAGC_VIEW_WORDS (OPENAGC_VIEW_W * OPENAGC_VIEW_H)
#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30

#define OPENAGC_LOG_BYTES (OPENAGC_VIEW_WORDS * 9u + 4096u)

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
    "/data/prosperoai/openagc-ib-dump-draw-raster-ab.log";

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

static uint32_t openagc_count_nonzero(const uint32_t *words, uint32_t count)
{
    uint32_t seen = 0u;
    uint32_t i;

    for (i = 0u; i < count; ++i) {
        if (words[i] != 0u) {
            ++seen;
        }
    }
    return seen;
}

static int openagc_write_draw_dump(int completed, uint64_t color_a_va,
                                   uint64_t color_b_va, uint32_t pixels_a,
                                   uint32_t outside_a, uint32_t guard,
                                   uint32_t value_a, uint32_t words_b,
                                   uint32_t first_b, int wait_seconds, int match,
                                   uint32_t ib_words, uint32_t word_count,
                                   const uint32_t *baseline,
                                   const uint32_t *probe, const uint32_t *ngg,
                                   const uint32_t *capture,
                                   const uint32_t *window)
{
    char buffer[OPENAGC_LOG_BYTES];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-draw-raster-ab-owned: a_va=%016llx b_va=%016llx "
                 "rect=%u,%u,%ux%u a_pixels=%u a_outside=%u a_value=%08x "
                 "b_nonzero=%u b_first=%08x guard=%u ib=%u wait=%ds match=%d\n",
                 (unsigned long long)color_a_va, (unsigned long long)color_b_va,
                 OPENAGC_VIEW_X, OPENAGC_VIEW_Y, OPENAGC_VIEW_W, OPENAGC_VIEW_H,
                 pixels_a, outside_a, value_a, words_b, first_b, guard, ib_words,
                 wait_seconds, match);
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

    /* Pass B's bind words, as the capture carries them, so the dump names
     * the exact set the second draw used. */
    n = snprintf(buffer + used, sizeof(buffer) - used, "\nopenagc-cb-capture:");
    if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
        return -1;
    }
    used += (size_t)n;
    for (i = 0u; i < OPENAGC_GFX10_CB_CAPTURE_COUNT; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %u=%08x",
                     openagc_gfx10_cb_capture_offsets[i], capture[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    n = snprintf(buffer + used, sizeof(buffer) - used,
                 "\nopenagc-raster-fixture: topology=%u gs_out=%u "
                 "linear_attrib3=%08x\n",
                 (unsigned)OPENAGC_GFX10_DI_PT_TRILIST,
                 (unsigned)OPENAGC_GFX10_GS_OUT_TRISTRIP,
                 (unsigned)OPENAGC_GFX10_CB_CAPTURE_ATTRIB3);
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

int main(void)
{
    uint8_t *arena = NULL;
    uint8_t *color_a = NULL;
    uint8_t *color_b = NULL;
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
    uint64_t ib_va, cb_va, color_a_va, color_b_va, marker_va;
    uint64_t vert_code_va, frag_code_va;
    uint64_t baseline_va, probe_va, ngg_probe_va;
    uint64_t context_table_va, uconfig_table_va;
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t capture_values[OPENAGC_GFX10_CB_CAPTURE_COUNT];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t window_a[OPENAGC_VIEW_WORDS];
    uint32_t pass_a;
    uint32_t pass_b;
    uint32_t word_count;
    uint32_t pixels_a = 0u;
    uint32_t outside_a = 0u;
    uint32_t guard = 0u;
    uint32_t value_a = 0u;
    uint32_t words_b = 0u;
    uint32_t first_b = 0u;
    uint32_t i;
    int gc_fd;
    int completed = 0;
    int match = 0;
    off_t physical = 0;

    {
        int rc = sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                                   &physical);

        if (rc != 0) {
            return openagc_logf("openagc-draw-raster-ab: allocate failed rc=%d\n",
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
            "openagc-draw-raster-ab");

        if (rc != 0) {
            return openagc_logf("openagc-draw-raster-ab: map failed rc=%d\n",
                                rc) == 0
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
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    color_a = arena + OPENAGC_COLOR_A_OFF;
    color_b = arena + OPENAGC_COLOR_B_OFF;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
    color_a_va = (uint64_t)(uintptr_t)color_a;
    color_b_va = (uint64_t)(uintptr_t)color_b;
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
    *marker = 0u;

    if ((color_a_va & 0xffull) != 0ull || (color_b_va & 0xffull) != 0ull ||
        (vert_code_va & 0xffull) != 0ull || (frag_code_va & 0xffull) != 0ull) {
        return openagc_logf("openagc-draw-raster-ab: VA not 256B aligned\n") == 0
                   ? 0
                   : 1;
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
    program.user_data.count = OPENAGC_NGG_USER_DATA_COUNT;
    program.user_data.offsets = openagc_ngg_user_data_offsets;
    program.user_data.values = openagc_ngg_user_data_values;
    program.user_data_layout_slot = OPENAGC_NGG_USER_DATA_LAYOUT_SLOT;
    program.user_data_layout = OPENAGC_NGG_USER_DATA_LAYOUT;
    program.di_primitive = OPENAGC_GFX10_DI_PT_TRILIST;

    memset(&draw, 0, sizeof(draw));
    draw.struct_size = (uint32_t)sizeof(draw);
    draw.api_version = OPENAGC_RASTER_API_VERSION;
    draw.viewport_x = OPENAGC_VIEW_X;
    draw.viewport_y = OPENAGC_VIEW_Y;
    draw.viewport_width = OPENAGC_VIEW_W;
    draw.viewport_height = OPENAGC_VIEW_H;
    draw.topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;
    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw.vertex_count = 3u;
    draw.program = &program;
    draw.vertex_code_va = vert_code_va;
    draw.fragment_code_va = frag_code_va;
    draw.context_table_va = context_table_va;
    draw.context_table = context_table;
    draw.uconfig_table_va = uconfig_table_va;
    draw.uconfig_table = uconfig_table;
    draw.gate_mask = OPENAGC_RASTER_GATE_ALL;
    draw.marker_va = marker_va;
    draw.sequence = OPENAGC_EOP_SEQUENCE;

    /* Pass A: the nine-word linear bind, full dumps, no EOP yet. */
    draw.color_va = color_a_va;
    draw.color_width = OPENAGC_COLOR_A_WIDTH;
    draw.color_height = OPENAGC_COLOR_A_HEIGHT;
    draw.color_pitch_bytes = OPENAGC_COLOR_A_PITCH;
    draw.baseline_va = baseline_va;
    draw.probe_va = probe_va;
    draw.ngg_probe_va = ngg_probe_va;
    draw.append_eop = 0u;
    pass_a = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    if (pass_a == 0u) {
        return openagc_logf("openagc-draw-raster-ab: pass A refused\n") == 0 ? 0 : 1;
    }

    /* Pass B: the capture's 16-record bind; readbacks only in pass A. */
    if (openagc_gfx10_cb_capture_words(color_b_va, OPENAGC_COLOR_B_WIDTH,
                                       OPENAGC_COLOR_B_HEIGHT,
                                       capture_values) == 0u) {
        return openagc_logf("openagc-draw-raster-ab: capture bind refused\n") == 0
                   ? 0
                   : 1;
    }
    draw.color_va = color_b_va;
    draw.color_width = OPENAGC_COLOR_B_WIDTH;
    draw.color_height = OPENAGC_COLOR_B_HEIGHT;
    draw.color_pitch_bytes = OPENAGC_COLOR_B_PITCH;
    draw.baseline_va = 0u;
    draw.probe_va = 0u;
    draw.ngg_probe_va = 0u;
    draw.cb_bind_offsets = openagc_gfx10_cb_capture_offsets;
    draw.cb_bind_values = capture_values;
    draw.cb_bind_count = OPENAGC_GFX10_CB_CAPTURE_COUNT;
    draw.append_eop = 1u;
    pass_b = openagc_raster_encode_draw(&draw, words + pass_a,
                                        OPENAGC_RASTER_MAX_WORDS - pass_a);
    if (pass_b == 0u) {
        return openagc_logf("openagc-draw-raster-ab: pass B refused\n") == 0 ? 0 : 1;
    }
    word_count = pass_a + pass_b;
    if ((uint64_t)word_count * 4u > OPENAGC_IB_BYTES) {
        return openagc_logf("openagc-draw-raster-ab: IB overruns its window\n") == 0
                   ? 0
                   : 1;
    }
    memcpy(ib, words, word_count * 4u);

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)word_count << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-draw-raster-ab: /dev/gc unavailable\n") == 0
                   ? 0
                   : 1;
    }
    {
        int rc = ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit);

        if (rc != 0) {
            close(gc_fd);
            return openagc_logf(
                       "openagc-draw-raster-ab: context query refused rc=%d\n",
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
            return openagc_logf("openagc-draw-raster-ab: submit refused rc=%d\n",
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

    if (openagc_pm4_draw_point_scan((const uint32_t *)(const void *)color_a,
                                    OPENAGC_COLOR_A_WIDTH, OPENAGC_COLOR_A_HEIGHT,
                                    OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                                    OPENAGC_VIEW_W, OPENAGC_VIEW_H, window_a,
                                    OPENAGC_VIEW_WORDS, &pixels_a, &outside_a,
                                    &value_a) == 0u) {
        return openagc_logf("openagc-draw-raster-ab: scan refused\n") == 0 ? 0 : 1;
    }
    words_b = openagc_count_nonzero((const uint32_t *)(const void *)color_b,
                                    OPENAGC_COLOR_B_BYTES / 4u);
    for (i = 0u; i < OPENAGC_COLOR_B_BYTES / 4u; ++i) {
        if (((const uint32_t *)(const void *)color_b)[i] != 0u) {
            first_b = ((const uint32_t *)(const void *)color_b)[i];
            break;
        }
    }
    /* The arena holds no target between A and B, nor after B. */
    guard = openagc_count_nonzero(
                (const uint32_t *)(const void *)(arena + OPENAGC_COLOR_A_OFF +
                                                 OPENAGC_COLOR_A_BYTES),
                (OPENAGC_COLOR_B_OFF - OPENAGC_COLOR_A_OFF -
                 OPENAGC_COLOR_A_BYTES) / 4u) +
            openagc_count_nonzero(
                (const uint32_t *)(const void *)(arena + OPENAGC_COLOR_B_OFF +
                                                 OPENAGC_COLOR_B_BYTES),
                (OPENAGC_ARENA - OPENAGC_COLOR_B_OFF - OPENAGC_COLOR_B_BYTES) / 4u);

    match = (completed && (pixels_a != 0u || words_b != 0u) && outside_a == 0u &&
             guard == 0u)
                ? 1
                : 0;
    for (i = 0u; i < OPENAGC_VIEW_WORDS; ++i) {
        if (window_a[i] != 0u && window_a[i] != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
            match = 0;
        }
    }
    if (pixels_a != 0u && value_a != OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
        match = 0;
    }

    if (openagc_write_draw_dump(completed, color_a_va, color_b_va, pixels_a,
                                outside_a, guard, value_a, words_b, first_b,
                                openagc_elapsed_seconds(&start), match,
                                word_count, OPENAGC_VIEW_WORDS, baseline, probe,
                                ngg, capture_values, window_a) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
