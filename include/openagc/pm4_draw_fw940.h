/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_DRAW_FW940_H
#define OPENAGC_PM4_DRAW_FW940_H

#include "openagc/pm4_context_regs_gfx10.h"
#include "openagc/pm4_copy_data_fw940.h"
#include "openagc/pm4_fw940.h"
#include "openagc/pm4_graphics_fw940.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Step-AC draw vehicle: the first OpenAGC IB that submits a DRAW.
 *
 * Public cites:
 *   Mesa sid.h: PKT3_DRAW_INDEX_AUTO=0x2D, PKT3_SET_UCONFIG_REG=0x79,
 *     CIK_UCONFIG_REG_OFFSET=0x30000 (with SI_CONTEXT_REG_OFFSET=0x28000 /
 *     SI_SH_REG_OFFSET=0xB000, which is why a register's packet offset is
 *     its mm low 12 bits)
 *   Mesa si_state_draw.cpp: `PKT3(PKT3_DRAW_INDEX_AUTO, 1)` + vertex count
 *     + `V_0287F0_DI_SRC_SEL_AUTO_INDEX | use_opaque` (use_opaque = 0), and
 *     `radeon_set_uconfig_reg(R_030908_VGT_PRIMITIVE_TYPE, vgt_prim)` for
 *     GFX10; `si_conv_pipe_prim` maps GL points to `V_008958_DI_PT_POINTLIST`
 *   Mesa si_state.c/si_state_viewport.c: the rasterizer state, the viewport
 *     and guardband *sequences* (`reg_seq(..., 6)`,
 *     `radeon_opt_set_context_reg5` for PA_SU_VTX_CNTL + the four GB
 *     registers), the per-viewport scissor pair, the framebuffer window
 *     scissor bound, and the cliprect rule for "no window rectangles"
 *   amdgpu gfx_v10_0.c: the kernel's PREAMBLE_BEGIN/END_CLEAR_STATE block
 *     is what loads the ASIC's clear-state data (including
 *     PA_SC_TILE_STEERING_OVERRIDE) into a context, so a user-mode IB must
 *     *not* emit a bare CLEAR_STATE of its own: it would reset the state the
 *     console already loaded and cannot restore the firmware extents.
 *
 * The IB is: a read-only baseline dump of the inherited context state, the
 * cited draw state, the smoke.vert+frag register program with the two PGMs
 * patched to the uploaded code VAs, the Step-AB nine-register color bind,
 * the POINTLIST topology, one DRAW_INDEX_AUTO of one vertex, and the shared
 * EOP+NOP trailer. It draws one point: no vertex buffer, no depth buffer, no
 * other primitive, and every colour write is bounded by the scissors and the
 * single viewport.
 *
 * gpu_submitted / hardware_qualified stay 0 in the host library; only a
 * bounded console payload submits this IB, once, with no retries.
 */

#define OPENAGC_PM4_OP_CLEAR_STATE 0x12u
#define OPENAGC_PM4_OP_NUM_INSTANCES 0x2Fu
#define OPENAGC_PM4_UCONFIG_REG_OFFSET 0x00030000u
/*
 * GFX10's ME can skip a register write because its CAM does not take the
 * GRBM_GFX_INDEX fields into account; the workaround is the packet header's
 * reset-filter-CAM bit (PKT3_RESET_FILTER_CAM_S, bit 2), which the kernel
 * sets for GFX-IP uconfig writes on GFX10+
 * (ac_cmdbuf_set_ucfg_perfctr_reg_seq). The console readback of
 * VGT_PRIMITIVE_TYPE through the uconfig aperture (0xC000+578) remained
 * zero with and without this bit; indexed-register readback is unresolved.
 */
#define OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX 2u
/* One instance: Mesa emits PKT3_NUM_INSTANCES before a draw whenever the
 * count changes, and a context that never had one has no instance count at
 * all, so the VGT would assemble nothing. */
#define OPENAGC_PM4_DRAW_INSTANCE_COUNT 1u

/* Smoke register program (fixture metadata under
 * tests/fixtures/psbc_smoke, the same pairs Steps S-U proved). The four PGM
 * dwords are patched from the uploaded code VAs by the encoder. */
#define OPENAGC_PM4_SMOKE_VERT_CTX_COUNT 3u
#define OPENAGC_PM4_SMOKE_VERT_SH_COUNT 4u
#define OPENAGC_PM4_SMOKE_LINK_COUNT 3u
#define OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT 9u
#define OPENAGC_PM4_SMOKE_FRAG_SH_COUNT 4u

static const uint32_t openagc_pm4_smoke_vert_ctx_offsets[OPENAGC_PM4_SMOKE_VERT_CTX_COUNT] = {
    433u, 451u, 519u
};
static const uint32_t openagc_pm4_smoke_vert_ctx_values[OPENAGC_PM4_SMOKE_VERT_CTX_COUNT] = {
    128u, 4u, 0u
};
static const uint32_t openagc_pm4_smoke_vert_sh_offsets[OPENAGC_PM4_SMOKE_VERT_SH_COUNT] = {
    72u, 73u, 74u, 75u
};
static const uint32_t openagc_pm4_smoke_vert_sh_values[OPENAGC_PM4_SMOKE_VERT_SH_COUNT] = {
    0u, 0u, 137101313u, 8u
};
static const uint32_t openagc_pm4_smoke_link_offsets[OPENAGC_PM4_SMOKE_LINK_COUNT] = {
    603u, 725u, 610u
};
static const uint32_t openagc_pm4_smoke_link_values[OPENAGC_PM4_SMOKE_LINK_COUNT] = {
    131200u, 65536u, 0u
};
static const uint32_t openagc_pm4_smoke_frag_ctx_offsets[OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT] = {
    452u, 453u, 435u, 436u, 438u, 440u, 515u, 143u, 784u
};
static const uint32_t openagc_pm4_smoke_frag_ctx_values[OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT] = {
    0u, 9u, 128u, 128u, 32768u, 0u, 16u, 15u, 0u
};
static const uint32_t openagc_pm4_smoke_frag_sh_offsets[OPENAGC_PM4_SMOKE_FRAG_SH_COUNT] = {
    8u, 9u, 10u, 11u
};
static const uint32_t openagc_pm4_smoke_frag_sh_values[OPENAGC_PM4_SMOKE_FRAG_SH_COUNT] = {
    0u, 0u, 36438017u, 4u
};

/*
 * smoke.frag exports vec4(1.0, 0.25, 0.0, 1.0) as SPI_SHADER_32_ABGR (9,
 * the fixture's SPI_SHADER_COL_FORMAT). Into a linear 8_8_8_8 UNORM target
 * with COMP_SWAP=SWAP_STD that lands as the little-endian RGBA bytes
 * 255, 64, 0, 255, i.e. this dword. It is the whole acceptance value for
 * the Step-AC pixel comparison: any other nonzero pixel is a failure.
 */
#define OPENAGC_PM4_SMOKE_FRAG_PIXEL_RGBA8 0xFF0040FFu

/*
 * Everything the draw state needs besides the viewport rectangle: the
 * colour bind is openagc_gfx10_cb_bind_offsets, the shader program is the
 * smoke tables above, and the topology is one uconfig register.
 */
typedef struct openagc_pm4_draw_point_state {
    uint64_t color_va;
    uint32_t color_width;
    uint32_t color_height;
    uint32_t viewport_x;
    uint32_t viewport_y;
    uint32_t viewport_width;
    uint32_t viewport_height;
    /* What the NGG stage's primitives are rasterized as (VGT_GS_OUT_PRIM_TYPE):
     * 0 for a point (the Step-AC draw), 2 for a triangle strip (the Step-AD
     * triangle, matching Sony's own NGG draw). */
    uint32_t gs_out_prim_type;
    /* Vertices the one DRAW_INDEX_AUTO consumes; a zero count is refused. */
    uint32_t vertex_count;
} openagc_pm4_draw_point_state;

/* Scalar draw-state registers, emitted one SET_CONTEXT_REG packet each. */
#define OPENAGC_PM4_DRAW_POINT_STATE_COUNT 28u

/*
 * Fill the scalar draw-state register pairs in probe order. offsets/values
 * must hold OPENAGC_PM4_DRAW_POINT_STATE_COUNT entries. The viewport
 * rectangle must be inside the colour target and at least 1x1; a
 * zero-sized or out-of-bounds rectangle is refused (returns 0).
 */
static inline uint32_t openagc_pm4_draw_point_state_pairs(
    const openagc_pm4_draw_point_state *state, uint32_t *offsets, uint32_t *values)
{
    uint32_t x2;
    uint32_t y2;
    uint32_t i = 0u;

    if (state == NULL || offsets == NULL || values == NULL) {
        return 0u;
    }
    if (state->viewport_width == 0u || state->viewport_height == 0u) {
        return 0u;
    }
    if (state->vertex_count == 0u) {
        return 0u;
    }
    x2 = state->viewport_x + state->viewport_width;
    y2 = state->viewport_y + state->viewport_height;
    if (x2 > state->color_width || y2 > state->color_height) {
        return 0u;
    }

    offsets[i] = OPENAGC_GFX10_DB_Z_INFO;
    values[i++] = openagc_gfx10_draw_db_z_info_unbound();
    offsets[i] = OPENAGC_GFX10_DB_STENCIL_INFO;
    values[i++] = openagc_gfx10_draw_db_stencil_info_unbound();
    offsets[i] = OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_TL;
    values[i++] = openagc_gfx10_screen_scissor_tl(state->viewport_x,
                                                  state->viewport_y);
    offsets[i] = OPENAGC_GFX10_PA_SC_SCREEN_SCISSOR_BR;
    values[i++] = openagc_gfx10_screen_scissor_br(x2, y2);
    offsets[i] = OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_TL;
    values[i++] = openagc_gfx10_window_scissor_tl();
    offsets[i] = OPENAGC_GFX10_PA_SC_WINDOW_SCISSOR_BR;
    values[i++] = openagc_gfx10_window_scissor_br(state->color_width,
                                                  state->color_height);
    offsets[i] = OPENAGC_GFX10_PA_SC_CLIPRECT_RULE;
    values[i++] = OPENAGC_GFX10_CLIPRECT_RULE_DISABLED;
    offsets[i] = OPENAGC_GFX10_PA_SC_EDGERULE;
    values[i++] = openagc_gfx10_draw_edgerule();
    offsets[i] = OPENAGC_GFX10_PA_SU_HARDWARE_SCREEN_OFFSET;
    values[i++] = 0u;
    offsets[i] = OPENAGC_GFX10_SPI_INTERP_CONTROL_0;
    values[i++] = openagc_gfx10_draw_interp_control_0();
    offsets[i] = OPENAGC_GFX10_PA_CL_CLIP_CNTL;
    values[i++] = openagc_gfx10_draw_clip_cntl();
    offsets[i] = OPENAGC_GFX10_PA_SU_SC_MODE_CNTL;
    values[i++] = openagc_gfx10_draw_sc_mode_cntl();
    offsets[i] = OPENAGC_GFX10_PA_SU_POINT_SIZE;
    values[i++] = openagc_gfx10_draw_point_size_8px();
    offsets[i] = OPENAGC_GFX10_PA_SU_POINT_MINMAX;
    values[i++] = openagc_gfx10_draw_point_minmax_8px();
    offsets[i] = OPENAGC_GFX10_PA_SC_MODE_CNTL_0;
    values[i++] = openagc_gfx10_draw_mode_cntl_0();
    /* The master colour-write switch before anything is drawn. */
    offsets[i] = OPENAGC_GFX10_CB_COLOR_CONTROL;
    values[i++] = openagc_gfx10_draw_cb_color_control_normal();
    offsets[i] = OPENAGC_GFX10_CB_BLEND0_CONTROL;
    values[i++] = 0u;
    /* MSAA-capable rasterizer block (PS5_Vulkan's one-sample words). */
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_CONFIG;
    values[i++] = OPENAGC_GFX10_AA_CONFIG_1X;
    offsets[i] = OPENAGC_GFX10_DB_EQAA;
    values[i++] = OPENAGC_GFX10_DB_EQAA_1X;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X0Y0;
    values[i++] = OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X1Y0;
    values[i++] = OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X0Y1;
    values[i++] = OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_SAMPLE_LOCS_X1Y1;
    values[i++] = OPENAGC_GFX10_AA_SAMPLE_LOCATIONS_4X;
    offsets[i] = OPENAGC_GFX10_PA_SC_CENTROID_PRIORITY_0;
    values[i++] = OPENAGC_GFX10_AA_CENTROID_PRIORITY;
    offsets[i] = OPENAGC_GFX10_PA_SC_CENTROID_PRIORITY_1;
    values[i++] = OPENAGC_GFX10_AA_CENTROID_PRIORITY;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_MASK_X0Y0_X1Y0;
    values[i++] = OPENAGC_GFX10_AA_MASK_FULL;
    offsets[i] = OPENAGC_GFX10_PA_SC_AA_MASK_X0Y1_X1Y1;
    values[i++] = OPENAGC_GFX10_AA_MASK_FULL;
    /* What the NGG stage's primitives are rasterized as: a point list by
     * default, a triangle strip when the caller asks for one. */
    offsets[i] = OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE;
    values[i++] = state->gs_out_prim_type;
    return i;
}

/* IB layout: 22 baseline reads x 6, 29 scalar pairs x 3, the viewport
 * sequence (6 values), the guardband sequence (PA_SU_VTX_CNTL + 4 GB
 * values), the vport scissor sequence (2 values), the probe reads, the smoke program, the colour bind, the uconfig
 * topology, one instance, the draw, and the EOP trailer. */
#define OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS \
    (OPENAGC_GFX10_DRAW_BASELINE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS)
#define OPENAGC_PM4_DRAW_POINT_EOP_WORDS                                      \
    (OPENAGC_PM4_DRAW_POINT_BASELINE_WORDS +                                  \
     (OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 1u) * OPENAGC_PM4_SET_CONTEXT_WORDS(1u) + \
     OPENAGC_PM4_SET_CONTEXT_WORDS(6u) + /* viewport */                       \
     OPENAGC_PM4_SET_CONTEXT_WORDS(5u) + /* PA_SU_VTX_CNTL + guardband */      \
     OPENAGC_PM4_SET_CONTEXT_WORDS(2u) + /* per-viewport scissor */            \
     OPENAGC_GFX10_DRAW_PROBE_COUNT * OPENAGC_PM4_COPY_DATA_WORDS + /* probe */ \
     (OPENAGC_PM4_SMOKE_VERT_CTX_COUNT + OPENAGC_PM4_SMOKE_VERT_SH_COUNT +    \
      OPENAGC_PM4_SMOKE_LINK_COUNT + OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT +       \
      OPENAGC_PM4_SMOKE_FRAG_SH_COUNT + OPENAGC_GFX10_CB_BIND_COUNT) *        \
         OPENAGC_PM4_SET_CONTEXT_WORDS(1u) +                                  \
     OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT *                                    \
         (3u + OPENAGC_PM4_COPY_DATA_WORDS) /* per-form writes + readbacks */ + \
     5u /* uconfig register-table load */ +                                 \
     OPENAGC_PM4_COPY_DATA_WORDS /* its readback */ +                       \
     OPENAGC_PM4_DRAW_USER_DATA_COUNT * OPENAGC_PM4_SET_SH_GFX_WORDS(1u) +  \
     OPENAGC_PM4_DRAW_USER_DATA_COUNT * OPENAGC_PM4_COPY_DATA_WORDS +       \
     2u /* NUM_INSTANCES */ + 3u /* DRAW_INDEX_AUTO */ +                   \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)

/*
 * Encode the Step-AC point-draw IB. baseline_va receives
 * OPENAGC_GFX10_DRAW_BASELINE_COUNT words (the inherited context state, read
 * before this IB writes anything) and probe_va receives
 * OPENAGC_GFX10_DRAW_PROBE_COUNT words, the registers this IB writes read
 * back after them, so the caller can prove its own writes landed. vert_code_va/frag_code_va must be 256-byte-aligned uploaded code
 * addresses (the PGM patch uses the same >>8 / >>40 encoding as the
 * console-proven compute path). words must hold
 * OPENAGC_PM4_DRAW_POINT_EOP_WORDS. Returns the dword count actually
 * written, or 0 when the state is refused.
 */
static inline uint32_t openagc_pm4_encode_draw_point_eop(
    const openagc_pm4_draw_point_state *state, uint64_t baseline_va,
    uint64_t probe_va, uint64_t uconfig_probe_va, uint64_t uconfig_table_va,
    uint64_t vert_code_va,
    uint64_t frag_code_va, uint32_t sequence, uint64_t marker_va,
    uint32_t *words)
{
    uint32_t offsets[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t values[OPENAGC_PM4_DRAW_POINT_STATE_COUNT];
    uint32_t cb_values[OPENAGC_GFX10_CB_BIND_COUNT];
    uint32_t vert_sh_values[OPENAGC_PM4_SMOKE_VERT_SH_COUNT];
    uint32_t frag_sh_values[OPENAGC_PM4_SMOKE_FRAG_SH_COUNT];
    uint32_t viewport[6];
    uint32_t guardband[4];
    uint32_t gb_sequence[5];
    uint32_t vport_scissor[2];
    uint32_t iamvp;
    uint32_t user_data;
    uint32_t cursor = 0u;
    uint32_t count;
    uint32_t i;

    if (state == NULL || words == NULL) {
        return 0u;
    }
    if ((vert_code_va & 0xffull) != 0ull || (frag_code_va & 0xffull) != 0ull) {
        return 0u;
    }
    count = openagc_pm4_draw_point_state_pairs(state, offsets, values);
    if (count != OPENAGC_PM4_DRAW_POINT_STATE_COUNT) {
        return 0u;
    }

    /* Read-only baseline: what the context already holds (the kernel loads
     * the ASIC clear-state data at context creation, so this is the state a
     * driver starts from, not the hardware reset values). */
    for (i = 0u; i < OPENAGC_GFX10_DRAW_BASELINE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(
                openagc_gfx10_draw_baseline_offsets[i]),
            baseline_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    cursor += openagc_pm4_encode_psbc_context_pairs(
        offsets, values, count, words + cursor);

    /* IA_MULTI_VGT_PARAM is written with index 1, the form Mesa uses; the
     * unindexed write does not read back. */
    iamvp = openagc_gfx10_draw_ia_multi_vgt_param();
    openagc_pm4_encode_set_context_reg_idx(OPENAGC_GFX10_IA_MULTI_VGT_PARAM,
                                           OPENAGC_PM4_IAMVP_INDEX, 1u, &iamvp,
                                           words + cursor);
    cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);

    /* Viewport and guardband are sequences, the shape Mesa emits. */
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

    /* Read the probed registers back: proof that this IB's own state writes
     * landed in the context the draw will use. */
    for (i = 0u; i < OPENAGC_GFX10_DRAW_PROBE_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            openagc_pm4_copy_data_src_context_abs(
                openagc_gfx10_draw_probe_offsets[i]),
            probe_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    /* Full smoke.vert + smoke.frag register program, host snapshot order. */
    cursor += openagc_pm4_encode_psbc_context_pairs(
        openagc_pm4_smoke_vert_ctx_offsets, openagc_pm4_smoke_vert_ctx_values,
        OPENAGC_PM4_SMOKE_VERT_CTX_COUNT, words + cursor);
    for (i = 0u; i < OPENAGC_PM4_SMOKE_VERT_SH_COUNT; ++i) {
        vert_sh_values[i] = openagc_pm4_smoke_vert_sh_values[i];
    }
    vert_sh_values[0] = (uint32_t)(vert_code_va >> 8);
    vert_sh_values[1] = (uint32_t)(vert_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        openagc_pm4_smoke_vert_sh_offsets, vert_sh_values,
        OPENAGC_PM4_SMOKE_VERT_SH_COUNT, words + cursor);
    cursor += openagc_pm4_encode_psbc_context_pairs(
        openagc_pm4_smoke_link_offsets, openagc_pm4_smoke_link_values,
        OPENAGC_PM4_SMOKE_LINK_COUNT, words + cursor);
    cursor += openagc_pm4_encode_psbc_context_pairs(
        openagc_pm4_smoke_frag_ctx_offsets, openagc_pm4_smoke_frag_ctx_values,
        OPENAGC_PM4_SMOKE_FRAG_CTX_COUNT, words + cursor);
    for (i = 0u; i < OPENAGC_PM4_SMOKE_FRAG_SH_COUNT; ++i) {
        frag_sh_values[i] = openagc_pm4_smoke_frag_sh_values[i];
    }
    frag_sh_values[0] = (uint32_t)(frag_code_va >> 8);
    frag_sh_values[1] = (uint32_t)(frag_code_va >> 40);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        openagc_pm4_smoke_frag_sh_offsets, frag_sh_values,
        OPENAGC_PM4_SMOKE_FRAG_SH_COUNT, words + cursor);

    /* Step-AB colour bind for the target this point is drawn into. */
    openagc_gfx10_cb_bind_linear_8888_words(
        state->color_va, state->color_width, state->color_height,
        OPENAGC_GFX10_CB_COMP_SWAP_RGBA8, cb_values);
    cursor += openagc_pm4_encode_psbc_context_pairs(
        openagc_gfx10_cb_bind_offsets, cb_values, OPENAGC_GFX10_CB_BIND_COUNT,
        words + cursor);

    /* Topology: one POINTLIST point. Every uconfig form a driver uses is
     * tried, each followed by a read back, so the dump names the form this
     * firmware accepts and the value the draw sees is the last one that
     * landed. The offset field is the aperture-relative dword index, which
     * for VGT_PRIMITIVE_TYPE is the 0x242 record ps5-opengl's driver writes. */
    for (i = 0u; i < OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT; ++i) {
        words[cursor++] =
            openagc_pm4_header3(OPENAGC_PM4_OP_SET_UCONFIG_REG, 3u, 0u) |
            OPENAGC_PM4_RESET_FILTER_CAM;
        words[cursor++] =
            ((OPENAGC_GFX10_UCONFIG_REG_VGT_PRIMITIVE_TYPE -
              OPENAGC_PM4_UCONFIG_REG_OFFSET) >> 2) |
            (openagc_pm4_uconfig_vgt_primitive_type_indices[i] << 28);
        words[cursor++] = OPENAGC_GFX10_DI_PT_POINTLIST;
        openagc_pm4_encode_copy_data_reg_to_mem(
            OPENAGC_PM4_UCONFIG_REG_START +
                OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE,
            uconfig_probe_va + (uint64_t)i * 4u, words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    /* The register-table load Sony's own helpers use: one uconfig record,
     * {VGT_PRIMITIVE_TYPE, DI_PT_POINTLIST}, then its readback — before the
     * draw, because the topology has to hold when the VGT assembles. */
    words[cursor++] =
        openagc_pm4_header3(OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD, 5u, 0u);
    words[cursor++] = (uint32_t)uconfig_table_va;
    words[cursor++] = (uint32_t)(uconfig_table_va >> 32);
    words[cursor++] = OPENAGC_PM4_TABLE_LOAD_CONTROL;
    words[cursor++] = OPENAGC_PM4_DRAW_UCONFIG_TABLE_RECORDS;
    openagc_pm4_encode_copy_data_reg_to_mem(
        OPENAGC_PM4_UCONFIG_REG_START + OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE,
        uconfig_probe_va +
            (uint64_t)OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT * 4u,
        words + cursor);
    cursor += OPENAGC_PM4_COPY_DATA_WORDS;

    /* The NGG vertex stage's user-data block, then its readback through the
     * SH aperture: this is where a real NGG program puts the dwords PSBC
     * declares (base vertex, ngg_lds_layout, descriptor pointers). */
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        user_data = OPENAGC_PM4_DRAW_USER_DATA_PROBE_BASE + i;
        openagc_pm4_encode_set_sh_graphics(
            OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 + i, 1u, &user_data,
            words + cursor);
        cursor += OPENAGC_PM4_SET_SH_GFX_WORDS(1u);
    }
    for (i = 0u; i < OPENAGC_PM4_DRAW_USER_DATA_COUNT; ++i) {
        openagc_pm4_encode_copy_data_reg_to_mem(
            OPENAGC_PM4_SH_REG_START + OPENAGC_GFX10_SPI_SHADER_USER_DATA_GS_0 +
                i,
            uconfig_probe_va +
                (uint64_t)(OPENAGC_PM4_DRAW_UCONFIG_PROBE_COUNT + 1u + i) * 4u,
            words + cursor);
        cursor += OPENAGC_PM4_COPY_DATA_WORDS;
    }

    /* One instance, then one draw: auto-indexed, one vertex, not EOP. The
     * instance count must be stated explicitly; without it the VGT has no
     * instances to assemble. */
    words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_NUM_INSTANCES, 2u, 0u);
    words[cursor++] = OPENAGC_PM4_DRAW_INSTANCE_COUNT;
    words[cursor++] = openagc_pm4_header3(OPENAGC_PM4_OP_DRAW_INDEX_AUTO, 3u, 0u);
    words[cursor++] = state->vertex_count;
    words[cursor++] = OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX;

    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    cursor += OPENAGC_PM4_EOP_WITH_NOP_WORDS;
    return cursor;
}

/*
 * Extract the drawn window from a linear 8_8_8_8 target and count writes
 * outside it. This is the acceptance check for the Step-AC draw, and it is
 * shared so the caller cannot walk past its own mapping: the scan touches
 * exactly target_width * target_height words.
 *
 * window receives view_w * view_h words, row major, and is fully written
 * by this call (untouched entries become zero). *out_pixels counts the
 * nonzero window words, *out_first_value is the first nonzero one (0 when
 * the window is untouched), and *out_outside counts nonzero words in the
 * target but outside the window. Returns 1 on a completed scan, 0 when the
 * arguments are refused (NULL, a zero dimension, a window that leaves the
 * target, or a window buffer that is too small).
 */
static inline uint32_t openagc_pm4_draw_point_scan(
    const uint32_t *target, uint32_t target_width, uint32_t target_height,
    uint32_t view_x, uint32_t view_y, uint32_t view_w, uint32_t view_h,
    uint32_t *window, uint32_t window_capacity, uint32_t *out_pixels,
    uint32_t *out_outside, uint32_t *out_first_value)
{
    uint32_t x;
    uint32_t y;
    uint32_t pixels = 0u;
    uint32_t outside = 0u;
    uint32_t first_value = 0u;
    uint32_t x2;
    uint32_t y2;

    if (target == NULL || window == NULL || out_pixels == NULL ||
        out_outside == NULL || out_first_value == NULL) {
        return 0u;
    }
    if (target_width == 0u || target_height == 0u || view_w == 0u ||
        view_h == 0u) {
        return 0u;
    }
    x2 = view_x + view_w;
    y2 = view_y + view_h;
    if (x2 > target_width || y2 > target_height) {
        return 0u;
    }
    if (window_capacity < view_w * view_h) {
        return 0u;
    }

    for (y = 0u; y < view_h; ++y) {
        for (x = 0u; x < view_w; ++x) {
            uint32_t word = target[(view_y + y) * target_width + view_x + x];

            window[y * view_w + x] = word;
            if (word != 0u) {
                ++pixels;
                if (first_value == 0u) {
                    first_value = word;
                }
            }
        }
    }
    for (y = 0u; y < target_height; ++y) {
        for (x = 0u; x < target_width; ++x) {
            if (x >= view_x && x < x2 && y >= view_y && y < y2) {
                continue;
            }
            if (target[y * target_width + x] != 0u) {
                ++outside;
            }
        }
    }
    *out_pixels = pixels;
    *out_outside = outside;
    *out_first_value = first_value;
    return 1u;
}

#endif /* OPENAGC_PM4_DRAW_FW940_H */
