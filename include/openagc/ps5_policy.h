/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PS5_POLICY_H
#define OPENAGC_PS5_POLICY_H

#include "openagc/openagc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_PS5_POLICY_API_VERSION 1u
/* The firmware identity every console step in docs/hardware-evidence.md
 * was observed on. An unknown identity is not qualified. */
#define OPENAGC_PS5_POLICY_FW940_ID 0x9400008u

/*
 * What a console submission may do, per capability, on a qualified
 * firmware. The mask is the qualification record: an operation is only
 * allowed once a reviewed console run observed it complete on the
 * identity being asked about. This is deliberately not a blanket deny -
 * the qualified operations below are the ones OpenAGC owns evidence for
 * - and it is not a blanket allow either: an unlisted capability, an
 * unobserved firmware, or a draw stays refused.
 */
typedef uint32_t openagc_ps5_capability;
enum {
    /* Step B: 31-dword IT_DMA_DATA + EOP, matched destination. */
    OPENAGC_PS5_CAP_COPY_EOP = 1u,
    /* Steps D-H, M, N: CP WRITE_DATA fills through the clear grid. */
    OPENAGC_PS5_CAP_WRITE_DATA_FILL = 2u,
    /* Steps C, I-L: compute store_const and 1..8 store_span chains. */
    OPENAGC_PS5_CAP_COMPUTE_STORE = 4u,
    /* Steps O-U: SET_CONTEXT/SET_SH/linkage register programs + EOP. */
    OPENAGC_PS5_CAP_REGISTER_PROGRAM = 8u,
    /* Steps X-Z, AB: absolute COPY_DATA readback and the owned nine-word
     * linear color bind round-trip. No DRAW. */
    OPENAGC_PS5_CAP_CB_BIND_READBACK = 16u,
    /* Steps V, W, X, Y, Z, AB, AA: the IB dump vehicle and its tags. */
    OPENAGC_PS5_CAP_IB_DUMP = 32u,
    /* Step AD: NGG context-table program, GE_CNTL/stages_en/ES PGM/LDS
     * readbacks. It completed and wrote no pixel. */
    OPENAGC_PS5_CAP_NGG_PROGRAM = 64u,
    /*
     * A draw that lands a pixel. No console run has produced one, so this
     * capability is never set by the table below; it is named so the
     * refusal is explicit rather than a missing bit.
     */
    OPENAGC_PS5_CAP_DRAW = 128u
};

/* Every capability the table may set, for mask validation. */
#define OPENAGC_PS5_CAP_KNOWN_MASK                                             \
    (OPENAGC_PS5_CAP_COPY_EOP | OPENAGC_PS5_CAP_WRITE_DATA_FILL |              \
     OPENAGC_PS5_CAP_COMPUTE_STORE | OPENAGC_PS5_CAP_REGISTER_PROGRAM |        \
     OPENAGC_PS5_CAP_CB_BIND_READBACK | OPENAGC_PS5_CAP_IB_DUMP |              \
     OPENAGC_PS5_CAP_NGG_PROGRAM | OPENAGC_PS5_CAP_DRAW)

/* What the console run set proved on the observed firmware. */
#define OPENAGC_PS5_QUALIFIED_MASK                                             \
    (OPENAGC_PS5_CAP_COPY_EOP | OPENAGC_PS5_CAP_WRITE_DATA_FILL |              \
     OPENAGC_PS5_CAP_COMPUTE_STORE | OPENAGC_PS5_CAP_REGISTER_PROGRAM |        \
     OPENAGC_PS5_CAP_CB_BIND_READBACK | OPENAGC_PS5_CAP_IB_DUMP |              \
     OPENAGC_PS5_CAP_NGG_PROGRAM)

typedef struct openagc_ps5_qualification {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t firmware_id;
    /* 1 only for the firmware identity the evidence was observed on. */
    uint32_t qualified;
    /* Capabilities with console evidence on that identity. */
    uint32_t capability_mask;
    /* Draw capabilities still refused, so a caller can report the gap. */
    uint32_t refused_mask;
} openagc_ps5_qualification;

#define OPENAGC_PS5_QUALIFICATION_INIT                                         \
    { (uint32_t)sizeof(openagc_ps5_qualification),                             \
      OPENAGC_PS5_POLICY_API_VERSION, 0u, 0u, 0u, 0u }

/* Fills info for any identity; returns UNSUPPORTED_FIRMWARE for one the
   evidence does not cover. A NULL info is INVALID_ARGUMENT. */
openagc_result openagc_ps5_policy_qualification(uint32_t firmware_id,
                                                openagc_ps5_qualification *info);
/* OK only when the identity is qualified and the capability is in its
   mask. An unknown capability bit is INVALID_ARGUMENT; a known bit
   without evidence is UNSUPPORTED_OPERATION. */
openagc_result openagc_ps5_policy_require(uint32_t firmware_id,
                                          openagc_ps5_capability capability);

#ifdef __cplusplus
}
#endif

#endif
