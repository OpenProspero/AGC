/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"
#include "openagc/psbc_metadata.h"
#include "openagc/store_const_code.h"
#include "openagc/store_span_code.h"
#include "openagc_gpu_internal.h"
#include "openagc_sha256.h"
#include "openagc_shader_internal.h"

#include <stdlib.h>
#include <string.h>

#define OPENAGC_SHADER_MAX_CODE_BYTES 65536u
#define OPENAGC_SHADER_MAX_METADATA_BYTES 65536u
#define OPENAGC_SHADER_MAX_BINDINGS 8u
#define OPENAGC_SHADER_MAX_ARTIFACTS 64u
#define OPENAGC_SHADER_MAX_PIPELINE_PLANS 32u
#define OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS 1024u
#define OPENAGC_SHADER_MAX_UNIFORM_BYTES 4096u
#define OPENAGC_SHADER_VARYING_MASK 0xffu

struct openagc_shader_artifact {
    openagc_gpu_device *device;
    uint8_t *code;
    uint8_t *compiler_metadata;
    openagc_shader_binding_decl *bindings;
    openagc_shader_texture_decl *textures;
    uint8_t code_sha256[32];
    uint32_t code_size;
    uint32_t compiler_metadata_size;
    uint32_t binding_count;
    uint32_t texture_count;
    uint32_t pipeline_references;
    openagc_shader_stage stage;
    uint32_t vertex_position_written;
    uint32_t varying_input_mask;
    uint32_t varying_output_mask;
    uint32_t color_output_mask;
    openagc_graphics_format color_export_format;
    uint32_t workgroup_x;
    uint32_t workgroup_y;
    uint32_t workgroup_z;
    uint32_t host_store_const;
    uint32_t host_store_span;
    /* 1 when intake accepted a pin-checked OPENGNM_PSBC envelope (still not executable). */
    uint32_t psbc_envelope;
};

struct openagc_shader_pipeline_plan {
    openagc_gpu_device *device;
    openagc_shader_artifact *vertex;
    openagc_shader_artifact *pixel;
    openagc_shader_artifact *compute;
    openagc_graphics_image *color_target;
    openagc_shader_resource_binding *resources;
    openagc_shader_texture_binding *textures;
    openagc_shader_pipeline_kind kind;
    openagc_graphics_format color_format;
    uint32_t target_image_id;
    uint32_t resource_count;
    uint32_t texture_count;
};

static int openagc_shader_digest_is_zero(const uint8_t digest[32])
{
    uint32_t i;

    for (i = 0u; i < 32u; ++i) {
        if (digest[i] != 0u) {
            return 0;
        }
    }
    return 1;
}

static int openagc_shader_revision_is_zero(const char revision[41])
{
    uint32_t i;

    for (i = 0u; i < 41u; ++i) {
        if (revision[i] != '\0') {
            return 0;
        }
    }
    return 1;
}

static openagc_result openagc_shader_validate_reflection(
    const openagc_shader_artifact_desc *desc,
    const openagc_shader_binding_decl *bindings,
    const openagc_shader_texture_decl *textures)
{
    uint32_t i;
    uint32_t j;

    if (desc->varying_input_mask > OPENAGC_SHADER_VARYING_MASK ||
        desc->varying_output_mask > OPENAGC_SHADER_VARYING_MASK ||
        desc->vertex_position_written > 1u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (i = 0u; i < desc->binding_count; ++i) {
        const openagc_shader_binding_decl *binding = &bindings[i];
        if (binding->set != 0u || binding->binding >= OPENAGC_SHADER_MAX_BINDINGS ||
            (i != 0u && binding->binding <= bindings[i - 1u].binding) ||
            binding->min_bytes < 4u ||
            binding->min_bytes > OPENAGC_SHADER_MAX_UNIFORM_BYTES ||
            (binding->min_bytes & 3u) != 0u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        if (binding->kind != OPENAGC_SHADER_BINDING_UNIFORM_BUFFER) {
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        }
    }
    for (i = 0u; i < desc->texture_count; ++i) {
        const openagc_shader_texture_decl *texture = &textures[i];
        if (texture->set != 0u || texture->binding >= OPENAGC_SHADER_MAX_BINDINGS ||
            (i != 0u && texture->binding <= textures[i - 1u].binding) ||
            (texture->format != OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
             texture->format != OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM)) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
        for (j = 0u; j < desc->binding_count; ++j) {
            if (bindings[j].binding == texture->binding) {
                return OPENAGC_ERROR_OUT_OF_RANGE;
            }
        }
    }

    if (desc->stage == OPENAGC_SHADER_STAGE_VERTEX) {
        if (desc->vertex_position_written != 1u ||
            desc->color_output_mask != 0u || desc->color_export_format != 0u ||
            desc->workgroup_x != 0u || desc->workgroup_y != 0u ||
            desc->workgroup_z != 0u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    } else if (desc->stage == OPENAGC_SHADER_STAGE_PIXEL) {
        if (desc->vertex_position_written != 0u ||
            desc->varying_output_mask != 0u ||
            desc->color_output_mask != 1u ||
            (desc->color_export_format != OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
             desc->color_export_format != OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM) ||
            desc->workgroup_x != 0u || desc->workgroup_y != 0u ||
            desc->workgroup_z != 0u) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    } else if (desc->stage == OPENAGC_SHADER_STAGE_COMPUTE) {
        if (desc->vertex_position_written != 0u ||
            desc->varying_input_mask != 0u || desc->varying_output_mask != 0u ||
            desc->color_output_mask != 0u || desc->color_export_format != 0u ||
            desc->workgroup_x == 0u || desc->workgroup_y == 0u ||
            desc->workgroup_z == 0u ||
            desc->workgroup_x > OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS ||
            desc->workgroup_y > OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS ||
            desc->workgroup_z > OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS ||
            (uint64_t)desc->workgroup_x * desc->workgroup_y * desc->workgroup_z >
                OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS) {
            return OPENAGC_ERROR_OUT_OF_RANGE;
        }
    } else {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    return OPENAGC_OK;
}

openagc_result openagc_shader_get_capabilities(
    const openagc_gpu_device *device, openagc_shader_capabilities *capabilities)
{
    if (device == NULL || capabilities == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (capabilities->struct_size != sizeof(*capabilities)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    capabilities->target = OPENAGC_SHADER_TARGET_GFX1013;
    capabilities->max_code_bytes = OPENAGC_SHADER_MAX_CODE_BYTES;
    capabilities->max_bindings = OPENAGC_SHADER_MAX_BINDINGS;
    capabilities->max_artifacts = OPENAGC_SHADER_MAX_ARTIFACTS;
    capabilities->max_pipeline_plans = OPENAGC_SHADER_MAX_PIPELINE_PLANS;
    capabilities->max_workgroup_invocations = OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS;
    capabilities->structural_intake = 1u;
    capabilities->compiler_available = 0u;
    capabilities->gpu_execution = 0u;
    capabilities->host_compute_simulation = 1u;
    return OPENAGC_OK;
}

static openagc_result openagc_shader_psbc_metadata_matches(const uint8_t *metadata, uint32_t size,
                                                           openagc_shader_stage stage,
                                                           uint32_t code_size)
{
    openagc_psbc_reflection reflection;
    uint32_t expected_stage;
    openagc_result result;

    if (stage == OPENAGC_SHADER_STAGE_VERTEX) {
        expected_stage = 1u;
    } else if (stage == OPENAGC_SHADER_STAGE_PIXEL) {
        expected_stage = 5u;
    } else {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    result = openagc_psbc_metadata_parse_reflection(metadata, size, &reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_reflection_validate(&reflection, expected_stage, code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (stage == OPENAGC_SHADER_STAGE_VERTEX && reflection.has_linkage == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (stage == OPENAGC_SHADER_STAGE_PIXEL && reflection.has_linkage != 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_OK;
}

static openagc_result openagc_shader_psbc_envelope_matches(
    const openagc_shader_artifact_desc *desc)
{
    openagc_psbc_reflection reflection;
    openagc_result result;

    result = openagc_shader_psbc_metadata_matches(desc->compiler_metadata,
                                                 desc->compiler_metadata_size, desc->stage,
                                                 desc->code_size);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_metadata_parse_reflection(desc->compiler_metadata,
                                                   desc->compiler_metadata_size, &reflection);
    if (result != OPENAGC_OK) {
        return result;
    }
    return openagc_psbc_reflection_check_artifact_desc(&reflection, desc);
}

openagc_result openagc_shader_artifact_intake_host(
    openagc_gpu_device *device, const openagc_shader_artifact_desc *desc,
    openagc_shader_artifact **out_artifact)
{
    openagc_shader_artifact_desc snapshot;
    openagc_shader_artifact *artifact;
    uint8_t actual_digest[32];
    openagc_result result;

    if (out_artifact == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_artifact = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    snapshot = *desc;
    if (snapshot.target != OPENAGC_SHADER_TARGET_GFX1013) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (snapshot.compiler != OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE &&
        snapshot.compiler != OPENAGC_SHADER_COMPILER_OPENGNM_PSBC) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (snapshot.code == NULL || snapshot.code_size < 4u ||
        snapshot.code_size > OPENAGC_SHADER_MAX_CODE_BYTES ||
        (snapshot.code_size & 3u) != 0u ||
        snapshot.binding_count > OPENAGC_SHADER_MAX_BINDINGS ||
        (snapshot.binding_count != 0u && snapshot.bindings == NULL) ||
        (snapshot.binding_count == 0u && snapshot.bindings != NULL) ||
        snapshot.texture_count > OPENAGC_SHADER_MAX_BINDINGS ||
        (snapshot.texture_count != 0u && snapshot.textures == NULL) ||
        (snapshot.texture_count == 0u && snapshot.textures != NULL)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE) {
        if (snapshot.compiler_metadata_version != 0u ||
            snapshot.toolchain_release_major != 0u ||
            snapshot.toolchain_release_minor != 0u ||
            snapshot.toolchain_release_patch != 0u ||
            !openagc_shader_revision_is_zero(snapshot.compiler_source_revision) ||
            !openagc_shader_digest_is_zero(snapshot.compiler_binary_sha256) ||
            snapshot.compiler_metadata != NULL || snapshot.compiler_metadata_size != 0u) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
    } else if (snapshot.compiler_metadata == NULL ||
               snapshot.compiler_metadata_size < 2u ||
               snapshot.compiler_metadata_size > OPENAGC_SHADER_MAX_METADATA_BYTES ||
               snapshot.compiler_metadata_version !=
                   OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION ||
               snapshot.toolchain_release_major != 0u ||
               snapshot.toolchain_release_minor != 3u ||
               snapshot.toolchain_release_patch != 0u ||
               memcmp(snapshot.compiler_source_revision,
                      OPENAGC_SHADER_PINNED_PSBC_REVISION,
                      sizeof(snapshot.compiler_source_revision)) != 0 ||
               openagc_shader_digest_is_zero(snapshot.compiler_binary_sha256)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    } else {
        uint8_t pinned_digest[32];

        if (openagc_psbc_decode_sha256_hex(OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256,
                                           pinned_digest) == 0 ||
            memcmp(snapshot.compiler_binary_sha256, pinned_digest, 32) != 0) {
            return OPENAGC_ERROR_INTEGRITY;
        }
    }
    if (device->shader_artifact_count >= OPENAGC_SHADER_MAX_ARTIFACTS) {
        return OPENAGC_ERROR_CAPACITY;
    }

    artifact = (openagc_shader_artifact *)calloc(1u, sizeof(*artifact));
    if (artifact == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    artifact->code = (uint8_t *)malloc((size_t)snapshot.code_size);
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC) {
        artifact->compiler_metadata =
            (uint8_t *)malloc((size_t)snapshot.compiler_metadata_size);
    }
    if (snapshot.binding_count != 0u) {
        artifact->bindings = (openagc_shader_binding_decl *)calloc(
            (size_t)snapshot.binding_count, sizeof(*artifact->bindings));
    }
    if (snapshot.texture_count != 0u) {
        artifact->textures = (openagc_shader_texture_decl *)calloc(
            (size_t)snapshot.texture_count, sizeof(*artifact->textures));
    }
    if (artifact->code == NULL ||
        (snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC &&
         artifact->compiler_metadata == NULL) ||
        (snapshot.binding_count != 0u && artifact->bindings == NULL) ||
        (snapshot.texture_count != 0u && artifact->textures == NULL)) {
        result = OPENAGC_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    memcpy(artifact->code, snapshot.code, snapshot.code_size);
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC) {
        memcpy(artifact->compiler_metadata, snapshot.compiler_metadata,
               snapshot.compiler_metadata_size);
        artifact->compiler_metadata_size = snapshot.compiler_metadata_size;
    }
    if (snapshot.binding_count != 0u) {
        memcpy(artifact->bindings, snapshot.bindings,
               (size_t)snapshot.binding_count * sizeof(*artifact->bindings));
    }
    if (snapshot.texture_count != 0u) {
        memcpy(artifact->textures, snapshot.textures,
               (size_t)snapshot.texture_count * sizeof(*artifact->textures));
    }
    result = openagc_shader_validate_reflection(&snapshot, artifact->bindings,
                                                artifact->textures);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    openagc_sha256(artifact->code, snapshot.code_size, actual_digest);
    if (memcmp(actual_digest, snapshot.code_sha256, sizeof(actual_digest)) != 0) {
        result = OPENAGC_ERROR_INTEGRITY;
        goto fail;
    }
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC) {
        result = openagc_shader_psbc_envelope_matches(&snapshot);
        if (result != OPENAGC_OK) {
            goto fail;
        }
    }
    artifact->device = device;
    artifact->code_size = snapshot.code_size;
    artifact->binding_count = snapshot.binding_count;
    artifact->texture_count = snapshot.texture_count;
    artifact->stage = snapshot.stage;
    artifact->vertex_position_written = snapshot.vertex_position_written;
    artifact->varying_input_mask = snapshot.varying_input_mask;
    artifact->varying_output_mask = snapshot.varying_output_mask;
    artifact->color_output_mask = snapshot.color_output_mask;
    artifact->color_export_format = snapshot.color_export_format;
    artifact->workgroup_x = snapshot.workgroup_x;
    artifact->workgroup_y = snapshot.workgroup_y;
    artifact->workgroup_z = snapshot.workgroup_z;
    artifact->host_store_const = 0u;
    artifact->host_store_span = 0u;
    artifact->psbc_envelope = snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC ? 1u : 0u;
    if (snapshot.stage == OPENAGC_SHADER_STAGE_COMPUTE &&
        openagc_store_const_code_matches(artifact->code, artifact->code_size) != 0 &&
        snapshot.binding_count >= 1u && snapshot.bindings[0].min_bytes >= 4u) {
        artifact->host_store_const = 1u;
    } else if (snapshot.stage == OPENAGC_SHADER_STAGE_COMPUTE &&
               openagc_store_span_code_matches(artifact->code, artifact->code_size) != 0 &&
               snapshot.binding_count >= 1u &&
               snapshot.bindings[0].min_bytes >= OPENAGC_STORE_SPAN_BYTES) {
        artifact->host_store_span = 1u;
    }
    memcpy(artifact->code_sha256, actual_digest, sizeof(actual_digest));
    device->shader_artifact_count++;
    *out_artifact = artifact;
    return OPENAGC_OK;

fail:
    free(artifact->textures);
    free(artifact->bindings);
    free(artifact->compiler_metadata);
    free(artifact->code);
    free(artifact);
    return result;
}

openagc_result openagc_shader_artifact_get_info(
    const openagc_shader_artifact *artifact, openagc_shader_artifact_info *info)
{
    if (artifact == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->stage = artifact->stage;
    info->target = OPENAGC_SHADER_TARGET_GFX1013;
    info->code_size = artifact->code_size;
    info->binding_count = artifact->binding_count;
    info->texture_count = artifact->texture_count;
    info->compiler_verified = 0u;
    info->gpu_executable = 0u;
    info->host_store_const = artifact->host_store_const;
    info->host_store_span = artifact->host_store_span;
    info->psbc_envelope = artifact->psbc_envelope;
    memcpy(info->code_sha256, artifact->code_sha256, sizeof(info->code_sha256));
    return OPENAGC_OK;
}

openagc_result openagc_shader_artifact_get_compiler_metadata(
    const openagc_shader_artifact *artifact, const uint8_t **out_metadata,
    uint32_t *out_size)
{
    if (artifact == NULL || out_metadata == NULL || out_size == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_metadata = NULL;
    *out_size = 0u;
    if (artifact->psbc_envelope == 0u || artifact->compiler_metadata == NULL ||
        artifact->compiler_metadata_size == 0u) {
        return OPENAGC_ERROR_NOT_READY;
    }
    *out_metadata = artifact->compiler_metadata;
    *out_size = artifact->compiler_metadata_size;
    return OPENAGC_OK;
}

openagc_result openagc_shader_artifact_get_binding(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_binding_decl *out_binding)
{
    if (artifact == NULL || out_binding == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= artifact->binding_count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    *out_binding = artifact->bindings[index];
    return OPENAGC_OK;
}

openagc_result openagc_shader_artifact_get_texture(
    const openagc_shader_artifact *artifact, uint32_t index,
    openagc_shader_texture_decl *out_texture)
{
    if (artifact == NULL || out_texture == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= artifact->texture_count) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    *out_texture = artifact->textures[index];
    return OPENAGC_OK;
}

openagc_result openagc_shader_artifact_destroy(openagc_shader_artifact *artifact)
{
    if (artifact == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (artifact->pipeline_references != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    artifact->device->shader_artifact_count--;
    free(artifact->textures);
    free(artifact->bindings);
    free(artifact->compiler_metadata);
    free(artifact->code);
    free(artifact);
    return OPENAGC_OK;
}

openagc_result openagc_shader_artifact_require_compiler(
    const openagc_shader_artifact *artifact)
{
    return artifact == NULL ? OPENAGC_ERROR_INVALID_ARGUMENT
                            : OPENAGC_ERROR_NOT_READY;
}

static void openagc_shader_require_bindings(
    const openagc_shader_artifact *artifact, uint32_t required[8])
{
    uint32_t i;

    for (i = 0u; i < artifact->binding_count; ++i) {
        uint32_t slot = artifact->bindings[i].binding;
        if (required[slot] < artifact->bindings[i].min_bytes) {
            required[slot] = artifact->bindings[i].min_bytes;
        }
    }
}

static openagc_result openagc_shader_require_textures(
    const openagc_shader_artifact *artifact,
    openagc_graphics_format required[OPENAGC_SHADER_MAX_BINDINGS])
{
    uint32_t i;

    for (i = 0u; i < artifact->texture_count; ++i) {
        uint32_t slot = artifact->textures[i].binding;
        if (required[slot] != 0u && required[slot] != artifact->textures[i].format) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        required[slot] = artifact->textures[i].format;
    }
    return OPENAGC_OK;
}

openagc_result openagc_shader_pipeline_plan_create_host(
    openagc_gpu_device *device, const openagc_shader_pipeline_desc *desc,
    openagc_shader_pipeline_plan **out_plan)
{
    openagc_shader_pipeline_plan *plan;
    openagc_shader_pipeline_desc snapshot;
    openagc_shader_resource_binding resources[OPENAGC_SHADER_MAX_BINDINGS] = { 0 };
    openagc_shader_texture_binding textures[OPENAGC_SHADER_MAX_BINDINGS] = { 0 };
    uint32_t required[OPENAGC_SHADER_MAX_BINDINGS] = { 0u };
    openagc_graphics_format required_textures[OPENAGC_SHADER_MAX_BINDINGS] = { 0u };
    uint32_t required_count = 0u;
    uint32_t required_texture_count = 0u;
    uint32_t target_id = 0u;
    uint32_t slot;
    uint32_t index;
    uint32_t i;
    openagc_result result;

    if (out_plan == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_plan = NULL;
    if (device == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->struct_size != sizeof(*desc) ||
        desc->api_version != OPENAGC_SHADER_API_VERSION) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    snapshot = *desc;
    if (snapshot.resource_count > OPENAGC_SHADER_MAX_BINDINGS ||
        (snapshot.resource_count != 0u && snapshot.resources == NULL) ||
        (snapshot.resource_count == 0u && snapshot.resources != NULL)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (snapshot.resource_count != 0u) {
        memcpy(resources, snapshot.resources,
               (size_t)snapshot.resource_count * sizeof(resources[0]));
    }
    if (snapshot.texture_count > OPENAGC_SHADER_MAX_BINDINGS ||
        (snapshot.texture_count != 0u && snapshot.textures == NULL) ||
        (snapshot.texture_count == 0u && snapshot.textures != NULL)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (snapshot.texture_count != 0u) {
        memcpy(textures, snapshot.textures,
               (size_t)snapshot.texture_count * sizeof(textures[0]));
    }
    if (snapshot.kind == OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        if (snapshot.vertex == NULL || snapshot.pixel == NULL ||
            snapshot.compute != NULL || snapshot.color_target == NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (snapshot.vertex->device != device || snapshot.pixel->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        if (snapshot.vertex->stage != OPENAGC_SHADER_STAGE_VERTEX ||
            snapshot.pixel->stage != OPENAGC_SHADER_STAGE_PIXEL ||
            (snapshot.pixel->varying_input_mask &
             ~snapshot.vertex->varying_output_mask) != 0u) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        result = openagc_graphics_shader_target_validate(
            device, snapshot.color_target, snapshot.pixel->color_export_format, &target_id);
        if (result != OPENAGC_OK) {
            return result;
        }
        openagc_shader_require_bindings(snapshot.vertex, required);
        openagc_shader_require_bindings(snapshot.pixel, required);
        result = openagc_shader_require_textures(snapshot.vertex, required_textures);
        if (result != OPENAGC_OK) {
            return result;
        }
        result = openagc_shader_require_textures(snapshot.pixel, required_textures);
        if (result != OPENAGC_OK) {
            return result;
        }
    } else if (snapshot.kind == OPENAGC_SHADER_PIPELINE_COMPUTE) {
        if (snapshot.compute == NULL || snapshot.vertex != NULL ||
            snapshot.pixel != NULL || snapshot.color_target != NULL) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (snapshot.compute->device != device) {
            return OPENAGC_ERROR_OWNERSHIP;
        }
        if (snapshot.compute->stage != OPENAGC_SHADER_STAGE_COMPUTE) {
            return OPENAGC_ERROR_BAD_STATE;
        }
        openagc_shader_require_bindings(snapshot.compute, required);
        result = openagc_shader_require_textures(snapshot.compute, required_textures);
        if (result != OPENAGC_OK) {
            return result;
        }
    } else {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    for (slot = 0u; slot < OPENAGC_SHADER_MAX_BINDINGS; ++slot) {
        required_count += required[slot] != 0u ? 1u : 0u;
    }
    if (snapshot.resource_count != required_count) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    index = 0u;
    for (slot = 0u; slot < OPENAGC_SHADER_MAX_BINDINGS; ++slot) {
        if (required[slot] != 0u) {
            const openagc_shader_resource_binding *resource = &resources[index];
            if (resource->set != 0u || resource->binding != slot ||
                resource->buffer == NULL) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            for (i = 0u; i < index; ++i) {
                if (resources[i].buffer == resource->buffer) {
                    return OPENAGC_ERROR_INVALID_ARGUMENT;
                }
            }
            result = openagc_gpu_shader_buffer_validate(
                device, resource->buffer, resource->offset,
                resource->size_bytes, required[slot]);
            if (result != OPENAGC_OK) {
                return result;
            }
            index++;
        }
    }
    for (slot = 0u; slot < OPENAGC_SHADER_MAX_BINDINGS; ++slot) {
        required_texture_count += required_textures[slot] != 0u ? 1u : 0u;
    }
    if (snapshot.texture_count != required_texture_count) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    index = 0u;
    for (slot = 0u; slot < OPENAGC_SHADER_MAX_BINDINGS; ++slot) {
        if (required_textures[slot] != 0u) {
            const openagc_shader_texture_binding *texture = &textures[index];
            if (texture->set != 0u || texture->binding != slot ||
                texture->image == NULL) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            for (i = 0u; i < index; ++i) {
                if (textures[i].image == texture->image) {
                    return OPENAGC_ERROR_INVALID_ARGUMENT;
                }
            }
            result = openagc_graphics_shader_texture_validate(
                device, texture->image, required_textures[slot]);
            if (result != OPENAGC_OK) {
                return result;
            }
            index++;
        }
    }
    if (device->shader_pipeline_plan_count >= OPENAGC_SHADER_MAX_PIPELINE_PLANS) {
        return OPENAGC_ERROR_CAPACITY;
    }
    if ((snapshot.vertex != NULL && snapshot.vertex->pipeline_references == UINT32_MAX) ||
        (snapshot.pixel != NULL && snapshot.pixel->pipeline_references == UINT32_MAX) ||
        (snapshot.compute != NULL && snapshot.compute->pipeline_references == UINT32_MAX)) {
        return OPENAGC_ERROR_OVERFLOW;
    }

    plan = (openagc_shader_pipeline_plan *)calloc(1u, sizeof(*plan));
    if (plan == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    if (snapshot.resource_count != 0u) {
        plan->resources = (openagc_shader_resource_binding *)calloc(
            (size_t)snapshot.resource_count, sizeof(*plan->resources));
        if (plan->resources == NULL) {
            free(plan);
            return OPENAGC_ERROR_OUT_OF_MEMORY;
        }
        memcpy(plan->resources, resources,
               (size_t)snapshot.resource_count * sizeof(*plan->resources));
    }
    if (snapshot.texture_count != 0u) {
        plan->textures = (openagc_shader_texture_binding *)calloc(
            (size_t)snapshot.texture_count, sizeof(*plan->textures));
        if (plan->textures == NULL) {
            free(plan->resources);
            free(plan);
            return OPENAGC_ERROR_OUT_OF_MEMORY;
        }
        memcpy(plan->textures, textures,
               (size_t)snapshot.texture_count * sizeof(*plan->textures));
    }
    plan->device = device;
    plan->kind = snapshot.kind;
    plan->vertex = snapshot.vertex;
    plan->pixel = snapshot.pixel;
    plan->compute = snapshot.compute;
    plan->color_target = snapshot.color_target;
    plan->resource_count = snapshot.resource_count;
    plan->texture_count = snapshot.texture_count;
    plan->target_image_id = target_id;
    if (snapshot.kind == OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        plan->color_format = snapshot.pixel->color_export_format;
        snapshot.vertex->pipeline_references++;
        snapshot.pixel->pipeline_references++;
        openagc_graphics_shader_image_retain(snapshot.color_target);
    } else {
        snapshot.compute->pipeline_references++;
    }
    for (i = 0u; i < snapshot.resource_count; ++i) {
        openagc_gpu_shader_buffer_retain(plan->resources[i].buffer);
    }
    for (i = 0u; i < snapshot.texture_count; ++i) {
        openagc_graphics_shader_image_retain(plan->textures[i].image);
    }
    device->shader_pipeline_plan_count++;
    *out_plan = plan;
    return OPENAGC_OK;
}

openagc_result openagc_shader_pipeline_plan_get_info(
    const openagc_shader_pipeline_plan *plan, openagc_shader_pipeline_info *info)
{
    if (plan == NULL || info == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (info->struct_size != sizeof(*info)) {
        return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    }
    info->kind = plan->kind;
    info->stage_count = plan->kind == OPENAGC_SHADER_PIPELINE_GRAPHICS ? 2u : 1u;
    info->resource_count = plan->resource_count;
    info->texture_count = plan->texture_count;
    info->color_format = plan->color_format;
    info->target_image_id = plan->target_image_id;
    info->compiler_verified = 0u;
    info->gpu_executable = 0u;
    return OPENAGC_OK;
}

openagc_result openagc_shader_pipeline_plan_slot(
    const openagc_shader_pipeline_plan *plan, uint32_t slot, openagc_gpu_buffer **buffer,
    uint64_t *offset, uint64_t *size_bytes, openagc_graphics_image **image)
{
    uint32_t index;

    if (buffer == NULL || offset == NULL || size_bytes == NULL || image == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *buffer = NULL;
    *offset = 0u;
    *size_bytes = 0u;
    *image = NULL;
    if (plan == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (slot >= OPENAGC_SHADER_MAX_BINDINGS) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    for (index = 0u; index < plan->resource_count; ++index) {
        if (plan->resources[index].binding == slot) {
            *buffer = plan->resources[index].buffer;
            *offset = plan->resources[index].offset;
            *size_bytes = plan->resources[index].size_bytes;
            break;
        }
    }
    for (index = 0u; index < plan->texture_count; ++index) {
        if (plan->textures[index].binding == slot) {
            *image = plan->textures[index].image;
            break;
        }
    }
    return OPENAGC_OK;
}

openagc_result openagc_shader_pipeline_plan_vertex_input(
    const openagc_shader_pipeline_plan *plan, uint32_t *input_mask)
{
    if (plan == NULL || input_mask == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *input_mask = plan->vertex != NULL ? plan->vertex->varying_input_mask : 0u;
    return OPENAGC_OK;
}

openagc_result openagc_shader_pipeline_plan_destroy(openagc_shader_pipeline_plan *plan)
{
    uint32_t i;

    if (plan == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < plan->resource_count; ++i) {
        openagc_gpu_shader_buffer_release(plan->resources[i].buffer);
    }
    for (i = 0u; i < plan->texture_count; ++i) {
        openagc_graphics_shader_image_release(plan->textures[i].image);
    }
    if (plan->kind == OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        plan->vertex->pipeline_references--;
        plan->pixel->pipeline_references--;
        openagc_graphics_shader_image_release(plan->color_target);
    } else {
        plan->compute->pipeline_references--;
    }
    plan->device->shader_pipeline_plan_count--;
    free(plan->textures);
    free(plan->resources);
    free(plan);
    return OPENAGC_OK;
}
