/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_GPU_INTERNAL_H
#define OPENAGC_GPU_INTERNAL_H

#include "openagc/driver.h"
#include "openagc_internal.h"

#define OPENAGC_GPU_MAX_ALLOCATION_BYTES 16777216u

struct openagc_gpu_device {
    openagc_context *context;
    uint64_t memory_budget_bytes;
    uint64_t allocated_bytes;
    uint64_t next_synthetic_va;
    uint32_t allocation_count;
    uint32_t buffer_count;
    uint32_t command_buffer_count;
    uint32_t queue_count;
    uint32_t fence_count;
    uint32_t image_count;
    uint32_t graphics_command_buffer_count;
    uint32_t next_graphics_image_id;
    uint32_t shader_artifact_count;
    uint32_t shader_pipeline_plan_count;
};

struct openagc_gpu_memory {
    openagc_gpu_device *device;
    uint8_t *bytes;
    uint64_t size_bytes;
    uint64_t synthetic_va;
    uint32_t bound_buffers;
    uint32_t bound_images;
};

#endif
