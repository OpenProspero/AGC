/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_WRITE_FW940_H
#define OPENAGC_PM4_WRITE_FW940_H

#include "openagc/pm4_fw940.h"

#include <stdint.h>

/*
 * IT_WRITE_DATA (PACKET3 opcode 0x37) — public AMD type-3 memory write.
 *
 * Independent cite (drm/amdgpu PM4 headers / ring emit patterns):
 *   PACKET3_WRITE_DATA = 0x37
 *   control dword: DST_SEL in bits[11:8], WR_CONFIRM in bit 20
 *   DST_SEL 5 = memory (async / TC)
 *   body: addr_lo, addr_hi, then N data dwords
 *   address is 4-byte aligned; default addr_incr=0 advances by 4 per dword
 *
 * Header count field is (total_dwords - 2). One data dword => 5 dwords total
 * => header 0xC0033700 with low bits 0.
 *
 * Step D (2026-09-25): one-dword WRITE_DATA+EOP console-proven on FW9.40.
 * Step E: same control with N data dwords (clear tile); addr increments.
 * Step F: multiple WRITE_DATA packets (one per image row) + one EOP.
 * Step G: full-width (16-dword) multi-row WRITE_DATA + one EOP.
 * Step H: IT_DMA_DATA + WRITE_DATA + one EOP in a single IB.
 * Step M: MAX_ROWS (=8) full-width WRITE_DATA + one EOP (host clear window).
 * Step N: MAX_COLS (=2) × MAX_ROWS grid WRITE_DATA + one EOP (32×8 window).
 * Do not claim hardware_qualified or gpu_execution from this header alone.
 */

#define OPENAGC_PM4_OP_WRITE_DATA 0x37u

/* DST_SEL(5) | WR_CONFIRM — memory write, wait for confirmation. */
#define OPENAGC_PM4_WRITE_DATA_DST_SEL_MEMORY 5u
#define OPENAGC_PM4_WRITE_DATA_CONTROL \
    ((OPENAGC_PM4_WRITE_DATA_DST_SEL_MEMORY << 8) | (1u << 20))

/* Fixed body before payload: control + addr_lo + addr_hi. */
#define OPENAGC_PM4_WRITE_DATA_PREFIX_WORDS 4u
#define OPENAGC_PM4_WRITE_DATA_WORDS(data_count) \
    (OPENAGC_PM4_WRITE_DATA_PREFIX_WORDS + (data_count))
#define OPENAGC_PM4_WRITE_DATA_ONE_WORDS OPENAGC_PM4_WRITE_DATA_WORDS(1u)
#define OPENAGC_PM4_WRITE_DATA_EOP_WORDS \
    (OPENAGC_PM4_WRITE_DATA_ONE_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS)

/* 4x4 RGBA8 clear tile (16 pixels / 16 dwords / 64 bytes). */
#define OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS 16u
#define OPENAGC_PM4_WRITE_DATA_CLEAR_BYTES \
    (OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS * 4u)
#define OPENAGC_PM4_WRITE_DATA_CLEAR_PKT_WORDS \
    OPENAGC_PM4_WRITE_DATA_WORDS(OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS)
#define OPENAGC_PM4_WRITE_DATA_CLEAR_EOP_WORDS \
    (OPENAGC_PM4_WRITE_DATA_CLEAR_PKT_WORDS + OPENAGC_PM4_EOP_WITH_NOP_WORDS)

/* Step F: up to 8 rows × ≤16 dwords per row, then one shared EOP. */
#define OPENAGC_PM4_WRITE_DATA_MAX_ROWS 8u
#define OPENAGC_PM4_WRITE_DATA_MAX_DWORDS OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS
#define OPENAGC_PM4_WRITE_DATA_ROWS_EOP_WORDS(row_count, dwords_per_row) \
    ((row_count) * OPENAGC_PM4_WRITE_DATA_WORDS(dwords_per_row) +       \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)
#define OPENAGC_PM4_WRITE_DATA_MAX_ROWS_EOP_WORDS                     \
    OPENAGC_PM4_WRITE_DATA_ROWS_EOP_WORDS(OPENAGC_PM4_WRITE_DATA_MAX_ROWS, \
                                          OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS)

/* Console Step F sample: two 8-dword rows (non-contiguous pitch). */
#define OPENAGC_PM4_WRITE_DATA_STEP_F_ROW_DWORDS 8u
#define OPENAGC_PM4_WRITE_DATA_STEP_F_ROWS 2u
#define OPENAGC_PM4_WRITE_DATA_STEP_F_EOP_WORDS                               \
    OPENAGC_PM4_WRITE_DATA_ROWS_EOP_WORDS(OPENAGC_PM4_WRITE_DATA_STEP_F_ROWS, \
                                          OPENAGC_PM4_WRITE_DATA_STEP_F_ROW_DWORDS)

/* Console Step G sample: two full-width (16-dword) rows, pitch 128. */
#define OPENAGC_PM4_WRITE_DATA_STEP_G_ROW_DWORDS \
    OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS
#define OPENAGC_PM4_WRITE_DATA_STEP_G_ROWS 2u
#define OPENAGC_PM4_WRITE_DATA_STEP_G_EOP_WORDS                               \
    OPENAGC_PM4_WRITE_DATA_ROWS_EOP_WORDS(OPENAGC_PM4_WRITE_DATA_STEP_G_ROWS, \
                                          OPENAGC_PM4_WRITE_DATA_STEP_G_ROW_DWORDS)

/* Step M: host clear-window ceiling — MAX_ROWS full-width rows + EOP. */
#define OPENAGC_PM4_WRITE_DATA_STEP_M_ROW_DWORDS \
    OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS
#define OPENAGC_PM4_WRITE_DATA_STEP_M_ROWS OPENAGC_PM4_WRITE_DATA_MAX_ROWS
#define OPENAGC_PM4_WRITE_DATA_STEP_M_EOP_WORDS \
    OPENAGC_PM4_WRITE_DATA_MAX_ROWS_EOP_WORDS

/*
 * Step N: multi-column grid — column_count packets per row (each ≤16
 * dwords), then one EOP. Host clear tiling for width ≤16*MAX_COLS uses
 * this when height ≤ MAX_ROWS.
 */
#define OPENAGC_PM4_WRITE_DATA_MAX_COLS 2u
#define OPENAGC_PM4_WRITE_DATA_GRID_EOP_WORDS(row_count, column_count, dwords_per_column) \
    ((row_count) * (column_count) * OPENAGC_PM4_WRITE_DATA_WORDS(dwords_per_column) +    \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)
#define OPENAGC_PM4_WRITE_DATA_MAX_GRID_EOP_WORDS                                      \
    OPENAGC_PM4_WRITE_DATA_GRID_EOP_WORDS(OPENAGC_PM4_WRITE_DATA_MAX_ROWS,             \
                                          OPENAGC_PM4_WRITE_DATA_MAX_COLS,             \
                                          OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS)
#define OPENAGC_PM4_WRITE_DATA_STEP_N_COLS OPENAGC_PM4_WRITE_DATA_MAX_COLS
#define OPENAGC_PM4_WRITE_DATA_STEP_N_ROWS OPENAGC_PM4_WRITE_DATA_MAX_ROWS
#define OPENAGC_PM4_WRITE_DATA_STEP_N_COL_DWORDS \
    OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS
#define OPENAGC_PM4_WRITE_DATA_STEP_N_EOP_WORDS OPENAGC_PM4_WRITE_DATA_MAX_GRID_EOP_WORDS

/* Step H: DMA + N-dword WRITE_DATA + shared EOP (one IB). */
#define OPENAGC_PM4_DMA_WRITE_EOP_WORDS(data_count) \
    (OPENAGC_PM4_DMA_WORDS + OPENAGC_PM4_WRITE_DATA_WORDS(data_count) + \
     OPENAGC_PM4_EOP_WITH_NOP_WORDS)
#define OPENAGC_PM4_WRITE_DATA_STEP_H_DWORDS 4u
#define OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS \
    OPENAGC_PM4_DMA_WRITE_EOP_WORDS(OPENAGC_PM4_WRITE_DATA_STEP_H_DWORDS)

static inline void openagc_pm4_encode_write_data(uint64_t destination,
                                                const uint32_t *data,
                                                uint32_t data_count,
                                                uint32_t *words)
{
    uint32_t i;
    uint32_t total = OPENAGC_PM4_WRITE_DATA_WORDS(data_count);

    words[0] = openagc_pm4_header3(OPENAGC_PM4_OP_WRITE_DATA, total, 0u);
    words[1] = OPENAGC_PM4_WRITE_DATA_CONTROL;
    words[2] = (uint32_t)destination & ~0x3u;
    words[3] = (uint32_t)(destination >> 32);
    for (i = 0u; i < data_count; ++i) {
        words[OPENAGC_PM4_WRITE_DATA_PREFIX_WORDS + i] = data[i];
    }
}

static inline void openagc_pm4_encode_write_data_one(uint64_t destination,
                                                    uint32_t value,
                                                    uint32_t words[5])
{
    uint32_t data[1];

    data[0] = value;
    openagc_pm4_encode_write_data(destination, data, 1u, words);
}

/*
 * One dword WRITE_DATA plus the shared action-based EOP+NOP trailer
 * (same trailer as copy_eop / store_const). Total
 * OPENAGC_PM4_WRITE_DATA_EOP_WORDS (29) dwords.
 */
static inline void openagc_pm4_encode_write_data_eop(uint64_t destination,
                                                    uint32_t value,
                                                    uint32_t sequence,
                                                    uint64_t marker_va,
                                                    uint32_t words[29])
{
    openagc_pm4_encode_write_data_one(destination, value, words);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence,
                                     words + OPENAGC_PM4_WRITE_DATA_ONE_WORDS);
}

/*
 * N identical dwords (1..CLEAR_DWORDS) plus EOP trailer. Used for small
 * buffer fills and 4x4 clear tiles. words must hold
 * OPENAGC_PM4_WRITE_DATA_WORDS(dword_count) + OPENAGC_PM4_EOP_WITH_NOP_WORDS.
 */
static inline void openagc_pm4_encode_write_data_fill_eop(uint64_t destination,
                                                         uint32_t value,
                                                         uint32_t dword_count,
                                                         uint32_t sequence,
                                                         uint64_t marker_va,
                                                         uint32_t *words)
{
    uint32_t data[OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS];
    uint32_t i;
    uint32_t packet_words;

    for (i = 0u; i < dword_count; ++i) {
        data[i] = value;
    }
    openagc_pm4_encode_write_data(destination, data, dword_count, words);
    packet_words = OPENAGC_PM4_WRITE_DATA_WORDS(dword_count);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + packet_words);
}

/*
 * One WRITE_DATA packet per row (identical fill), then one EOP trailer.
 * pitch_bytes is the byte stride between row starts. words must hold
 * OPENAGC_PM4_WRITE_DATA_ROWS_EOP_WORDS(row_count, dwords_per_row).
 */
static inline void openagc_pm4_encode_write_data_rows_eop(uint64_t base_va,
                                                         uint32_t pitch_bytes,
                                                         uint32_t value,
                                                         uint32_t dwords_per_row,
                                                         uint32_t row_count,
                                                         uint32_t sequence,
                                                         uint64_t marker_va,
                                                         uint32_t *words)
{
    uint32_t data[OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS];
    uint32_t row;
    uint32_t i;
    uint32_t cursor = 0u;
    uint32_t packet_words = OPENAGC_PM4_WRITE_DATA_WORDS(dwords_per_row);

    for (i = 0u; i < dwords_per_row; ++i) {
        data[i] = value;
    }
    for (row = 0u; row < row_count; ++row) {
        uint64_t row_va = base_va + (uint64_t)row * (uint64_t)pitch_bytes;

        openagc_pm4_encode_write_data(row_va, data, dwords_per_row, words + cursor);
        cursor += packet_words;
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
}

/*
 * column_count WRITE_DATA packets per row (identical fill width), then one
 * EOP. Column k starts at base + row*pitch + k*dwords_per_column*4.
 * words must hold OPENAGC_PM4_WRITE_DATA_GRID_EOP_WORDS(...).
 */
static inline void openagc_pm4_encode_write_data_grid_eop(uint64_t base_va,
                                                         uint32_t pitch_bytes,
                                                         uint32_t value,
                                                         uint32_t dwords_per_column,
                                                         uint32_t column_count,
                                                         uint32_t row_count,
                                                         uint32_t sequence,
                                                         uint64_t marker_va,
                                                         uint32_t *words)
{
    uint32_t data[OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS];
    uint32_t row;
    uint32_t column;
    uint32_t i;
    uint32_t cursor = 0u;
    uint32_t packet_words = OPENAGC_PM4_WRITE_DATA_WORDS(dwords_per_column);
    uint32_t column_bytes = dwords_per_column * 4u;

    for (i = 0u; i < dwords_per_column; ++i) {
        data[i] = value;
    }
    for (row = 0u; row < row_count; ++row) {
        for (column = 0u; column < column_count; ++column) {
            uint64_t cell_va = base_va + (uint64_t)row * (uint64_t)pitch_bytes +
                               (uint64_t)column * (uint64_t)column_bytes;

            openagc_pm4_encode_write_data(cell_va, data, dwords_per_column, words + cursor);
            cursor += packet_words;
        }
    }
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
}

/*
 * One IT_DMA_DATA, then one WRITE_DATA fill (identical dwords), then the
 * shared EOP+NOP trailer. words must hold
 * OPENAGC_PM4_DMA_WRITE_EOP_WORDS(dword_count).
 */
static inline void openagc_pm4_encode_dma_write_data_eop(uint64_t source_va,
                                                        uint64_t destination_va,
                                                        uint32_t dma_bytes,
                                                        uint32_t value,
                                                        uint32_t dword_count,
                                                        uint32_t sequence,
                                                        uint64_t marker_va,
                                                        uint32_t *words)
{
    uint32_t data[OPENAGC_PM4_WRITE_DATA_CLEAR_DWORDS];
    uint32_t i;
    uint32_t packet_words;

    openagc_pm4_encode_dma(source_va, destination_va, dma_bytes, words);
    for (i = 0u; i < dword_count; ++i) {
        data[i] = value;
    }
    openagc_pm4_encode_write_data(destination_va, data, dword_count,
                                  words + OPENAGC_PM4_DMA_WORDS);
    packet_words = OPENAGC_PM4_DMA_WORDS + OPENAGC_PM4_WRITE_DATA_WORDS(dword_count);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + packet_words);
}

#endif /* OPENAGC_PM4_WRITE_FW940_H */
