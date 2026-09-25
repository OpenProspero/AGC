/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_IB_DUMP_FW940_H
#define OPENAGC_PM4_IB_DUMP_FW940_H

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
 * tag=step-u means register program + EOP only (no CB color, no DRAW).
 * tag=ctxreg-cb means Step-W COLOR_BASE-class readback (relative COPY_DATA
 * src; console-negative). tag=ctxreg-abs means Step-X absolute aperture
 * COPY_DATA src (CONTEXT_REG_START+offset). tag=ctxreg-rt means Step-Y
 * SET_CONTEXT smoke-owned values then absolute COPY_DATA readback.
 * None of these is a CB_BIND pin; evidence_qualified stays 0 until an
 * owned capture is pinned separately.
 */

#define OPENAGC_IB_DUMP_API_VERSION 1u
#define OPENAGC_IB_DUMP_FW940_ID 0x9400008u
#define OPENAGC_IB_DUMP_MAX_WORDS 256u
#define OPENAGC_IB_DUMP_TAG_STEP_U "step-u"
#define OPENAGC_IB_DUMP_TAG_CTXREG_CB "ctxreg-cb"
#define OPENAGC_IB_DUMP_TAG_CTXREG_ABS "ctxreg-abs"
#define OPENAGC_IB_DUMP_TAG_CTXREG_RT "ctxreg-rt"

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
    OPENAGC_IB_DUMP_KIND_CTXREG_RT = 4u
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

#endif /* OPENAGC_PM4_IB_DUMP_FW940_H */
