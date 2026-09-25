/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_GRAPHICS_INTERNAL_H
#define OPENAGC_GRAPHICS_INTERNAL_H

#include "openagc/graphics.h"

/* Refuses any image overlapping the range that is not copy-owned in `required`. */
openagc_result openagc_graphics_image_copy_access(
    const openagc_gpu_device *device, const openagc_gpu_memory *memory,
    uint64_t offset, uint64_t size_bytes, openagc_graphics_image_state required);

#endif
