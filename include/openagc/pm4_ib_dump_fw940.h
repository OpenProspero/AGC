/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_IB_DUMP_FW940_H
#define OPENAGC_PM4_IB_DUMP_FW940_H

#include "openagc/pm4_context_regs_gfx10.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Console IB dump text contract for FW9.40 evidence payloads.
 *
 * Purpose: record the exact dwords submitted in a bounded IB (after a
 * proven Step-U-shaped register program + EOP) to an FTP-retrievable log
 * so a later independently owned CB/DB/DRAW capture can be pinned into
 * pm4_cb_capture_fw940.h without inventing register values.
 *
 * This dump vehicle is NOT a Stage 5 CB unlock. A Step-U register dump
 * must never be treated as CB_BIND / DB_BIND / DRAW evidence.
 * hardware_qualified and gpu_executable stay false.
 *
 * Log format (ASCII, one file, fopen "w"):
 *   openagc-ib-dump: tag=<token> fw=0x9400008 completed=<0|1> words=<N>
 *   ib <N hex dwords, lowercase, space-separated, may wrap lines>
 *
 * The Step-Z payload writes one `openagc-cb-bind-owned:` expect line before
 * the header; openagc_ib_dump_parse skips leading non-header lines and
 * still refuses text with no header at all.
 *
 * tag=step-u means register program + EOP only (no CB color, no DRAW).
 * tag=ctxreg-cb means Step-W COLOR_BASE-class readback (relative COPY_DATA
 * src; console-negative). tag=ctxreg-abs means Step-X absolute aperture
 * COPY_DATA src (CONTEXT_REG_START+offset). tag=ctxreg-rt means Step-Y
 * SET_CONTEXT smoke-owned values then absolute COPY_DATA readback.
 * tag=ctxreg-cb-bind means Step-Z SET owned CB_COLOR0_BASE(+EXT) from a
 * GPU VA then absolute COPY_DATA of the CB probe set.
 * tag=ctxreg-cb-bind-full means Step-AB SET of the nine gc_10_1_0 linear
 * color-bind registers with cited compositions, then their readback.
 * tag=mmio-tilemode means Step-AA read-only absolute COPY_DATA of
 * GB_ADDR_CONFIG then GB_TILE_MODE0..31 (console tile-mode table; no
 * write, no CB bind, no DRAW).
 * tag=draw-point-eop and tag=draw-ngg-eop mean the words are the drawn
 * color-target pixel window, not IB dwords (Steps AC and AD).
 * tag=draw-raster-eop means the same window for the shared rasterizer's
 * AGC-shaped draw IB.
 * None of these alone is a full CB_BIND pin: evidence_qualified stays 0
 * until an independently owned capture is pinned separately.
 * Use openagc_ib_dump_cb_bind_full_match for the Step-AB word set and
 * openagc_ib_dump_cb_bind_legacy_base_match for the Step-Z probe layout.
 */

#define OPENAGC_IB_DUMP_API_VERSION 1u
#define OPENAGC_IB_DUMP_FW940_ID 0x9400008u
#define OPENAGC_IB_DUMP_MAX_WORDS 256u
#define OPENAGC_IB_DUMP_TAG_STEP_U "step-u"
#define OPENAGC_IB_DUMP_TAG_CTXREG_CB "ctxreg-cb"
#define OPENAGC_IB_DUMP_TAG_CTXREG_ABS "ctxreg-abs"
#define OPENAGC_IB_DUMP_TAG_CTXREG_RT "ctxreg-rt"
#define OPENAGC_IB_DUMP_TAG_CTXREG_CB_BIND "ctxreg-cb-bind"
#define OPENAGC_IB_DUMP_TAG_CTXREG_CB_BIND_FULL "ctxreg-cb-bind-full"
#define OPENAGC_IB_DUMP_TAG_DRAW_POINT "draw-point-eop"
#define OPENAGC_IB_DUMP_TAG_DRAW_NGG "draw-ngg-eop"
#define OPENAGC_IB_DUMP_TAG_DRAW_RASTER "draw-raster-eop"
#define OPENAGC_IB_DUMP_TAG_MMIO_TILEMODE "mmio-tilemode"

typedef uint32_t openagc_ib_dump_kind;
enum {
    OPENAGC_IB_DUMP_KIND_NONE = 0u,
    /* Proven Step-U register program + EOP. Not a CB/DB/DRAW cite. */
    OPENAGC_IB_DUMP_KIND_REGISTER_EOP = 1u,
    /*
     * Step-W CB probe readback (relative src_lo). Console-negative.
     * Never treat as evidence_qualified CB_BIND without a pin.
     */
    OPENAGC_IB_DUMP_KIND_CTXREG_CB = 2u,
    /*
     * Step-X CB probe readback (absolute CONTEXT_REG_START+offset src_lo).
     * Never treat as evidence_qualified CB_BIND without a pin.
     */
    OPENAGC_IB_DUMP_KIND_CTXREG_ABS = 3u,
    /*
     * Step-Y SET_CONTEXT smoke-owned + absolute COPY_DATA round-trip.
     * Not a COLOR_BASE / CB_BIND cite; evidence_qualified stays 0.
     */
    OPENAGC_IB_DUMP_KIND_CTXREG_RT = 4u,
    /*
     * Step-Z SET owned CB_COLOR0_BASE(+EXT) + SHADER_MASK then absolute
     * COPY_DATA CB probe. owned_base_match may be 1; pin table stays 0
     * until a full CB_BIND capture (INFO/ATTRIB) is cited.
     */
    OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND = 5u,
    /*
     * Step-AA read-only GB_ADDR_CONFIG + GB_TILE_MODE0..31 capture.
     * Owns the console tile-mode table for (ARRAY_MODE,
     * MICRO_TILE_MODE_NEW) lookups; not a CB_BIND cite.
     */
    OPENAGC_IB_DUMP_KIND_MMIO_TILEMODE = 6u,
    /*
     * Step-AB full nine-register linear color bind + readback
     * (BASE/BASE_EXT/VIEW/INFO/ATTRIB/ATTRIB2/ATTRIB3/TARGET_MASK/
     * SHADER_MASK). owned_bind_match may be 1; the pin table stays 0
     * because no DRAW or independently owned capture backs it.
     */
    OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND_FULL = 7u,
    /*
     * Step-AC point draw: the words are the color-target pixel window the
     * draw wrote, not IB dwords. A positive match means the shader's color
     * reached the target and nothing else did.
     */
    OPENAGC_IB_DUMP_KIND_DRAW_POINT = 8u,
    /*
     * AGC-shaped draw: the words are the color-target pixel window the
     * shared rasterizer's IB wrote, not IB dwords. The same fail-closed
     * pixel rule as the point draw applies.
     */
    OPENAGC_IB_DUMP_KIND_DRAW_RASTER = 9u
};

typedef struct openagc_ib_dump_info {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_ib_dump_kind kind;
    uint32_t firmware_id;
    uint32_t word_count;
    uint32_t completed;
    /* 1 after structural parse of a well-formed dump text. */
    uint32_t dump_parsed;
    /* Always 0: dumps do not qualify CB evidence pins. */
    uint32_t evidence_qualified;
} openagc_ib_dump_info;

#define OPENAGC_IB_DUMP_INFO_INIT                                              \
    ((openagc_ib_dump_info){                                                   \
        (uint32_t)sizeof(openagc_ib_dump_info), OPENAGC_IB_DUMP_API_VERSION,  \
        OPENAGC_IB_DUMP_KIND_NONE, 0u, 0u, 0u, 0u, 0u                          \
    })

/*
 * Fail-closed owned CB BASE match for the legacy tag=ctxreg-cb-bind dumps
 * (Step Z probe layout).
 *
 * Returns 1 only when:
 *   - info is parsed CTXREG_CB_BIND with completed=1
 *   - word_count covers the CB probe set
 *   - expected_base_lo is non-zero (zero BASE is never a pin)
 *   - words[BASE] equals expected_base_lo
 *   - words[SHADER_MASK] equals expected_shader_mask
 *
 * Mismatch, incomplete, wrong kind, or zero expected BASE → 0.
 *
 * Correction (Step AB): the Step-Z payload's second SET target (offset 793)
 * is CB_COLOR0_PITCH, a GFX10 hole, not CB_COLOR0_BASE_EXT (912). The
 * earlier base_ext comparison is therefore withdrawn: this matcher no
 * longer scores probe index 1, and its third argument is now the shader
 * mask. Use openagc_ib_dump_cb_bind_full_match for the 912-based set.
 *
 * Does not set evidence_qualified and does not invent INFO/ATTRIB.
 */
static inline uint32_t openagc_ib_dump_cb_bind_legacy_base_match(
    const openagc_ib_dump_info *info, const uint32_t *words,
    uint32_t expected_base_lo, uint32_t expected_shader_mask)
{
    if (info == NULL || words == NULL) {
        return 0u;
    }
    if (info->dump_parsed == 0u || info->completed == 0u) {
        return 0u;
    }
    if (info->kind != OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND) {
        return 0u;
    }
    if (info->word_count < OPENAGC_GFX10_CB_PROBE_COUNT) {
        return 0u;
    }
    if (expected_base_lo == 0u) {
        return 0u;
    }
    if (words[OPENAGC_GFX10_CB_PROBE_IDX_BASE] != expected_base_lo) {
        return 0u;
    }
    if (words[OPENAGC_GFX10_CB_PROBE_IDX_SHADER_MASK] != expected_shader_mask) {
        return 0u;
    }
    return 1u;
}

/*
 * Fail-closed owned-bind match for tag=ctxreg-cb-bind-full dumps (Step AB).
 *
 * Returns 1 only when:
 *   - info is parsed CTXREG_CB_BIND_FULL with completed=1
 *   - word_count covers OPENAGC_GFX10_CB_BIND_COUNT
 *   - expected is non-NULL, expected[BASE] is non-zero, and every one of
 *     the nine words equals the expected composition
 *
 * A partial readback, a wrong kind, a zero BASE, or any single-word
 * mismatch → 0. Does not set evidence_qualified: the word set is composed
 * host-side from public cites, not captured from an independent driver.
 */
static inline uint32_t openagc_ib_dump_cb_bind_full_match(
    const openagc_ib_dump_info *info, const uint32_t *words,
    const uint32_t *expected)
{
    uint32_t i;

    if (info == NULL || words == NULL || expected == NULL) {
        return 0u;
    }
    if (info->dump_parsed == 0u || info->completed == 0u) {
        return 0u;
    }
    if (info->kind != OPENAGC_IB_DUMP_KIND_CTXREG_CB_BIND_FULL) {
        return 0u;
    }
    if (info->word_count < OPENAGC_GFX10_CB_BIND_COUNT) {
        return 0u;
    }
    if (expected[OPENAGC_GFX10_CB_BIND_IDX_BASE] == 0u) {
        return 0u;
    }
    for (i = 0u; i < OPENAGC_GFX10_CB_BIND_COUNT; ++i) {
        if (words[i] != expected[i]) {
            return 0u;
        }
    }
    return 1u;
}

/*
 * Fail-closed point-draw pixel match for tag=draw-point-eop dumps (Step AC).
 *
 * The dump words are the linear pixel window the draw was bounded to, one
 * 32-bit word per pixel. Returns 1 only when:
 *   - info is parsed DRAW_POINT with completed=1 and at least one word
 *   - every word is either 0 (untouched) or exactly expected_pixel
 *   - at least one word is expected_pixel
 *
 * Any other value (a different color, a partially written pixel, or a
 * write outside the shader's export) → 0. *pixel_count (may be NULL)
 * receives the number of pixels that matched. Does not set
 * evidence_qualified: this proves the draw's own target, not a display
 * path or a general renderer.
 */
static inline uint32_t openagc_ib_dump_draw_pixels_match_common(
    const openagc_ib_dump_info *info, const uint32_t *words,
    uint32_t expected_pixel, openagc_ib_dump_kind kind, uint32_t *pixel_count)
{
    uint32_t i;
    uint32_t matched = 0u;

    if (info == NULL || words == NULL) {
        return 0u;
    }
    if (info->dump_parsed == 0u || info->completed == 0u) {
        return 0u;
    }
    if (info->kind != kind) {
        return 0u;
    }
    if (info->word_count == 0u) {
        return 0u;
    }
    if (expected_pixel == 0u) {
        return 0u;
    }
    for (i = 0u; i < info->word_count; ++i) {
        if (words[i] == 0u) {
            continue;
        }
        if (words[i] != expected_pixel) {
            return 0u;
        }
        ++matched;
    }
    if (matched == 0u) {
        return 0u;
    }
    if (pixel_count != NULL) {
        *pixel_count = matched;
    }
    return 1u;
}

static inline uint32_t openagc_ib_dump_draw_point_pixels_match(
    const openagc_ib_dump_info *info, const uint32_t *words,
    uint32_t expected_pixel, uint32_t *pixel_count)
{
    return openagc_ib_dump_draw_pixels_match_common(
        info, words, expected_pixel, OPENAGC_IB_DUMP_KIND_DRAW_POINT,
        pixel_count);
}

/*
 * The same fail-closed pixel rule for tag=draw-raster-eop dumps, whose
 * words are the window the shared rasterizer's IB drew into. Different
 * kind, same acceptance: every nonzero word must be the shader's export
 * and at least one pixel must have matched.
 */
static inline uint32_t openagc_ib_dump_draw_raster_pixels_match(
    const openagc_ib_dump_info *info, const uint32_t *words,
    uint32_t expected_pixel, uint32_t *pixel_count)
{
    return openagc_ib_dump_draw_pixels_match_common(
        info, words, expected_pixel, OPENAGC_IB_DUMP_KIND_DRAW_RASTER,
        pixel_count);
}

/*
 * Fail-closed GB tile-mode lookup over a parsed tag=mmio-tilemode dump.
 *
 * Dump layout: words[0] = GB_ADDR_CONFIG, words[1 + i] = GB_TILE_MODEi.
 * Returns 1 and writes *index (0..31) only when:
 *   - info is parsed MMIO_TILEMODE with completed=1
 *   - word_count covers the whole probe set
 *   - exactly the entry whose ARRAY_MODE / MICRO_TILE_MODE_NEW fields
 *     equal the requested pair
 *
 * Mismatch, incomplete dump, wrong kind, or NULL outputs → 0. Never sets
 * evidence_qualified and never invents a tile mode.
 */
static inline uint32_t openagc_ib_dump_mmio_tilemode_lookup(
    const openagc_ib_dump_info *info, const uint32_t *words, uint32_t array_mode,
    uint32_t micro_tile_mode_new, uint32_t *index)
{
    uint32_t i;

    if (info == NULL || words == NULL || index == NULL) {
        return 0u;
    }
    if (info->dump_parsed == 0u || info->completed == 0u) {
        return 0u;
    }
    if (info->kind != OPENAGC_IB_DUMP_KIND_MMIO_TILEMODE) {
        return 0u;
    }
    if (info->word_count < OPENAGC_GFX10_MMIO_TILEMODE_PROBE_COUNT) {
        return 0u;
    }
    for (i = 0u; i < OPENAGC_GFX10_MMIO_GB_TILE_MODE_COUNT; ++i) {
        uint32_t word = words[1u + i];

        if (openagc_gfx10_gb_tile_mode_array_mode(word) == array_mode &&
            openagc_gfx10_gb_tile_mode_micro_tile_mode_new(word) ==
                micro_tile_mode_new) {
            *index = i;
            return 1u;
        }
    }
    return 0u;
}

#endif /* OPENAGC_PM4_IB_DUMP_FW940_H */
