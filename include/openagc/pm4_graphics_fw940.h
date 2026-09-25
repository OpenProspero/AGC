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

#endif /* OPENAGC_PM4_GRAPHICS_FW940_H */
