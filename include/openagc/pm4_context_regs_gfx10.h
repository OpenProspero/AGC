/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_CONTEXT_REGS_GFX10_H
#define OPENAGC_PM4_CONTEXT_REGS_GFX10_H

#include <stdint.h>

/*
 * Public gfx10 context / SH register atlas for PSBC smoke metadata.
 *
 * Convention (drm/amdgpu gc_10_1_0_offset.h + Mesa SET_CONTEXT_REG):
 *   PACKET3_SET_CONTEXT_REG offset == mmNAME when NAME_BASE_IDX == 1
 *   Absolute MMIO = 0xA000 + (offset << 2)
 *   Documented R_028xxx = 0x28000 + (offset << 2)
 *
 * SET_SH_REG offsets use SH base 0xB000: pkt = (R_00Bxxx - 0xB000) >> 2.
 *
 * Offsets only — never invent register *values*. Smoke metadata values
 * are owned via PSBC fixtures + Steps P–U; CB_COLOR*_BASE values are not
 * present in smoke. Step W (relative COPY_DATA src) was console-negative;
 * Step X proved absolute CONTEXT_REG_START+offset; Step Y used that path
 * after SET_CONTEXT of smoke-owned SPI/PA/DB_SHADER/CB_SHADER_MASK values.
 * Step Z SETs owned CB_COLOR0_BASE(+EXT) from a GPU VA (Mesa: va>>8) then
 * absolute COPY_DATA readback — not INFO/ATTRIB invent, not DRAW.
 * hardware_qualified stays false.
 */

typedef struct openagc_gfx10_reg_name {
    uint32_t offset;
    const char *name;
    /* 1 = SET_CONTEXT_REG space; 0 = SET_SH_REG space. */
    uint32_t is_context;
    /* 1 when smoke metadata owns a value for this offset. */
    uint32_t smoke_owned;
    /* Short note; may be NULL. */
    const char *note;
} openagc_gfx10_reg_name;

/* smoke.vert context_registers */
#define OPENAGC_GFX10_SPI_VS_OUT_CONFIG 433u
#define OPENAGC_GFX10_SPI_SHADER_POS_FORMAT 451u
#define OPENAGC_GFX10_PA_CL_VS_OUT_CNTL 519u

/* smoke.vert linkage (also SET_CONTEXT_REG) */
#define OPENAGC_GFX10_GE_CNTL 603u
#define OPENAGC_GFX10_GE_USER_VGPR_EN 610u
#define OPENAGC_GFX10_VGT_SHADER_STAGES_EN 725u

/* smoke.frag context_registers */
#define OPENAGC_GFX10_SPI_SHADER_Z_FORMAT 452u
#define OPENAGC_GFX10_SPI_SHADER_COL_FORMAT 453u
#define OPENAGC_GFX10_SPI_PS_INPUT_ENA 435u
#define OPENAGC_GFX10_SPI_PS_INPUT_ADDR 436u
#define OPENAGC_GFX10_SPI_PS_IN_CONTROL 438u
#define OPENAGC_GFX10_SPI_BARYC_CNTL 440u
#define OPENAGC_GFX10_DB_SHADER_CONTROL 515u
#define OPENAGC_GFX10_CB_SHADER_MASK 143u
#define OPENAGC_GFX10_PA_SC_SHADER_CONTROL 784u

/* Public CB bind class — NOT in smoke metadata (gap for Stage 5). */
#define OPENAGC_GFX10_CB_TARGET_MASK 142u
#define OPENAGC_GFX10_CB_COLOR0_BASE 792u
/*
 * GFX10 (Mesa R_028C64_CB_COLOR0_BASE_EXT): dword after BASE is BASE_EXT,
 * not SI-era PITCH. Value = (gpu_va >> 8) >> 32. Do not program as pitch.
 */
#define OPENAGC_GFX10_CB_COLOR0_BASE_EXT 793u
#define OPENAGC_GFX10_CB_COLOR0_PITCH OPENAGC_GFX10_CB_COLOR0_BASE_EXT
/*
 * GFX10 (Mesa R_028C68_CB_COLOR0_ATTRIB2): not SI-era SLICE. INFO/ATTRIB
 * remain unowned; do not invent values for a "full" bind.
 */
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB2 794u
#define OPENAGC_GFX10_CB_COLOR0_SLICE OPENAGC_GFX10_CB_COLOR0_ATTRIB2
#define OPENAGC_GFX10_CB_COLOR0_VIEW 795u
#define OPENAGC_GFX10_CB_COLOR0_INFO 796u
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB 797u

/* Owned smoke.frag CB_SHADER_MASK — cite fixture; used by Steps Y/Z. */
#define OPENAGC_GFX10_CB_SHADER_MASK_OWNED 15u

/* smoke.vert / smoke.frag shader_registers (SET_SH_REG) */
#define OPENAGC_GFX10_SPI_SHADER_PGM_LO_VS 72u
#define OPENAGC_GFX10_SPI_SHADER_PGM_HI_VS 73u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC1_VS 74u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC2_VS 75u
#define OPENAGC_GFX10_SPI_SHADER_PGM_LO_PS 8u
#define OPENAGC_GFX10_SPI_SHADER_PGM_HI_PS 9u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC1_PS 10u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC2_PS 11u

/*
 * Fixed CB probe order for COLOR_BASE-class readback dumps (Steps W/X/Z).
 * Indices: [0]=BASE [1]=BASE_EXT [2]=ATTRIB2 [3]=VIEW [4]=INFO [5]=ATTRIB
 * [6]=TARGET_MASK [7]=SHADER_MASK. Values from console COPY_DATA only.
 */
#define OPENAGC_GFX10_CB_PROBE_COUNT 8u
#define OPENAGC_GFX10_CB_PROBE_IDX_BASE 0u
#define OPENAGC_GFX10_CB_PROBE_IDX_BASE_EXT 1u
#define OPENAGC_GFX10_CB_PROBE_IDX_SHADER_MASK 7u

static const uint32_t openagc_gfx10_cb_probe_offsets[OPENAGC_GFX10_CB_PROBE_COUNT] = {
    OPENAGC_GFX10_CB_COLOR0_BASE,   OPENAGC_GFX10_CB_COLOR0_BASE_EXT,
    OPENAGC_GFX10_CB_COLOR0_ATTRIB2, OPENAGC_GFX10_CB_COLOR0_VIEW,
    OPENAGC_GFX10_CB_COLOR0_INFO,   OPENAGC_GFX10_CB_COLOR0_ATTRIB,
    OPENAGC_GFX10_CB_TARGET_MASK,   OPENAGC_GFX10_CB_SHADER_MASK
};

/*
 * Public GFX10 MMIO cites for the GB tile-mode table (Step AA).
 *
 * gc_10_1_0_offset.h (BASE_IDX is 0 for all of these, so the address is
 * used as-is in PACKET3_COPY_DATA, whose src_sel=0 is "mem-mapped
 * register" — the same aperture Step X proved with 0xA000+ctxreg):
 *   mmGB_ADDR_CONFIG = 0x13DE
 *   mmGB_TILE_MODE0..31 = 0x13E4..0x1403
 * gc_10_1_0_sh_mask.h:
 *   GB_TILE_MODE0__ARRAY_MODE__SHIFT=2       mask 0x0000003C
 *   GB_TILE_MODE0__PIPE_CONFIG__SHIFT=6      mask 0x000007C0
 *   GB_TILE_MODE0__TILE_SPLIT__SHIFT=11
 *   GB_TILE_MODE0__MICRO_TILE_MODE_NEW__SHIFT=22 mask 0x01C00000
 *   GB_TILE_MODE0__SAMPLE_SPLIT__SHIFT=25
 *   GB_ADDR_CONFIG NUM_PIPES=0, PIPE_INTERLEAVE_SIZE=3,
 *   MAX_COMPRESSED_FRAGS=6, NUM_SHADER_ENGINES=19, NUM_RB_PER_SE=26
 *
 * The table is console state, never a value to invent: read it, then look
 * up the index for a requested (ARRAY_MODE, MICRO_TILE_MODE_NEW) pair.
 * Reading it owns no CB bind and opens no DRAW.
 */
#define OPENAGC_GFX10_MMIO_GB_ADDR_CONFIG 0x13DEu
#define OPENAGC_GFX10_MMIO_GB_TILE_MODE_BASE 0x13E4u
#define OPENAGC_GFX10_MMIO_GB_TILE_MODE_COUNT 32u
#define OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT \
    (1u + OPENAGC_GFX10_MMIO_GB_TILE_MODE_COUNT)

#define OPENAGC_GFX10_GB_TILE_MODE_ARRAY_MODE_SHIFT 2u
#define OPENAGC_GFX10_GB_TILE_MODE_ARRAY_MODE_MASK 0x0000003Cu
#define OPENAGC_GFX10_GB_TILE_MODE_MICRO_TILE_MODE_NEW_SHIFT 22u
#define OPENAGC_GFX10_GB_TILE_MODE_MICRO_TILE_MODE_NEW_MASK 0x01C00000u

static inline uint32_t openagc_gfx10_mmio_gb_tile_mode_offset(uint32_t index)
{
    return OPENAGC_GFX10_MMIO_GB_TILE_MODE_BASE + index;
}

static inline uint32_t openagc_gfx10_gb_tile_mode_array_mode(uint32_t word)
{
    return (word & OPENAGC_GFX10_GB_TILE_MODE_ARRAY_MODE_MASK) >>
           OPENAGC_GFX10_GB_TILE_MODE_ARRAY_MODE_SHIFT;
}

static inline uint32_t openagc_gfx10_gb_tile_mode_micro_tile_mode_new(uint32_t word)
{
    return (word & OPENAGC_GFX10_GB_TILE_MODE_MICRO_TILE_MODE_NEW_MASK) >>
           OPENAGC_GFX10_GB_TILE_MODE_MICRO_TILE_MODE_NEW_SHIFT;
}

/*
 * Step Y round-trip probe: smoke.frag context pairs with non-zero values
 * from tests/fixtures/psbc_smoke/smoke.frag.metadata.json. Deliberately
 * excludes zeros (ambiguous vs. clear) and COLOR_BASE-class offsets.
 * CB_SHADER_MASK=15 is distinct from Step X live residue (0xffffffff).
 */
#define OPENAGC_GFX10_CTXREG_RT_COUNT 6u

static const uint32_t openagc_gfx10_ctxreg_rt_offsets[OPENAGC_GFX10_CTXREG_RT_COUNT] = {
    OPENAGC_GFX10_SPI_SHADER_COL_FORMAT, OPENAGC_GFX10_SPI_PS_INPUT_ENA,
    OPENAGC_GFX10_SPI_PS_INPUT_ADDR,     OPENAGC_GFX10_SPI_PS_IN_CONTROL,
    OPENAGC_GFX10_DB_SHADER_CONTROL,     OPENAGC_GFX10_CB_SHADER_MASK
};

/* Owned smoke.frag values — do not invent; cite the fixture JSON. */
static const uint32_t openagc_gfx10_ctxreg_rt_values[OPENAGC_GFX10_CTXREG_RT_COUNT] = {
    9u, 128u, 128u, 32768u, 16u, OPENAGC_GFX10_CB_SHADER_MASK_OWNED
};

/*
 * Step Z owned CB BASE bind pairs (dynamic BASE/BASE_EXT from color VA).
 * INFO/ATTRIB/VIEW/TARGET_MASK are deliberately absent — public offsets
 * exist but safe values are not owned without more capture evidence.
 */
#define OPENAGC_GFX10_CTXREG_CB_BIND_SET_COUNT 3u

static const openagc_gfx10_reg_name openagc_gfx10_psbc_smoke_regs[] = {
    /* vert context */
    { OPENAGC_GFX10_SPI_VS_OUT_CONFIG, "SPI_VS_OUT_CONFIG", 1u, 1u, "smoke.vert ctx" },
    { OPENAGC_GFX10_SPI_SHADER_POS_FORMAT, "SPI_SHADER_POS_FORMAT", 1u, 1u,
      "smoke.vert ctx" },
    { OPENAGC_GFX10_PA_CL_VS_OUT_CNTL, "PA_CL_VS_OUT_CNTL", 1u, 1u, "smoke.vert ctx" },
    /* vert linkage */
    { OPENAGC_GFX10_GE_CNTL, "GE_CNTL", 1u, 1u,
      "smoke.vert linkage ge_cntl; Linux mmGE_CNTL=0x225B (low12=0x25B)" },
    { OPENAGC_GFX10_VGT_SHADER_STAGES_EN, "VGT_SHADER_STAGES_EN", 1u, 1u,
      "smoke.vert linkage stages_en" },
    { OPENAGC_GFX10_GE_USER_VGPR_EN, "GE_USER_VGPR_EN", 1u, 1u,
      "smoke.vert linkage user_vgpr_en; Linux mm=0x2262 (low12=0x262)" },
    /* frag context */
    { OPENAGC_GFX10_SPI_SHADER_Z_FORMAT, "SPI_SHADER_Z_FORMAT", 1u, 1u, "smoke.frag ctx" },
    { OPENAGC_GFX10_SPI_SHADER_COL_FORMAT, "SPI_SHADER_COL_FORMAT", 1u, 1u,
      "smoke.frag ctx" },
    { OPENAGC_GFX10_SPI_PS_INPUT_ENA, "SPI_PS_INPUT_ENA", 1u, 1u, "smoke.frag ctx" },
    { OPENAGC_GFX10_SPI_PS_INPUT_ADDR, "SPI_PS_INPUT_ADDR", 1u, 1u, "smoke.frag ctx" },
    { OPENAGC_GFX10_SPI_PS_IN_CONTROL, "SPI_PS_IN_CONTROL", 1u, 1u, "smoke.frag ctx" },
    { OPENAGC_GFX10_SPI_BARYC_CNTL, "SPI_BARYC_CNTL", 1u, 1u, "smoke.frag ctx" },
    { OPENAGC_GFX10_DB_SHADER_CONTROL, "DB_SHADER_CONTROL", 1u, 1u,
      "smoke.frag ctx; shader DB control, not DB_*_BASE bind" },
    { OPENAGC_GFX10_CB_SHADER_MASK, "CB_SHADER_MASK", 1u, 1u,
      "smoke.frag ctx; only CB_* owned by smoke — not COLOR_BASE" },
    { OPENAGC_GFX10_PA_SC_SHADER_CONTROL, "PA_SC_SHADER_CONTROL", 1u, 1u,
      "smoke.frag ctx" },
    /* SH */
    { OPENAGC_GFX10_SPI_SHADER_PGM_LO_VS, "SPI_SHADER_PGM_LO_VS", 0u, 1u,
      "smoke.vert SH; host patches PGM" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_HI_VS, "SPI_SHADER_PGM_HI_VS", 0u, 1u,
      "smoke.vert SH; host patches PGM" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_RSRC1_VS, "SPI_SHADER_PGM_RSRC1_VS", 0u, 1u,
      "smoke.vert SH" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_RSRC2_VS, "SPI_SHADER_PGM_RSRC2_VS", 0u, 1u,
      "smoke.vert SH" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_LO_PS, "SPI_SHADER_PGM_LO_PS", 0u, 1u,
      "smoke.frag SH; host patches PGM" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_HI_PS, "SPI_SHADER_PGM_HI_PS", 0u, 1u,
      "smoke.frag SH; host patches PGM" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_RSRC1_PS, "SPI_SHADER_PGM_RSRC1_PS", 0u, 1u,
      "smoke.frag SH" },
    { OPENAGC_GFX10_SPI_SHADER_PGM_RSRC2_PS, "SPI_SHADER_PGM_RSRC2_PS", 0u, 1u,
      "smoke.frag SH" },
    /* CB bind gap (offsets public; BASE value owned only after Step Z VA) */
    { OPENAGC_GFX10_CB_TARGET_MASK, "CB_TARGET_MASK", 1u, 0u, "not in smoke" },
    { OPENAGC_GFX10_CB_COLOR0_BASE, "CB_COLOR0_BASE", 1u, 0u,
      "Stage 5; Mesa va>>8; Step Z may own from GPU VA" },
    { OPENAGC_GFX10_CB_COLOR0_BASE_EXT, "CB_COLOR0_BASE_EXT", 1u, 0u,
      "GFX10 BASE_EXT (not SI PITCH); (va>>8)>>32" },
    { OPENAGC_GFX10_CB_COLOR0_ATTRIB2, "CB_COLOR0_ATTRIB2", 1u, 0u,
      "GFX10 ATTRIB2 (not SI SLICE); value unowned" },
    { OPENAGC_GFX10_CB_COLOR0_VIEW, "CB_COLOR0_VIEW", 1u, 0u, "Stage 5 gap" },
    { OPENAGC_GFX10_CB_COLOR0_INFO, "CB_COLOR0_INFO", 1u, 0u, "Stage 5 gap" },
    { OPENAGC_GFX10_CB_COLOR0_ATTRIB, "CB_COLOR0_ATTRIB", 1u, 0u, "Stage 5 gap" },
};

#define OPENAGC_GFX10_PSBC_SMOKE_REG_COUNT \
    ((uint32_t)(sizeof(openagc_gfx10_psbc_smoke_regs) / sizeof(openagc_gfx10_psbc_smoke_regs[0])))

static inline const openagc_gfx10_reg_name *openagc_gfx10_lookup_reg(uint32_t offset,
                                                                    uint32_t is_context)
{
    uint32_t i;

    for (i = 0u; i < OPENAGC_GFX10_PSBC_SMOKE_REG_COUNT; ++i) {
        if (openagc_gfx10_psbc_smoke_regs[i].offset == offset &&
            openagc_gfx10_psbc_smoke_regs[i].is_context == is_context) {
            return &openagc_gfx10_psbc_smoke_regs[i];
        }
    }
    return (const openagc_gfx10_reg_name *)0;
}

#endif /* OPENAGC_PM4_CONTEXT_REGS_GFX10_H */
