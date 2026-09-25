/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_COMPUTE_FW940_H
#define OPENAGC_PM4_COMPUTE_FW940_H

#include "openagc/pm4_fw940.h"

#include <stdint.h>

/*
 * FW9.40 compute dispatch facts from the operator's empirical notes
 * (ProsperoAI notes/re/940-gpu-empirics.md), not a copied implementation:
 *   SET_SH_REG with compute-bank bit 0 set
 *   DISPATCH_DIRECT with compute-bank bit 0 and initiator 0x41
 *   RSRC1 0x602C0000 (WGP_MODE + W32_EN) launches
 *   Full CONTEXT_CONTROL preamble is not required on 9.40
 *   flat_load hangs; this path only stores
 *
 * SH register offsets are the public gfx10 compute map.
 */

#define OPENAGC_PM4_OP_DISPATCH_DIRECT 0x15u
#define OPENAGC_PM4_OP_SET_SH_REG 0x76u
#define OPENAGC_PM4_DISPATCH_INITIATOR 0x41u

#define OPENAGC_SH_COMPUTE_NUM_THREAD_X 0x0207u
#define OPENAGC_SH_COMPUTE_PGM_LO 0x020Cu
#define OPENAGC_SH_COMPUTE_PGM_RSRC1 0x0212u
#define OPENAGC_SH_COMPUTE_PGM_RSRC3 0x0228u
#define OPENAGC_SH_COMPUTE_USER_DATA_0 0x0240u

#define OPENAGC_COMPUTE_RSRC1_W32 0x602C0000u
/* USER_SGPR count 4 => s0..s3 (destination VA in s2:s3). */
#define OPENAGC_COMPUTE_RSRC2_USER4 0x00000008u
#define OPENAGC_COMPUTE_RSRC3_ZERO 0u

#define OPENAGC_PM4_SET_SH_WORDS(value_count) ((value_count) + 2u)
#define OPENAGC_PM4_DISPATCH_WORDS 5u

/* PGM_LO/HI(4) + RSRC1/2(4) + RSRC3(3) + NUM_THREAD(5) + USER_DATA4(6) + DISPATCH(5) */
#define OPENAGC_PM4_COMPUTE_DISPATCH_WORDS 27u
#define OPENAGC_PM4_COMPUTE_STORE_WORDS \
    (OPENAGC_PM4_COMPUTE_DISPATCH_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS)

static inline void openagc_pm4_encode_set_sh_compute(uint32_t offset, uint32_t value_count,
                                                     const uint32_t *values,
                                                     uint32_t *words)
{
    uint32_t i;

    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG,
                                   OPENAGC_PM4_SET_SH_WORDS(value_count), 1u);
    words[1] = offset & 0xffffu;
    for (i = 0u; i < value_count; ++i) {
        words[2u + i] = values[i];
    }
}

static inline void openagc_pm4_encode_dispatch_direct(uint32_t groups_x, uint32_t groups_y,
                                                      uint32_t groups_z, uint32_t *words)
{
    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_DISPATCH_DIRECT,
                                   OPENAGC_PM4_DISPATCH_WORDS, 1u);
    words[1] = groups_x;
    words[2] = groups_y;
    words[3] = groups_z;
    words[4] = OPENAGC_PM4_DISPATCH_INITIATOR;
}

/*
 * Encode one 1x1x1 group, 1-thread store-const dispatch plus the shared
 * EOP+NOP trailer. code_va must be 256-byte aligned. destination_va is
 * placed in user SGPRs s2:s3.
 */
static inline void openagc_pm4_encode_compute_store_const(uint64_t code_va,
                                                          uint64_t destination_va,
                                                          uint32_t sequence,
                                                          uint64_t marker_va,
                                                          uint32_t words[51])
{
    uint32_t values[4];
    uint32_t *cursor = words;

    values[0] = (uint32_t)(code_va >> 8);
    values[1] = (uint32_t)(code_va >> 40);
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_LO, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC1_W32;
    values[1] = OPENAGC_COMPUTE_RSRC2_USER4;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC1, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC3_ZERO;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC3, 1u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(1u);

    values[0] = 1u;
    values[1] = 1u;
    values[2] = 1u;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_NUM_THREAD_X, 3u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(3u);

    values[0] = 0u;
    values[1] = 0u;
    values[2] = (uint32_t)destination_va;
    values[3] = (uint32_t)(destination_va >> 32);
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_USER_DATA_0, 4u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(4u);

    openagc_pm4_encode_dispatch_direct(1u, 1u, 1u, cursor);
    cursor += OPENAGC_PM4_DISPATCH_WORDS;

    openagc_pm4_encode_eop_with_nops(marker_va, sequence, cursor);
}

/*
 * Same preamble as store_const but NUM_THREAD_X = 8 for the store_span
 * kernel (8 flat_store_dword lanes at base+tid*4). Total words still
 * OPENAGC_PM4_COMPUTE_STORE_WORDS (51).
 */
static inline void openagc_pm4_encode_compute_store_span(uint64_t code_va,
                                                         uint64_t destination_va,
                                                         uint32_t sequence,
                                                         uint64_t marker_va,
                                                         uint32_t words[51])
{
    uint32_t values[4];
    uint32_t *cursor = words;

    values[0] = (uint32_t)(code_va >> 8);
    values[1] = (uint32_t)(code_va >> 40);
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_LO, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC1_W32;
    values[1] = OPENAGC_COMPUTE_RSRC2_USER4;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC1, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC3_ZERO;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC3, 1u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(1u);

    values[0] = 8u;
    values[1] = 1u;
    values[2] = 1u;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_NUM_THREAD_X, 3u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(3u);

    values[0] = 0u;
    values[1] = 0u;
    values[2] = (uint32_t)destination_va;
    values[3] = (uint32_t)(destination_va >> 32);
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_USER_DATA_0, 4u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(4u);

    openagc_pm4_encode_dispatch_direct(1u, 1u, 1u, cursor);
    cursor += OPENAGC_PM4_DISPATCH_WORDS;

    openagc_pm4_encode_eop_with_nops(marker_va, sequence, cursor);
}

/* USER_DATA + DISPATCH for one store_span base (11 dwords). */
#define OPENAGC_PM4_COMPUTE_SPAN_BODY_WORDS \
    (OPENAGC_PM4_SET_SH_WORDS(4u) + OPENAGC_PM4_DISPATCH_WORDS)
/* Preamble through NUM_THREAD for store_span (16 dwords). */
#define OPENAGC_PM4_COMPUTE_SPAN_PREAMBLE_WORDS                               \
    (OPENAGC_PM4_SET_SH_WORDS(2u) + OPENAGC_PM4_SET_SH_WORDS(2u) +            \
     OPENAGC_PM4_SET_SH_WORDS(1u) + OPENAGC_PM4_SET_SH_WORDS(3u))
#define OPENAGC_PM4_COMPUTE_SPAN_MAX 8u
#define OPENAGC_PM4_COMPUTE_STORE_SPAN_N_WORDS(span_count)                    \
    (OPENAGC_PM4_COMPUTE_SPAN_PREAMBLE_WORDS +                                \
     (span_count) * OPENAGC_PM4_COMPUTE_SPAN_BODY_WORDS +                     \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)
#define OPENAGC_PM4_COMPUTE_STORE_SPAN2_WORDS \
    OPENAGC_PM4_COMPUTE_STORE_SPAN_N_WORDS(2u)
#define OPENAGC_PM4_COMPUTE_STORE_SPAN_MAX_WORDS \
    OPENAGC_PM4_COMPUTE_STORE_SPAN_N_WORDS(OPENAGC_PM4_COMPUTE_SPAN_MAX)
/* Step K sample: four spans (128 bytes). */
#define OPENAGC_PM4_COMPUTE_STORE_SPAN4_WORDS \
    OPENAGC_PM4_COMPUTE_STORE_SPAN_N_WORDS(4u)

/*
 * span_count store_span dispatches (bases dest + i*32), then one EOP.
 * 1 <= span_count <= OPENAGC_PM4_COMPUTE_SPAN_MAX. words must hold
 * OPENAGC_PM4_COMPUTE_STORE_SPAN_N_WORDS(span_count).
 */
static inline void openagc_pm4_encode_compute_store_span_n(uint64_t code_va,
                                                           uint64_t destination_va,
                                                           uint32_t span_count,
                                                           uint32_t sequence,
                                                           uint64_t marker_va,
                                                           uint32_t *words)
{
    uint32_t values[4];
    uint32_t *cursor = words;
    uint32_t span;

    values[0] = (uint32_t)(code_va >> 8);
    values[1] = (uint32_t)(code_va >> 40);
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_LO, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC1_W32;
    values[1] = OPENAGC_COMPUTE_RSRC2_USER4;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC1, 2u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(2u);

    values[0] = OPENAGC_COMPUTE_RSRC3_ZERO;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_PGM_RSRC3, 1u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(1u);

    values[0] = 8u;
    values[1] = 1u;
    values[2] = 1u;
    openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_NUM_THREAD_X, 3u, values, cursor);
    cursor += OPENAGC_PM4_SET_SH_WORDS(3u);

    for (span = 0u; span < span_count; ++span) {
        uint64_t span_va = destination_va + (uint64_t)span * 32u;

        values[0] = 0u;
        values[1] = 0u;
        values[2] = (uint32_t)span_va;
        values[3] = (uint32_t)(span_va >> 32);
        openagc_pm4_encode_set_sh_compute(OPENAGC_SH_COMPUTE_USER_DATA_0, 4u, values, cursor);
        cursor += OPENAGC_PM4_SET_SH_WORDS(4u);
        openagc_pm4_encode_dispatch_direct(1u, 1u, 1u, cursor);
        cursor += OPENAGC_PM4_DISPATCH_WORDS;
    }

    openagc_pm4_encode_eop_with_nops(marker_va, sequence, cursor);
}

static inline void openagc_pm4_encode_compute_store_span2(uint64_t code_va,
                                                          uint64_t destination_va,
                                                          uint32_t sequence,
                                                          uint64_t marker_va,
                                                          uint32_t *words)
{
    openagc_pm4_encode_compute_store_span_n(code_va, destination_va, 2u, sequence, marker_va,
                                            words);
}

#endif /* OPENAGC_PM4_COMPUTE_FW940_H */
