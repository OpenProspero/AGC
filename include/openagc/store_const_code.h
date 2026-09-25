/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_STORE_CONST_CODE_H
#define OPENAGC_STORE_CONST_CODE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Exact gfx1013 machine code from tools/shaders/store_const.s.
 * Console-proven on FW9.40 (tools/payload/store_const.c).
 * Host compute simulation may execute only this blob.
 */
#define OPENAGC_STORE_CONST_VALUE 0xA5A5A5A5u
#define OPENAGC_STORE_CONST_CODE_SIZE 28u

static const uint8_t openagc_store_const_code[OPENAGC_STORE_CONST_CODE_SIZE] = {
    0x02, 0x02, 0x04, 0x7e, 0x03, 0x02, 0x06, 0x7e, 0xff, 0x02, 0x00, 0x7e,
    0xa5, 0xa5, 0xa5, 0xa5, 0x00, 0x00, 0x70, 0xdc, 0x02, 0x00, 0x7d, 0x00,
    0x00, 0x00, 0x81, 0xbf
};

static inline int openagc_store_const_code_matches(const uint8_t *code, uint32_t size)
{
    return code != NULL && size == OPENAGC_STORE_CONST_CODE_SIZE &&
           memcmp(code, openagc_store_const_code, OPENAGC_STORE_CONST_CODE_SIZE) == 0;
}

#endif /* OPENAGC_STORE_CONST_CODE_H */
