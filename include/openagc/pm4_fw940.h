/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_FW940_H
#define OPENAGC_PM4_FW940_H

#include <stdint.h>

/*
 * FW9.40 copy + action-based EOP layout observed on console
 * (tools/payload/copy_eop.c, 2026-09-25):
 *   7 dwords IT_DMA_DATA
 *   8 dwords IT_RELEASE_MEM EOP
 *  16 dwords NOP trailer (eight pairs of 0xC0001000, 0)
 * Total 31 dwords. Host encoding and the evidence payload must match.
 */

#define OPENAGC_PM4_DMA_WORDS 7u
#define OPENAGC_PM4_EOP_EVENT_WORDS 8u
#define OPENAGC_PM4_NOP_PAIRS 8u
#define OPENAGC_PM4_NOP_WORDS (OPENAGC_PM4_NOP_PAIRS * 2u)
#define OPENAGC_PM4_EOP_WITH_NOP_WORDS \
    (OPENAGC_PM4_EOP_EVENT_WORDS + OPENAGC_PM4_NOP_WORDS)
#define OPENAGC_PM4_COPY_EOP_WORDS \
    (OPENAGC_PM4_DMA_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS)

#define OPENAGC_PM4_DMA_HEADER 0xC0055002u
#define OPENAGC_PM4_DMA_CONTROL 0x8C00C000u
#define OPENAGC_PM4_EOP_HEADER 0xC0064900u
#define OPENAGC_PM4_EOP_CONTROL 0x06703514u
#define OPENAGC_PM4_EOP_DATA_SEL 0x20000000u
#define OPENAGC_PM4_NOP_HEADER 0xC0001000u

/* Type-3 header: bits[31:30]=3, bits[29:16]=dword_count-2, bits[15:8]=opcode. */
static inline uint32_t openagc_pm4_header3(uint32_t opcode, uint32_t dword_count,
                                          uint32_t low_bits)
{
    return (3u << 30) | (((dword_count - 2u) & 0x3fffu) << 16) | (opcode << 8) |
           (low_bits & 0xffu);
}

static inline void openagc_pm4_encode_dma(uint64_t source, uint64_t destination,
                                         uint32_t bytes, uint32_t words[7])
{
    words[0] = OPENAGC_PM4_DMA_HEADER;
    words[1] = OPENAGC_PM4_DMA_CONTROL;
    words[2] = (uint32_t)source;
    words[3] = (uint32_t)(source >> 32);
    words[4] = (uint32_t)destination;
    words[5] = (uint32_t)(destination >> 32);
    words[6] = bytes;
}

static inline void openagc_pm4_encode_eop_with_nops(uint64_t marker, uint32_t sequence,
                                                   uint32_t words[24])
{
    uint32_t i;

    words[0] = OPENAGC_PM4_EOP_HEADER;
    words[1] = OPENAGC_PM4_EOP_CONTROL;
    words[2] = OPENAGC_PM4_EOP_DATA_SEL;
    words[3] = (uint32_t)marker;
    words[4] = (uint32_t)(marker >> 32);
    words[5] = sequence;
    words[6] = 0u;
    words[7] = 0u;
    for (i = 0u; i < OPENAGC_PM4_NOP_PAIRS; ++i) {
        words[8u + i * 2u] = OPENAGC_PM4_NOP_HEADER;
        words[9u + i * 2u] = 0u;
    }
}

#endif /* OPENAGC_PM4_FW940_H */
