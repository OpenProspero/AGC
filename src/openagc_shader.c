/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"
#include "openagc_gpu_internal.h"
#include "openagc_sha256.h"
#include "openagc_shader_internal.h"

#include <stdlib.h>
#include <string.h>

#define OPENAGC_SHADER_MAX_CODE_BYTES 65536u
#define OPENAGC_SHADER_MAX_BINDINGS 8u
#define OPENAGC_SHADER_MAX_ARTIFACTS 64u
#define OPENAGC_SHADER_MAX_PIPELINE_PLANS 32u
#define OPENAGC_SHADER_MAX_WORKGROUP_INVOCATIONS 1024u
#define OPENAGC_SHADER_MAX_UNIFORM_BYTES 4096u
#define OPENAGC_SHADER_VARYING_MASK 0xffu

struct openagc_shader_artifact {
    openagc_gpu_device *device;
    uint8_t *code;
    openagc_shader_binding_decl *bindings;
    uint8_t code_sha256[32];
    uint32_t code_size;
    uint32_t binding_count;
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
};

struct openagc_shader_pipeline_plan {
    openagc_gpu_device *device;
    openagc_shader_artifact *vertex;
    openagc_shader_artifact *pixel;
    openagc_shader_artifact *compute;
    openagc_graphics_image *color_target;
    openagc_shader_resource_binding *resources;
    openagc_shader_pipeline_kind kind;
    openagc_graphics_format color_format;
    uint32_t target_image_id;
    uint32_t resource_count;
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
    const openagc_shader_binding_decl *bindings)
{
    uint32_t i;

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
    return OPENAGC_OK;
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
        (snapshot.binding_count == 0u && snapshot.bindings != NULL)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE) {
        if (snapshot.compiler_metadata_version != 0u ||
            snapshot.toolchain_release_major != 0u ||
            snapshot.toolchain_release_minor != 0u ||
            snapshot.toolchain_release_patch != 0u ||
            !openagc_shader_revision_is_zero(snapshot.compiler_source_revision) ||
            !openagc_shader_digest_is_zero(snapshot.compiler_binary_sha256)) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
    } else if (snapshot.compiler_metadata_version == 0u ||
               snapshot.toolchain_release_major != 0u ||
               snapshot.toolchain_release_minor != 3u ||
               snapshot.toolchain_release_patch != 0u ||
               memcmp(snapshot.compiler_source_revision,
                      OPENAGC_SHADER_PINNED_PSBC_REVISION,
                      sizeof(snapshot.compiler_source_revision)) != 0 ||
               openagc_shader_digest_is_zero(snapshot.compiler_binary_sha256)) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (device->shader_artifact_count >= OPENAGC_SHADER_MAX_ARTIFACTS) {
        return OPENAGC_ERROR_CAPACITY;
    }

    artifact = (openagc_shader_artifact *)calloc(1u, sizeof(*artifact));
    if (artifact == NULL) {
        return OPENAGC_ERROR_OUT_OF_MEMORY;
    }
    artifact->code = (uint8_t *)malloc((size_t)snapshot.code_size);
    if (snapshot.binding_count != 0u) {
        artifact->bindings = (openagc_shader_binding_decl *)calloc(
            (size_t)snapshot.binding_count, sizeof(*artifact->bindings));
    }
    if (artifact->code == NULL ||
        (snapshot.binding_count != 0u && artifact->bindings == NULL)) {
        result = OPENAGC_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    memcpy(artifact->code, snapshot.code, snapshot.code_size);
    if (snapshot.binding_count != 0u) {
        memcpy(artifact->bindings, snapshot.bindings,
               (size_t)snapshot.binding_count * sizeof(*artifact->bindings));
    }
    result = openagc_shader_validate_reflection(&snapshot, artifact->bindings);
    if (result != OPENAGC_OK) {
        goto fail;
    }
    openagc_sha256(artifact->code, snapshot.code_size, actual_digest);
    if (memcmp(actual_digest, snapshot.code_sha256, sizeof(actual_digest)) != 0) {
        result = OPENAGC_ERROR_INTEGRITY;
        goto fail;
    }
    if (snapshot.compiler == OPENAGC_SHADER_COMPILER_OPENGNM_PSBC) {
        result = OPENAGC_ERROR_NOT_READY;
        goto fail;
    }
    artifact->device = device;
    artifact->code_size = snapshot.code_size;
    artifact->binding_count = snapshot.binding_count;
    artifact->stage = snapshot.stage;
    artifact->vertex_position_written = snapshot.vertex_position_written;
    artifact->varying_input_mask = snapshot.varying_input_mask;
    artifact->varying_output_mask = snapshot.varying_output_mask;
    artifact->color_output_mask = snapshot.color_output_mask;
    artifact->color_export_format = snapshot.color_export_format;
    artifact->workgroup_x = snapshot.workgroup_x;
    artifact->workgroup_y = snapshot.workgroup_y;
    artifact->workgroup_z = snapshot.workgroup_z;
    memcpy(artifact->code_sha256, actual_digest, sizeof(actual_digest));
    device->shader_artifact_count++;
    *out_artifact = artifact;
    return OPENAGC_OK;

fail:
    free(artifact->bindings);
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
    info->compiler_verified = 0u;
    info->gpu_executable = 0u;
    memcpy(info->code_sha256, artifact->code_sha256, sizeof(info->code_sha256));
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

openagc_result openagc_shader_artifact_destroy(openagc_shader_artifact *artifact)
{
    if (artifact == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (artifact->pipeline_references != 0u) {
        return OPENAGC_ERROR_BUSY;
    }
    artifact->device->shader_artifact_count--;
    free(artifact->bindings);
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

openagc_result openagc_shader_pipeline_plan_create_host(
    openagc_gpu_device *device, const openagc_shader_pipeline_desc *desc,
    openagc_shader_pipeline_plan **out_plan)
{
    openagc_shader_pipeline_plan *plan;
    openagc_shader_pipeline_desc snapshot;
    openagc_shader_resource_binding resources[OPENAGC_SHADER_MAX_BINDINGS] = { 0 };
    uint32_t required[OPENAGC_SHADER_MAX_BINDINGS] = { 0u };
    uint32_t required_count = 0u;
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
    plan->device = device;
    plan->kind = snapshot.kind;
    plan->vertex = snapshot.vertex;
    plan->pixel = snapshot.pixel;
    plan->compute = snapshot.compute;
    plan->color_target = snapshot.color_target;
    plan->resource_count = snapshot.resource_count;
    plan->target_image_id = target_id;
    if (snapshot.kind == OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        plan->color_format = snapshot.pixel->color_export_format;
        snapshot.vertex->pipeline_references++;
        snapshot.pixel->pipeline_references++;
        openagc_graphics_shader_target_retain(snapshot.color_target);
    } else {
        snapshot.compute->pipeline_references++;
    }
    for (i = 0u; i < snapshot.resource_count; ++i) {
        openagc_gpu_shader_buffer_retain(plan->resources[i].buffer);
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
    info->color_format = plan->color_format;
    info->target_image_id = plan->target_image_id;
    info->compiler_verified = 0u;
    info->gpu_executable = 0u;
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
    if (plan->kind == OPENAGC_SHADER_PIPELINE_GRAPHICS) {
        plan->vertex->pipeline_references--;
        plan->pixel->pipeline_references--;
        openagc_graphics_shader_target_release(plan->color_target);
    } else {
        plan->compute->pipeline_references--;
    }
    plan->device->shader_pipeline_plan_count--;
    free(plan->resources);
    free(plan);
    return OPENAGC_OK;
}
