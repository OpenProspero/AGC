/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_RASTER_H
#define OPENAGC_RASTER_H

#include "openagc/driver.h"
#include "openagc/pm4_ngg_draw_fw940.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_RASTER_API_VERSION 1u
/* One draw IB, bounded: the Step-AD program plus the uconfig table. Two
 * passes with different color binds fit well inside this. */
#define OPENAGC_RASTER_MAX_WORDS 1024u
/* The COLOR0 record count a public AGC capture writes for one target. */
#define OPENAGC_RASTER_CB_BIND_MAX 16u
#define OPENAGC_RASTER_GATE_ALL 0x1fffu
/* The uconfig table the draw's linkage records are loaded through. */
#define OPENAGC_RASTER_UCONFIG_TABLE_RECORDS 8u
#define OPENAGC_RASTER_UCONFIG_TABLE_WORDS (2u * OPENAGC_RASTER_UCONFIG_TABLE_RECORDS)

/*
 * GPU rasterizer: composes the complete hardware draw state for one
 * draw - linear color bind, viewport/scissor, the vertex and pixel
 * register programs, the AGC linkage records (GE_CNTL, user VGPR
 * enable, VGT_PRIMITIVE_TYPE), one draw initiator - and encodes it as
 * the single IB the console submits. The pixels are produced by the
 * GPU; nothing here rasterizes on the host, and nothing here writes
 * image bytes.
 *
 * Qualification: gpu_rasterization stays 0 until a console run proves
 * a pixel. OPENAGC_RASTER_GPU_QUALIFIED is that pin; it is only raised
 * with a reviewed hardware-evidence entry, never by a host test.
 *
 * The state program is the one the public native runtime builds
 * (src/platform/ps5_agc_native_runtime.c, release commit
 * 6cb291abea32281571c49705735046425cf000fd): sceAgcLinkShaders writes
 * 34 context records and three uconfig records, the runtime loads them
 * with set_cx/set_uc, writes the ES/PS shader registers with set_sh,
 * the NGG user data at SH 0x8c with set_sh_direct, and then draws. The
 * three uconfig records are GE_CNTL (0x25b), SPI_SHADER_USER_VGPR_EN
 * (0x262) and VGT_PRIMITIVE_TYPE (0x242) - the input topology the VGT
 * assembles, which is why it may not be written into the context table.
 */

#define OPENAGC_RASTER_GPU_QUALIFIED 0u

typedef uint32_t openagc_raster_topology;
enum {
    OPENAGC_RASTER_TOPOLOGY_POINT_LIST = 1u,
    OPENAGC_RASTER_TOPOLOGY_LINE_LIST = 2u,
    OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST = 3u
};

/*
 * How VGT_PRIMITIVE_TYPE reaches the VGT. Both forms are cited, and the
 * value is the same either way: the indirect table is what the public
 * native runtime emits (sceAgcDcbSetUcRegistersIndirect from
 * set_linkage_uc_state), the plain packet is Mesa's GFX10+ form
 * (si_emit_draw_registers: `radeon_set_uconfig_reg(R_030908_...)` for
 * GFX_VERSION >= GFX10, the indexed form only for GFX7-GFX9).
 */
typedef uint32_t openagc_raster_topology_write;
enum {
    OPENAGC_RASTER_TOPOLOGY_WRITE_INDIRECT_TABLE = 1u,
    OPENAGC_RASTER_TOPOLOGY_WRITE_PLAIN_UCONFIG = 2u,
    /* Both forms in one IB: the table first, then the plain packet. */
    OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH = 3u
};

typedef struct openagc_raster_capabilities {
    uint32_t struct_size;
    uint32_t pm4_draw_encoding;
    uint32_t gpu_rasterization;
    uint32_t host_rasterization;
    uint32_t topology_write_forms;
    uint32_t max_words;
    uint32_t evidence_pin_count;
} openagc_raster_capabilities;

#define OPENAGC_RASTER_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_raster_capabilities), 0u, 0u, 0u, 0u, 0u, 0u }

/*
 * Everything one draw needs. The register tables are borrowed for the
 * call; offsets/values pairs follow openagc_pm4_ngg_table. The program's
 * linkage table carries all three AGC linkage records - the two uconfig
 * ones (GE_CNTL, the user-VGPR enable) and the context one
 * (VGT_SHADER_STAGES_EN) - and the encoder routes each to its own table.
 * The draw's topology value replaces VGT_PRIMITIVE_TYPE.
 *
 * context_table and uconfig_table are caller-owned scratch that the GPU
 * reads while the IB runs, so both must stay mapped and must not
 * overlap: OPENAGC_PM4_NGG_TABLE_WORDS dwords for the context table and
 * OPENAGC_RASTER_UCONFIG_TABLE_WORDS for the uconfig one.
 */
typedef struct openagc_raster_gpu_draw {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t color_va;
    uint32_t color_width;
    uint32_t color_height;
    /* The implicit linear COLOR0 bind uses tightly packed 4-byte pixels. */
    uint32_t color_pitch_bytes;
    /* 0 is RGBA8 (COMP_SWAP standard), 1 is BGRA8. */
    uint32_t color_bgra;
    uint32_t viewport_x;
    uint32_t viewport_y;
    uint32_t viewport_width;
    uint32_t viewport_height;
    openagc_raster_topology topology;
    openagc_raster_topology_write topology_write;
    uint32_t vertex_count;
    /* Zero draws auto-indexed. Otherwise DRAW_INDEX_2 reads index_count
     * indices of index_size bytes from index_va, bounded by index_max_size. */
    uint64_t index_va;
    uint32_t index_count;
    uint32_t index_size;
    uint32_t index_max_size;
    const openagc_pm4_ngg_program *program;
    uint64_t vertex_code_va;
    uint64_t fragment_code_va;
    /* Dump addresses written by this IB; zero skips that dump. */
    uint64_t baseline_va;
    uint64_t probe_va;
    uint64_t ngg_probe_va;
    uint64_t context_table_va;
    uint32_t *context_table;
    uint64_t uconfig_table_va;
    uint32_t *uconfig_table;
    /*
     * Optional explicit color bind, offsets/values pairs. Zero selects
     * the nine-word linear bind this header composes from color_va and
     * the target geometry; a caller that supplies a bind owns its words,
     * which is how a captured AGC COLOR0 record set is exercised.
     */
    const uint32_t *cb_bind_offsets;
    const uint32_t *cb_bind_values;
    uint32_t cb_bind_count;
    /* Which fragment-gate registers to write; every bit set by
     * OPENAGC_RASTER_GATE_ALL is the full block. */
    uint32_t gate_mask;
    /* 0 leaves the shared EOP trailer to a later pass in the same IB. */
    uint32_t append_eop;
    uint32_t sequence;
    uint64_t marker_va;
} openagc_raster_gpu_draw;

#define OPENAGC_RASTER_GPU_DRAW_INIT \
    { (uint32_t)sizeof(openagc_raster_gpu_draw), OPENAGC_RASTER_API_VERSION, \
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
      (const openagc_pm4_ngg_program *)0, 0u, 0u, 0u, 0u, 0u, 0u, \
      (uint32_t *)0, 0u, (uint32_t *)0, (const uint32_t *)0, \
      (const uint32_t *)0, 0u, (uint32_t)OPENAGC_RASTER_GATE_ALL, 1u, 0u, 0u }

/*
 * The three linkage records the AGC linker emits into its uconfig table
 * (PS5_Vulkan's public C1 capture, chunk 0x6000): GE_CNTL, the user-VGPR
 * enable and VGT_PRIMITIVE_TYPE. They are uconfig indices, so a program
 * that carries them must not route them through the context table - the
 * context aperture has a different register at the same index.
 */
#define OPENAGC_RASTER_UCONFIG_USER_VGPR_EN 610u

static inline uint32_t openagc_raster_linkage_is_uconfig(uint32_t offset)
{
    return offset == OPENAGC_GFX10_UCONFIG_GE_CNTL ||
                   offset == OPENAGC_RASTER_UCONFIG_USER_VGPR_EN ||
                   offset == OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE
               ? 1u
               : 0u;
}

static inline uint32_t openagc_raster_topology_di(openagc_raster_topology topology)
{
    switch (topology) {
    case OPENAGC_RASTER_TOPOLOGY_POINT_LIST:
        return OPENAGC_GFX10_DI_PT_POINTLIST;
    case OPENAGC_RASTER_TOPOLOGY_LINE_LIST:
        return OPENAGC_GFX10_DI_PT_LINELIST;
    case OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST:
        return OPENAGC_GFX10_DI_PT_TRILIST;
    default:
        return 0u;
    }
}

/*
 * The NGG stage's rasterized primitive (VGT_GS_OUT_PRIM_TYPE): a triangle
 * list is rasterized as a strip, the value Sony's own NGG triangle draw
 * carries in its context table.
 */
static inline uint32_t openagc_raster_gs_out_prim(openagc_raster_topology topology)
{
    switch (topology) {
    case OPENAGC_RASTER_TOPOLOGY_POINT_LIST:
        return OPENAGC_GFX10_GS_OUT_POINTLIST;
    case OPENAGC_RASTER_TOPOLOGY_LINE_LIST:
        return OPENAGC_GFX10_GS_OUT_LINESTRIP;
    case OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST:
        return OPENAGC_GFX10_GS_OUT_TRISTRIP;
    default:
        return 0xFFFFFFFFu;
    }
}

static inline uint32_t openagc_raster_program_valid(const openagc_pm4_ngg_program *program)
{
    if (program == NULL ||
        program->user_data.count != OPENAGC_PM4_NGG_USER_DATA_COUNT ||
        !openagc_pm4_ngg_table_valid(&program->vertex_context) ||
        !openagc_pm4_ngg_table_valid(&program->linkage) ||
        !openagc_pm4_ngg_table_valid(&program->fragment_context) ||
        !openagc_pm4_ngg_table_valid(&program->vertex_shader) ||
        !openagc_pm4_ngg_table_valid(&program->fragment_shader) ||
        !openagc_pm4_ngg_table_valid(&program->user_data) ||
        program->vertex_pgm_lo_slot >= program->vertex_shader.count ||
        program->vertex_pgm_hi_slot >= program->vertex_shader.count ||
        program->fragment_pgm_lo_slot >= program->fragment_shader.count ||
        program->fragment_pgm_hi_slot >= program->fragment_shader.count ||
        program->user_data_layout_slot >= program->user_data.count) {
        return 0u;
    }
    if (program->vertex_shader.offsets[program->vertex_pgm_lo_slot] !=
            OPENAGC_GFX10_SPI_SHADER_PGM_LO_ES ||
        program->vertex_shader.offsets[program->vertex_pgm_hi_slot] !=
            OPENAGC_GFX10_SPI_SHADER_PGM_HI_ES ||
        program->fragment_shader.offsets[program->fragment_pgm_lo_slot] !=
            OPENAGC_GFX10_SPI_SHADER_PGM_LO_PS ||
        program->fragment_shader.offsets[program->fragment_pgm_hi_slot] !=
            OPENAGC_GFX10_SPI_SHADER_PGM_HI_PS ||
        program->user_data.offsets[program->user_data_layout_slot] !=
            OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 +
                program->user_data_layout_slot) {
        return 0u;
    }
    return 1u;
}

/*
 * Encodes the draw. Returns the dword count written, or 0 when anything
 * is refused: a missing or misaligned target, a viewport that leaves it,
 * an unknown topology, a zero vertex count, a program without the ES/GS
 * or PS PGM slots, a linkage block without GE_CNTL, a uconfig index
 * inside a context table, code VAs that are not 256-byte aligned, or
 * words that would not fit max_words.
 */
static inline uint32_t openagc_raster_encode_draw(const openagc_raster_gpu_draw *draw,
                                                  uint32_t *words, uint32_t max_words)
{
    uint32_t state_offsets[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t state_values[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t cb_values[OPENAGC_GFX10_CB_BIND_COUNT];
    uint32_t vert_sh_values[OPENAGC_PM4_NGG_MAX_WRITES];
    uint32_t frag_sh_values[OPENAGC_PM4_NGG_MAX_WRITES];
    uint32_t user_data_values[OPENAGC_PM4_NGG_USER_DATA_COUNT];
    uint32_t viewport[6];
    uint32_t guardband[4];
    uint32_t gb_sequence[5];
    uint32_t vport_scissor[2];
    uint32_t generic_scissor[2];
    uint32_t depth_range[2];
    uint32_t ps_input_cntl[OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT];
    uint32_t ngg_probe[OPENAGC_PM4_NGG_PROBE_COUNT];
    openagc_pm4_draw_point_state state;
    uint32_t di_primitive;
    uint32_t gs_out_prim;
    uint32_t context_records = 0u;
    uint32_t uconfig_records = 0u;
    uint32_t gate_count = 0u;
    uint32_t cb_count;
    uint32_t required_words;
    uint32_t x2;
    uint32_t y2;
    uint32_t cursor = 0u;
    uint32_t record;
    uint32_t i;

    if (draw == NULL || words == NULL ||
        draw->struct_size != sizeof(*draw) ||
        draw->api_version != OPENAGC_RASTER_API_VERSION) {
        return 0u;
    }
    if (!openagc_raster_program_valid(draw->program)) {
        return 0u;
    }
    di_primitive = openagc_raster_topology_di(draw->topology);
    gs_out_prim = openagc_raster_gs_out_prim(draw->topology);
    if (di_primitive == 0u || gs_out_prim == 0xFFFFFFFFu) {
        return 0u;
    }
    /* A triangle list must request whole triangles; the console rejected a
     * one-vertex request for a tri-list before this bound existed. */
    if (draw->vertex_count == 0u ||
        (draw->topology == OPENAGC_RASTER_TOPOLOGY_LINE_LIST &&
         draw->vertex_count % 2u != 0u) ||
        (draw->topology == OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST &&
         (draw->vertex_count < 3u || draw->vertex_count % 3u != 0u))) {
        return 0u;
    }
    if (draw->index_va != 0u) {
        uint64_t index_va = draw->index_va;
        uint32_t index_size = draw->index_size;

        if ((index_size != 2u && index_size != 4u) || draw->index_count == 0u ||
            draw->index_max_size < draw->index_count ||
            (index_va & ((uint64_t)index_size - 1u)) != 0ull) {
            return 0u;
        }
        if (draw->topology == OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST &&
            (draw->index_count < 3u || draw->index_count % 3u != 0u)) {
            return 0u;
        }
        if (draw->topology == OPENAGC_RASTER_TOPOLOGY_LINE_LIST &&
            draw->index_count % 2u != 0u) {
            return 0u;
        }
    } else if (draw->index_count != 0u || draw->index_size != 0u ||
               draw->index_max_size != 0u) {
        return 0u;
    }
    if (draw->color_width == 0u || draw->color_height == 0u ||
        draw->color_width > 16384u || draw->color_height > 16384u ||
        draw->color_pitch_bytes != draw->color_width * 4u ||
        draw->color_va == 0u || draw->vertex_code_va == 0u ||
        draw->fragment_code_va == 0u ||
        draw->color_va > UINT64_MAX -
                             (uint64_t)draw->color_pitch_bytes *
                                 draw->color_height ||
        (draw->color_va & 0xffull) != 0ull ||
        (draw->vertex_code_va & 0xffull) != 0ull ||
        (draw->fragment_code_va & 0xffull) != 0ull) {
        return 0u;
    }
    if (draw->viewport_width == 0u || draw->viewport_height == 0u ||
        draw->viewport_x >= draw->color_width ||
        draw->viewport_y >= draw->color_height ||
        draw->viewport_width > draw->color_width - draw->viewport_x ||
        draw->viewport_height > draw->color_height - draw->viewport_y) {
        return 0u;
    }
    x2 = draw->viewport_x + draw->viewport_width;
    y2 = draw->viewport_y + draw->viewport_height;
    if (draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_INDIRECT_TABLE &&
        draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_PLAIN_UCONFIG &&
        draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH) {
        return 0u;
    }
    if (draw->context_table == NULL || draw->context_table_va == 0u ||
        draw->uconfig_table == NULL || draw->uconfig_table_va == 0u ||
        (draw->context_table_va & 3ull) != 0ull ||
        (draw->uconfig_table_va & 3ull) != 0ull ||
        draw->context_table_va > UINT64_MAX - OPENAGC_PM4_NGG_TABLE_WORDS * 4u ||
        draw->uconfig_table_va >
            UINT64_MAX - OPENAGC_RASTER_UCONFIG_TABLE_WORDS * 4u) {
        return 0u;
    }
    if ((draw->gate_mask & ~(uint32_t)OPENAGC_RASTER_GATE_ALL) != 0u ||
        draw->cb_bind_count > OPENAGC_RASTER_CB_BIND_MAX ||
        (draw->cb_bind_count != 0u &&
         (draw->cb_bind_offsets == NULL || draw->cb_bind_values == NULL)) ||
        (draw->append_eop != 0u &&
         (draw->marker_va == 0u || (draw->marker_va & 7ull) != 0ull)) ||
        (draw->baseline_va & 3ull) != 0ull ||
        (draw->probe_va & 3ull) != 0ull ||
        (draw->ngg_probe_va & 3ull) != 0ull) {
        return 0u;
    }

    /* The context table carries the vertex records, the context linkage
     * record and the pixel records; a uconfig index in any of them is a
     * routing error, not a draw. */
    for (i = 0u; i < draw->program->vertex_context.count; ++i) {
        if (openagc_raster_linkage_is_uconfig(
                draw->program->vertex_context.offsets[i]) != 0u) {
            return 0u;
        }
    }
    for (i = 0u; i < draw->program->fragment_context.count; ++i) {
        if (openagc_raster_linkage_is_uconfig(
                draw->program->fragment_context.offsets[i]) != 0u) {
            return 0u;
        }
    }
    {
        uint32_t has_ge_cntl = 0u;
        uint32_t has_primitive_type = 0u;
        uint32_t has_stages_en = 0u;

        for (i = 0u; i < draw->program->linkage.count; ++i) {
            uint32_t offset = draw->program->linkage.offsets[i];

            if (openagc_raster_linkage_is_uconfig(offset) != 0u) {
                ++uconfig_records;
                if (offset == OPENAGC_GFX10_UCONFIG_GE_CNTL) {
                    has_ge_cntl = 1u;
                }
                if (offset == OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE) {
                    has_primitive_type = 1u;
                }
            } else {
                ++context_records;
                if (offset == OPENAGC_GFX10_VGT_SHADER_STAGES_EN) {
                    has_stages_en = 1u;
                }
            }
        }
        /* The NGG program is incomplete without GE_CNTL, and the VGT
         * assembles from VGT_PRIMITIVE_TYPE, which the draw owns: append
         * it when the linker's linkage block does not name it. */
        if (has_ge_cntl == 0u || has_stages_en == 0u) {
            return 0u;
        }
        if (has_primitive_type == 0u) {
            ++uconfig_records;
        }
    }
    context_records += draw->program->vertex_context.count +
                       draw->program->fragment_context.count;
    if (uconfig_records > OPENAGC_RASTER_UCONFIG_TABLE_RECORDS ||
        context_records > OPENAGC_PM4_NGG_TABLE_MAX_RECORDS ||
        context_records * 2u > OPENAGC_PM4_NGG_TABLE_WORDS) {
        return 0u;
    }
    if (draw->context_table_va < draw->uconfig_table_va +
                                     OPENAGC_RASTER_UCONFIG_TABLE_WORDS * 4u &&
        draw->uconfig_table_va < draw->context_table_va +
                                     OPENAGC_PM4_NGG_TABLE_WORDS * 4u) {
        return 0u;
    }

    for (i = 0u; i < OPENAGC_GFX10_FRAGMENT_GATE_COUNT; ++i) {
        gate_count += (draw->gate_mask >> i) & 1u;
    }
    cb_count = draw->cb_bind_count == 0u ? OPENAGC_GFX10_CB_BIND_COUNT
                                         : draw->cb_bind_count;
    required_words =
        (draw->baseline_va != 0u ? OPENAGC_GFX10_DRAW_BASELINE_COUNT *
                                       OPENAGC_PM4_COPY_DATA_WORDS : 0u) +
        OPENAGC_PM4_DRAW_POINT_STATE_COUNT * OPENAGC_PM4_SET_CONTEXT_WORDS(1u) +
        OPENAGC_PM4_SET_CONTEXT_WORDS(6u) +
        OPENAGC_PM4_SET_CONTEXT_WORDS(5u) +
        3u * OPENAGC_PM4_SET_CONTEXT_WORDS(2u) +
        (draw->probe_va != 0u ? OPENAGC_GFX10_DRAW_PROBE_COUNT *
                                    OPENAGC_PM4_COPY_DATA_WORDS : 0u) +
        5u +
        (draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_PLAIN_UCONFIG
             ? 5u : 0u) +
        (draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_INDIRECT_TABLE
             ? 3u : 0u) +
        (draw->program->vertex_shader.count +
         draw->program->fragment_shader.count +
         draw->program->user_data.count) * OPENAGC_PM4_SET_SH_GFX_WORDS(1u) +
        OPENAGC_PM4_SET_CONTEXT_WORDS(OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT) +
        gate_count * OPENAGC_PM4_SET_CONTEXT_WORDS(1u) +
        (draw->ngg_probe_va != 0u ? OPENAGC_PM4_NGG_PROBE_COUNT *
                                        OPENAGC_PM4_COPY_DATA_WORDS : 0u) +
        cb_count * OPENAGC_PM4_SET_CONTEXT_WORDS(1u) + 2u +
        (draw->index_va != 0u ? 3u + OPENAGC_PM4_DRAW_INDEX_2_WORDS : 3u) +
        (draw->append_eop != 0u ? OPENAGC_PM4_EOP_WITH_NOP_WORDS : 0u);
    if (required_words > max_words || required_words > OPENAGC_RASTER_MAX_WORDS) {
        return 0u;
    }

    cursor = 0u;
    memset(&state, 0, sizeof(state));
    state.color_va = draw->color_va;
    state.color_width = draw->color_width;
    state.color_height = draw->color_height;
    state.viewport_x = draw->viewport_x;
    state.viewport_y = draw->viewport_y;
    state.viewport_width = draw->viewport_width;
    state.viewport_height = draw->viewport_height;
    state.gs_out_prim_type = gs_out_prim;
    state.vertex_count = draw->vertex_count;
    if (openagc_pm4_draw_point_state_pairs(&state, state_offsets, state_values) !=
        OPENAGC_PM4_DRAW_POINT_STATE_COUNT) {
        return 0u;
    }

    if (draw->baseline_va != 0u) {
        for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
            openagc_pm4_encode_copy_data_reg_to_mem(
                openagc_pm4_copy_data_src_context_abs(
                    openagc_gfx10_draw_baseline_offsets[i]),
                draw->baseline_va + (uint64_t)i * 4u, words + cursor);
            cursor += OPENAGC_PM4_COPY_DATA_WORDS;
        }
    }

    cursor += openagc_pm4_encode_psbc_context_pairs(
        state_offsets, state_values, OPENAGC_PM4_DRAW_POINT_STATE_COUNT,
        words + cursor);
    openagc_gfx10_viewport_gl(draw->viewport_x, draw->viewport_y,
                              draw->viewport_width, draw->viewport_height,
                              viewport);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_CL_VPORT_XSCALE, 6u,
                                       viewport, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(6u);
    openagc_gfx10_guardband_gl(draw->viewport_x, draw->viewport_y,
                               draw->viewport_width, draw->viewport_height,
                               guardband);
    gb_sequence[0] = openagc_gfx10_draw_vtx_cntl();
    gb_sequence[1] = guardband[0];
    gb_sequence[2] = guardband[1];
    gb_sequence[3] = guardband[2];
    gb_sequence[4] = guardband[3];
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SU_VTX_CNTL, 5u,
                                       gb_sequence, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(5u);
    vport_scissor[0] = openagc_gfx10_vport_scissor_tl(draw->viewport_x,
                                                      draw->viewport_y);
    vport_scissor[1] = openagc_gfx10_vport_scissor_br(x2, y2);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_TL,
                                       2u, vport_scissor, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(2u);
    /* Without the generic scissor the scan converter keeps the reset
     * value and discards every fragment. */
    generic_scissor[0] = openagc_gfx10_generic_scissor_tl(draw->viewport_x,
                                                          draw->viewport_y);
    generic_scissor[1] = openagc_gfx10_generic_scissor_br(x2, y2);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SC_GENERIC_SCISSOR_TL,
                                       2u, generic_scissor, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(2u);
    depth_range[0] = 0u;
    depth_range[1] = 0x3f800000u;
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SC_VPORT_ZMIN_0, 2u,
                                       depth_range, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(2u);
    if (draw->probe_va != 0u) {
        for (i = 0u; i < OPENAGC_GFX10_DRAW_PROBE_COUNT; ++i) {
            openagc_pm4_encode_copy_data_reg_to_mem(
                openagc_pm4_copy_data_src_context_abs(
                    openagc_gfx10_draw_probe_offsets[i]),
                draw->probe_va + (uint64_t)i * 4u, words + cursor);
            cursor += OPENAGC_PM4_COPY_DATA_WORDS;
        }
    }

    record = 0u;
    for (i = 0u; i < draw->program->vertex_context.count; ++i) {
        draw->context_table[2u * record] = draw->program->vertex_context.offsets[i];
        draw->context_table[2u * record + 1u] =
            draw->program->vertex_context.values[i];
        ++record;
    }
    for (i = 0u; i < draw->program->linkage.count; ++i) {
        if (openagc_raster_linkage_is_uconfig(draw->program->linkage.offsets[i]) != 0u) {
            continue;
        }
        draw->context_table[2u * record] = draw->program->linkage.offsets[i];
        draw->context_table[2u * record + 1u] = draw->program->linkage.values[i];
        ++record;
    }
    for (i = 0u; i < draw->program->fragment_context.count; ++i) {
        draw->context_table[2u * record] = draw->program->fragment_context.offsets[i];
        draw->context_table[2u * record + 1u] =
            draw->program->fragment_context.values[i];
        ++record;
    }
    words[cursor++] =
        openagc_pm4_header3(OPENAGC_PM4_OP_CONTEXT_TABLE_LOAD, 5u, 0u);
    words[cursor++] = (uint32_t)draw->context_table_va;
    words[cursor++] = (uint32_t)(draw->context_table_va >> 32);
    words[cursor++] = OPENAGC_PM4_TABLE_LOAD_CONTROL;
    words[cursor++] = record;

    /*
     * The uconfig table: GE_CNTL, the user-VGPR enable and the input
     * topology, loaded the way the public native runtime loads its
     * linker output, with the topology value coming from this draw.
     * VGT_PRIMITIVE_TYPE is the register the VGT assembles from; a
     * context-table write of the same index would land on a different
     * register, so it is refused above.
     */
    record = 0u;
    {
        uint32_t has_primitive_type = 0u;

        for (i = 0u; i < draw->program->linkage.count; ++i) {
            uint32_t offset = draw->program->linkage.offsets[i];
            uint32_t value = draw->program->linkage.values[i];

            if (openagc_raster_linkage_is_uconfig(offset) == 0u) {
                continue;
            }
            if (offset == OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE) {
                value = di_primitive;
                has_primitive_type = 1u;
            }
            draw->uconfig_table[2u * record] = offset;
            draw->uconfig_table[2u * record + 1u] = value;
            ++record;
        }
        if (has_primitive_type == 0u) {
            draw->uconfig_table[2u * record] =
                OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE;
            draw->uconfig_table[2u * record + 1u] = di_primitive;
            ++record;
        }
    }
    if (draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_PLAIN_UCONFIG) {
        words[cursor++] =
            openagc_pm4_header3(OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD, 5u, 0u);
        words[cursor++] = (uint32_t)draw->uconfig_table_va;
        words[cursor++] = (uint32_t)(draw->uconfig_table_va >> 32);
        words[cursor++] = OPENAGC_PM4_TABLE_LOAD_CONTROL;
        words[cursor++] = record;
    }
    if (draw->topology_write != OPENAGC_RASTER_TOPOLOGY_WRITE_INDIRECT_TABLE) {
        words[cursor++] =
            openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
            OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] = OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE;
        words[cursor++] = di_primitive;
    }

    for (i = 0u; i < draw->program->vertex_shader.count; ++i) {
        vert_sh_values[i] = draw->program->vertex_shader.values[i];
    }
    vert_sh_values[draw->program->vertex_pgm_lo_slot] =
        (uint32_t)(draw->vertex_code_va >> 8);
    vert_sh_values[draw->program->vertex_pgm_hi_slot] =
        (uint32_t)(draw->vertex_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        draw->program->vertex_shader.offsets, vert_sh_values,
        draw->program->vertex_shader.count, words + cursor);
    for (i = 0u; i < draw->program->fragment_shader.count; ++i) {
        frag_sh_values[i] = draw->program->fragment_shader.values[i];
    }
    frag_sh_values[draw->program->fragment_pgm_lo_slot] =
        (uint32_t)(draw->fragment_code_va >> 8);
    frag_sh_values[draw->program->fragment_pgm_hi_slot] =
        (uint32_t)(draw->fragment_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        draw->program->fragment_shader.offsets, frag_sh_values,
        draw->program->fragment_shader.count, words + cursor);

    for (i = 0u; i < draw->program->user_data.count; ++i) {
        user_data_values[i] = draw->program->user_data.values[i];
    }
    user_data_values[draw->program->user_data_layout_slot] =
        draw->program->user_data_layout;
    for (i = 0u; i < draw->program->user_data.count; ++i) {
        openagc_pm4_encode_set_sh_graphics(draw->program->user_data.offsets[i], 1u,
                                           &user_data_values[i], words + cursor);
        cursor += OPENAGC_PM4_SET_SH_GFX_WORDS(1u);
    }

    openagc_gfx10_ps_input_cntl_defaults(ps_input_cntl);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_SPI_PS_INPUT_CNTL_0,
                                       OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT,
                                       ps_input_cntl, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT);
    for (i = 0u; i < OPENAGC_GFX10_FRAGMENT_GATE_COUNT; ++i) {
        if ((draw->gate_mask & (1u << i)) == 0u) {
            continue;
        }
        openagc_pm4_encode_set_context_reg(
            openagc_gfx10_fragment_gate_offsets[i], 1u,
            &openagc_gfx10_fragment_gate_values[i], words + cursor);
        cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);
    }

    if (draw->ngg_probe_va != 0u) {
        ngg_probe[0] = openagc_pm4_copy_data_src_context_abs(
            OPENAGC_GFX10_VGT_SHADER_STAGES_EN);
        ngg_probe[1] = OPENAGC_PM4_UCONFIG_REG_START +
                       OPENAGC_GFX10_UCONFIG_GE_CNTL;
        ngg_probe[2] = OPENAGC_PM4_SH_REG_START +
                       OPENAGC_GFX10_SPI_SHADER_PGM_LO_ES;
        ngg_probe[3] = OPENAGC_PM4_SH_REG_START +
                       OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 +
                       draw->program->user_data_layout_slot;
        ngg_probe[4] = OPENAGC_PM4_UCONFIG_REG_START +
                       OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE;
        ngg_probe[5] = openagc_pm4_copy_data_src_context_abs(
            OPENAGC_GFX10_UCONFIG_GE_CNTL);
        ngg_probe[6] = openagc_pm4_copy_data_src_context_abs(
            OPENAGC_GFX10_SPI_SHADER_COL_FORMAT);
        ngg_probe[7] = openagc_pm4_copy_data_src_context_abs(
            OPENAGC_GFX10_SPI_PS_INPUT_ENA);
        ngg_probe[8] = openagc_pm4_copy_data_src_context_abs(
            OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE);
        for (i = 0u; i < OPENAGC_PM4_NGG_PROBE_COUNT; ++i) {
            openagc_pm4_encode_copy_data_reg_to_mem(
                ngg_probe[i], draw->ngg_probe_va + (uint64_t)i * 4u,
                words + cursor);
            cursor += OPENAGC_PM4_COPY_DATA_WORDS;
        }
    }

    if (draw->cb_bind_count == 0u) {
        openagc_gfx10_cb_bind_linear_8888_words(
            draw->color_va, draw->color_width, draw->color_height,
            draw->color_bgra != 0u ? OPENAGC_GFX10_CB_COMP_SWAP_BGRA8
                                   : OPENAGC_GFX10_CB_COMP_SWAP_RGBA8,
            cb_values);
        cursor += openagc_pm4_encode_psbc_context_pairs(
            openagc_gfx10_cb_bind_offsets, cb_values, OPENAGC_GFX10_CB_BIND_COUNT,
            words + cursor);
    } else {
        cursor += openagc_pm4_encode_psbc_context_pairs(
            draw->cb_bind_offsets, draw->cb_bind_values, draw->cb_bind_count,
            words + cursor);
    }

    words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_NUM_INSTANCES, 2u, 0u);
    words[cursor++] = OPENAGC_PM4_DRAW_INSTANCE_COUNT;
    if (draw->index_va != 0u) {
        uint32_t index_type =
            ((draw->index_size >> 2) | (draw->index_size << 1)) & 3u;

        /* VGT_INDEX_TYPE is an indexed uconfig register: the plain packet
         * leaves the width the previous draw set. */
        words[cursor++] =
            openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG_INDEX, 3u, 0u) |
            OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] =
            OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE |
            OPENAGC_PM4_UCONFIG_VGT_INDEX_TYPE_HDR(
                OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE_INDEX);
        words[cursor++] = index_type;
        words[cursor++] = OPENAGC_PM4_DRAW_INDEX_2_HDR;
        words[cursor++] = draw->index_max_size;
        words[cursor++] = (uint32_t)draw->index_va;
        words[cursor++] = (uint32_t)(draw->index_va >> 32);
        words[cursor++] = draw->index_count;
        words[cursor++] = OPENAGC_PM4_DI_SRC_SEL_DMA;
    } else {
        words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_DRAW_INDEX_AUTO, 3u, 0u);
        words[cursor++] = draw->vertex_count;
        words[cursor++] = OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX;
    }

    if (draw->append_eop != 0u) {
        openagc_pm4_encode_eop_with_nops(draw->marker_va, draw->sequence,
                                         words + cursor);
        cursor += OPENAGC_PM4_EOP_WITH_NOP_WORDS;
    }
    return cursor;
}

openagc_result openagc_raster_get_capabilities(
    openagc_raster_capabilities *capabilities);

#ifdef __cplusplus
}
#endif

#endif
