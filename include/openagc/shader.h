/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_SHADER_H
#define OPENAGC_SHADER_H

#include "openagc/graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPENAGC_SHADER_API_VERSION 1u
#define OPENAGC_SHADER_TARGET_GFX1013 1013u
#define OPENAGC_SHADER_PINNED_PSBC_REVISION "a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92"
#define OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION 14u
/*
 * Verified build-time opengnm-psbc executable digest from the private
 * Actions artifact (manifest verified=true, hardware_qualified=false,
 * openagc_runtime_compiler_enabled=false). Recording the pin does not
 * enable OPENGNM_PSBC intake or gpu_executable by itself.
 */
#define OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256 \
    "2f2cbab5a971742fa3b09e4e3f4e104455c85f569bd3925897c6a00112233fa9"

typedef struct openagc_shader_artifact openagc_shader_artifact;
typedef struct openagc_shader_pipeline_plan openagc_shader_pipeline_plan;

typedef uint32_t openagc_shader_compiler;
enum {
    OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE = 0u,
    OPENAGC_SHADER_COMPILER_OPENGNM_PSBC = 1u
};

typedef uint32_t openagc_shader_stage;
enum {
    OPENAGC_SHADER_STAGE_VERTEX = 1u,
    OPENAGC_SHADER_STAGE_PIXEL = 2u,
    OPENAGC_SHADER_STAGE_COMPUTE = 3u
};

typedef uint32_t openagc_shader_binding_kind;
enum {
    OPENAGC_SHADER_BINDING_UNIFORM_BUFFER = 1u
};

typedef uint32_t openagc_shader_pipeline_kind;
enum {
    OPENAGC_SHADER_PIPELINE_GRAPHICS = 1u,
    OPENAGC_SHADER_PIPELINE_COMPUTE = 2u
};

typedef struct openagc_shader_capabilities {
    uint32_t struct_size;
    uint32_t target;
    uint32_t max_code_bytes;
    uint32_t max_bindings;
    uint32_t max_artifacts;
    uint32_t max_pipeline_plans;
    uint32_t max_workgroup_invocations;
    uint32_t structural_intake;
    uint32_t compiler_available;
    uint32_t gpu_execution;
    /* Host CPU simulation of the console-proven store-const compute blob. */
    uint32_t host_compute_simulation;
} openagc_shader_capabilities;

typedef struct openagc_shader_binding_decl {
    uint32_t set;
    uint32_t binding;
    openagc_shader_binding_kind kind;
    uint32_t min_bytes;
} openagc_shader_binding_decl;

typedef struct openagc_shader_texture_decl {
    uint32_t set;
    uint32_t binding;
    openagc_graphics_format format;
} openagc_shader_texture_decl;

typedef struct openagc_shader_artifact_desc {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t target;
    openagc_shader_compiler compiler;
    uint32_t compiler_metadata_version;
    uint32_t toolchain_release_major;
    uint32_t toolchain_release_minor;
    uint32_t toolchain_release_patch;
    char compiler_source_revision[41];
    openagc_shader_stage stage;
    const uint8_t *code;
    uint32_t code_size;
    uint8_t code_sha256[32];
    uint8_t compiler_binary_sha256[32];
    const openagc_shader_binding_decl *bindings;
    uint32_t binding_count;
    const openagc_shader_texture_decl *textures;
    uint32_t texture_count;
    uint32_t vertex_position_written;
    uint32_t varying_input_mask;
    uint32_t varying_output_mask;
    uint32_t color_output_mask;
    openagc_graphics_format color_export_format;
    uint32_t workgroup_x;
    uint32_t workgroup_y;
    uint32_t workgroup_z;
    const uint8_t *compiler_metadata;
    uint32_t compiler_metadata_size;
} openagc_shader_artifact_desc;

typedef struct openagc_shader_artifact_info {
    uint32_t struct_size;
    openagc_shader_stage stage;
    uint32_t target;
    uint32_t code_size;
    uint32_t binding_count;
    uint32_t texture_count;
    uint32_t compiler_verified;
    uint32_t gpu_executable;
    /* 1 when code is the console-proven store-const blob; host CPU may simulate it. */
    uint32_t host_store_const;
    /* 1 when code is the console-proven 8-lane store-span blob (Step I). */
    uint32_t host_store_span;
    /* 1 when the artifact is a pin-checked OPENGNM_PSBC envelope; still not gpu_executable. */
    uint32_t psbc_envelope;
    uint8_t code_sha256[32];
} openagc_shader_artifact_info;

typedef struct openagc_shader_resource_binding {
    uint32_t set;
    uint32_t binding;
    openagc_gpu_buffer *buffer;
    uint64_t offset;
    uint64_t size_bytes;
} openagc_shader_resource_binding;

typedef struct openagc_shader_texture_binding {
    uint32_t set;
    uint32_t binding;
    openagc_graphics_image *image;
} openagc_shader_texture_binding;

typedef struct openagc_shader_pipeline_desc {
    uint32_t struct_size;
    uint32_t api_version;
    openagc_shader_pipeline_kind kind;
    openagc_shader_artifact *vertex;
    openagc_shader_artifact *pixel;
    openagc_shader_artifact *compute;
    openagc_graphics_image *color_target;
    const openagc_shader_resource_binding *resources;
    uint32_t resource_count;
    const openagc_shader_texture_binding *textures;
    uint32_t texture_count;
} openagc_shader_pipeline_desc;

typedef struct openagc_shader_pipeline_info {
    uint32_t struct_size;
    openagc_shader_pipeline_kind kind;
    uint32_t stage_count;
    uint32_t resource_count;
    uint32_t texture_count;
    openagc_graphics_format color_format;
    uint32_t target_image_id;
    uint32_t compiler_verified;
    uint32_t gpu_executable;
} openagc_shader_pipeline_info;

#define OPENAGC_SHADER_CAPABILITIES_INIT \
    { (uint32_t)sizeof(openagc_shader_capabilities), 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u, 0u }
#define OPENAGC_SHADER_ARTIFACT_DESC_INIT \
    { (uint32_t)sizeof(openagc_shader_artifact_desc), OPENAGC_SHADER_API_VERSION, \
      OPENAGC_SHADER_TARGET_GFX1013, OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE, \
      0u, 0u, 0u, 0u, { 0 }, 0u, (const uint8_t *)0, 0u, { 0 }, { 0 }, \
      (const openagc_shader_binding_decl *)0, 0u, \
      (const openagc_shader_texture_decl *)0, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
      (const uint8_t *)0, 0u }
#define OPENAGC_SHADER_ARTIFACT_INFO_INIT \
    { (uint32_t)sizeof(openagc_shader_artifact_info), 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u, 0u, 0u, { 0 } }
#define OPENAGC_SHADER_PIPELINE_DESC_INIT \
    { (uint32_t)sizeof(openagc_shader_pipeline_desc), OPENAGC_SHADER_API_VERSION, \
      0u, (openagc_shader_artifact *)0, (openagc_shader_artifact *)0, \
      (openagc_shader_artifact *)0, (openagc_graphics_image *)0, \
      (const openagc_shader_resource_binding *)0, 0u, \
      (const openagc_shader_texture_binding *)0, 0u }
#define OPENAGC_SHADER_PIPELINE_INFO_INIT \
    { (uint32_t)sizeof(openagc_shader_pipeline_info), 0u, 0u, 0u, 0u, \
      0u, 0u, 0u, 0u }

openagc_result openagc_shader_get_capabilities(
    const openagc_gpu_device *device, openagc_shader_capabilities *capabilities);

/* Deep-copies a structural TEST fixture. Never verifies a compiler or GPU code. */
openagc_result openagc_shader_artifact_intake_host(
    openagc_gpu_device *device, const openagc_shader_artifact_desc *desc,
    openagc_shader_artifact **out_artifact);
openagc_result openagc_shader_artifact_get_info(
    const openagc_shader_artifact *artifact, openagc_shader_artifact_info *info);
openagc_result openagc_shader_artifact_get_binding(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_binding_decl *out_binding);
openagc_result openagc_shader_artifact_get_texture(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_texture_decl *out_texture);
/*
 * Retained PSBC metadata for a pin-checked envelope. Pointers are owned by
 * the artifact until destroy. NOT_READY when the artifact is not a PSBC
 * envelope. Does not imply compiler_verified or gpu_executable.
 */
openagc_result openagc_shader_artifact_get_compiler_metadata(
    const openagc_shader_artifact *artifact, const uint8_t **out_metadata,
    uint32_t *out_size);
/*
 * Retained machine code for any accepted artifact. Pointer owned by the
 * artifact until destroy. Does not imply compiler_verified or gpu_executable.
 */
openagc_result openagc_shader_artifact_get_code(const openagc_shader_artifact *artifact,
                                                const uint8_t **out_code,
                                                uint32_t *out_size);
openagc_result openagc_shader_artifact_destroy(openagc_shader_artifact *artifact);
/* No pinned compiler is installed; never upgrades a fixture to executable. */
openagc_result openagc_shader_artifact_require_compiler(
    const openagc_shader_artifact *artifact);

/* Validates reflection and retains resources; does not build/record GPU commands. */
openagc_result openagc_shader_pipeline_plan_create_host(
    openagc_gpu_device *device, const openagc_shader_pipeline_desc *desc,
    openagc_shader_pipeline_plan **out_plan);
openagc_result openagc_shader_pipeline_plan_get_info(
    const openagc_shader_pipeline_plan *plan, openagc_shader_pipeline_info *info);
/* OUT_OF_RANGE above slot 7. NULL means the slot is not reflected. */
openagc_result openagc_shader_pipeline_plan_slot(
    const openagc_shader_pipeline_plan *plan, uint32_t slot, openagc_gpu_buffer **buffer,
    uint64_t *offset, uint64_t *size_bytes, openagc_graphics_image **image);
/* Graphics plans copy the vertex stage input mask. Compute plans report 0. */
openagc_result openagc_shader_pipeline_plan_vertex_input(
    const openagc_shader_pipeline_plan *plan, uint32_t *input_mask);
openagc_result openagc_shader_pipeline_plan_destroy(openagc_shader_pipeline_plan *plan);

#ifdef __cplusplus
}
#endif

#endif
