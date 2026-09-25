/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_COPY_DATA_FW940_H
#define OPENAGC_PM4_COPY_DATA_FW940_H

#include "openagc/pm4_context_regs_gfx10.h"
#include "openagc/pm4_fw940.h"
#include "openagc/pm4_graphics_fw940.h"

#include <stdint.h>

/*
 * IT_COPY_DATA (PACKET3 opcode 0x40) register→memory — public AMD cite.
 *
 * Independent cites:
 *   Mesa sid.h: PKT3_COPY_DATA=0x40, COPY_DATA_SRC_SEL/DST_SEL/WR_CONFIRM
 *   drm/amdgpu gfx_v10_0_ring_emit_rreg:
 *     PACKET3(COPY_DATA, 4)  → 6 dwords
 *     control = SRC_SEL(0=reg) | DST_SEL(5=mem) | WR_CONFIRM
 *     src_lo = absolute mm-mapped register dword address
 *     src_hi = 0
 *     dst_lo/hi = memory VA
 *   drm/amdgpu soc15d.h: PACKET3_SET_CONTEXT_REG_START = 0xA000
 *     SET_CONTEXT_REG packet offset is relative; COPY_DATA MEM_MAPPED_REGISTER
 *     src uses the absolute aperture address (START + relative offset).
 *
 * Step W (negative on FW9.40): src_lo = SET_CONTEXT relative offsets only
 *   (e.g. CB_COLOR0_BASE=792). completed=0 / poison — encoding not proven.
 * Step X: src_lo = OPENAGC_PM4_CONTEXT_REG_START + relative offset (e.g.
 *   0xA000+792). Distinct offset encoding; same control word as emit_rreg.
 *   Console-proven completed=1 (COLOR_BASE-class zeros; masks ffffffff).
 * Step Y: SET_CONTEXT smoke-owned SPI/PA/DB_SHADER/CB_SHADER_MASK values,
 *   then absolute COPY_DATA readback (round-trip). Not a COLOR_BASE bind.
 * Step Z: SET_CONTEXT CB_COLOR0_BASE + BASE_EXT from owned GPU VA (Mesa
 *   encoding va>>8 / (va>>8)>>32) plus owned CB_SHADER_MASK, then absolute
 *   COPY_DATA of the CB probe set. Does not invent INFO/ATTRIB/VIEW/DRAW.
 *
 * Does not invent CB INFO/ATTRIB values.
 * hardware_qualified / gpu_executable stay false.
 */

#define OPENAGC_PM4_OP_COPY_DATA 0x40u

/* soc15d.h PACKET3_SET_CONTEXT_REG_START — absolute context aperture base. */
#define OPENAGC_PM4_CONTEXT_REG_START 0xA000u

#define OPENAGC_PM4_COPY_DATA_SRC_SEL_REG 0u
#define OPENAGC_PM4_COPY_DATA_DST_SEL_MEM 5u
#define OPENAGC_PM4_COPY_DATA_CONTROL                                      \
    ((OPENAGC_PM4_COPY_DATA_SRC_SEL_REG) |                                 \
     (OPENAGC_PM4_COPY_DATA_DST_SEL_MEM << 8) | (1u << 20))

#define OPENAGC_PM4_COPY_DATA_WORDS 6u
#define OPENAGC_PM4_COPY_DATA_EOP_WORDS(reg_count)                         \
    ((uint32_t)((reg_count) * OPENAGC_PM4_COPY_DATA_WORDS +                \
                OPENAGC_PM4_EOP_WITH_NOP_WORDS))
#define OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS                            \
    OPENAGC_PM4_COPY_DATA_EOP_WORDS(OPENAGC_GFX10_CB_PROBE_COUNT)

/* Step Y: SET_CONTEXT pairs + abs COPY_DATA readback + EOP. */
#define OPENAGC_PM4_CTXREG_RT_SET_WORDS                                     \
    ((uint32_t)(OPENAGC_GFX10_CTXREG_RT_COUNT *                             \
                OPENAGC_PM4_SET_CONTEXT_WORDS(1u)))
#define OPENAGC_PM4_CTXREG_RT_COPY_WORDS                                    \
    ((uint32_t)(OPENAGC_GFX10_CTXREG_RT_COUNT * OPENAGC_PM4_COPY_DATA_WORDS))
#define OPENAGC_PM4_CTXREG_RT_EOP_WORDS                                     \
    (OPENAGC_PM4_CTXREG_RT_SET_WORDS + OPENAGC_PM4_CTXREG_RT_COPY_WORDS +  \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)

/* Step Z: SET BASE+BASE_EXT+SHADER_MASK + abs COPY_DATA CB probe + EOP. */
#define OPENAGC_PM4_CTXREG_CB_BIND_SET_WORDS                                 \
    ((uint32_t)(OPENAGC_GFX10_CTXREG_CB_BIND_SET_COUNT *                   \
                OPENAGC_PM4_SET_CONTEXT_WORDS(1u)))
#define OPENAGC_PM4_CTXREG_CB_BIND_COPY_WORDS                               \
    ((uint32_t)(OPENAGC_GFX10_CB_PROBE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS))
#define OPENAGC_PM4_CTXREG_CB_BIND_EOP_WORDS                                \
    (OPENAGC_PM4_CTXREG_CB_BIND_SET_WORDS +                                \
     OPENAGC_PM4_CTXREG_CB_BIND_COPY_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS)

/* Mesa radeonsi/radv: CB_COLORi_BASE = (gpu_va + surf_offset) >> 8. */
static inline uint32_t openagc_pm4_cb_color0_base_lo(uint64_t color_va)
{
    return (uint32_t)(color_va >> 8);
}

static inline uint32_t openagc_pm4_cb_color0_base_ext(uint64_t color_va)
{
    return (uint32_t)((color_va >> 8) >> 32);
}

/* Absolute COPY_DATA src for a SET_CONTEXT_REG relative offset. */
static inline uint32_t openagc_pm4_copy_data_src_context_abs(uint32_t relative_offset)
{
    return OPENAGC_PM4_CONTEXT_REG_START + relative_offset;
}

/* One register→memory COPY_DATA (6 dwords). src_lo is caller-supplied. */
static inline void openagc_pm4_encode_copy_data_reg_to_mem(uint32_t reg_src,
                                                           uint64_t dst_va,
                                                           uint32_t *words)
{
    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_COPY_DATA,
                                   OPENAGC_PM4_COPY_DATA_WORDS, 0u);
    words[1] = OPENAGC_PM4_COPY_DATA_CONTROL;
    words[2] = reg_src;
    words[3] = 0u;
    words[4] = (uint32_t)dst_va;
    words[5] = (uint32_t)(dst_va >> 32);
}

/*
 * Step W vehicle (relative SET_CONTEXT offsets as src_lo). Console-negative
 * on FW9.40; kept for host regression / historical encode lock.
 * words must hold OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS.
 */
static inline uint32_t openagc_pm4_encode_copy_data_cb_probe_eop(
    uint64_t dst_base_va, uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t i;
    uint32_t cursor = 0u;

    for (i = 0u; i < OPENAGC_GFX10_CB_PROBE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_gfx10_cb_probe_offsets[i],
            dst_base_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Step X vehicle: same CB probe set, src_lo = CONTEXT_REG_START + relative.
 * Distinct from Step W; cite PACKET3_SET_CONTEXT_REG_START + emit_rreg.
 * words must hold OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS.
 */
static inline uint32_t openagc_pm4_encode_copy_data_cb_probe_abs_eop(
    uint64_t dst_base_va, uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t i;
    uint32_t cursor = 0u;

    for (i = 0u; i < OPENAGC_GFX10_CB_PROBE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(openagc_gfx10_cb_probe_offsets[i]),
            dst_base_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Step Y vehicle: SET_CONTEXT smoke-owned RT pairs, then absolute COPY_DATA
 * register→memory of the same offsets, then EOP. words must hold
 * OPENAGC_PM4_CTXREG_RT_EOP_WORDS. Values are the owned smoke fixture
 * constants — not COLOR_BASE invent.
 */
static inline uint32_t openagc_pm4_encode_ctxreg_rt_abs_eop(
    uint64_t dst_base_va, uint32_t sequence, uint64_t marker_va, uint32_t *words)
{
    uint32_t i;
    uint32_t cursor;

    cursor = openagc_pm4_encode_psbc_context_pairs(
        openagc_gfx10_ctxreg_rt_offsets, openagc_gfx10_ctxreg_rt_values,
        OPENAGC_GFX10_CTXREG_RT_COUNT, words);
    for (i = 0u; i < OPENAGC_GFX10_CTXREG_RT_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(openagc_gfx10_ctxreg_rt_offsets[i]),
            dst_base_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

/*
 * Step Z vehicle: SET_CONTEXT owned CB_COLOR0_BASE + BASE_EXT (from
 * color_va via public Mesa va>>8 encoding) and owned CB_SHADER_MASK, then
 * absolute COPY_DATA of the eight CB probe offsets, then EOP.
 * Does not SET INFO/ATTRIB/VIEW/TARGET_MASK (unowned). words must hold
 * OPENAGC_PM4_CTXREG_CB_BIND_EOP_WORDS.
 */
static inline uint32_t openagc_pm4_encode_ctxreg_cb_bind_abs_eop(
    uint64_t color_va, uint64_t dst_base_va, uint32_t sequence,
    uint64_t marker_va, uint32_t *words)
{
    static const uint32_t offsets[OPENAGC_GFX10_CTXREG_CB_BIND_SET_COUNT] = {
        OPENAGC_GFX10_CB_COLOR0_BASE, OPENAGC_GFX10_CB_COLOR0_BASE_EXT,
        OPENAGC_GFX10_CB_SHADER_MASK
    };
    uint32_t values[OPENAGC_GFX10_CTXREG_CB_BIND_SET_COUNT];
    uint32_t i;
    uint32_t cursor;

    values[0] = openagc_pm4_cb_color0_base_lo(color_va);
    values[1] = openagc_pm4_cb_color0_base_ext(color_va);
    values[2] = OPENAGC_GFX10_CB_SHADER_MASK_OWNED;

    cursor = openagc_pm4_encode_psbc_context_pairs(
        offsets, values, OPENAGC_GFX10_CTXREG_CB_BIND_SET_COUNT, words);
    for (i = 0u; i < OPENAGC_GFX10_CB_PROBE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(openagc_gfx10_cb_probe_offsets[i]),
            dst_base_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

#endif /* OPENAGC_PM4_COPY_DATA_FW940_H */
