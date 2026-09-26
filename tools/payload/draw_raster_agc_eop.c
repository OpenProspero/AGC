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
#if OPENAGC_AGC_SUBMIT
#include <dlfcn.h>
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
#ifndef OPENAGC_POINT_DRAW
#define OPENAGC_POINT_DRAW 0
#endif
#ifndef OPENAGC_GATE_MASK
#define OPENAGC_GATE_MASK OPENAGC_RASTER_GATE_ALL
#endif
#define OPENAGC_DEADLINE_SECONDS 30

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

#if OPENAGC_AGC_SUBMIT
struct openagc_agc_description {
    void *words;
    uint32_t word_count;
    uint8_t flag;
    uint8_t padding[3];
};
#endif

#ifndef OPENAGC_LOG_PATH
#define OPENAGC_LOG_PATH "/data/prosperoai/openagc-ib-dump-draw-agc.log"
#endif
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
    uint8_t *color = NULL;
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
    uint64_t ib_va, cb_va, color_va, marker_va, vert_code_va, frag_code_va;
    uint64_t baseline_va, probe_va, ngg_probe_va;
    uint64_t context_table_va, uconfig_table_va;
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
    uint32_t outside = 0u;
    uint32_t guard = 0u;
    uint32_t value = 0u;
    uint32_t i;
    int gc_fd = -1;
    int completed = 0;
    int match = 0;
    off_t physical = 0;

    {
        int rc = sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                                   &physical);

        if (rc != 0) {
            return openagc_logf("openagc-draw-raster: allocate failed rc=%d\n",
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
            "openagc-draw-raster");

        if (rc != 0) {
            return openagc_logf("openagc-draw-raster: map failed rc=%d\n", rc) == 0
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
    color = arena + OPENAGC_COLOR_OFF;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;
    color_va = (uint64_t)(uintptr_t)color;
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

    if ((color_va & 0xffull) != 0ull || (vert_code_va & 0xffull) != 0ull ||
        (frag_code_va & 0xffull) != 0ull) {
        return openagc_logf("openagc-draw-raster: VA not 256B aligned\n") == 0
                   ? 0
                   : 1;
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
        return openagc_logf("openagc-draw-raster: no ESGS ring record\n") == 0
                   ? 0 : 1;
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
    draw.color_pitch_bytes = 128u;
    draw.color_bgra = 0u;
    draw.viewport_x = OPENAGC_VIEW_X;
    draw.viewport_y = OPENAGC_VIEW_Y;
    draw.viewport_width = OPENAGC_VIEW_W;
    draw.viewport_height = OPENAGC_VIEW_H;
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
    draw.gate_mask = OPENAGC_GATE_MASK;
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
        return openagc_logf("openagc-draw-raster: encode refused\n") == 0 ? 0 : 1;
    }
    if ((uint64_t)word_count * 4u > OPENAGC_CB_OFF - OPENAGC_IB_OFF) {
        return openagc_logf("openagc-draw-raster: IB overruns its window\n") == 0
                   ? 0
                   : 1;
    }
    memcpy(ib, words, word_count * 4u);

#if OPENAGC_AGC_SUBMIT
    {
        void *agc_module = dlopen("libSceAgc.sprx", RTLD_NOW | RTLD_LOCAL);
        void *driver_module = dlopen("libSceAgcDriver.sprx", RTLD_NOW | RTLD_LOCAL);
        int32_t (*agc_init)(uint32_t);
        int32_t (*submit_dcb)(void *);
        int32_t (*suspend_point)(void);
        struct openagc_agc_description description;
        int32_t rc;

        if (agc_module == NULL || driver_module == NULL) {
            return openagc_logf("openagc-draw-raster: agc module missing\n") == 0
                       ? 0
                       : 1;
        }
        *(void **)(&agc_init) = dlsym(agc_module, "sceAgcInit");
        *(void **)(&suspend_point) = dlsym(agc_module, "sceAgcSuspendPoint");
        *(void **)(&submit_dcb) = dlsym(driver_module, "sceAgcDriverSubmitDcb");
        if (agc_init == NULL || submit_dcb == NULL || suspend_point == NULL) {
            return openagc_logf(
                       "openagc-draw-raster: agc symbols missing init=%d submit=%d "
                       "suspend=%d\n",
                       agc_init != NULL, submit_dcb != NULL,
                       suspend_point != NULL) == 0
                       ? 0
                       : 1;
        }
        rc = agc_init(8u);
        if (rc != 0) {
            return openagc_logf("openagc-draw-raster: agc init refused rc=%d\n",
                                rc) == 0
                       ? 0
                       : 1;
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
            return openagc_logf(
                       "openagc-draw-raster: agc submit refused rc=%d\n", rc) == 0
                       ? 0
                       : 1;
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
        return openagc_logf("openagc-draw-raster: /dev/gc unavailable\n") == 0
                   ? 0
                   : 1;
    }
    {
        int rc = ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit);

        if (rc != 0) {
            close(gc_fd);
            return openagc_logf(
                       "openagc-draw-raster: context query refused rc=%d\n",
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
            return openagc_logf("openagc-draw-raster: submit refused rc=%d\n",
                                rc) == 0
                       ? 0
                       : 1;
        }
    }
#endif

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    if (gc_fd >= 0) {
        close(gc_fd);
    }

    if (openagc_pm4_draw_point_scan((const uint32_t *)(const void *)color,
                                    OPENAGC_COLOR_WIDTH, OPENAGC_COLOR_HEIGHT,
                                    OPENAGC_VIEW_X, OPENAGC_VIEW_Y,
                                    OPENAGC_VIEW_W, OPENAGC_VIEW_H, window,
                                    OPENAGC_VIEW_WORDS, &pixels, &outside,
                                    &value) == 0u) {
        return openagc_logf("openagc-draw-raster: scan refused\n") == 0 ? 0 : 1;
    }
    for (i = (OPENAGC_COLOR_OFF + OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT *
                                       4u) / 4u;
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
            uint32_t word = target[i];

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

    /* Whole-target summary: where the writes landed and what they hold. */
    {
        const uint32_t *target = (const uint32_t *)(const void *)color;
        uint32_t nonzero = 0u;
        uint32_t expected = 0u;
        uint32_t min_x = 0xffffffffu, max_x = 0u, min_y = 0xffffffffu, max_y = 0u;
        uint32_t first_value = 0u;

        for (i = 0u; i < OPENAGC_COLOR_WIDTH * OPENAGC_COLOR_HEIGHT; ++i) {
            uint32_t word = target[i];

            if (word == 0u) {
                continue;
            }
            ++nonzero;
            if (word == OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8) {
                ++expected;
            }
            if (first_value == 0u) {
                first_value = word;
            }
            if (i % OPENAGC_COLOR_WIDTH < min_x) min_x = i % OPENAGC_COLOR_WIDTH;
            if (i % OPENAGC_COLOR_WIDTH > max_x) max_x = i % OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH < min_y) min_y = i / OPENAGC_COLOR_WIDTH;
            if (i / OPENAGC_COLOR_WIDTH > max_y) max_y = i / OPENAGC_COLOR_WIDTH;
        }
        (void)openagc_logf("openagc-draw-raster-target: nonzero=%u expected=%u "
                           "bbox=%u,%u..%u,%u first=%08x\n",
                           nonzero, expected, min_x, min_y, max_x, max_y,
                           first_value);
    }

    if (openagc_write_draw_dump(target_nonzero, target_expected, target_min_x,
                                target_min_y, target_max_x, target_max_y,
                                target_first, completed, color_va, pixels, outside, guard,
                                value, openagc_elapsed_seconds(&start), match,
                                OPENAGC_VIEW_WORDS, draw.topology, gs_out,
                                baseline, probe, ngg, window) != 0) {
        return 1;
    }
    return match ? 0 : 1;
}
