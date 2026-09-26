/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/raster.h"

#include <string.h>

openagc_result openagc_raster_get_capabilities(
    openagc_raster_capabilities *capabilities)
{
    if (capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->pm4_draw_encoding = 1u;
    capabilities->gpu_rasterization = OPENAGC_RASTER_GPU_QUALIFIED;
    capabilities->host_rasterization = 0u;
    capabilities->topology_write_forms = 3u;
    capabilities->max_words = OPENAGC_RASTER_MAX_WORDS;
    capabilities->evidence_pin_count = 0u;
    return OPENAGC_OK;
}
