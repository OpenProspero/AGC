/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_GRAPHICS_FW940_H
#define OPENAGC_PM4_GRAPHICS_FW940_H

#include "openagc/pm4_fw940.h"

#include <stdint.h>

/*
 * Graphics-bank PM4 helpers for FW9.40 host snapshots.
 *
 * Independent public cites (drm/amdgpu / umr PM4 decoders):
 *   PACKET3_SET_CONTEXT_REG = 0x69  (reg base 0xA000 + offset)
 *   PACKET3_SET_SH_REG      = 0x76  (graphics SH; low_bits=0)
 *   PACKET3_DRAW_INDEX_AUTO = 0x2D  — opcode only; not encoded here
 *
 * Register *offsets and values* must come from a verified PSBC metadata
 * object (context_registers / shader_registers), never invented.
 * No DRAW packet is emitted. Console CB/DB bind and draw remain gated
 * until an independently owned FW9.40 IB capture exists.
 */

#define OPENAGC_PM4_OP_SET_CONTEXT_REG 0x69u
#define OPENAGC_PM4_OP_SET_SH_REG 0x76u
#define OPENAGC_PM4_OP_DRAW_INDEX_AUTO 0x2du

#define OPENAGC_PM4_SET_CONTEXT_WORDS(value_count) ((value_count) + 2u)
#define OPENAGC_PM4_SET_SH_GFX_WORDS(value_count) ((value_count) + 2u)

/* One SET_CONTEXT_REG: offset dword then value_count data dwords. */
static inline void openagc_pm4_encode_set_context_reg(uint32_t offset,
                                                      uint32_t value_count,
                                                      const uint32_t *values,
                                                      uint32_t *words)
{
    uint32_t i;

    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG,
                                   OPENAGC_PM4_SET_CONTEXT_WORDS(value_count), 0u);
    words[1] = offset & 0xffffu;
    for (i = 0u; i < value_count; ++i) {
        words[2u + i] = values[i];
    }
}

/* Graphics SET_SH_REG (compute-bank bit clear). */
static inline void openagc_pm4_encode_set_sh_graphics(uint32_t offset,
                                                      uint32_t value_count,
                                                      const uint32_t *values,
                                                      uint32_t *words)
{
    uint32_t i;

    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG,
                                   OPENAGC_PM4_SET_SH_GFX_WORDS(value_count), 0u);
    words[1] = offset & 0xffffu;
    for (i = 0u; i < value_count; ++i) {
        words[2u + i] = values[i];
    }
}

/*
 * Emit one SET_CONTEXT_REG packet per (offset,value) pair from PSBC
 * context_registers. words must hold 3 * pair_count dwords.
 */
static inline uint32_t openagc_pm4_encode_psbc_context_pairs(
    const uint32_t *offsets, const uint32_t *values, uint32_t pair_count,
    uint32_t *words)
{
    uint32_t i;
    uint32_t cursor = 0u;

    for (i = 0u; i < pair_count; ++i) {
        openagc_pm4_encode_set_context_reg(offsets[i], 1u, &values[i], words + cursor);
        cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);
    }
    return cursor;
}

/*
 * Emit one graphics SET_SH_REG packet per (offset,value) pair from PSBC
 * shader_registers. words must hold 3 * pair_count dwords.
 */
static inline uint32_t openagc_pm4_encode_psbc_shader_pairs(
    const uint32_t *offsets, const uint32_t *values, uint32_t pair_count,
    uint32_t *words)
{
    uint32_t i;
    uint32_t cursor = 0u;

    for (i = 0u; i < pair_count; ++i) {
        openagc_pm4_encode_set_sh_graphics(offsets[i], 1u, &values[i], words + cursor);
        cursor += OPENAGC_PM4_SET_SH_GFX_WORDS(1u);
    }
    return cursor;
}

/*
 * Graphics-bank SET_SH_REG pairs + shared EOP+NOP (Step O vehicle).
 * No SET_CONTEXT_REG, no DRAW. words must hold 3*pair_count + EOP dwords.
 */
#define OPENAGC_PM4_GRAPHICS_SH_EOP_WORDS(pair_count) \
    ((uint32_t)(3u * (pair_count) + OPENAGC_PM4_EOP_WITH_NOP_WORDS))

static inline uint32_t openagc_pm4_encode_graphics_sh_eop(
    const uint32_t *offsets, const uint32_t *values, uint32_t pair_count,
    uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t cursor;

    cursor = openagc_pm4_encode_psbc_shader_pairs(offsets, values, pair_count, words);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * SET_CONTEXT_REG pairs + shared EOP+NOP (Step P / Step R vehicle).
 * Offsets/values must come from verified PSBC metadata:
 *   Step P — context_registers only (no linkage);
 *   Step R — linkage ge_cntl/stages_en/user_vgpr_en only.
 * No SET_SH, no DRAW, no CB/DB. words must hold 3*pair_count + EOP dwords.
 */
#define OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(pair_count) \
    ((uint32_t)(3u * (pair_count) + OPENAGC_PM4_EOP_WITH_NOP_WORDS))

/* Step R: three linkage context pairs + EOP (same word count as Step P). */
#define OPENAGC_PM4_GRAPHICS_LINKAGE_EOP_WORDS \
    OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(3u)

static inline uint32_t openagc_pm4_encode_graphics_context_eop(
    const uint32_t *offsets, const uint32_t *values, uint32_t pair_count,
    uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t cursor;

    cursor = openagc_pm4_encode_psbc_context_pairs(offsets, values, pair_count, words);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Linkage SET_CONTEXT_REG ×3 + EOP (Step R vehicle). Offsets/values from
 * verified PSBC linkage (ge_cntl, stages_en, user_vgpr_en). Same encoder
 * as Step P; no SET_SH, no DRAW, no CB/DB.
 */
static inline uint32_t openagc_pm4_encode_graphics_linkage_eop(
    uint32_t ge_cntl_offset, uint32_t ge_cntl_value, uint32_t stages_en_offset,
    uint32_t stages_en_value, uint32_t user_vgpr_en_offset,
    uint32_t user_vgpr_en_value, uint32_t sequence, uint64_t marker_va,
    uint32_t *words)
{
    uint32_t offsets[3];
    uint32_t values[3];

    offsets[0] = ge_cntl_offset;
    offsets[1] = stages_en_offset;
    offsets[2] = user_vgpr_en_offset;
    values[0] = ge_cntl_value;
    values[1] = stages_en_value;
    values[2] = user_vgpr_en_value;
    return openagc_pm4_encode_graphics_context_eop(offsets, values, 3u, sequence,
                                                   marker_va, words);
}

/*
 * SET_CONTEXT_REG pairs then graphics SET_SH_REG pairs + shared EOP+NOP
 * (Step Q / Step T vehicle). Step Q uses smoke.vert (3+4); Step T uses
 * smoke.frag (9+4, no linkage). Order matches host
 * openagc_psbc_reflection_encode_register_program without linkage. No
 * ge_cntl/stages_en, no DRAW, no CB/DB.
 * words must hold 3*(ctx_count+sh_count) + EOP dwords.
 */
#define OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(ctx_count, sh_count) \
    ((uint32_t)(3u * ((ctx_count) + (sh_count)) + OPENAGC_PM4_EOP_WITH_NOP_WORDS))

static inline uint32_t openagc_pm4_encode_graphics_context_sh_eop(
    const uint32_t *ctx_offsets, const uint32_t *ctx_values, uint32_t ctx_count,
    const uint32_t *sh_offsets, const uint32_t *sh_values, uint32_t sh_count,
    uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t cursor;

    cursor = openagc_pm4_encode_psbc_context_pairs(ctx_offsets, ctx_values, ctx_count,
                                                   words);
    cursor += openagc_pm4_encode_psbc_shader_pairs(sh_offsets, sh_values, sh_count,
                                                   words + cursor);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Full host-aligned register program + EOP (Step S vehicle): SET_CONTEXT
 * (context_registers) then graphics SET_SH then linkage SET_CONTEXT ×3,
 * matching openagc_psbc_reflection_encode_register_program order. Same
 * proven pair sources as Steps Q and R. No DRAW, no CB/DB.
 * words must hold 3*(ctx_count+sh_count+3) + EOP dwords.
 */
#define OPENAGC_PM4_GRAPHICS_CONTEXT_SH_LINKAGE_EOP_WORDS(ctx_count, sh_count) \
    ((uint32_t)(3u * ((ctx_count) + (sh_count) + 3u) + OPENAGC_PM4_EOP_WITH_NOP_WORDS))

static inline uint32_t openagc_pm4_encode_graphics_context_sh_linkage_eop(
    const uint32_t *ctx_offsets, const uint32_t *ctx_values, uint32_t ctx_count,
    const uint32_t *sh_offsets, const uint32_t *sh_values, uint32_t sh_count,
    uint32_t ge_cntl_offset, uint32_t ge_cntl_value, uint32_t stages_en_offset,
    uint32_t stages_en_value, uint32_t user_vgpr_en_offset,
    uint32_t user_vgpr_en_value, uint32_t sequence, uint64_t marker_va,
    uint32_t *words)
{
    uint32_t link_offsets[3];
    uint32_t link_values[3];
    uint32_t cursor;

    link_offsets[0] = ge_cntl_offset;
    link_offsets[1] = stages_en_offset;
    link_offsets[2] = user_vgpr_en_offset;
    link_values[0] = ge_cntl_value;
    link_values[1] = stages_en_value;
    link_values[2] = user_vgpr_en_value;
    cursor = openagc_pm4_encode_psbc_context_pairs(ctx_offsets, ctx_values, ctx_count,
                                                   words);
    cursor += openagc_pm4_encode_psbc_shader_pairs(sh_offsets, sh_values, sh_count,
                                                   words + cursor);
    cursor += openagc_pm4_encode_psbc_context_pairs(link_offsets, link_values, 3u,
                                                    words + cursor);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Combined vert + frag host register program + EOP (Step U vehicle):
 * smoke.vert context + SH + linkage (Step S body) then smoke.frag context
 * + SH (Step T body), matching host
 * openagc_psbc_reflection_encode_register_program(vert) then (frag).
 * One shared EOP+NOP trailer. No DRAW, no CB/DB.
 * words must hold 3*(v_ctx+v_sh+3+f_ctx+f_sh) + EOP dwords.
 */
#define OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(v_ctx, v_sh, f_ctx, f_sh) \
    ((uint32_t)(3u * ((v_ctx) + (v_sh) + 3u + (f_ctx) + (f_sh)) +       \
                OPENAGC_PM4_EOP_WITH_NOP_WORDS))

static inline uint32_t openagc_pm4_encode_graphics_vert_frag_eop(
    const uint32_t *vert_ctx_offsets, const uint32_t *vert_ctx_values,
    uint32_t vert_ctx_count, const uint32_t *vert_sh_offsets,
    const uint32_t *vert_sh_values, uint32_t vert_sh_count,
    uint32_t ge_cntl_offset, uint32_t ge_cntl_value, uint32_t stages_en_offset,
    uint32_t stages_en_value, uint32_t user_vgpr_en_offset,
    uint32_t user_vgpr_en_value, const uint32_t *frag_ctx_offsets,
    const uint32_t *frag_ctx_values, uint32_t frag_ctx_count,
    const uint32_t *frag_sh_offsets, const uint32_t *frag_sh_values,
    uint32_t frag_sh_count, uint32_t sequence, uint64_t marker_va,
    uint32_t *words)
{
    uint32_t link_offsets[3];
    uint32_t link_values[3];
    uint32_t cursor;

    link_offsets[0] = ge_cntl_offset;
    link_offsets[1] = stages_en_offset;
    link_offsets[2] = user_vgpr_en_offset;
    link_values[0] = ge_cntl_value;
    link_values[1] = stages_en_value;
    link_values[2] = user_vgpr_en_value;
    cursor = openagc_pm4_encode_psbc_context_pairs(vert_ctx_offsets, vert_ctx_values,
                                                   vert_ctx_count, words);
    cursor += openagc_pm4_encode_psbc_shader_pairs(vert_sh_offsets, vert_sh_values,
                                                   vert_sh_count, words + cursor);
    cursor += openagc_pm4_encode_psbc_context_pairs(link_offsets, link_values, 3u,
                                                    words + cursor);
    cursor += openagc_pm4_encode_psbc_context_pairs(frag_ctx_offsets, frag_ctx_values,
                                                    frag_ctx_count, words + cursor);
    cursor += openagc_pm4_encode_psbc_shader_pairs(frag_sh_offsets, frag_sh_values,
                                                   frag_sh_count, words + cursor);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

#endif /* OPENAGC_PM4_GRAPHICS_FW940_H */
