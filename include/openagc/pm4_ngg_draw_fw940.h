/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/*
 * Step AD: the NGG draw IB.
 *
 * The legacy point path did not produce pixels in Steps AC-10..AC-12.
 * Step AD proved that uconfig GE_CNTL is writable, while the
 * VGT_PRIMITIVE_TYPE readback remained zero; the indexed readback may not
 * expose its bank. No conclusion that all uconfig writes are blocked follows.
 * NGG is the path the console's own driver uses, and for NGG the input
 * topology is a *context* register, VGT_GS_OUT_PRIM_TYPE (0x29b), which this
 * IB can write. Everything else comes from the pinned compiler's NGG output:
 * the ES/GS shader registers, the linkage writes (GE_CNTL 0x25b,
 * VGT_SHADER_STAGES_EN 0x2d5, SPI_SHADER_USER_VGPR_EN 0x262) and the LDS
 * layout dword. Nothing here is invented: the tables are the fixture's
 * metadata, and the draw state is the Step-AC state this console already
 * accepted.
 *
 * Evidence sources for the NGG register placement:
 *   - the pinned compiler's metadata (linkage_stages_en.offset == 0x2d5,
 *     linkage_ge_cntl.offset == 0x25b, linkage_user_vgpr_en.offset == 0x262);
 *   - ps5-opengl's src/platform/ps5_agc_package.c, which packs the NGG
 *     vertex stage at SH 0x0c8/0x0c9 (PGM) and 0x08a/0x08b (RSRC);
 *   - a decoded public capture of an NGG triangle draw, whose context table
 *     carries VGT_SHADER_STAGES_EN = 0x12010 (ES_STAGE_REAL | PRIMGEN_EN) and
 *     VGT_GS_OUT_PRIM_TYPE = 2, and whose stream writes the NGG user data
 *     with one SET_SH_REG at 0x8c.
 *
 * Bounding: the program is a bounded number of SET_CONTEXT_REG packets, the
 * draw is one DRAW_INDEX_AUTO, and every refused input returns 0 instead of
 * emitting a partial IB.
 */

#ifndef OPENAGC_PM4_NGG_DRAW_FW940_H
#define OPENAGC_PM4_NGG_DRAW_FW940_H

#include "openagc/pm4_draw_fw940.h"

/* Largest number of writes read out of one fixture metadata table. */
#define OPENAGC_PM4_NGG_MAX_WRITES 32u
/* GS user-data dwords the NGG vertex stage declares. The console-proven
 * block is SPI_SHADER_USER_DATA_GS_0..3 (SH 140..143). */
#define OPENAGC_PM4_NGG_USER_DATA_COUNT 4u
/* NGG registers read back after the program, in this order:
 * VGT_SHADER_STAGES_EN (context, written directly), GE_CNTL (uconfig),
 * ES PGM LO (SH), the GS user-data dword that carries the LDS layout (SH),
 * VGT_PRIMITIVE_TYPE (uconfig), GE_CNTL (context aperture), then three
 * registers that come from the context *table*: SPI_SHADER_COL_FORMAT,
 * SPI_PS_INPUT_ENA and VGT_ESGS_RING_ITEMSIZE (context 0x2ab). The last
 * three say whether the table load landed; older logs probed 0x2d3, which
 * is GE_NGG_SUBGRP_CNTL, under the ring-item-size name. */
#define OPENAGC_PM4_NGG_PROBE_COUNT 9u
/* The context register table: what Sony's own helpers load, and the form
 * this console accepted for every context register Step-AC probes. One
 * record is {offset u32, value u32}; the load packet is the opcode, the
 * table address low/high, the control word and the record count. */
#define OPENAGC_PM4_NGG_TABLE_MAX_RECORDS                                        \
    (2u * OPENAGC_PM4_NGG_MAX_WRITES + OPENAGC_PM4_NGG_MAX_WRITES +              \
     2u * OPENAGC_PM4_NGG_MAX_WRITES)
#define OPENAGC_PM4_NGG_TABLE_WORDS (2u * OPENAGC_PM4_NGG_TABLE_MAX_RECORDS)

/*
 * One register-write table from the fixture metadata: offsets[i] is paired
 * with values[i], written with SET_CONTEXT_REG packets (the form the
 * console-proven smoke program already uses for every context and SH
 * register it does not have to index).
 */
typedef struct openagc_pm4_ngg_table {
    uint32_t count;
    const uint32_t *offsets;
    const uint32_t *values;
} openagc_pm4_ngg_table;

typedef struct openagc_pm4_ngg_program {
    /* smoke.vert compiled with --ngg: context registers, ES/GS shader
     * registers, and the linkage writes VGT_SHADER_STAGES_EN (which carries
     * ES_STAGE_REAL and PRIMGEN_EN), GE_CNTL and SPI_SHADER_USER_VGPR_EN. */
    openagc_pm4_ngg_table vertex_context;
    openagc_pm4_ngg_table vertex_shader;
    openagc_pm4_ngg_table linkage;
    /* smoke.frag: context registers and PS shader registers. */
    openagc_pm4_ngg_table fragment_context;
    openagc_pm4_ngg_table fragment_shader;
    /* Index inside vertex_shader/fragment_shader of the PGM LO and PGM HI
     * dwords; the encoder patches them from the uploaded code VAs with the
     * console-proven >>8 / >>40 encoding. */
    uint32_t vertex_pgm_lo_slot;
    uint32_t vertex_pgm_hi_slot;
    uint32_t fragment_pgm_lo_slot;
    uint32_t fragment_pgm_hi_slot;
    /* GS user-data dwords (SH offsets from SPI_SHADER_USER_DATA_GS_0) and
     * which of them receives the compiler's ngg_lds_layout. */
    openagc_pm4_ngg_table user_data;
    uint32_t user_data_layout_slot;
    uint32_t user_data_layout;
    /* The uconfig VGT_PRIMITIVE_TYPE value for the draw's input topology:
     * DI_PT_POINTLIST (1) for the point, DI_PT_TRILIST (4) for the triangle.
     * GE_CNTL comes from the program's linkage instead, because it is the
     * one linkage register that lives in the uconfig space. */
    uint32_t di_primitive;
} openagc_pm4_ngg_program;

/* IB layout: the Step-AC preamble (baseline reads, scalar state, viewport,
 * guardband, per-viewport scissor, draw-state probe), the context register
 * table load that carries the NGG program, the ES/GS and PS shader
 * registers, the GS user data, the readbacks, the colour bind, one
 * instance, one draw and the EOP trailer. The uconfig writes and readbacks
 * of Step AC are deliberately absent: the NGG topology lives in the
 * context. */
#define OPENAGC_PM4_NGG_DRAW_EOP_WORDS                                        \
    (OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +                                  \
     (OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 1u) *                              \
         OPENAGC_PM4_SET_CONTEXT_WORDS(1u) +                                   \
     OPENAGC_PM4_SET_CONTEXT_WORDS(6u) +                                       \
     OPENAGC_PM4_SET_CONTEXT_WORDS(5u) +                                       \
     OPENAGC_PM4_SET_CONTEXT_WORDS(2u) +                                       \
     OPENAGC_GFX10_DRAW_PROBE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS +            \
     5u /* the context register table load */ +                                \
     3u * (3u + OPENAGC_PM4_COPY_DATA_WORDS) /* GE_CNTL, broadcast, type */ + \
     5u * OPENAGC_PM4_COPY_DATA_WORDS /* the extra readbacks */ +  \
     2u * OPENAGC_PM4_NGG_MAX_WRITES * OPENAGC_PM4_SET_SH_GFX_WORDS(1u) +      \
     OPENAGC_PM4_NGG_USER_DATA_COUNT * OPENAGC_PM4_SET_SH_GFX_WORDS(1u) +      \
     OPENAGC_PM4_NGG_PROBE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS +               \
     OPENAGC_GFX10_CB_BIND_COUNT * OPENAGC_PM4_SET_CONTEXT_WORDS(1u) +         \
     2u /* NUM_INSTANCES */ + 3u /* DRAW_INDEX_AUTO */ +                       \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)

static inline uint32_t openagc_pm4_ngg_table_valid(
    const openagc_pm4_ngg_table *table)
{
    if (table == NULL || table->count == 0u) {
        return 0u;
    }
    if (table->count > OPENAGC_PM4_NGG_MAX_WRITES) {
        return 0u;
    }
    if (table->offsets == NULL || table->values == NULL) {
        return 0u;
    }
    return 1u;
}

/*
 * Encode the Step-AD NGG draw IB. The state, baseline, probe and dump
 * addresses follow the Step-AC encoder; ngg_probe_va receives
 * OPENAGC_PM4_NGG_PROBE_COUNT words with the NGG registers above, so the
 * dump proves the compiler's linkage and shader registers landed.
 * vert_code_va/frag_code_va must be 256-byte-aligned uploaded code
 * addresses. words must hold OPENAGC_PM4_NGG_DRAW_EOP_WORDS. Returns the
 * dword count actually written, or 0 when anything is refused.
 */
static inline uint32_t openagc_pm4_encode_draw_ngg_eop(
    const openagc_pm4_draw_point_state *state,
    const openagc_pm4_ngg_program *program, uint64_t baseline_va,
    uint64_t probe_va, uint64_t ngg_probe_va, uint64_t vert_code_va,
    uint64_t frag_code_va, uint32_t *table, uint64_t table_va,
    uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t offsets[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t values[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t cb_values[OPENAGC_GFX10_CB_BIND_COUNT];
    uint32_t vert_sh_values[OPENAGC_PM4_NGG_MAX_WRITES];
    uint32_t frag_sh_values[OPENAGC_PM4_NGG_MAX_WRITES];
    uint32_t user_data_values[OPENAGC_PM4_NGG_USER_DATA_COUNT];
    uint32_t viewport[6];
    uint32_t guardband[4];
    uint32_t gb_sequence[5];
    uint32_t vport_scissor[2];
    uint32_t ngg_probe[OPENAGC_PM4_NGG_PROBE_COUNT];
    uint32_t table_records;
    uint32_t ge_cntl;
    uint32_t iamvp;
    uint32_t cursor = 0u;
    uint32_t count;
    uint32_t i;

    if (state == NULL || program == NULL || words == NULL || table == NULL) {
        return 0u;
    }
    /* Only the two shapes exercised by the public ps5-opengl draw-state
     * contract are represented here. In particular, a triangle list needs
     * three vertices per primitive; a one-vertex draw silently assembles
     * nothing even when all register writes complete. */
    if (!((program->di_primitive == OPENAGC_GFX10_DI_PT_POINTLIST &&
           state->gs_out_prim_type == OPENAGC_GFX10_GS_OUT_POINTLIST) ||
          (program->di_primitive == OPENAGC_GFX10_DI_PT_TRILIST &&
           state->gs_out_prim_type == OPENAGC_GFX10_GS_OUT_TRISTRIP &&
           state->vertex_count >= 3u && state->vertex_count % 3u == 0u))) {
        return 0u;
    }
    if ((vert_code_va & 0xffull) != 0ull || (frag_code_va & 0xffull) != 0ull) {
        return 0u;
    }
    if (!openagc_pm4_ngg_table_valid(&program->vertex_context) ||
        !openagc_pm4_ngg_table_valid(&program->vertex_shader) ||
        !openagc_pm4_ngg_table_valid(&program->linkage) ||
        !openagc_pm4_ngg_table_valid(&program->fragment_context) ||
        !openagc_pm4_ngg_table_valid(&program->fragment_shader) ||
        !openagc_pm4_ngg_table_valid(&program->user_data)) {
        return 0u;
    }
    if (program->user_data.count != OPENAGC_PM4_NGG_USER_DATA_COUNT) {
        return 0u;
    }
    if (program->vertex_pgm_lo_slot >= program->vertex_shader.count ||
        program->vertex_pgm_hi_slot >= program->vertex_shader.count ||
        program->fragment_pgm_lo_slot >= program->fragment_shader.count ||
        program->fragment_pgm_hi_slot >= program->fragment_shader.count ||
        program->user_data_layout_slot >= program->user_data.count) {
        return 0u;
    }
    count = openagc_pm4_draw_point_state_pairs(state, offsets, values);
    if (count != OPENAGC_PM4_DRAW_POINT_STATE_COUNT) {
        return 0u;
    }

    for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(
                openagc_gfx10_draw_baseline_offsets[i]),
            baseline_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    cursor += openagc_pm4_encode_psbc_context_pairs(
        offsets, values, count, words + cursor);

    iamvp = openagc_gfx10_draw_ia_multi_vgt_param();
    openagc_pm4_encode_set_context_reg_idx(OPENAGC_GFX10_IA_MULTI_VGT_PARAM,
                                           OPENAGC_PM4_IAMVP_INDEX, 1u, &iamvp,
                                           words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);

    openagc_gfx10_viewport_gl(state->viewport_x, state->viewport_y,
                              state->viewport_width, state->viewport_height,
                              viewport);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_CL_VPORT_XSCALE, 6u,
                                       viewport, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(6u);
    openagc_gfx10_guardband_gl(state->viewport_x, state->viewport_y,
                               state->viewport_width, state->viewport_height,
                               guardband);
    gb_sequence[0] = openagc_gfx10_draw_vtx_cntl();
    gb_sequence[1] = guardband[0];
    gb_sequence[2] = guardband[1];
    gb_sequence[3] = guardband[2];
    gb_sequence[4] = guardband[3];
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SU_VTX_CNTL, 5u,
                                       gb_sequence, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(5u);
    vport_scissor[0] = openagc_gfx10_vport_scissor_tl(state->viewport_x,
                                                      state->viewport_y);
    vport_scissor[1] = openagc_gfx10_vport_scissor_br(
        state->viewport_x + state->viewport_width,
        state->viewport_y + state->viewport_height);
    openagc_pm4_encode_set_context_reg(OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_TL,
                                       2u, vport_scissor, words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(2u);

    for (i = 0u; i < OPENAGC_GFX10_DRAW_PROBE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(
                openagc_gfx10_draw_probe_offsets[i]),
            probe_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    /* The NGG program, written the way the capture writes it: one context
     * register table carrying the vertex context, the linkage and the
     * fragment context records, in fixture order. The uconfig registers are
     * written separately, below. */
    table_records = program->vertex_context.count +
                    program->fragment_context.count;
    for (count = 0u; count < program->linkage.count; ++count) {
        if (program->linkage.offsets[count] != OPENAGC_GFX10_UCONFIG_GE_CNTL) {
            ++table_records;
        }
    }
    if (table_records == 0u ||
        table_records > OPENAGC_PM4_NGG_TABLE_MAX_RECORDS) {
        return 0u;
    }
    i = 0u;
    for (count = 0u; count < program->vertex_context.count; ++count) {
        table[2u * i] = program->vertex_context.offsets[count];
        table[2u * i + 1u] = program->vertex_context.values[count];
        ++i;
    }
    for (count = 0u; count < program->linkage.count; ++count) {
        if (program->linkage.offsets[count] == OPENAGC_GFX10_UCONFIG_GE_CNTL) {
            continue;
        }
        table[2u * i] = program->linkage.offsets[count];
        table[2u * i + 1u] = program->linkage.values[count];
        ++i;
    }
    for (count = 0u; count < program->fragment_context.count; ++count) {
        table[2u * i] = program->fragment_context.offsets[count];
        table[2u * i + 1u] = program->fragment_context.values[count];
        ++i;
    }
    words[cursor++] =
        openagc_pm4_header3(OPENAGC_PM4_OP_CONTEXT_TABLE_LOAD, 5u, 0u);
    words[cursor++] = (uint32_t)table_va;
    words[cursor++] = (uint32_t)(table_va >> 32);
    words[cursor++] = OPENAGC_PM4_TABLE_LOAD_CONTROL;
    words[cursor++] = table_records;

    /* GE_CNTL and the input topology are uconfig registers: Mesa's offset
     * table gives them the 0x2xxx form (0x225b and 0x2242), and the packet's
     * offset field is the uconfig index itself — not a difference against
     * the context aperture. Step AC addressed it as a difference and wrote
     * every uconfig register to a nonsense place, which is what made the
     * uconfig space look read-only. The CAM bit is the header flag their
     * decoded stream uses. */
    ge_cntl = 0u;
    for (count = 0u; count < program->linkage.count; ++count) {
        if (program->linkage.offsets[count] == OPENAGC_GFX10_UCONFIG_GE_CNTL) {
            ge_cntl = program->linkage.values[count];
        }
    }
    if (ge_cntl != 0u) {
        words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG,
                                              3u, 0u) |
                          OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] = OPENAGC_GFX10_UCONFIG_GE_CNTL;
        words[cursor++] = ge_cntl;
    }
    if (program->di_primitive != 0u) {
        words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG,
                                              3u, 0u) |
                          OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] = OPENAGC_GFX10_UCONFIG_GRBM_GFX_INDEX;
        words[cursor++] = OPENAGC_GFX10_GRBM_GFX_INDEX_BROADCAST;
        words[cursor++] =
            openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG_INDEX, 3u, 0u) |
            OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] =
            OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE |
            (OPENAGC_PM4_UCONFIG_PRIMITIVE_TYPE_INDEX << 28);
        words[cursor++] = program->di_primitive;
    }

    /* ES/GS shader registers (PGM patched to the uploaded code) and the PS
     * shader registers (PGM patched). */
    for (i = 0u; i < program->vertex_shader.count; ++i) {
        vert_sh_values[i] = program->vertex_shader.values[i];
    }
    vert_sh_values[program->vertex_pgm_lo_slot] = (uint32_t)(vert_code_va >> 8);
    vert_sh_values[program->vertex_pgm_hi_slot] = (uint32_t)(vert_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        program->vertex_shader.offsets, vert_sh_values,
        program->vertex_shader.count, words + cursor);
    for (i = 0u; i < program->fragment_shader.count; ++i) {
        frag_sh_values[i] = program->fragment_shader.values[i];
    }
    frag_sh_values[program->fragment_pgm_lo_slot] = (uint32_t)(frag_code_va >> 8);
    frag_sh_values[program->fragment_pgm_hi_slot] =
        (uint32_t)(frag_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        program->fragment_shader.offsets, frag_sh_values,
        program->fragment_shader.count, words + cursor);

    /* The NGG vertex stage's declared user data. The compiler's
     * ngg_lds_layout goes in the dword it names; every other dword keeps the
     * fixture's value, and the driver-supplied ones (base vertex, indexed
     * draw) are zero for a one-vertex auto-indexed draw. */
    for (i = 0u; i < program->user_data.count; ++i) {
        user_data_values[i] = program->user_data.values[i];
    }
    user_data_values[program->user_data_layout_slot] = program->user_data_layout;
    for (i = 0u; i < program->user_data.count; ++i) {
        openagc_pm4_encode_set_sh_graphics(program->user_data.offsets[i], 1u,
                                           &user_data_values[i],
                                           words + cursor);
        cursor += OPENAGC_PM4_SET_SH_GFX_WORDS(1u);
    }

    /* Readbacks, in the order the probe count documents: the two linkage
     * registers that say this is an NGG ES (stages_en carries PRIMGEN_EN),
     * the ES program address, the LDS layout dword, and the two uconfig
     * registers read back through their own aperture and through the context
     * one, so "the write was dropped" and "the wrong aperture was written"
     * cannot be confused. */
    ngg_probe[0] = openagc_pm4_copy_data_src_context_abs(
        OPENAGC_GFX10_VGT_SHADER_STAGES_EN);
    ngg_probe[1] = OPENAGC_PM4_UCONFIG_REG_START +
                   OPENAGC_GFX10_UCONFIG_GE_CNTL;
    ngg_probe[2] = OPENAGC_PM4_SH_REG_START +
                   OPENAGC_GFX10_SPI_SHADER_PGM_LO_ES;
    ngg_probe[3] = OPENAGC_PM4_SH_REG_START +
                   OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 +
                   program->user_data_layout_slot;
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
            ngg_probe[i], ngg_probe_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    openagc_gfx10_cb_bind_linear_8888_words(
        state->color_va, state->color_width, state->color_height,
        OPENAGC_GFX10_CB_COMP_SWAP_RGBA8, cb_values);
    cursor += openagc_pm4_encode_psbc_context_pairs(
        openagc_gfx10_cb_bind_offsets, cb_values, OPENAGC_GFX10_CB_BIND_COUNT,
        words + cursor);

    words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_NUM_INSTANCES, 2u, 0u);
    words[cursor++] = OPENAGC_PM4_DRAW_INSTANCE_COUNT;
    words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_DRAW_INDEX_AUTO, 3u, 0u);
    words[cursor++] = state->vertex_count;
    words[cursor++] = OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX;

    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    cursor += OPENAGC_PM4_EOP_WITH_NOP_WORDS;
    return cursor;
}

#endif /* OPENAGC_PM4_NGG_DRAW_FW940_H */
