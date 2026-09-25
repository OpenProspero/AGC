/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_GPU_INTERNAL_H
#define OPENAGC_GPU_INTERNAL_H

#include "openagc/driver.h"
#include "openagc_internal.h"

#define OPENAGC_GPU_MAX_ALLOCATION_BYTES 16777216u

struct openagc_graphics_image;

struct openagc_gpu_device {
    openagc_context *context;
    uint64_t memory_budget_bytes;
    uint64_t allocated_bytes;
    uint64_t next_synthetic_va;
    uint64_t store_const_code_va;
    uint64_t store_const_marker_va;
    uint64_t write_data_marker_va;
    uint32_t *last_compute_words;
    uint32_t last_compute_word_count;
    uint32_t compute_sequence;
    uint32_t *last_write_words;
    uint32_t last_write_word_count;
    uint32_t write_sequence;
    uint32_t cb_capture_verified;
    uint32_t cb_capture_evidence_qualified;
    uint32_t cb_capture_kind;
    uint32_t cb_capture_word_count;
    uint32_t allocation_count;
    uint32_t buffer_count;
    uint32_t command_buffer_count;
    uint32_t queue_count;
    uint32_t fence_count;
    /* Intrusive list owned by the graphics layer; used to police copy access. */
    struct openagc_graphics_image *images;
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
