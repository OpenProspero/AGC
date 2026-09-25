/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_STORE_SPAN_CODE_H
#define OPENAGC_STORE_SPAN_CODE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Exact gfx1013 machine code from tools/shaders/store_span.s.
 * Writes OPENAGC_STORE_SPAN_LANES dwords (tid*4) of
 * OPENAGC_STORE_SPAN_VALUE at the VA in s2:s3.
 * Console Step I (FW9.40) when proven via tools/payload/store_span.c.
 */
#define OPENAGC_STORE_SPAN_VALUE 0xA5A5A5A5u
#define OPENAGC_STORE_SPAN_LANES 8u
#define OPENAGC_STORE_SPAN_BYTES (OPENAGC_STORE_SPAN_LANES * 4u)
#define OPENAGC_STORE_SPAN_CODE_SIZE 32u

static const uint8_t openagc_store_span_code[OPENAGC_STORE_SPAN_CODE_SIZE] = {
    0x82, 0x00, 0x02, 0x34, 0x02, 0x02, 0x04, 0x4a, 0x03, 0x02, 0x06, 0x7e,
    0xff, 0x02, 0x00, 0x7e, 0xa5, 0xa5, 0xa5, 0xa5, 0x00, 0x00, 0x70, 0xdc,
    0x02, 0x00, 0x7d, 0x00, 0x00, 0x00, 0x81, 0xbf
};

static inline int openagc_store_span_code_matches(const uint8_t *code, uint32_t size)
{
    return code != NULL && size == OPENAGC_STORE_SPAN_CODE_SIZE &&
           memcmp(code, openagc_store_span_code, OPENAGC_STORE_SPAN_CODE_SIZE) == 0;
}

#endif /* OPENAGC_STORE_SPAN_CODE_H */
