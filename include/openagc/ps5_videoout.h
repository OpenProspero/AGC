/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PS5_VIDEOOUT_H
#define OPENAGC_PS5_VIDEOOUT_H

#include "openagc/openagc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native presentation of the OpenAGC command recorder's 1920x1080 frames.
 * Rasterization is CPU-side; this does not claim Vulkan/OpenGL GPU execution.
 * The returned platform error is the failing PS5 API result, when available. */
typedef struct openagc_ps5_videoout openagc_ps5_videoout;

openagc_result openagc_ps5_videoout_create(openagc_ps5_videoout **out_display,
                                           int *out_platform_error);
openagc_result openagc_ps5_videoout_present(openagc_ps5_videoout *display,
                                            const openagc_frame_view *frame,
                                            int *out_platform_error);
openagc_result openagc_ps5_videoout_destroy(openagc_ps5_videoout *display);

#ifdef __cplusplus
}
#endif

#endif
