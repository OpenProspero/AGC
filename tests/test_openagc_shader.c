/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/shader.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define EXPECT(expression, expected) do { \
    openagc_result actual = (expression); \
    if (actual != (expected)) { \
        fprintf(stderr, "%s:%d: %s returned %s; expected %s\n", \
                __FILE__, __LINE__, #expression, openagc_result_string(actual), \
                openagc_result_string(expected)); \
        return 1; \
    } \
} while (0)

/* "test" and repeated 'a' are structural fixtures, NOT compiled shaders. */
static const uint8_t test_hash[32] = {
    0x9fu, 0x86u, 0xd0u, 0x81u, 0x88u, 0x4cu, 0x7du, 0x65u,
    0x9au, 0x2fu, 0xeau, 0xa0u, 0xc5u, 0x5au, 0xd0u, 0x15u,
    0xa3u, 0xbfu, 0x4fu, 0x1bu, 0x2bu, 0x0bu, 0x82u, 0x2cu,
    0xd1u, 0x5du, 0x6cu, 0x15u, 0xb0u, 0xf0u, 0x0au, 0x08u
};

static const uint8_t hash_56_a[32] = {
    0xb3u, 0x54u, 0x39u, 0xa4u, 0xacu, 0x6fu, 0x09u, 0x48u,
    0xb6u, 0xd6u, 0xf9u, 0xe3u, 0xc6u, 0xafu, 0x0fu, 0x5fu,
    0x59u, 0x0cu, 0xe2u, 0x0fu, 0x1bu, 0xdeu, 0x70u, 0x90u,
    0xefu, 0x79u, 0x70u, 0x68u, 0x6eu, 0xc6u, 0x73u, 0x8au
};

static const uint8_t hash_64_a[32] = {
    0xffu, 0xe0u, 0x54u, 0xfeu, 0x7au, 0xe0u, 0xcbu, 0x6du,
    0xc6u, 0x5cu, 0x3au, 0xf9u, 0xb6u, 0x1du, 0x52u, 0x09u,
    0xf4u, 0x39u, 0x85u, 0x1du, 0xb4u, 0x3du, 0x0bu, 0xa5u,
    0x99u, 0x73u, 0x37u, 0xdfu, 0x15u, 0x46u, 0x68u, 0xebu
};

static int make_device(openagc_context **context, openagc_gpu_device **device)
{
    openagc_context_desc context_desc =
        OPENAGC_CONTEXT_DESC_INIT(OPENAGC_BACKEND_HOST_REFERENCE);
    openagc_gpu_device_desc device_desc = OPENAGC_GPU_DEVICE_DESC_INIT;

    EXPECT(openagc_context_create(&context_desc, context), OPENAGC_OK);
    EXPECT(openagc_gpu_device_create(*context, &device_desc, device), OPENAGC_OK);
    return 0;
}

static openagc_shader_artifact_desc fixture_desc(openagc_shader_stage stage,
                                                  const uint8_t *code)
{
    openagc_shader_artifact_desc desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;

    desc.stage = stage;
    desc.code = code;
    desc.code_size = 4u;
    memcpy(desc.code_sha256, test_hash, sizeof(test_hash));
    if (stage == OPENAGC_SHADER_STAGE_VERTEX) {
        desc.vertex_position_written = 1u;
        desc.varying_output_mask = 3u;
    } else if (stage == OPENAGC_SHADER_STAGE_PIXEL) {
        desc.varying_input_mask = 2u;
        desc.color_output_mask = 1u;
        desc.color_export_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
    } else if (stage == OPENAGC_SHADER_STAGE_COMPUTE) {
        desc.workgroup_x = 8u;
        desc.workgroup_y = 4u;
        desc.workgroup_z = 1u;
    }
    return desc;
}

static int test_intake_and_integrity(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_shader_capabilities capabilities = OPENAGC_SHADER_CAPABILITIES_INIT;
    openagc_shader_artifact_info info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_binding_decl binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
    };
    openagc_shader_binding_decl duplicate_bindings[2] = {
        { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u },
        { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 32u }
    };
    openagc_shader_binding_decl read_binding;
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_shader_artifact_desc desc = fixture_desc(OPENAGC_SHADER_STAGE_VERTEX, code);
    openagc_shader_artifact *artifact = NULL;
    openagc_shader_artifact *candidate = NULL;

    CHECK(make_device(&context, &device) == 0);
    capabilities.struct_size--;
    EXPECT(openagc_shader_get_capabilities(device, &capabilities),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    capabilities.struct_size++;
    EXPECT(openagc_shader_get_capabilities(device, &capabilities), OPENAGC_OK);
    CHECK(capabilities.target == OPENAGC_SHADER_TARGET_GFX1013);
    CHECK(capabilities.structural_intake == 1u);
    CHECK(capabilities.compiler_available == 0u && capabilities.gpu_execution == 0u);
    CHECK(capabilities.host_compute_simulation == 1u);
    CHECK(capabilities.max_code_bytes == 65536u && capabilities.max_bindings == 8u);
    desc.bindings = &binding;
    desc.binding_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &artifact), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_require_compiler(artifact), OPENAGC_ERROR_NOT_READY);
    info.struct_size--;
    EXPECT(openagc_shader_artifact_get_info(artifact, &info),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    info.struct_size++;
    EXPECT(openagc_shader_artifact_get_info(artifact, &info), OPENAGC_OK);
    CHECK(info.stage == OPENAGC_SHADER_STAGE_VERTEX &&
          info.target == OPENAGC_SHADER_TARGET_GFX1013);
    CHECK(info.code_size == 4u && info.binding_count == 1u);
    CHECK(info.compiler_verified == 0u && info.gpu_executable == 0u);
    CHECK(info.host_store_const == 0u);
    CHECK(memcmp(info.code_sha256, test_hash, sizeof(test_hash)) == 0);

    code[0] = 'X';
    binding.min_bytes = 64u;
    EXPECT(openagc_shader_artifact_get_binding(artifact, 0u, &read_binding), OPENAGC_OK);
    CHECK(read_binding.min_bytes == 16u && read_binding.binding == 0u);
    EXPECT(openagc_shader_artifact_get_binding(artifact, 1u, &read_binding),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_shader_artifact_get_info(artifact, &info), OPENAGC_OK);
    CHECK(memcmp(info.code_sha256, test_hash, sizeof(test_hash)) == 0);
    code[0] = 't';
    binding.min_bytes = 16u;

    desc.struct_size--;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.struct_size++;
    desc.api_version++;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    desc.api_version = OPENAGC_SHADER_API_VERSION;
    desc.target = 1014u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    desc.target = OPENAGC_SHADER_TARGET_GFX1013;
    desc.code_sha256[0] ^= 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INTEGRITY);
    desc.code_sha256[0] ^= 1u;
    desc.code_size = 3u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.code_size = 65540u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.code_size = 4u;
    desc.code = NULL;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.code = code;
    desc.bindings = NULL;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.bindings = &binding;
    desc.binding_count = 9u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.binding_count = 1u;
    desc.bindings = duplicate_bindings;
    desc.binding_count = 2u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.bindings = &binding;
    desc.binding_count = 1u;
    binding.binding = 8u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    binding.binding = 0u;
    binding.kind = 99u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    binding.kind = OPENAGC_SHADER_BINDING_UNIFORM_BUFFER;
    binding.min_bytes = 5u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    binding.min_bytes = 16u;
    desc.vertex_position_written = 0u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.vertex_position_written = 1u;
    desc.stage = OPENAGC_SHADER_STAGE_PIXEL;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.stage = OPENAGC_SHADER_STAGE_VERTEX;
    desc.compiler_metadata_version = 14u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.compiler = OPENAGC_SHADER_COMPILER_OPENGNM_PSBC;
    desc.compiler_binary_sha256[0] = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.toolchain_release_minor = 3u;
    memcpy(desc.compiler_source_revision, OPENAGC_SHADER_PINNED_PSBC_REVISION,
           sizeof(desc.compiler_source_revision));
    desc.compiler_metadata_version = 13u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(candidate == NULL);
    desc.compiler_metadata_version = OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    {
        static const char metadata[] =
            "{\"version\":14,\"target\":2,\"source_stage\":1,\"machine_code_size\":4,"
            "\"hardware_stage\":0,\"unresolved_fields\":0}";
        static const char wrong_size[] =
            "{\"version\":14,\"target\":2,\"source_stage\":1,\"machine_code_size\":8,"
            "\"hardware_stage\":0,\"unresolved_fields\":0}";

        desc.compiler_metadata = (const uint8_t *)wrong_size;
        desc.compiler_metadata_size = (uint32_t)(sizeof(wrong_size) - 1u);
        EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
               OPENAGC_ERROR_INTEGRITY);
        CHECK(candidate == NULL);
        desc.compiler_metadata = (const uint8_t *)metadata;
        desc.compiler_metadata_size = (uint32_t)(sizeof(metadata) - 1u);
    }
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_NOT_READY);
    CHECK(candidate == NULL);
    {
        openagc_shader_artifact_desc pixel = fixture_desc(OPENAGC_SHADER_STAGE_PIXEL, code);
        static const char pixel_metadata[] =
            "{\"version\":14,\"target\":2,\"source_stage\":5,\"machine_code_size\":4,"
            "\"hardware_stage\":1,\"unresolved_fields\":0}";
        static const char vertex_stage_on_pixel[] =
            "{\"version\":14,\"target\":2,\"source_stage\":1,\"machine_code_size\":4,"
            "\"hardware_stage\":1,\"unresolved_fields\":0}";

        pixel.compiler = OPENAGC_SHADER_COMPILER_OPENGNM_PSBC;
        pixel.compiler_metadata_version = OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION;
        pixel.toolchain_release_minor = 3u;
        pixel.compiler_binary_sha256[0] = 1u;
        memcpy(pixel.compiler_source_revision, OPENAGC_SHADER_PINNED_PSBC_REVISION,
               sizeof(pixel.compiler_source_revision));
        pixel.compiler_metadata = (const uint8_t *)vertex_stage_on_pixel;
        pixel.compiler_metadata_size = (uint32_t)(sizeof(vertex_stage_on_pixel) - 1u);
        EXPECT(openagc_shader_artifact_intake_host(device, &pixel, &candidate),
               OPENAGC_ERROR_INTEGRITY);
        pixel.compiler_metadata = (const uint8_t *)pixel_metadata;
        pixel.compiler_metadata_size = (uint32_t)(sizeof(pixel_metadata) - 1u);
        EXPECT(openagc_shader_artifact_intake_host(device, &pixel, &candidate),
               OPENAGC_ERROR_NOT_READY);
        CHECK(candidate == NULL);
    }

    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_sha256_block_boundaries_and_compute(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    uint8_t code[64];
    openagc_shader_artifact_desc desc;
    openagc_shader_artifact *artifact = NULL;
    openagc_shader_artifact *candidate = NULL;
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_pipeline_info info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_shader_pipeline_plan *plan = NULL;

    CHECK(make_device(&context, &device) == 0);
    memset(code, 'a', sizeof(code));
    desc = fixture_desc(OPENAGC_SHADER_STAGE_COMPUTE, code);
    desc.code_size = 56u;
    memcpy(desc.code_sha256, hash_56_a, sizeof(hash_56_a));
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &artifact), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    artifact = NULL;
    desc.code_size = 64u;
    memcpy(desc.code_sha256, hash_64_a, sizeof(hash_64_a));
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &artifact), OPENAGC_OK);
    desc.workgroup_x = 1024u;
    desc.workgroup_y = 2u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.workgroup_x = 0u;
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    CHECK(candidate == NULL);

    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    pipeline_desc.compute = artifact;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_get_info(plan, &info), OPENAGC_OK);
    CHECK(info.kind == OPENAGC_SHADER_PIPELINE_COMPUTE && info.stage_count == 1u);
    CHECK(info.resource_count == 0u && info.target_image_id == 0u);
    CHECK(info.compiler_verified == 0u && info.gpu_executable == 0u);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(artifact), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_graphics_plan_and_resource_lifetime(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_buffer_desc buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(1u);
    openagc_graphics_transition_desc transition = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    uint8_t vertex_code[4] = { 't', 'e', 's', 't' };
    uint8_t pixel_code[4] = { 't', 'e', 's', 't' };
    openagc_shader_binding_decl vertex_binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
    };
    openagc_shader_binding_decl pixel_binding = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 32u
    };
    openagc_shader_artifact_desc vertex_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_VERTEX, vertex_code);
    openagc_shader_artifact_desc pixel_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_PIXEL, pixel_code);
    openagc_shader_resource_binding resource = { 0u, 0u, NULL, 0u, 32u };
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_pipeline_info plan_info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_gpu_memory *buffer_memory = NULL;
    openagc_gpu_memory *image_memory = NULL;
    openagc_gpu_buffer *buffer = NULL;
    openagc_graphics_image *image = NULL;
    openagc_graphics_command_buffer *command = NULL;
    openagc_shader_artifact *vertex = NULL;
    openagc_shader_artifact *pixel = NULL;
    openagc_shader_pipeline_plan *plan = NULL;
    openagc_shader_pipeline_plan *candidate = NULL;

    CHECK(make_device(&context, &device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &buffer_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &buffer_desc, &buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(buffer, buffer_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &image), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(image, image_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command),
           OPENAGC_OK);

    vertex_desc.bindings = &vertex_binding;
    vertex_desc.binding_count = 1u;
    pixel_desc.bindings = &pixel_binding;
    pixel_desc.binding_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &vertex_desc, &vertex), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &pixel), OPENAGC_OK);
    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_GRAPHICS;
    pipeline_desc.vertex = vertex;
    pipeline_desc.pixel = pixel;
    pipeline_desc.color_target = image;
    resource.buffer = buffer;
    pipeline_desc.resources = &resource;
    pipeline_desc.resource_count = 1u;

    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    CHECK(candidate == NULL);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, image, &transition), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_reset(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_destroy(command), OPENAGC_OK);

    pipeline_desc.pixel = vertex;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    pipeline_desc.pixel = pixel;
    pipeline_desc.resource_count = 0u;
    pipeline_desc.resources = NULL;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pipeline_desc.resource_count = 1u;
    pipeline_desc.resources = &resource;
    resource.binding = 1u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    resource.binding = 0u;
    resource.size_bytes = 16u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    resource.size_bytes = 32u;
    resource.offset = UINT64_MAX;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    resource.offset = 0u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_get_info(plan, &plan_info), OPENAGC_OK);
    CHECK(plan_info.kind == OPENAGC_SHADER_PIPELINE_GRAPHICS &&
          plan_info.stage_count == 2u && plan_info.resource_count == 1u);
    CHECK(plan_info.color_format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
          plan_info.target_image_id == 1u);
    CHECK(plan_info.compiler_verified == 0u && plan_info.gpu_executable == 0u);
    resource.buffer = NULL;
    EXPECT(openagc_shader_artifact_destroy(vertex), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_shader_artifact_destroy(pixel), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_ERROR_BUSY);

    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(vertex), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(pixel), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(image), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(buffer_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

static int test_pipeline_format_bindings_and_ownership(void)
{
    openagc_context *context = NULL;
    openagc_context *other_context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_device *other_device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(64u);
    openagc_gpu_buffer_desc shader_buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_gpu_buffer_desc copy_buffer_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_COPY_SOURCE_BIT);
    openagc_graphics_image_desc image_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        4u, 4u, 16u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(1u);
    openagc_graphics_transition_desc transition = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_shader_artifact_desc vertex_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_VERTEX, code);
    openagc_shader_artifact_desc pixel_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_PIXEL, code);
    openagc_shader_binding_decl uniform = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
    };
    openagc_shader_resource_binding resource = { 0u, 0u, NULL, 0u, 32u };
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_gpu_memory *uniform_memory = NULL;
    openagc_gpu_memory *image_memory = NULL;
    openagc_gpu_memory *other_memory = NULL;
    openagc_gpu_buffer *shader_buffer = NULL;
    openagc_gpu_buffer *wrong_usage = NULL;
    openagc_gpu_buffer *unbound = NULL;
    openagc_gpu_buffer *foreign = NULL;
    openagc_graphics_image *target = NULL;
    openagc_graphics_image *unbound_target = NULL;
    openagc_graphics_command_buffer *command = NULL;
    openagc_shader_artifact *vertex = NULL;
    openagc_shader_artifact *pixel = NULL;
    openagc_shader_artifact *bad_format = NULL;
    openagc_shader_artifact *bad_varying = NULL;
    openagc_shader_pipeline_plan *candidate = NULL;

    CHECK(make_device(&context, &device) == 0);
    CHECK(make_device(&other_context, &other_device) == 0);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &uniform_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(other_device, &memory_desc, &other_memory),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &shader_buffer_desc, &shader_buffer),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &copy_buffer_desc, &wrong_usage), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &shader_buffer_desc, &unbound), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(other_device, &shader_buffer_desc, &foreign),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(shader_buffer, uniform_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(wrong_usage, uniform_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(foreign, other_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &target), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &image_desc, &unbound_target),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(target, image_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, target, &transition), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_destroy(command), OPENAGC_OK);

    vertex_desc.bindings = &uniform;
    vertex_desc.binding_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &vertex_desc, &vertex), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &pixel), OPENAGC_OK);
    pixel_desc.color_export_format = OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &bad_format),
           OPENAGC_OK);
    pixel_desc.color_export_format = OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM;
    pixel_desc.varying_input_mask = 4u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &bad_varying),
           OPENAGC_OK);

    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_GRAPHICS;
    pipeline_desc.vertex = vertex;
    pipeline_desc.pixel = bad_varying;
    pipeline_desc.color_target = target;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    pipeline_desc.pixel = bad_format;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    pipeline_desc.pixel = pixel;
    pipeline_desc.color_target = unbound_target;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    pipeline_desc.color_target = target;
    pipeline_desc.resources = &resource;
    pipeline_desc.resource_count = 1u;
    resource.buffer = wrong_usage;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    resource.buffer = unbound;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    resource.buffer = foreign;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_OWNERSHIP);
    resource.buffer = shader_buffer;
    resource.size_bytes = 8u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_OUT_OF_RANGE);
    resource.size_bytes = 32u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_destroy(candidate), OPENAGC_OK);
    candidate = NULL;
    CHECK(candidate == NULL);

    EXPECT(openagc_shader_artifact_destroy(vertex), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(pixel), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(bad_format), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(bad_varying), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(shader_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(wrong_usage), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(unbound), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(foreign), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(target), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(unbound_target), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(uniform_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(other_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(other_device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    EXPECT(openagc_context_destroy(other_context), OPENAGC_OK);
    return 0;
}

static int test_sampled_texture_bindings(void)
{
    openagc_context *context = NULL;
    openagc_context *other_context = NULL;
    openagc_gpu_device *device = NULL;
    openagc_gpu_device *other_device = NULL;
    openagc_gpu_memory_desc memory_desc = OPENAGC_GPU_MEMORY_DESC_INIT(96u);
    openagc_gpu_buffer_desc uniform_desc =
        OPENAGC_GPU_BUFFER_DESC_INIT(64u, OPENAGC_GPU_BUFFER_SHADER_READ_BIT);
    openagc_graphics_image_desc sampled_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_image_desc bgra_sampled_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM);
    openagc_graphics_image_desc target_desc = OPENAGC_GRAPHICS_IMAGE_DESC_INIT(
        2u, 2u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    openagc_graphics_command_buffer_desc command_desc =
        OPENAGC_GRAPHICS_COMMAND_BUFFER_DESC_INIT(5u);
    openagc_graphics_transition_desc to_shader_read = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_SHADER_READ, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    openagc_graphics_transition_desc to_target = OPENAGC_GRAPHICS_TRANSITION_DESC_INIT(
        OPENAGC_GRAPHICS_STATE_UNDEFINED, OPENAGC_GRAPHICS_OWNER_HOST,
        OPENAGC_GRAPHICS_STATE_COLOR_TARGET, OPENAGC_GRAPHICS_OWNER_GRAPHICS);
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_shader_binding_decl uniform = {
        0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
    };
    openagc_shader_binding_decl colliding_uniform = {
        0u, 1u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u
    };
    openagc_shader_texture_decl texture = { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM };
    openagc_shader_texture_decl bgra_texture = { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_BGRA8_UNORM };
    openagc_shader_texture_decl two_textures[2] = {
        { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM },
        { 0u, 2u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM }
    };
    openagc_shader_texture_decl duplicate_textures[2] = {
        { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM },
        { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM }
    };
    openagc_shader_texture_decl depth_texture = {
        0u, 1u, OPENAGC_GRAPHICS_FORMAT_D24_UNORM_S8_UINT
    };
    openagc_shader_texture_decl set_texture = { 1u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM };
    openagc_shader_texture_decl slot_texture = { 0u, 8u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM };
    openagc_shader_texture_decl read_texture;
    openagc_shader_artifact_desc vertex_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_VERTEX, code);
    openagc_shader_artifact_desc pixel_desc =
        fixture_desc(OPENAGC_SHADER_STAGE_PIXEL, code);
    openagc_shader_resource_binding resource = { 0u, 0u, NULL, 0u, 32u };
    openagc_shader_texture_binding textures[2] = { { 0u, 1u, NULL }, { 0u, 2u, NULL } };
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_artifact_info artifact_info = OPENAGC_SHADER_ARTIFACT_INFO_INIT;
    openagc_shader_pipeline_info plan_info = OPENAGC_SHADER_PIPELINE_INFO_INIT;
    openagc_gpu_memory *uniform_memory = NULL;
    openagc_gpu_memory *image_memory = NULL;
    openagc_gpu_memory *other_memory = NULL;
    openagc_gpu_buffer *uniform_buffer = NULL;
    openagc_graphics_image *sampled = NULL;
    openagc_graphics_image *sampled_second = NULL;
    openagc_graphics_image *sampled_bgra = NULL;
    openagc_graphics_image *target_only = NULL;
    openagc_graphics_image *sampled_idle = NULL;
    openagc_graphics_image *unbound_sampled = NULL;
    openagc_graphics_image *foreign_sampled = NULL;
    openagc_graphics_command_buffer *command = NULL;
    openagc_shader_artifact *vertex = NULL;
    openagc_shader_artifact *pixel = NULL;
    openagc_shader_artifact *two_slots = NULL;
    openagc_shader_artifact *vertex_texture = NULL;
    openagc_shader_artifact *pixel_bgra_texture = NULL;
    openagc_shader_artifact *depth_artifact = NULL;
    openagc_shader_artifact *set_artifact = NULL;
    openagc_shader_artifact *slot_artifact = NULL;
    openagc_shader_artifact *duplicate_artifact = NULL;
    openagc_shader_artifact *collision_artifact = NULL;
    openagc_shader_artifact *compute_texture = NULL;
    openagc_shader_artifact_desc compute_desc = OPENAGC_SHADER_ARTIFACT_DESC_INIT;
    openagc_shader_pipeline_plan *plan = NULL;
    openagc_shader_pipeline_plan *candidate = NULL;
    openagc_shader_artifact *artifact_candidate = NULL;

    CHECK(make_device(&context, &device) == 0);
    CHECK(make_device(&other_context, &other_device) == 0);
    sampled_desc.usage = OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
    bgra_sampled_desc.usage = OPENAGC_GRAPHICS_USAGE_SAMPLED_BIT;
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &uniform_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(device, &memory_desc, &image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_allocate(other_device, &memory_desc, &other_memory),
           OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_create(device, &uniform_desc, &uniform_buffer), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_bind_memory(uniform_buffer, uniform_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &sampled_desc, &sampled), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &sampled_desc, &sampled_second),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &bgra_sampled_desc, &sampled_bgra),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &target_desc, &target_only), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &sampled_desc, &sampled_idle), OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(device, &sampled_desc, &unbound_sampled),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_create(other_device, &sampled_desc, &foreign_sampled),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(sampled, image_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(sampled_second, image_memory, 16u),
           OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(sampled_bgra, image_memory, 32u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(target_only, image_memory, 48u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(sampled_idle, image_memory, 64u), OPENAGC_OK);
    EXPECT(openagc_graphics_image_bind_memory(foreign_sampled, other_memory, 0u), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_create(device, &command_desc, &command),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_begin(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled, &to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled_second, &to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, sampled_bgra, &to_shader_read),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_transition(command, target_only, &to_target),
           OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_end(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_apply_host_state(command), OPENAGC_OK);
    EXPECT(openagc_graphics_command_buffer_destroy(command), OPENAGC_OK);

    vertex_desc.bindings = &uniform;
    vertex_desc.binding_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &vertex_desc, &vertex), OPENAGC_OK);
    pixel_desc.textures = &texture;
    pixel_desc.texture_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &pixel), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_get_info(pixel, &artifact_info), OPENAGC_OK);
    CHECK(artifact_info.binding_count == 0u && artifact_info.texture_count == 1u);
    EXPECT(openagc_shader_artifact_get_texture(pixel, 0u, &read_texture), OPENAGC_OK);
    CHECK(read_texture.set == 0u && read_texture.binding == 1u);
    CHECK(read_texture.format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM);
    EXPECT(openagc_shader_artifact_get_texture(vertex, 0u, &read_texture),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_shader_artifact_get_texture(pixel, 1u, &read_texture),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_shader_artifact_get_texture(pixel, 0u, NULL),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    pixel_desc.textures = NULL;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &artifact_candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pixel_desc.textures = &texture;
    pixel_desc.texture_count = 0u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &artifact_candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pixel_desc.texture_count = 9u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &artifact_candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pixel_desc.texture_count = 1u;
    pixel_desc.textures = &depth_texture;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &depth_artifact),
           OPENAGC_ERROR_OUT_OF_RANGE);
    pixel_desc.textures = &set_texture;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &set_artifact),
           OPENAGC_ERROR_OUT_OF_RANGE);
    pixel_desc.textures = &slot_texture;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &slot_artifact),
           OPENAGC_ERROR_OUT_OF_RANGE);
    pixel_desc.textures = duplicate_textures;
    pixel_desc.texture_count = 2u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &duplicate_artifact),
           OPENAGC_ERROR_OUT_OF_RANGE);
    pixel_desc.bindings = &colliding_uniform;
    pixel_desc.binding_count = 1u;
    pixel_desc.textures = &texture;
    pixel_desc.texture_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &collision_artifact),
           OPENAGC_ERROR_OUT_OF_RANGE);
    CHECK(artifact_candidate == NULL);
    pixel_desc.bindings = NULL;
    pixel_desc.binding_count = 0u;
    pixel_desc.textures = two_textures;
    pixel_desc.texture_count = 2u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &two_slots), OPENAGC_OK);
    pixel_desc.textures = &bgra_texture;
    pixel_desc.texture_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &pixel_desc, &pixel_bgra_texture),
           OPENAGC_OK);
    vertex_desc.textures = &texture;
    vertex_desc.texture_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &vertex_desc, &vertex_texture),
           OPENAGC_OK);
    pixel_desc.textures = &texture;
    pixel_desc.texture_count = 1u;

    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_GRAPHICS;
    pipeline_desc.vertex = vertex;
    pipeline_desc.pixel = pixel;
    pipeline_desc.color_target = target_only;
    resource.buffer = uniform_buffer;
    pipeline_desc.resources = &resource;
    pipeline_desc.resource_count = 1u;
    textures[0].image = sampled;
    pipeline_desc.textures = textures;
    pipeline_desc.texture_count = 1u;

    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_get_info(plan, &plan_info), OPENAGC_OK);
    {
        uint32_t input_mask = 1u;

        EXPECT(openagc_shader_pipeline_plan_vertex_input(plan, &input_mask), OPENAGC_OK);
        CHECK(input_mask == 0u);
    }
    CHECK(plan_info.resource_count == 1u && plan_info.texture_count == 1u);
    CHECK(plan_info.color_format == OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM &&
          plan_info.target_image_id == 4u);
    EXPECT(openagc_graphics_image_destroy(sampled), OPENAGC_ERROR_BUSY);
    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    plan = NULL;

    pipeline_desc.textures = NULL;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pipeline_desc.textures = textures;
    pipeline_desc.texture_count = 0u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pipeline_desc.texture_count = 9u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pipeline_desc.texture_count = 1u;
    textures[0].binding = 0u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    textures[0].binding = 1u;
    textures[0].set = 1u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    textures[0].set = 0u;
    textures[0].image = NULL;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    textures[0].image = unbound_sampled;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    textures[0].image = sampled_idle;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    textures[0].image = target_only;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    textures[0].image = sampled_bgra;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    textures[0].image = foreign_sampled;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_OWNERSHIP);
    textures[0].image = sampled;
    pipeline_desc.pixel = two_slots;
    pipeline_desc.texture_count = 2u;
    textures[1].image = sampled;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    textures[1].image = sampled_bgra;
    textures[1].binding = 1u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    textures[1].binding = 2u;
    textures[1].image = sampled_second;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_get_info(plan, &plan_info), OPENAGC_OK);
    CHECK(plan_info.texture_count == 2u);
    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    plan = NULL;

    pipeline_desc.pixel = pixel_bgra_texture;
    pipeline_desc.texture_count = 1u;
    textures[0].image = sampled_bgra;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    plan = NULL;
    pipeline_desc.vertex = vertex_texture;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_BAD_STATE);
    pipeline_desc.vertex = vertex;
    pipeline_desc.pixel = pixel;
    textures[0].image = sampled;
    CHECK(candidate == NULL);

    compute_desc = fixture_desc(OPENAGC_SHADER_STAGE_COMPUTE, code);
    compute_desc.textures = &texture;
    compute_desc.texture_count = 1u;
    EXPECT(openagc_shader_artifact_intake_host(device, &compute_desc, &compute_texture),
           OPENAGC_OK);
    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    pipeline_desc.vertex = NULL;
    pipeline_desc.pixel = NULL;
    pipeline_desc.compute = compute_texture;
    pipeline_desc.color_target = NULL;
    pipeline_desc.resources = NULL;
    pipeline_desc.resource_count = 0u;
    pipeline_desc.texture_count = 1u;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plan),
           OPENAGC_OK);
    EXPECT(openagc_shader_pipeline_plan_get_info(plan, &plan_info), OPENAGC_OK);
    CHECK(plan_info.kind == OPENAGC_SHADER_PIPELINE_COMPUTE &&
          plan_info.texture_count == 1u && plan_info.target_image_id == 0u);
    textures[0].image = sampled_bgra;
    pipeline_desc.textures = NULL;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    pipeline_desc.textures = textures;
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &candidate),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    textures[0].image = sampled;
    CHECK(candidate == NULL);
    EXPECT(openagc_shader_pipeline_plan_destroy(plan), OPENAGC_OK);
    plan = NULL;

    EXPECT(openagc_shader_artifact_destroy(compute_texture), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(vertex), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(pixel), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(two_slots), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(vertex_texture), OPENAGC_OK);
    EXPECT(openagc_shader_artifact_destroy(pixel_bgra_texture), OPENAGC_OK);
    EXPECT(openagc_gpu_buffer_destroy(uniform_buffer), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(sampled), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(sampled_second), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(sampled_bgra), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(target_only), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(sampled_idle), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(unbound_sampled), OPENAGC_OK);
    EXPECT(openagc_graphics_image_destroy(foreign_sampled), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(uniform_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(image_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_memory_destroy(other_memory), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(other_device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    EXPECT(openagc_context_destroy(other_context), OPENAGC_OK);
    return 0;
}

static int test_artifact_and_plan_capacity(void)
{
    openagc_context *context = NULL;
    openagc_gpu_device *device = NULL;
    uint8_t code[4] = { 't', 'e', 's', 't' };
    openagc_shader_artifact_desc desc = fixture_desc(OPENAGC_SHADER_STAGE_COMPUTE, code);
    openagc_shader_pipeline_desc pipeline_desc = OPENAGC_SHADER_PIPELINE_DESC_INIT;
    openagc_shader_artifact *artifacts[64] = { NULL };
    openagc_shader_pipeline_plan *plans[32] = { NULL };
    openagc_shader_artifact *extra_artifact = NULL;
    openagc_shader_pipeline_plan *extra_plan = NULL;
    uint32_t i;

    CHECK(make_device(&context, &device) == 0);
    for (i = 0u; i < 64u; ++i) {
        EXPECT(openagc_shader_artifact_intake_host(device, &desc, &artifacts[i]),
               OPENAGC_OK);
    }
    EXPECT(openagc_shader_artifact_intake_host(device, &desc, &extra_artifact),
           OPENAGC_ERROR_CAPACITY);
    CHECK(extra_artifact == NULL);
    for (i = 1u; i < 64u; ++i) {
        EXPECT(openagc_shader_artifact_destroy(artifacts[i]), OPENAGC_OK);
    }
    pipeline_desc.kind = OPENAGC_SHADER_PIPELINE_COMPUTE;
    pipeline_desc.compute = artifacts[0];
    for (i = 0u; i < 32u; ++i) {
        EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &plans[i]),
               OPENAGC_OK);
    }
    EXPECT(openagc_shader_pipeline_plan_create_host(device, &pipeline_desc, &extra_plan),
           OPENAGC_ERROR_CAPACITY);
    CHECK(extra_plan == NULL);
    EXPECT(openagc_shader_artifact_destroy(artifacts[0]), OPENAGC_ERROR_BUSY);
    for (i = 0u; i < 32u; ++i) {
        EXPECT(openagc_shader_pipeline_plan_destroy(plans[i]), OPENAGC_OK);
    }
    EXPECT(openagc_shader_artifact_destroy(artifacts[0]), OPENAGC_OK);
    EXPECT(openagc_gpu_device_destroy(device), OPENAGC_OK);
    EXPECT(openagc_context_destroy(context), OPENAGC_OK);
    return 0;
}

int main(void)
{
    if (test_intake_and_integrity() != 0 ||
        test_sha256_block_boundaries_and_compute() != 0 ||
        test_graphics_plan_and_resource_lifetime() != 0 ||
        test_pipeline_format_bindings_and_ownership() != 0 ||
        test_sampled_texture_bindings() != 0 ||
        test_artifact_and_plan_capacity() != 0) {
        return 1;
    }
    puts("OpenAGC shader structural tests passed");
    return 0;
}
