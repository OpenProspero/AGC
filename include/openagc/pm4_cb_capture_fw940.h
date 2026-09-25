/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_CB_CAPTURE_FW940_H
#define OPENAGC_PM4_CB_CAPTURE_FW940_H

#include <stdint.h>

/*
 * Fail-closed CB/DB/DRAW capture intake for FW9.40.
 *
 * No independently owned FW9.40 CB/DB bind or DRAW IB capture exists in
 * this repository (see docs/hardware-evidence.md Stage 5 gate). This
 * header defines the manifest + SHA-256 digest contract so host code can
 * accept a verified artifact without inventing register values or DRAW
 * sequences. hardware_qualified and gpu_executable stay false until a
 * real cite is pinned in the evidence table (currently empty).
 *
 * PSBC smoke metadata owns SPI/PA/GE/VGT/DB_SHADER_CONTROL/CB_SHADER_MASK
 * SET_CONTEXT pairs (console Steps P–U) but NOT CB_COLOR0_BASE/pitch/
 * tiling — do not pin a CB_BIND digest derived from smoke alone.
 *
 * Do not emit PACKET3_DRAW_INDEX_AUTO or CB/DB register programs from
 * guessed offsets/values. Capture bytes must come from an independently
 * owned console IB dump; the host only verifies and records.
 */

#define OPENAGC_CB_CAPTURE_API_VERSION 1u
#define OPENAGC_CB_CAPTURE_FW940_ID 0x9400008u
#define OPENAGC_CB_CAPTURE_MAX_WORDS 256u

typedef uint32_t openagc_cb_capture_kind;
enum {
    OPENAGC_CB_CAPTURE_KIND_NONE = 0u,
    OPENAGC_CB_CAPTURE_KIND_CB_BIND = 1u,
    OPENAGC_CB_CAPTURE_KIND_DB_BIND = 2u,
    OPENAGC_CB_CAPTURE_KIND_DRAW = 3u
};

typedef struct openagc_cb_capture_manifest {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_cb_capture_kind kind;
    /* Must be OPENAGC_CB_CAPTURE_FW940_ID for FW9.40 intake. */
    uint32_t firmware_id;
    uint32_t word_count;
    uint8_t words_sha256[32];
} openagc_cb_capture_manifest;

typedef struct openagc_cb_capture_info {
    uint32_t struct_size;
    openagc_cb_capture_kind kind;
    uint32_t word_count;
    /* 1 after structural verify + host record. */
    uint32_t capture_verified;
    /* 1 only when digest matches an evidence pin (table is empty). */
    uint32_t evidence_qualified;
    /* Always 0: host records only; no GPU submit. */
    uint32_t gpu_submitted;
} openagc_cb_capture_info;

#define OPENAGC_CB_CAPTURE_MANIFEST_INIT                                       \
    ((openagc_cb_capture_manifest){                                            \
        (uint32_t)sizeof(openagc_cb_capture_manifest), OPENAGC_CB_CAPTURE_API_VERSION, \
        OPENAGC_CB_CAPTURE_KIND_NONE, 0u, 0u, { 0 }                            \
    })

#define OPENAGC_CB_CAPTURE_INFO_INIT                                           \
    ((openagc_cb_capture_info){                                                \
        (uint32_t)sizeof(openagc_cb_capture_info), OPENAGC_CB_CAPTURE_KIND_NONE, \
        0u, 0u, 0u, 0u                                                         \
    })

/*
 * Independently owned FW9.40 capture digests. Empty until a real CB/DB or
 * DRAW IB dump is cited and pinned. Count stays 0; do not invent entries.
 */
#define OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT 0u

#endif /* OPENAGC_PM4_CB_CAPTURE_FW940_H */
