/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_SHA256_H
#define OPENAGC_SHA256_H

#include <stddef.h>
#include <stdint.h>

void openagc_sha256(const uint8_t *bytes, size_t length, uint8_t digest[32]);

#endif
