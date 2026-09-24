/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_INTERNAL_H
#define OPENAGC_INTERNAL_H

#include "openagc/openagc.h"

#include <stddef.h>

struct openagc_context {
    openagc_backend backend;
    size_t device_count;
};

#endif
