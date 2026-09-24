/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_SHADER_INTERNAL_H
#define OPENAGC_SHADER_INTERNAL_H

#include "openagc/shader.h"

openagc_result openagc_gpu_shader_buffer_validate(
    const openagc_gpu_device *device, const openagc_gpu_buffer *buffer,
    uint64_t offset, uint64_t size_bytes, uint32_t minimum_bytes);
void openagc_gpu_shader_buffer_retain(openagc_gpu_buffer *buffer);
void openagc_gpu_shader_buffer_release(openagc_gpu_buffer *buffer);

openagc_result openagc_graphics_shader_target_validate(
    const openagc_gpu_device *device, const openagc_graphics_image *image,
    openagc_graphics_format format, uint32_t *image_id);
void openagc_graphics_shader_target_retain(openagc_graphics_image *image);
void openagc_graphics_shader_target_release(openagc_graphics_image *image);

#endif
