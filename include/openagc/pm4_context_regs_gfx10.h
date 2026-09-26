/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_CONTEXT_REGS_GFX10_H
#define OPENAGC_PM4_CONTEXT_REGS_GFX10_H

#include <stddef.h>
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
 * Step AB composes all nine gc_10_1_0 bind words above from cited field
 * encodings and reads them back (VIEW/INFO/ATTRIB/ATTRIB2/ATTRIB3/
 * TARGET_MASK/BASE_EXT at 912); it also corrected this atlas (793/794 are
 * PITCH/SLICE holes, not BASE_EXT/ATTRIB2). No DRAW is submitted.
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
/* Mesa gfx10.json maps context 0x2ab to VGT_ESGS_RING_ITEMSIZE and 0x2d3
 * to GE_NGG_SUBGRP_CNTL. Earlier probes read 0x2d3 under the wrong name. */
#define OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE 683u
#define OPENAGC_GFX10_GE_NGG_SUBGRP_CNTL 723u
#define OPENAGC_GFX10_SPI_PS_INPUT_ADDR 436u
#define OPENAGC_GFX10_SPI_PS_IN_CONTROL 438u
#define OPENAGC_GFX10_SPI_BARYC_CNTL 440u
#define OPENAGC_GFX10_DB_SHADER_CONTROL 515u
#define OPENAGC_GFX10_CB_SHADER_MASK 143u
#define OPENAGC_GFX10_PA_SC_SHADER_CONTROL 784u

/*
 * Public CB bind class — NOT in smoke metadata (gap for Stage 5).
 *
 * Offsets from drm/amdgpu gc_10_1_0_offset.h (every entry BASE_IDX == 1):
 *   mmCB_TARGET_MASK     0x008E = 142
 *   mmCB_SHADER_MASK     0x008F = 143
 *   mmCB_COLOR0_BASE     0x0318 = 792
 *   mmCB_COLOR0_PITCH    0x0319 = 793   (GFX10 hole; a driver emits 0)
 *   mmCB_COLOR0_SLICE    0x031A = 794   (GFX10 hole; a driver emits 0)
 *   mmCB_COLOR0_VIEW     0x031B = 795
 *   mmCB_COLOR0_INFO     0x031C = 796
 *   mmCB_COLOR0_ATTRIB   0x031D = 797
 *   mmCB_COLOR0_BASE_EXT 0x0390 = 912
 *   mmCB_COLOR0_ATTRIB2  0x03B0 = 944
 *   mmCB_COLOR0_ATTRIB3  0x03B8 = 952
 *
 * Correction (Step AB): this atlas previously read Mesa's SI-era names
 * R_028C64_CB_COLOR0_BASE_EXT and R_028C68_CB_COLOR0_ATTRIB2 as offsets
 * 793/794. gc_10_1_0 places BASE_EXT at 912 and ATTRIB2/ATTRIB3 at
 * 944/952; 793/794 are PITCH/SLICE, which radeonsi's GFX10 block emits as
 * zeros ("hole"). The Step-Z readback index 1 was therefore
 * CB_COLOR0_PITCH, not BASE_EXT, and its zero is not a BASE_EXT
 * round-trip: that claim is withdrawn in docs/hardware-evidence.md.
 * GFX10 also does not use CB_COLOR0_ATTRIB.TILE_MODE_INDEX for color
 * buffers — tiling is ATTRIB3.COLOR_SW_MODE, a fixed enum (see below).
 */
#define OPENAGC_GFX10_CB_TARGET_MASK 142u
#define OPENAGC_GFX10_CB_COLOR0_BASE 792u
#define OPENAGC_GFX10_CB_COLOR0_PITCH 793u
#define OPENAGC_GFX10_CB_COLOR0_SLICE 794u
#define OPENAGC_GFX10_CB_COLOR0_VIEW 795u
#define OPENAGC_GFX10_CB_COLOR0_INFO 796u
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB 797u
#define OPENAGC_GFX10_CB_COLOR0_BASE_EXT 912u
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB2 944u
#define OPENAGC_GFX10_CB_COLOR0_ATTRIB3 952u

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
 * Indices, with the corrected gc_10_1_0 names: [0]=BASE [1]=PITCH
 * [2]=SLICE [3]=VIEW [4]=INFO [5]=ATTRIB [6]=TARGET_MASK [7]=SHADER_MASK.
 * PITCH/SLICE are GFX10 holes, so index 1 carried a zero that Step Z read
 * as "BASE_EXT". Values from console COPY_DATA only; keep this order, it
 * is what the already-recorded Steps W/X/Z dumps were decoded with.
 */
#define OPENAGC_GFX10_CB_PROBE_COUNT 8u
#define OPENAGC_GFX10_CB_PROBE_IDX_BASE 0u
#define OPENAGC_GFX10_CB_PROBE_IDX_PITCH 1u
#define OPENAGC_GFX10_CB_PROBE_IDX_SLICE 2u
#define OPENAGC_GFX10_CB_PROBE_IDX_VIEW 3u
#define OPENAGC_GFX10_CB_PROBE_IDX_INFO 4u
#define OPENAGC_GFX10_CB_PROBE_IDX_ATTRIB 5u
#define OPENAGC_GFX10_CB_PROBE_IDX_TARGET_MASK 6u
#define OPENAGC_GFX10_CB_PROBE_IDX_SHADER_MASK 7u

static const uint32_t openagc_gfx10_cb_probe_offsets[OPENAGC_GFX10_CB_PROBE_COUNT] = {
    OPENAGC_GFX10_CB_COLOR0_BASE,    OPENAGC_GFX10_CB_COLOR0_PITCH,
    OPENAGC_GFX10_CB_COLOR0_SLICE,   OPENAGC_GFX10_CB_COLOR0_VIEW,
    OPENAGC_GFX10_CB_COLOR0_INFO,    OPENAGC_GFX10_CB_COLOR0_ATTRIB,
    OPENAGC_GFX10_CB_TARGET_MASK,    OPENAGC_GFX10_CB_SHADER_MASK
};

/* addrtypes.h AddrSwizzleMode: ADDR_SW_LINEAR = 0. ADDR_SW_LINEAR_GENERAL
 * is 32 and does not fit the 5-bit COLOR_SW_MODE field. */
#define OPENAGC_GFX10_SW_MODE_LINEAR 0u
/* addrtypes.h AddrResourceType / ac_surface.h gfx9_resource_type. */
#define OPENAGC_GFX10_RESOURCE_TYPE_2D 1u

/*
 * Step-AC draw-state atlas: the minimum a GFX10 driver programs on top of
 * `PACKET3_CLEAR_STATE` so one point primitive reaches the color buffer.
 *
 * Offsets from drm/amdgpu gc_10_1_0_offset.h (all BASE_IDX == 1, so the
 * SET_CONTEXT_REG offset is the low 12 bits of the mm name, which is the
 * same value Mesa computes as (R_028xxx - SI_CONTEXT_REG_OFFSET) >> 2):
 *   DB_Z_INFO 0x0010, DB_STENCIL_INFO 0x0011
 *   PA_SC_SCREEN_SCISSOR_TL/BR 0x000C/0x000D
 *   PA_SC_WINDOW_SCISSOR_TL/BR 0x0081/0x0082, PA_SC_CLIPRECT_RULE 0x0083
 *   PA_SC_EDGERULE 0x008C, PA_SU_HARDWARE_SCREEN_OFFSET 0x008D
 *   PA_SC_VPORT_ZMIN_0/ZMAX_0 0x00B4/0x00B5
 *   PA_CL_VPORT_XSCALE..ZOFFSET 0x010F..0x0114
 *   SPI_INTERP_CONTROL_0 0x01B5, PA_CL_CLIP_CNTL 0x0204,
 *   PA_SU_SC_MODE_CNTL 0x0205, PA_SU_POINT_SIZE 0x0280,
 *   PA_SU_POINT_MINMAX 0x0281, PA_SC_MODE_CNTL_0 0x0292,
 *   PA_SU_VTX_CNTL 0x02F9, PA_CL_GB_VERT/HORZ_{CLIP,DISC}_ADJ 0x02FA..0x02FD
 * VGT_PRIMITIVE_TYPE is *not* a context register: Mesa writes it with
 * `radeon_set_uconfig_reg` on GFX10 (si_state_draw.cpp), i.e. packet
 * SET_UCONFIG_REG (0x79) at offset (R_030908 - CIK_UCONFIG_REG_OFFSET) >> 2
 * = mm low 12 = 578.
 *
 * Values come from Mesa's `si_emit_rasterizer_prim_state` /
 * `si_create_rs_state` / `si_emit_framebuffer_state` /
 * `si_emit_viewport`+`ac_compute_guardband` / `si_emit_window_rectangles`,
 * all at the pinned revision; enum values from registers/gfx10.json.
 * Only one point is drawn, with no depth buffer and no DRAW of any other
 * primitive, so this state is a bounded experiment, not a general path.
 */
#define OPENAGC_GFX10_DB_Z_INFO 16u
#define OPENAGC_GFX10_DB_STENCIL_INFO 17u
#define OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_TL 12u
#define OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_BR 13u
#define OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_TL 129u
#define OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_BR 130u
#define OPENAGC_GFX10_PA_SC_CLIPRECT_RULE 131u
#define OPENAGC_GFX10_PA_SC_EDGERULE 140u
#define OPENAGC_GFX10_PA_SU_HARDWARE_SCREEN_OFFSET 141u
#define OPENAGC_GFX10_PA_SC_VPORT_ZMIN_0 180u
#define OPENAGC_GFX10_PA_SC_VPORT_ZMAX_0 181u
#define OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_TL 148u
#define OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_BR 149u
#define OPENAGC_GFX10_PA_CL_VPORT_XSCALE 271u
#define OPENAGC_GFX10_PA_CL_VPORT_XOFFSET 272u
#define OPENAGC_GFX10_PA_CL_VPORT_YSCALE 273u
#define OPENAGC_GFX10_PA_CL_VPORT_YOFFSET 274u
#define OPENAGC_GFX10_PA_CL_VPORT_ZSCALE 275u
#define OPENAGC_GFX10_PA_CL_VPORT_ZOFFSET 276u
#define OPENAGC_GFX10_SPI_INTERP_CONTROL_0 437u
#define OPENAGC_GFX10_PA_CL_CLIP_CNTL 516u
#define OPENAGC_GFX10_PA_SU_SC_MODE_CNTL 517u
#define OPENAGC_GFX10_PA_SU_POINT_SIZE 640u
#define OPENAGC_GFX10_PA_SU_POINT_MINMAX 641u
#define OPENAGC_GFX10_PA_SC_MODE_CNTL_0 658u
#define OPENAGC_GFX10_PA_SC_MODE_CNTL_1 659u
#define OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE 667u
#define OPENAGC_GFX10_IA_MULTI_VGT_PARAM 682u
#define OPENAGC_GFX10_CB_BLEND0_CONTROL 480u
#define OPENAGC_GFX10_CB_COLOR_CONTROL 514u
#define OPENAGC_GFX10_DB_EQAA 513u
#define OPENAGC_GFX10_PA_SC_CENTROID_PRIORITY_0 757u
#define OPENAGC_GFX10_PA_SC_CENTROID_PRIORITY_1 758u
#define OPENAGC_GFX10_PA_SC_AA_CONFIG 760u
#define OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X0Y0 766u
#define OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X1Y0 770u
#define OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X0Y1 774u
#define OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X1Y1 778u
#define OPENAGC_GFX10_PA_SC_AA_MASK_X0Y0_X1Y0 782u
#define OPENAGC_GFX10_PA_SC_AA_MASK_X0Y1_X1Y1 783u
#define OPENAGC_GFX10_PA_SU_VTX_CNTL 761u
#define OPENAGC_GFX10_PA_CL_GB_VERT_CLIP_ADJ 762u
#define OPENAGC_GFX10_PA_CL_GB_VERT_DISC_ADJ 763u
#define OPENAGC_GFX10_PA_CL_GB_HORZ_CLIP_ADJ 764u
#define OPENAGC_GFX10_PA_CL_GB_HORZ_DISC_ADJ 765u

/* VGT_PRIMITIVE_TYPE uconfig address: mm 0x2242 (BASE_IDX 1) -> low 12 bits
 * 0x242 -> Mesa's CIK_UCONFIG_REG_OFFSET + (0x242 << 2) = 0x030908. The
 * SET_UCONFIG_REG packet offset is (address - base) >> 2 = 0x242 = 578. */
#define OPENAGC_GFX10_UCONFIG_REG_VGT_PRIMITIVE_TYPE 0x00030908u
#define OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE 578u
/* mmVGT_INDEX_TYPE 0x2243 -> uconfig index 0x243. */
#define OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE 579u
#define OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE_INDEX 2u
#define OPENAGC_PM4_UCONFIG_VGT_INDEX_TYPE_HDR(idx) ((idx) << 28)

/*
 * The uconfig aperture: soc15d.h gives PACKET3_SET_UCONFIG_REG_START
 * 0xC000 and _END 0xC400, the same shape as the context aperture
 * (0xA000) that Step X proved for COPY_DATA register->memory. Reading
 * VGT_PRIMITIVE_TYPE back at 0xC000 + 578 is therefore the one way for
 * user mode to check its own topology write.
 */
#define OPENAGC_PM4_UCONFIG_REG_START 0xC000u

/*
 * The SH aperture: soc15d.h gives PACKET3_SET_SH_REG_START 0x2C00 and
 * _END 0x3000, so a COPY_DATA read of an SH-space register (SH offset n,
 * the same numbering the SET_SH_REG packets use) is taken at 0x2C00 + n.
 */
#define OPENAGC_PM4_SH_REG_START 0x2C00u

/*
 * Stage user-data blocks, in the same SH numbering the PGM registers use
 * (the pinned fixtures put SPI_SHADER_PGM_LO/HI/RSRC1/RSRC2_VS at 72-75 and
 * the PS ones at 8-11). PS5_Vulkan's decoded AGC stream names the NGG
 * vertex stage's block: `SET_SH_REG` at 0xb230 = SPI_SHADER_USER_DATA_GS_*,
 * i.e. SH offset 140, and `0xb030` = SPI_SHADER_USER_DATA_PS_* at 12.
 */
#define OPENAGC_GFX10_SPI_SHADER_USER_DATA_VS_0 76u
#define OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 140u
#define OPENAGC_GFX10_SPI_SHADER_USER_DATA_PS_0 12u

/*
 * The NGG vertex stage's program registers. Two independent sources agree:
 * ps5-opengl's src/platform/ps5_agc_package.c packs an NGG vertex+geometry
 * pair at SH 0x0c8/0x0c9 (PGM LO/HI) and 0x08a/0x08b (PGM RSRC1/RSRC2_GS),
 * and a decoded public capture of Sony's own NGG triangle draw writes
 * exactly those four offsets. The ES block is the NGG vertex stage; the GS
 * RSRC pair describes its waves.
 */
#define OPENAGC_GFX10_SPI_SHADER_PGM_LO_ES 200u
#define OPENAGC_GFX10_SPI_SHADER_PGM_HI_ES 201u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC1_GS 138u
#define OPENAGC_GFX10_SPI_SHADER_PGM_RSRC2_GS 139u

/*
 * Register-table loads, the form Sony's AGC helpers emit on this console
 * (`sceAgcDcbSetUcRegistersIndirect` and its Cx/Sh siblings). PS5_Vulkan's
 * golden captures and their PM4 decoder fix the shape word for word:
 * opcode 0x64 (uconfig; 0x9F context, 0x63 SH), payload = table address
 * low, address high, 0x80000000, record count; each record is 8 bytes with
 * the register offset in the first u16 and the value in bytes 4..8.
 * `sid.h` names neither 0x64 nor 0x7A.
 */
#define OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD 0x64u
#define OPENAGC_PM4_OP_SET_UCONFIG_REG_INDEX 0x7Au
/*
 * ac_cmdbuf.h in the pinned Mesa tree documents the GFX10 CAM bug: "the ME
 * implementation of its content addressable memory (CAM) ... can skip
 * register writes due to not taking correctly into account the fields from
 * the GRBM_GFX_INDEX", and radv writes VGT_PRIMITIVE_TYPE with
 * radeon_set_uconfig_reg_idx(..., idx = 1, ...), which is exactly this
 * packet: the opcode changes to SET_UCONFIG_REG_INDEX and the index rides in
 * bits 28+ of the offset word. The plain opcode is the form the CAM skips.
 */
#define OPENAGC_PM4_UCONFIG_PRIMITIVE_TYPE_INDEX 1u
/*
 * GRBM_GFX_INDEX (uconfig index 0x200, the 0x2200 form with BASE_IDX 1) is
 * the register Mesa's ac_cmdbuf comment names in the CAM bug, and its golden
 * table for this family writes 0xe0000000 there (the three broadcast bits,
 * SE/INSTANCE/SA). The NGG draw writes it before the input topology so the
 * CAM cannot attribute the topology write to another instance.
 */
#define OPENAGC_GFX10_UCONFIG_GRBM_GFX_INDEX 512u
#define OPENAGC_GFX10_GRBM_GFX_INDEX_BROADCAST 0xE0000000u
#define OPENAGC_PM4_OP_CONTEXT_TABLE_LOAD 0x9Fu
#define OPENAGC_PM4_OP_SH_TABLE_LOAD 0x63u
#define OPENAGC_PM4_TABLE_LOAD_CONTROL 0x80000000u
#define OPENAGC_PM4_TABLE_RECORD_BYTES 8u
/*
 * A decoded capture of Sony's own NGG draw reads the record the other way
 * round and settles it: the first u32 holds the register offset and the
 * second the value, so the "offset in the first u16" reading above is the
 * same bytes with a zero pad. The whole Step-AD NGG program is written with
 * a context table load in that layout, which is how the capture writes
 * GE_CNTL — the register this console drops when it is written as a single
 * SET_CONTEXT_REG packet.
 */
/* The Step-AC table carries exactly one record: VGT_PRIMITIVE_TYPE. */
#define OPENAGC_PM4_DRAW_UCONFIG_TABLE_RECORDS 1u

/*
 * PA_SC_TILE_STEERING_OVERRIDE is *not* written by any UMD: the kernel's
 * clear-state preamble programs it from the ASIC configuration data
 * (gfx_v10_0.c: `SOC15_REG_OFFSET(GC, 0, mmPA_SC_TILE_STEERING_OVERRIDE)`
 * inside the PREAMBLE_BEGIN/END_CLEAR_STATE block). A user-mode IB can read
 * it back to see whether the context it inherited carries the console's
 * own defaults.
 */
#define OPENAGC_GFX10_PA_SC_TILE_STEERING_OVERRIDE 215u

/*
 * Baseline readback order for the Step-AC diagnostic: the context state a
 * fresh console context already holds, read *before* this IB writes
 * anything. layout: 22 registers, one absolute COPY_DATA word each.
 */
#define OPENAGC_GFX10_DRAW_BASELINE_COUNT 22u

static const uint32_t openagc_gfx10_draw_baseline_offsets[OPENAGC_GFX10_DRAW_BASELINE_COUNT] = {
    OPENAGC_GFX10_DB_Z_INFO,
    OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_TL,
    OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_BR,
    OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_TL,
    OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_BR,
    OPENAGC_GFX10_PA_SC_CLIPRECT_RULE,
    OPENAGC_GFX10_PA_SC_EDGERULE,
    OPENAGC_GFX10_PA_SU_HARDWARE_SCREEN_OFFSET,
    OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_TL,
    OPENAGC_GFX10_PA_SC_VPORT_SCISSOR_0_BR,
    OPENAGC_GFX10_PA_SC_TILE_STEERING_OVERRIDE,
    OPENAGC_GFX10_PA_CL_CLIP_CNTL,
    OPENAGC_GFX10_PA_SU_SC_MODE_CNTL,
    OPENAGC_GFX10_PA_SU_POINT_SIZE,
    OPENAGC_GFX10_PA_SU_POINT_MINMAX,
    OPENAGC_GFX10_PA_SC_MODE_CNTL_0,
    OPENAGC_GFX10_PA_SC_MODE_CNTL_1,
    OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE,
    OPENAGC_GFX10_IA_MULTI_VGT_PARAM,
    OPENAGC_GFX10_PA_SU_VTX_CNTL,
    OPENAGC_GFX10_CB_COLOR_CONTROL,
    OPENAGC_GFX10_CB_BLEND0_CONTROL
};

/*
 * Pre-draw probe: the registers whose writes a previous run could not see,
 * plus the two that gate the colour path. Read back after this IB writes
 * them, before the draw.
 */
#define OPENAGC_GFX10_DRAW_PROBE_COUNT 8u
/*
 * SET_UCONFIG_REG index variants a driver uses for a per-context uconfig
 * register, and the offsets tried here in order: the plain form (Mesa on
 * GFX10), index 1 (Mesa's radeon_opt_set_uconfig_reg_idx on GFX7-9),
 * index 2 (soc15d.h PACKET3_SET_UCONFIG_REG_INDEX_TYPE = 2 << 28) and
 * index 4 (amdgpu's radeon_opt_set_uconfig_reg_idx(..., 4, ...) on GFX9).
 * Each variant is written and read back, so the dump names the one that
 * lands; the last one that lands is what the draw runs with.
 */
/* The NGG vertex stage's user-data block: four words, one per declared
 * dword, written and read back before the draw. Distinctive values prove
 * the per-index mapping; a real NGG program puts the metadata's declared
 * dwords here (base vertex, the LDS layout, descriptor pointers). */
#define OPENAGC_PM4_DRAW_USER_DATA_COUNT 4u
#define OPENAGC_PM4_DRAW_USER_DATA_PROBE_BASE 0xA5A5A500u

#define OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT 1u

static const uint32_t openagc_pm4_uconfig_vgt_primitive_type_indices[OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT] = {
    0u
};
/* Mesa writes IA_MULTI_VGT_PARAM with index 1. */
#define OPENAGC_PM4_IAMVP_INDEX 1u

static const uint32_t openagc_gfx10_draw_probe_offsets[OPENAGC_GFX10_DRAW_PROBE_COUNT] = {
    OPENAGC_GFX10_CB_COLOR_CONTROL,
    OPENAGC_GFX10_CB_BLEND0_CONTROL,
    OPENAGC_GFX10_PA_SC_MODE_CNTL_0,
    OPENAGC_GFX10_IA_MULTI_VGT_PARAM,
    OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE,
    OPENAGC_GFX10_PA_SC_AA_CONFIG,
    OPENAGC_GFX10_DB_EQAA,
    OPENAGC_GFX10_PA_SU_POINT_SIZE
};

/* Field shifts, all from gc_10_1_0_sh_mask.h. */
#define OPENAGC_GFX10_DB_Z_INFO_FORMAT_SHIFT 0u
#define OPENAGC_GFX10_DB_Z_INFO_NUM_SAMPLES_SHIFT 2u
#define OPENAGC_GFX10_DB_Z_INFO_SW_MODE_SHIFT 4u
#define OPENAGC_GFX10_DB_STENCIL_INFO_FORMAT_SHIFT 0u
#define OPENAGC_GFX10_SCREEN_SCISSOR_TL_X_SHIFT 0u
#define OPENAGC_GFX10_SCREEN_SCISSOR_TL_Y_SHIFT 16u
#define OPENAGC_GFX10_SCREEN_SCISSOR_BR_X_SHIFT 0u
#define OPENAGC_GFX10_SCREEN_SCISSOR_BR_Y_SHIFT 16u
#define OPENAGC_GFX10_WINDOW_SCISSOR_TL_X_SHIFT 0u
#define OPENAGC_GFX10_WINDOW_SCISSOR_TL_Y_SHIFT 16u
#define OPENAGC_GFX10_WINDOW_SCISSOR_TL_OFFSET_DISABLE_SHIFT 31u
#define OPENAGC_GFX10_WINDOW_SCISSOR_BR_X_SHIFT 0u
#define OPENAGC_GFX10_WINDOW_SCISSOR_BR_Y_SHIFT 16u
#define OPENAGC_GFX10_EDGERULE_ER_TRI_SHIFT 0u
#define OPENAGC_GFX10_EDGERULE_ER_POINT_SHIFT 4u
#define OPENAGC_GFX10_EDGERULE_ER_RECT_SHIFT 8u
#define OPENAGC_GFX10_EDGERULE_ER_LINE_LR_SHIFT 12u
#define OPENAGC_GFX10_EDGERULE_ER_LINE_RL_SHIFT 18u
#define OPENAGC_GFX10_EDGERULE_ER_LINE_TB_SHIFT 24u
#define OPENAGC_GFX10_EDGERULE_ER_LINE_BT_SHIFT 28u
#define OPENAGC_GFX10_HW_SCREEN_OFFSET_X_SHIFT 0u
#define OPENAGC_GFX10_HW_SCREEN_OFFSET_Y_SHIFT 16u
#define OPENAGC_GFX10_INTERP_FLAT_SHADE_ENA_SHIFT 0u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_ENA_SHIFT 1u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_X_SHIFT 2u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_Y_SHIFT 5u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_Z_SHIFT 8u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_W_SHIFT 11u
#define OPENAGC_GFX10_INTERP_PNT_SPRITE_TOP_1_SHIFT 14u
#define OPENAGC_GFX10_CLIP_CNTL_DX_LINEAR_ATTR_CLIP_ENA_SHIFT 24u
#define OPENAGC_GFX10_SC_MODE_CNTL_PROVOKING_VTX_LAST_SHIFT 19u
#define OPENAGC_GFX10_POINT_SIZE_WIDTH_SHIFT 16u
#define OPENAGC_GFX10_POINT_MINMAX_MAX_SIZE_SHIFT 16u
#define OPENAGC_GFX10_MODE_CNTL_0_MSAA_ENABLE_SHIFT 0u
#define OPENAGC_GFX10_MODE_CNTL_0_VPORT_SCISSOR_ENABLE_SHIFT 1u
#define OPENAGC_GFX10_MODE_CNTL_0_ALTERNATE_RBS_PER_TILE_SHIFT 5u
#define OPENAGC_GFX10_VTX_CNTL_PIX_CENTER_SHIFT 0u
#define OPENAGC_GFX10_VTX_CNTL_ROUND_MODE_SHIFT 1u
#define OPENAGC_GFX10_VTX_CNTL_QUANT_MODE_SHIFT 3u

/* Enum values (registers/gfx10.json). */
#define OPENAGC_GFX10_Z_INVALID 0u
#define OPENAGC_GFX10_STENCIL_INVALID 0u
#define OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_0 0u
#define OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_1 1u
#define OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_S 2u
#define OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_T 3u
#define OPENAGC_GFX10_VTX_ROUND_TO_EVEN 2u
#define OPENAGC_GFX10_VTX_QUANT_1_256TH 5u
#define OPENAGC_GFX10_DI_PT_POINTLIST 1u
#define OPENAGC_GFX10_DI_PT_LINELIST 2u
/* amdgfxregs.h in the pinned compiler tree: V_008958_DI_PT_POINTLIST 1,
 * V_008958_DI_PT_LINELIST 2, V_008958_DI_PT_TRILIST 4. This is the
 * uconfig VGT_PRIMITIVE_TYPE value. */
#define OPENAGC_GFX10_DI_PT_TRILIST 4u
/* VGT_GS_OUT_PRIM_TYPE's own enum (V_028A6C_*): POINTLIST 0, LINESTRIP 1,
 * TRISTRIP 2. Sony's own NGG triangle draw carries 2 in its context table,
 * so a triangle list is rasterized as a strip. */
#define OPENAGC_GFX10_GS_OUT_POINTLIST 0u
#define OPENAGC_GFX10_GS_OUT_LINESTRIP 1u
#define OPENAGC_GFX10_GS_OUT_TRISTRIP 2u
/* GE_CNTL's uconfig index. Mesa's gc_10_1_0_offset.h gives mmGE_CNTL 0x225b
 * with BASE_IDX 1, and 0x2242 for VGT_PRIMITIVE_TYPE; the 0x2xxx form is the
 * uconfig space. It is the one NGG linkage register that is not a context
 * register, and Sony's context table does not carry it. */
#define OPENAGC_GFX10_UCONFIG_GE_CNTL 603u

/* One point of `pixels` pixels: POINT_SIZE and MINMAX both use 1/8-pixel
 * units, and the cited non-per-vertex branch forces min = max. */
static inline uint32_t openagc_gfx10_draw_point_size_px(uint32_t pixels)
{
    uint32_t units = pixels * 8u;

    return (units << OPENAGC_GFX10_POINT_SIZE_WIDTH_SHIFT) | units;
}

static inline uint32_t openagc_gfx10_draw_point_minmax_px(uint32_t pixels)
{
    uint32_t units = pixels * 8u;

    return (units << OPENAGC_GFX10_POINT_MINMAX_MAX_SIZE_SHIFT) | units;
}

/* The 8x8 point the Step-AC draw uses: one tile of the drawn rect. */
static inline uint32_t openagc_gfx10_draw_point_size_8px(void)
{
    return openagc_gfx10_draw_point_size_px(8u);
}

static inline uint32_t openagc_gfx10_draw_point_minmax_8px(void)
{
    return openagc_gfx10_draw_point_minmax_px(8u);
}

/*
 * CB_COLOR_CONTROL.MODE is the master colour-write switch: Mesa sets
 * MODE(CB_NORMAL) only when the blend target mask is non-empty and
 * MODE(CB_DISABLE) otherwise, and CB_DISABLE is 0 -- the reset value a
 * context that has never drawn carries. ROP3 is 0xcc ("copy source") when
 * logic ops are off; CB_BLEND0_CONTROL stays 0 for a disabled blend.
 */
#define OPENAGC_GFX10_CB_COLOR_CONTROL_MODE_SHIFT 4u
#define OPENAGC_GFX10_CB_COLOR_CONTROL_ROP3_SHIFT 16u
#define OPENAGC_GFX10_CB_MODE_DISABLE 0u
#define OPENAGC_GFX10_CB_MODE_NORMAL 1u
#define OPENAGC_GFX10_CB_ROP3_COPY_SOURCE 0xccu

#define OPENAGC_GFX10_CB_COLOR_CONTROL_DISABLE_DUAL_QUAD_SHIFT 0u
#define OPENAGC_GFX10_CB_COLOR_CONTROL_NORMAL_WORD 0x00cc0011u

/* MODE(CB_NORMAL) | ROP3(0xcc) | DISABLE_DUAL_QUAD(1): PS5_Vulkan's word
 * (ps5vk_draw.c, PS5VK_COLOR_CONTROL_WORD 0x00cc0011, "colour control with
 * RB+ off"); the AGC runtime's own default is the same word with RB+
 * enabled (0x00cc0010), which is what this driver wrote in AC-5. */
static inline uint32_t openagc_gfx10_draw_cb_color_control_normal(void)
{
    return (OPENAGC_GFX10_CB_MODE_NORMAL
            << OPENAGC_GFX10_CB_COLOR_CONTROL_MODE_SHIFT) |
           (OPENAGC_GFX10_CB_ROP3_COPY_SOURCE
            << OPENAGC_GFX10_CB_COLOR_CONTROL_ROP3_SHIFT) |
           (1u << OPENAGC_GFX10_CB_COLOR_CONTROL_DISABLE_DUAL_QUAD_SHIFT);
}

/*
 * MSAA-capable rasterizer block for a one-sample target, transcribed from
 * PS5_Vulkan's driver (ps5vk_draw.c, ps5vk_multisample_..._registers with
 * samples = 1): PA_SC_AA_CONFIG carries MAX_SAMPLE_DIST 6 in bits 13-16
 * with a zero sample count, DB_EQAA the quality bits 16/17/20, the four
 * sample-location registers share the 4x pattern, both centroid-priority
 * registers carry PS5VK's constant and both coverage masks are full. The
 * driver runs every draw with PA_SC_MODE_CNTL_0 = 0x23, i.e. MSAA_ENABLE
 * plus the per-viewport scissor plus the per-tile RB alternation, which is
 * why this block travels with it; the driver's default point state
 * (ps5_agc_native_runtime.c: POINT_SIZE and MINMAX 0x00080008) agrees with
 * the one-pixel composition this file already used.
 */
#define OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X 0x622AE6AEu
#define OPENAGC_GFX10_AA_CENTROID_PRIORITY 0x32103210u
#define OPENAGC_GFX10_AA_CONFIG_1X 0x0000C000u
#define OPENAGC_GFX10_DB_EQAA_1X 0x00310000u
#define OPENAGC_GFX10_AA_MASK_FULL 0xFFFFFFFFu

/* IA_MULTI_VGT_PARAM, the per-draw value Mesa always writes on GFX7+:
 * PRIMGROUP_SIZE(primgroup_size - 1) with primgroup_size = 128 "recommended
 * without a GS and tess", plus WD_SWITCH_ON_EOP, which
 * si_get_init_multi_vgt_param sets when the chip has at most 2 shader
 * engines (and which "has no effect on GPUs with less than 4 shader
 * engines"). */
#define OPENAGC_GFX10_IA_MULTI_VGT_PARAM_PRIMGROUP_SIZE_SHIFT 0u
#define OPENAGC_GFX10_IA_MULTI_VGT_PARAM_WD_SWITCH_ON_EOP_SHIFT 20u

static inline uint32_t openagc_gfx10_draw_ia_multi_vgt_param(void)
{
    return (127u << OPENAGC_GFX10_IA_MULTI_VGT_PARAM_PRIMGROUP_SIZE_SHIFT) |
           (1u << OPENAGC_GFX10_IA_MULTI_VGT_PARAM_WD_SWITCH_ON_EOP_SHIFT);
}

/* MSAA on, per-viewport scissor on, alternate RBs per tile: the word every
 * draw of PS5_Vulkan's driver runs with (ps5vk_draw.c: 0x23) with the
 * MSAA block above; Mesa's non-MSAA variant is the same word without
 * MSAA_ENABLE. */
static inline uint32_t openagc_gfx10_draw_mode_cntl_0(void)
{
    return (1u << OPENAGC_GFX10_MODE_CNTL_0_MSAA_ENABLE_SHIFT) |
           (1u << OPENAGC_GFX10_MODE_CNTL_0_VPORT_SCISSOR_ENABLE_SHIFT) |
           (1u << OPENAGC_GFX10_MODE_CNTL_0_ALTERNATE_RBS_PER_TILE_SHIFT);
}

/* No culling, solid fill (POLY_MODE off): the cited composition with GL
 * defaults, where only the provoking-vertex bit could differ and has no
 * effect on points. */
static inline uint32_t openagc_gfx10_draw_sc_mode_cntl(void)
{
    return (0u << OPENAGC_GFX10_SC_MODE_CNTL_PROVOKING_VTX_LAST_SHIFT);
}

/* GL clip space (z in [-1,1]), no user clip planes, no rasterizer kill,
 * linear attribute clipping on. */
static inline uint32_t openagc_gfx10_draw_clip_cntl(void)
{
    return (1u << OPENAGC_GFX10_CLIP_CNTL_DX_LINEAR_ATTR_CLIP_ENA_SHIFT);
}

/* "OpenGL FBOs and Direct3D should set this" branch of si_create_rs_state. */
static inline uint32_t openagc_gfx10_draw_edgerule(void)
{
    return (0xAu << OPENAGC_GFX10_EDGERULE_ER_TRI_SHIFT) |
           (0x6u << OPENAGC_GFX10_EDGERULE_ER_POINT_SHIFT) |
           (0xAu << OPENAGC_GFX10_EDGERULE_ER_RECT_SHIFT) |
           (0x19u << OPENAGC_GFX10_EDGERULE_ER_LINE_LR_SHIFT) |
           (0x25u << OPENAGC_GFX10_EDGERULE_ER_LINE_RL_SHIFT) |
           (0xAu << OPENAGC_GFX10_EDGERULE_ER_LINE_TB_SHIFT) |
           (0xAu << OPENAGC_GFX10_EDGERULE_ER_LINE_BT_SHIFT);
}

/* Flat shading on, point sprites off; the override selectors are the cited
 * defaults S/T/0/1 and the top-left sprite origin. */
static inline uint32_t openagc_gfx10_draw_interp_control_0(void)
{
    return (1u << OPENAGC_GFX10_INTERP_FLAT_SHADE_ENA_SHIFT) |
           (0u << OPENAGC_GFX10_INTERP_PNT_SPRITE_ENA_SHIFT) |
           (OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_S
            << OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_X_SHIFT) |
           (OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_T
            << OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_Y_SHIFT) |
           (OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_0
            << OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_Z_SHIFT) |
           (OPENAGC_GFX10_SPI_PNT_SPRITE_SEL_1
            << OPENAGC_GFX10_INTERP_PNT_SPRITE_OVRD_W_SHIFT);
}

/* Half-pixel center (GL), round to even, 1/256-pixel quantization. */
static inline uint32_t openagc_gfx10_draw_vtx_cntl(void)
{
    return (1u << OPENAGC_GFX10_VTX_CNTL_PIX_CENTER_SHIFT) |
           (OPENAGC_GFX10_VTX_ROUND_TO_EVEN
            << OPENAGC_GFX10_VTX_CNTL_ROUND_MODE_SHIFT) |
           (OPENAGC_GFX10_VTX_QUANT_1_256TH
            << OPENAGC_GFX10_VTX_CNTL_QUANT_MODE_SHIFT);
}

/* No Z/S is bound: FORMAT invalid disables the depth/stencil test. */
static inline uint32_t openagc_gfx10_draw_db_z_info_unbound(void)
{
    return (OPENAGC_GFX10_Z_INVALID << OPENAGC_GFX10_DB_Z_INFO_FORMAT_SHIFT) |
           (0u << OPENAGC_GFX10_DB_Z_INFO_NUM_SAMPLES_SHIFT) |
           (OPENAGC_GFX10_SW_MODE_LINEAR
            << OPENAGC_GFX10_DB_Z_INFO_SW_MODE_SHIFT);
}

static inline uint32_t openagc_gfx10_draw_db_stencil_info_unbound(void)
{
    return (OPENAGC_GFX10_STENCIL_INVALID
            << OPENAGC_GFX10_DB_STENCIL_INFO_FORMAT_SHIFT);
}

/* Screen scissor: (x, y) inclusive top-left, (x2, y2) bound in pixels. */
static inline uint32_t openagc_gfx10_screen_scissor_tl(uint32_t x, uint32_t y)
{
    return (x << OPENAGC_GFX10_SCREEN_SCISSOR_TL_X_SHIFT) |
           (y << OPENAGC_GFX10_SCREEN_SCISSOR_TL_Y_SHIFT);
}

static inline uint32_t openagc_gfx10_screen_scissor_br(uint32_t x2, uint32_t y2)
{
    return (x2 << OPENAGC_GFX10_SCREEN_SCISSOR_BR_X_SHIFT) |
           (y2 << OPENAGC_GFX10_SCREEN_SCISSOR_BR_Y_SHIFT);
}

/* Window scissor bound, as si_emit_framebuffer_state writes it from the
 * framebuffer dimensions; the top-left keeps the clear-state origin but
 * disables the window offset so screen coordinates are framebuffer
 * coordinates. */
static inline uint32_t openagc_gfx10_window_scissor_tl(void)
{
    return (1u << OPENAGC_GFX10_WINDOW_SCISSOR_TL_OFFSET_DISABLE_SHIFT);
}

static inline uint32_t openagc_gfx10_window_scissor_br(uint32_t width, uint32_t height)
{
    return (width << OPENAGC_GFX10_WINDOW_SCISSOR_BR_X_SHIFT) |
           (height << OPENAGC_GFX10_WINDOW_SCISSOR_BR_Y_SHIFT);
}

/* Per-viewport scissor, the form si_emit_one_scissor emits for GFX6-GFX11
 * (TL carries WINDOW_OFFSET_DISABLE, BR is the exclusive bound). */
static inline uint32_t openagc_gfx10_vport_scissor_tl(uint32_t x, uint32_t y)
{
    return openagc_gfx10_screen_scissor_tl(x, y) |
           (1u << OPENAGC_GFX10_WINDOW_SCISSOR_TL_OFFSET_DISABLE_SHIFT);
}

static inline uint32_t openagc_gfx10_vport_scissor_br(uint32_t x2, uint32_t y2)
{
    return (x2 << OPENAGC_GFX10_SCREEN_SCISSOR_BR_X_SHIFT) |
           (y2 << OPENAGC_GFX10_SCREEN_SCISSOR_BR_Y_SHIFT);
}

/* Registers that gate or discard fragments. A context that inherits them
 * may have triangle filtering, a depth test against no depth buffer, or a
 * colour-write override left on, and every one of those drops fragments
 * without a fault. Each value below is the field layout's neutral form:
 * every viewport transform enable on, and no disable or override set. */
#define OPENAGC_GFX10_SPI_PS_INPUT_CNTL_0 401u
#define OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT 32u
#define OPENAGC_GFX10_PA_SC_BINNER_CNTL_0 785u
#define OPENAGC_GFX10_DB_DEPTH_CONTROL 512u
#define OPENAGC_GFX10_PA_CL_VTE_CNTL 518u
#define OPENAGC_GFX10_PA_CL_NANINF_CNTL 520u
#define OPENAGC_GFX10_PA_SU_PRIM_FILTER_CNTL 523u
#define OPENAGC_GFX10_PA_SU_SMALL_PRIM_FILTER_CNTL 524u
#define OPENAGC_GFX10_PA_CL_NGG_CNTL 526u
#define OPENAGC_GFX10_PA_SU_OVER_RASTERIZATION_CNTL 527u
#define OPENAGC_GFX10_FRAGMENT_GATE_COUNT 11u
/* CB_DISABLE is useful for a diagnostic draw with color writes suppressed. */
#define OPENAGC_GFX10_CB_COLOR_CONTROL_DISABLE 0u
/* MSAA off: a target with no CMASK cannot take the multisample path. */
#define OPENAGC_GFX10_MODE_CNTL_0_SINGLE_SAMPLE 0x2u
/* VPORT_X/Y/Z_SCALE_ENA and VPORT_X/Y/Z_OFFSET_ENA. */
#define OPENAGC_GFX10_VTE_CNTL_VIEWPORT_TRANSFORM 0x3fu

/* One record per pixel-shader input: the capture carries the identity
 * mapping, and an unset table leaves the parameter cache reading a
 * reserved input. */
static inline void openagc_gfx10_ps_input_cntl_defaults(uint32_t *values)
{
    uint32_t index;

    for (index = 0u; index < OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT; ++index) {
        values[index] = index;
    }
}

static const uint32_t
    openagc_gfx10_fragment_gate_offsets[OPENAGC_GFX10_FRAGMENT_GATE_COUNT] = {
        OPENAGC_GFX10_PA_CL_VTE_CNTL,
        OPENAGC_GFX10_DB_DEPTH_CONTROL,
        OPENAGC_GFX10_PA_SU_PRIM_FILTER_CNTL,
        OPENAGC_GFX10_PA_SU_SMALL_PRIM_FILTER_CNTL,
        OPENAGC_GFX10_PA_CL_NANINF_CNTL,
        OPENAGC_GFX10_PA_SU_OVER_RASTERIZATION_CNTL,
        OPENAGC_GFX10_PA_CL_NGG_CNTL,
        OPENAGC_GFX10_PA_SC_MODE_CNTL_0,
        OPENAGC_GFX10_PA_SC_AA_CONFIG,
        OPENAGC_GFX10_DB_EQAA,
        OPENAGC_GFX10_CB_COLOR_CONTROL
    };

static const uint32_t
    openagc_gfx10_fragment_gate_values[OPENAGC_GFX10_FRAGMENT_GATE_COUNT] = {
        OPENAGC_GFX10_VTE_CNTL_VIEWPORT_TRANSFORM,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u,
        OPENAGC_GFX10_MODE_CNTL_0_SINGLE_SAMPLE,
        0u,
        0u,
        /* Keep the target writable. The scalar draw state already selects
         * CB_NORMAL; writing CB_DISABLE here undid it just before DRAW. */
        OPENAGC_GFX10_CB_COLOR_CONTROL_NORMAL_WORD
    };

/* The generic scissor and the depth range the rasterizer also reads.
 * Mesa programs the generic scissor from the user scissor; the public C1
 * capture carries TL 0x80000000 (WINDOW_OFFSET_DISABLE, origin 0,0) and
 * BR (4096,4096) for its full-screen target, plus a 0..1 depth range. */
#define OPENAGC_GFX10_PA_SC_GENERIC_SCISSOR_TL 144u
#define OPENAGC_GFX10_PA_SC_GENERIC_SCISSOR_BR 145u
#define OPENAGC_GFX10_PA_SC_VPORT_ZMIN_0 180u
#define OPENAGC_GFX10_PA_SC_VPORT_ZMAX_0 181u

static inline uint32_t openagc_gfx10_generic_scissor_tl(uint32_t x, uint32_t y)
{
    return openagc_gfx10_screen_scissor_tl(x, y) |
           (1u << OPENAGC_GFX10_WINDOW_SCISSOR_TL_OFFSET_DISABLE_SHIFT);
}

static inline uint32_t openagc_gfx10_generic_scissor_br(uint32_t x2, uint32_t y2)
{
    return openagc_gfx10_screen_scissor_br(x2, y2);
}

/* "If CLIPRECT_RULE & (1 << number), the pixel is rasterized"; with no
 * window rectangles Mesa programs 0xffff = every inside/outside case. */
#define OPENAGC_GFX10_CLIPRECT_RULE_DISABLED 0xffffu

/* One viewport, in the XSCALE, XOFFSET, YSCALE, YOFFSET, ZSCALE,
 * ZOFFSET order si_emit_one_viewport uses. GL y-flip and the default
 * [0,1] depth range are the caller's choice; this fills what a GL-like
 * frontend passes. */
static inline uint32_t openagc_gfx10_float_bits(float value)
{
    union {
        float f;
        uint32_t u;
    } bits;

    bits.f = value;
    return bits.u;
}

static inline void openagc_gfx10_viewport_gl(uint32_t x, uint32_t y,
                                             uint32_t width, uint32_t height,
                                             uint32_t out[6])
{
    out[0] = openagc_gfx10_float_bits((float)width / 2.0f);
    out[1] = openagc_gfx10_float_bits((float)x + (float)width / 2.0f);
    out[2] = openagc_gfx10_float_bits(-(float)height / 2.0f);
    out[3] = openagc_gfx10_float_bits((float)y + (float)height / 2.0f);
    out[4] = openagc_gfx10_float_bits(0.5f);
    out[5] = openagc_gfx10_float_bits(0.5f);
}

/*
 * ac_compute_guardband for one viewport that is not shifted by a hardware
 * screen offset: clip = the guardband distance in clip space given the
 * viewport scale/translate, discard = 1.0 (+ half the clip/discard
 * distance, which is zero here) so only primitives entirely outside the
 * viewport are dropped. Register order is VERT_CLIP, VERT_DISC,
 * HORZ_CLIP, HORZ_DISC.
 */
static inline void openagc_gfx10_guardband_gl(uint32_t x, uint32_t y,
                                              uint32_t width, uint32_t height,
                                              uint32_t out[4])
{
    float scale = (float)width / 2.0f;
    float translate = (float)x + (float)width / 2.0f;
    float max_range = 32768.0f;
    float left = (-max_range - 1.0f - translate) / scale;
    float right = (max_range - translate) / scale;
    float guardband = (-left < right) ? -left : right;

    (void)y;
    (void)height;
    out[0] = openagc_gfx10_float_bits(guardband);
    out[1] = openagc_gfx10_float_bits(1.0f);
    out[2] = openagc_gfx10_float_bits(guardband);
    out[3] = openagc_gfx10_float_bits(1.0f);
}

/*
 * Step-AB full linear color-bind register set (gc_10_1_0 order).
 *
 * Register *set* from Mesa si_state.c (si_set_framebuffer_state GFX10
 * branch: BASE + the 0x028C60 block + BASE_EXT + ATTRIB2 + ATTRIB3), in
 * that order here: BASE, BASE_EXT, VIEW, INFO, ATTRIB, ATTRIB2, ATTRIB3,
 * TARGET_MASK, SHADER_MASK. All nine are context registers (BASE_IDX 1),
 * so the Step-X absolute aperture CONTEXT_REG_START + offset reads them.
 */
#define OPENAGC_GFX10_CB_BIND_COUNT 9u
#define OPENAGC_GFX10_CB_BIND_IDX_BASE 0u
#define OPENAGC_GFX10_CB_BIND_IDX_BASE_EXT 1u
#define OPENAGC_GFX10_CB_BIND_IDX_VIEW 2u
#define OPENAGC_GFX10_CB_BIND_IDX_INFO 3u
#define OPENAGC_GFX10_CB_BIND_IDX_ATTRIB 4u
#define OPENAGC_GFX10_CB_BIND_IDX_ATTRIB2 5u
#define OPENAGC_GFX10_CB_BIND_IDX_ATTRIB3 6u
#define OPENAGC_GFX10_CB_BIND_IDX_TARGET_MASK 7u
#define OPENAGC_GFX10_CB_BIND_IDX_SHADER_MASK 8u

static const uint32_t openagc_gfx10_cb_bind_offsets[OPENAGC_GFX10_CB_BIND_COUNT] = {
    OPENAGC_GFX10_CB_COLOR0_BASE,     OPENAGC_GFX10_CB_COLOR0_BASE_EXT,
    OPENAGC_GFX10_CB_COLOR0_VIEW,     OPENAGC_GFX10_CB_COLOR0_INFO,
    OPENAGC_GFX10_CB_COLOR0_ATTRIB,   OPENAGC_GFX10_CB_COLOR0_ATTRIB2,
    OPENAGC_GFX10_CB_COLOR0_ATTRIB3,  OPENAGC_GFX10_CB_TARGET_MASK,
    OPENAGC_GFX10_CB_SHADER_MASK
};

/*
 * The 16 COLOR0 records a public AGC target state carries: the same list
 * the native runtime's append_target_state writes and PS5_Vulkan's C1
 * capture records (commit 3a6f00df). Names resolved against Mesa
 * gfx10.json by mm address = 0x28000 + 4 * index:
 *   0x318 BASE, 0x31b VIEW, 0x31c INFO, 0x31d ATTRIB, 0x31e DCC_CONTROL,
 *   0x31f CMASK, 0x321 FMASK, 0x323/0x324 CLEAR_WORD0/1, 0x325 DCC_BASE,
 *   0x390 BASE_EXT, 0x398 CMASK_BASE_EXT, 0x3a0 FMASK_BASE_EXT,
 *   0x3a8 DCC_BASE_EXT, 0x3b0 ATTRIB2, 0x3b8 ATTRIB3.
 * (0x31d is CB_COLOR0_ATTRIB, so 0x31e is DCC_CONTROL, not an ATTRIB
 * alias: Step AB's nine-word set never writes DCC_CONTROL at all.)
 *
 * The capture's own words, for its 3840x2160 BGRA8 display surface:
 *   INFO         0x00008828  FORMAT 8_8_8_8, COMP_SWAP SWAP_ALT, BLEND_CLAMP
 *   DCC_CONTROL  0x00000048  MAX_UNCOMPRESSED_BLOCK_SIZE(2) |
 *                            MAX_COMPRESSED_BLOCK_SIZE(2); DCC itself off
 *   ATTRIB3      0x4dc6c000  RESOURCE_TYPE 2D, COLOR_SW_MODE 27,
 *                            FMASK_SW_MODE 24, CMASK and DCC pipe aligned
 *   metadata bases, CMASK/FMASK/DCC and clear words: zero
 * A linear COLOR_SW_MODE is a host composition; no public driver binds
 * one, which is why the two-target A/B exists.
 */
#define OPENAGC_GFX10_CB_COLOR0_DCC_CONTROL 798u
#define OPENAGC_GFX10_CB_COLOR0_CMASK 799u
#define OPENAGC_GFX10_CB_COLOR0_FMASK 801u
#define OPENAGC_GFX10_CB_COLOR0_CLEAR_WORD0 803u
#define OPENAGC_GFX10_CB_COLOR0_CLEAR_WORD1 804u
#define OPENAGC_GFX10_CB_COLOR0_DCC_BASE 805u
#define OPENAGC_GFX10_CB_COLOR0_CMASK_BASE_EXT 902u
#define OPENAGC_GFX10_CB_COLOR0_FMASK_BASE_EXT 904u
#define OPENAGC_GFX10_CB_COLOR0_DCC_BASE_EXT 906u
#define OPENAGC_GFX10_CB_CAPTURE_COUNT 16u
#define OPENAGC_GFX10_CB_CAPTURE_INFO 0x00008828u
#define OPENAGC_GFX10_CB_CAPTURE_DCC_CONTROL 0x00000048u
#define OPENAGC_GFX10_CB_CAPTURE_ATTRIB3 0x4dc6c000u

static const uint32_t
    openagc_gfx10_cb_capture_offsets[OPENAGC_GFX10_CB_CAPTURE_COUNT] = {
        OPENAGC_GFX10_CB_COLOR0_BASE,
        OPENAGC_GFX10_CB_COLOR0_VIEW,
        OPENAGC_GFX10_CB_COLOR0_INFO,
        OPENAGC_GFX10_CB_COLOR0_ATTRIB,
        OPENAGC_GFX10_CB_COLOR0_DCC_CONTROL,
        OPENAGC_GFX10_CB_COLOR0_CMASK,
        OPENAGC_GFX10_CB_COLOR0_FMASK,
        OPENAGC_GFX10_CB_COLOR0_CLEAR_WORD0,
        OPENAGC_GFX10_CB_COLOR0_CLEAR_WORD1,
        OPENAGC_GFX10_CB_COLOR0_DCC_BASE,
        OPENAGC_GFX10_CB_COLOR0_BASE_EXT,
        OPENAGC_GFX10_CB_COLOR0_CMASK_BASE_EXT,
        OPENAGC_GFX10_CB_COLOR0_FMASK_BASE_EXT,
        OPENAGC_GFX10_CB_COLOR0_DCC_BASE_EXT,
        OPENAGC_GFX10_CB_COLOR0_ATTRIB2,
        OPENAGC_GFX10_CB_COLOR0_ATTRIB3
    };

/*
 * The capture's record set with the caller's base and geometry: BASE /
 * BASE_EXT from the 256-byte VA encoding, ATTRIB2 from width and height,
 * INFO and ATTRIB3 exactly as the capture carries them. values must hold
 * OPENAGC_GFX10_CB_CAPTURE_COUNT words. Returns 1, or 0 when the target
 * is not 256-byte aligned or its dimensions are zero.
 */
static inline uint32_t openagc_gfx10_cb_capture_words(uint64_t va, uint32_t width,
                                                      uint32_t height,
                                                      uint32_t *values)
{
    if (values == NULL || width == 0u || height == 0u ||
        (va & 0xffull) != 0ull) {
        return 0u;
    }
    values[0] = (uint32_t)(va >> 8);
    values[1] = 0u;
    values[2] = OPENAGC_GFX10_CB_CAPTURE_INFO;
    values[3] = 0u;
    values[4] = OPENAGC_GFX10_CB_CAPTURE_DCC_CONTROL;
    values[5] = 0u;
    values[6] = 0u;
    values[7] = 0u;
    values[8] = 0u;
    values[9] = 0u;
    values[10] = (uint32_t)(va >> 40);
    values[11] = 0u;
    values[12] = 0u;
    values[13] = 0u;
    values[14] = (height - 1u) | ((width - 1u) << 14);
    values[15] = OPENAGC_GFX10_CB_CAPTURE_ATTRIB3;
    return 1u;
}

/*
 * Cited field encodings for one GFX10 linear 8_8_8_8 color target.
 *
 * Field shifts/masks: public drm/amdgpu gc_10_1_0_sh_mask.h.
 * Word composition:   Mesa src/amd/common/ac_descriptors.c at the pinned
 *   revision (ac_init_cb_surface -> ac_init_gfx10_cb_surface and
 *   ac_set_mutable_cb_surface_fields), the code RADV and radeonsi share
 *   for GFX10+, plus src/amd/common/si_state.c for the register set and
 *   CB_TARGET_MASK; src/amd/common/ac_formats.c
 *   (ac_translate_colorswap) for COMP_SWAP.
 * Enum values:        Mesa src/amd/registers/gfx10.json (ColorFormat,
 *   SurfaceEndian, SurfaceNumber, SurfaceSwap) and
 *   src/amd/addrlib/inc/addrtypes.h (AddrSwizzleMode, AddrResourceType,
 *   mirrored by src/amd/common/ac_surface.h gfx9_resource_type).
 *
 * This composing code produces *words*, not a claimed draw-ready bind:
 * no DRAW is submitted, so neither addressing sufficiency nor the swizzle
 * mode's semantics is asserted. hardware_qualified stays false.
 */
#define OPENAGC_GFX10_CB_INFO_NUMBER_TYPE_SHIFT 8u
#define OPENAGC_GFX10_CB_INFO_FORMAT_SHIFT 2u
#define OPENAGC_GFX10_CB_INFO_COMP_SWAP_SHIFT 11u
#define OPENAGC_GFX10_CB_INFO_BLEND_CLAMP_SHIFT 15u
#define OPENAGC_GFX10_CB_INFO_SIMPLE_FLOAT_SHIFT 17u
#define OPENAGC_GFX10_CB_ATTRIB_NUM_SAMPLES_SHIFT 12u
#define OPENAGC_GFX10_CB_ATTRIB_NUM_FRAGMENTS_SHIFT 15u
#define OPENAGC_GFX10_CB_ATTRIB2_MIP0_WIDTH_SHIFT 14u
#define OPENAGC_GFX10_CB_ATTRIB3_COLOR_SW_MODE_SHIFT 14u
#define OPENAGC_GFX10_CB_ATTRIB3_RESOURCE_TYPE_SHIFT 24u

/* gfx10.json ColorFormat / SurfaceEndian / SurfaceNumber / SurfaceSwap. */
#define OPENAGC_GFX10_COLOR_8_8_8_8 10u
#define OPENAGC_GFX10_ENDIAN_NONE 0u
#define OPENAGC_GFX10_NUMBER_UNORM 0u
#define OPENAGC_GFX10_SWAP_STD 0u
#define OPENAGC_GFX10_SWAP_ALT 1u
/* COMP_SWAP for a memory-order RGBA8 (XYZW) and BGRA8 (ZYXW) target. */
#define OPENAGC_GFX10_CB_COMP_SWAP_RGBA8 OPENAGC_GFX10_SWAP_STD
#define OPENAGC_GFX10_CB_COMP_SWAP_BGRA8 OPENAGC_GFX10_SWAP_ALT

/*
 * INFO for an uncompressed, non-mipmapped 8_8_8_8 UNORM target:
 * ENDIAN(NONE) | FORMAT_GFX6(COLOR_8_8_8_8) | COMPRESSION(0) |
 * COMP_SWAP(comp_swap) | BLEND_CLAMP(1, all NORM types) |
 * BLEND_BYPASS(0) | SIMPLE_FLOAT(1) | ROUND_MODE(0, UNORM) |
 * NUMBER_TYPE(UNORM). COMP is 0 because no FMASK is attached.
 */
static inline uint32_t openagc_gfx10_cb_color0_info_8888(uint32_t comp_swap)
{
    return (OPENAGC_GFX10_ENDIAN_NONE) |
           (OPENAGC_GFX10_COLOR_8_8_8_8 << OPENAGC_GFX10_CB_INFO_FORMAT_SHIFT) |
           (comp_swap << OPENAGC_GFX10_CB_INFO_COMP_SWAP_SHIFT) |
           (1u << OPENAGC_GFX10_CB_INFO_BLEND_CLAMP_SHIFT) |
           (1u << OPENAGC_GFX10_CB_INFO_SIMPLE_FLOAT_SHIFT) |
           (OPENAGC_GFX10_NUMBER_UNORM << OPENAGC_GFX10_CB_INFO_NUMBER_TYPE_SHIFT);
}

/*
 * ATTRIB: NUM_SAMPLES(log2 samples) | NUM_FRAGMENTS_GFX6(log2 storage
 * samples) | FORCE_DST_ALPHA_1(swizzle[3] == 1). Single-sample RGBA8
 * with a real alpha channel is all zeros. GFX10 color bindings do not
 * program TILE_MODE_INDEX/COLOR_SW_MODE here (see ATTRIB3).
 */
static inline uint32_t openagc_gfx10_cb_color0_attrib_single_sample(void)
{
    return (0u << OPENAGC_GFX10_CB_ATTRIB_NUM_SAMPLES_SHIFT) |
           (0u << OPENAGC_GFX10_CB_ATTRIB_NUM_FRAGMENTS_SHIFT);
}

/* VIEW: SLICE_START(first_layer) | SLICE_MAX(last_layer) |
 * MIP_LEVEL(base_level); one slice at level 0 is zero. */
static inline uint32_t openagc_gfx10_cb_color0_view_2d(void)
{
    return 0u;
}

/* ATTRIB2: MIP0_WIDTH(width - 1) | MIP0_HEIGHT(height - 1) |
 * MAX_MIP(levels - 1); width/height are the surface view dimensions in
 * pixels (GFX10.3+ may instead carry a 256B-multiple pitch here). */
static inline uint32_t openagc_gfx10_cb_color0_attrib2(uint32_t width,
                                                      uint32_t height)
{
    return ((width - 1u) << OPENAGC_GFX10_CB_ATTRIB2_MIP0_WIDTH_SHIFT) |
           (height - 1u);
}

/* ATTRIB3: MIP0_DEPTH(num_layers) | META_LINEAR(0) |
 * COLOR_SW_MODE(ADDR_SW_LINEAR) | RESOURCE_TYPE(2D) | RESOURCE_LEVEL(0).
 * RESOURCE_LEVEL is a shader-compiler descriptor flag, not a property of
 * this bind; it stays at its reset value. */
static inline uint32_t openagc_gfx10_cb_color0_attrib3_linear_2d(void)
{
    return (OPENAGC_GFX10_RESOURCE_TYPE_2D
            << OPENAGC_GFX10_CB_ATTRIB3_RESOURCE_TYPE_SHIFT) |
           (OPENAGC_GFX10_SW_MODE_LINEAR
            << OPENAGC_GFX10_CB_ATTRIB3_COLOR_SW_MODE_SHIFT);
}

/* si_state.c: cb_target_mask |= colormask << (4 * target); target 0 with
 * an RGBA write mask is 0xf. CB_SHADER_MASK uses the same layout and
 * reuses the smoke-owned value. */
static inline uint32_t openagc_gfx10_cb_target_mask_rgba0(void)
{
    return 0x0000000Fu;
}

/*
 * Compose the nine Step-AB bind words for a linear, uncompressed,
 * single-sample, single-mip, single-layer 8_8_8_8 target from an owned
 * 256-byte-aligned color VA. out must hold OPENAGC_GFX10_CB_BIND_COUNT
 * dwords in openagc_gfx10_cb_bind_offsets order.
 */
static inline void openagc_gfx10_cb_bind_linear_8888_words(
    uint64_t color_va, uint32_t width, uint32_t height, uint32_t comp_swap,
    uint32_t *out)
{
    out[OPENAGC_GFX10_CB_BIND_IDX_BASE] = (uint32_t)(color_va >> 8);
    out[OPENAGC_GFX10_CB_BIND_IDX_BASE_EXT] =
        (uint32_t)((color_va >> 8) >> 32);
    out[OPENAGC_GFX10_CB_BIND_IDX_VIEW] = openagc_gfx10_cb_color0_view_2d();
    out[OPENAGC_GFX10_CB_BIND_IDX_INFO] =
        openagc_gfx10_cb_color0_info_8888(comp_swap);
    out[OPENAGC_GFX10_CB_BIND_IDX_ATTRIB] =
        openagc_gfx10_cb_color0_attrib_single_sample();
    out[OPENAGC_GFX10_CB_BIND_IDX_ATTRIB2] =
        openagc_gfx10_cb_color0_attrib2(width, height);
    out[OPENAGC_GFX10_CB_BIND_IDX_ATTRIB3] =
        openagc_gfx10_cb_color0_attrib3_linear_2d();
    out[OPENAGC_GFX10_CB_BIND_IDX_TARGET_MASK] =
        openagc_gfx10_cb_target_mask_rgba0();
    out[OPENAGC_GFX10_CB_BIND_IDX_SHADER_MASK] =
        OPENAGC_GFX10_CB_SHADER_MASK_OWNED;
}

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
 *
 * Scope correction (Step AB): this read was designed against the GFX6-8
 * belief that a color target needs CB_COLOR0_ATTRIB.TILE_MODE_INDEX. The
 * cited gfx9+ driver path does not: ac_descriptors.c programs
 * CB_COLOR0_ATTRIB3.COLOR_SW_MODE(surf->u.gfx9.swizzle_mode) for color and
 * DB_Z_INFO.SW_MODE for depth, both fixed enums. The Step-AA negative
 * therefore does not block a GFX10 color/depth bind; the table stays
 * unowned and unneeded for the linear path.
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
    /* CB bind gap (offsets public; Step-AB words are cited composition) */
    { OPENAGC_GFX10_CB_TARGET_MASK, "CB_TARGET_MASK", 1u, 0u, "not in smoke" },
    { OPENAGC_GFX10_CB_COLOR0_BASE, "CB_COLOR0_BASE", 1u, 0u,
      "Stage 5; Mesa va>>8; Step Z owned from GPU VA" },
    { OPENAGC_GFX10_CB_COLOR0_PITCH, "CB_COLOR0_PITCH", 1u, 0u,
      "GFX10 hole; a driver emits 0" },
    { OPENAGC_GFX10_CB_COLOR0_SLICE, "CB_COLOR0_SLICE", 1u, 0u,
      "GFX10 hole; a driver emits 0" },
    { OPENAGC_GFX10_CB_COLOR0_BASE_EXT, "CB_COLOR0_BASE_EXT", 1u, 0u,
      "0x0390; (va>>8)>>32; Step AB reads it" },
    { OPENAGC_GFX10_CB_COLOR0_ATTRIB2, "CB_COLOR0_ATTRIB2", 1u, 0u,
      "0x03B0; MIP0_WIDTH/HEIGHT/MAX_MIP; Step AB builds it" },
    { OPENAGC_GFX10_CB_COLOR0_ATTRIB3, "CB_COLOR0_ATTRIB3", 1u, 0u,
      "0x03B8; RESOURCE_TYPE + COLOR_SW_MODE; no TILE_MODE_INDEX on GFX10" },
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
