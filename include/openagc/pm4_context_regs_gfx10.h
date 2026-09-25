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
 * are owned via PSBC fixtures + Steps P–U; CB_COLOR*_BASE/PITCH/… values
 * are not present in smoke and must come from an independently owned
 * console readback/capture (Step W). hardware_qualified stays false.
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
#define OPENAGC_GFX10_CB_COLOR0_PITCH 793u
#define OPENAGC_GFX10_CB_COLOR0_SLICE 794u
#define OPENAGC_GFX10_CB_COLOR0_VIEW 795u
#define OPENAGC_GFX10_CB_COLOR0_INFO 796u
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB 797u

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
 * Fixed Step-W probe order for COLOR_BASE-class readback dumps.
 * Values must come from console COPY_DATA; do not invent.
 */
#define OPENAGC_GFX10_CB_PROBE_COUNT 8u

static const uint32_t openagc_gfx10_cb_probe_offsets[OPENAGC_GFX10_CB_PROBE_COUNT] = {
    OPENAGC_GFX10_CB_COLOR0_BASE,   OPENAGC_GFX10_CB_COLOR0_PITCH,
    OPENAGC_GFX10_CB_COLOR0_SLICE,  OPENAGC_GFX10_CB_COLOR0_VIEW,
    OPENAGC_GFX10_CB_COLOR0_INFO,   OPENAGC_GFX10_CB_COLOR0_ATTRIB,
    OPENAGC_GFX10_CB_TARGET_MASK,   OPENAGC_GFX10_CB_SHADER_MASK
};

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
    /* CB bind gap (offsets public; values not smoke-owned) */
    { OPENAGC_GFX10_CB_TARGET_MASK, "CB_TARGET_MASK", 1u, 0u, "not in smoke" },
    { OPENAGC_GFX10_CB_COLOR0_BASE, "CB_COLOR0_BASE", 1u, 0u, "Stage 5 gap" },
    { OPENAGC_GFX10_CB_COLOR0_PITCH, "CB_COLOR0_PITCH", 1u, 0u, "Stage 5 gap" },
    { OPENAGC_GFX10_CB_COLOR0_SLICE, "CB_COLOR0_SLICE", 1u, 0u, "Stage 5 gap" },
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
