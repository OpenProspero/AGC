/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/pm4_graphics_fw940.h"
#include "openagc/psbc_metadata.h"
#include "openagc/shader.h"
#include "openagc_sha256.h"

#include <stdio.h>
#include <stdlib.h>
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

static int load_file(const char *path, uint8_t **out_bytes, uint32_t *out_size)
{
    FILE *handle = fopen(path, "rb");
    long length;
    uint8_t *bytes;

    if (handle == NULL) {
        return 0;
    }
    if (fseek(handle, 0, SEEK_END) != 0) {
        fclose(handle);
        return 0;
    }
    length = ftell(handle);
    if (length < 0 || length > 1048576) {
        fclose(handle);
        return 0;
    }
    if (fseek(handle, 0, SEEK_SET) != 0) {
        fclose(handle);
        return 0;
    }
    bytes = (uint8_t *)malloc((size_t)length + 1u);
    if (bytes == NULL) {
        fclose(handle);
        return 0;
    }
    if (fread(bytes, 1u, (size_t)length, handle) != (size_t)length) {
        free(bytes);
        fclose(handle);
        return 0;
    }
    fclose(handle);
    bytes[length] = 0;
    *out_bytes = bytes;
    *out_size = (uint32_t)length;
    return 1;
}

/*
 * Host adapter smoke: encode SET_CONTEXT_REG / SET_SH_REG from the pinned
 * PSBC smoke.vert metadata pairs. Does not intake artifacts, does not
 * emit DRAW, and does not claim gpu_execution.
 */
static int test_psbc_vert_register_program(void)
{
    /* From tests/fixtures/psbc_smoke/smoke.vert.metadata.json */
    static const uint32_t context_offsets[] = { 433u, 451u, 519u };
    static const uint32_t context_values[] = { 128u, 4u, 0u };
    static const uint32_t shader_offsets[] = { 72u, 73u, 74u, 75u };
    static const uint32_t shader_values[] = { 0u, 0u, 137101313u, 8u };
    uint32_t words[32];
    uint32_t count;
    uint32_t i;

    CHECK(strlen(OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256) == 64u);

    count = openagc_pm4_encode_psbc_context_pairs(context_offsets, context_values, 3u, words);
    CHECK(count == 9u);
    CHECK(words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[1] == 433u && words[2] == 128u);
    CHECK(words[3] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[4] == 451u && words[5] == 4u);
    CHECK(words[6] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[7] == 519u && words[8] == 0u);

    count = openagc_pm4_encode_psbc_shader_pairs(shader_offsets, shader_values, 4u, words);
    CHECK(count == 12u);
    for (i = 0u; i < 4u; ++i) {
        CHECK(words[i * 3u] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(words[i * 3u + 1u] == shader_offsets[i]);
        CHECK(words[i * 3u + 2u] == shader_values[i]);
    }
    /* DRAW opcode is cited but never encoded by this adapter. */
    CHECK(OPENAGC_PM4_OP_DRAW_INDEX_AUTO == 0x2du);
    return 0;
}

static int test_psbc_frag_register_program(void)
{
    static const uint32_t context_offsets[] = { 452u, 453u, 435u, 436u, 438u,
                                                 440u, 515u, 143u, 784u };
    static const uint32_t context_values[] = { 0u, 9u, 128u, 128u, 32768u,
                                               0u, 16u, 15u, 0u };
    static const uint32_t shader_offsets[] = { 8u, 9u, 10u, 11u };
    static const uint32_t shader_values[] = { 0u, 0u, 36438017u, 4u };
    uint32_t words[64];
    uint32_t count;

    count = openagc_pm4_encode_psbc_context_pairs(context_offsets, context_values, 9u, words);
    CHECK(count == 27u);
    CHECK(words[1] == 452u && words[2] == 0u);
    CHECK(words[25] == 784u && words[26] == 0u);

    count = openagc_pm4_encode_psbc_shader_pairs(shader_offsets, shader_values, 4u, words);
    CHECK(count == 12u);
    CHECK(words[7] == 10u && words[8] == 36438017u);
    return 0;
}

static int test_psbc_fixture_json_extract(void)
{
    uint8_t *json = NULL;
    uint32_t json_size = 0u;
    uint32_t offsets[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t values[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t count = 0u;
    uint32_t words[64];
    uint32_t word_count;
    uint8_t pinned[32];
    openagc_psbc_reflection reflection;

    CHECK(openagc_psbc_decode_sha256_hex(OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256,
                                         pinned) == 1);
    CHECK(load_file("tests/fixtures/psbc_smoke/smoke.vert.metadata.json", &json, &json_size) ||
          load_file("../tests/fixtures/psbc_smoke/smoke.vert.metadata.json", &json, &json_size));
    EXPECT(openagc_psbc_metadata_extract_register_pairs(json, json_size, "context_registers",
                                                        offsets, values,
                                                        OPENAGC_PSBC_MAX_REGISTER_PAIRS, &count),
           OPENAGC_OK);
    CHECK(count == 3u);
    CHECK(offsets[0] == 433u && values[0] == 128u);
    CHECK(offsets[1] == 451u && values[1] == 4u);
    CHECK(offsets[2] == 519u && values[2] == 0u);
    word_count = openagc_pm4_encode_psbc_context_pairs(offsets, values, count, words);
    CHECK(word_count == 9u);

    EXPECT(openagc_psbc_metadata_extract_register_pairs(json, json_size, "shader_registers",
                                                        offsets, values,
                                                        OPENAGC_PSBC_MAX_REGISTER_PAIRS, &count),
           OPENAGC_OK);
    CHECK(count == 4u);
    CHECK(offsets[2] == 74u && values[2] == 137101313u);

    EXPECT(openagc_psbc_metadata_parse_reflection(json, json_size, &reflection), OPENAGC_OK);
    EXPECT(openagc_psbc_reflection_validate(&reflection, 1u, 44u), OPENAGC_OK);
    CHECK(reflection.has_linkage == 1u);
    CHECK(reflection.linkage_ge_cntl_offset == 603u &&
          reflection.linkage_ge_cntl_value == 131200u);
    CHECK(reflection.has_base_vertex_user_data == 1u &&
          reflection.base_vertex_user_data_dword == 2u);
    CHECK(reflection.user_sgpr_count == 4u);
    word_count = openagc_psbc_reflection_encode_register_program(&reflection, words);
    CHECK(word_count == openagc_psbc_reflection_register_program_dwords(&reflection));
    CHECK(word_count == 3u * (reflection.context_count + reflection.shader_reg_count) + 9u);
    /* Linkage appends after context+shader pairs. */
    CHECK(words[word_count - 9u] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
    CHECK(words[word_count - 8u] == 603u && words[word_count - 7u] == 131200u);
    CHECK(words[word_count - 5u] == 725u && words[word_count - 4u] == 65536u);
    CHECK(words[word_count - 2u] == 610u && words[word_count - 1u] == 0u);
    free(json);
    json = NULL;

    CHECK(load_file("tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &json, &json_size) ||
          load_file("../tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &json, &json_size));
    EXPECT(openagc_psbc_metadata_parse_reflection(json, json_size, &reflection), OPENAGC_OK);
    EXPECT(openagc_psbc_reflection_validate(&reflection, 5u, 48u), OPENAGC_OK);
    CHECK(reflection.has_linkage == 0u);
    CHECK(reflection.has_base_vertex_user_data == 0u);
    CHECK(reflection.user_sgpr_count == 2u);
    CHECK(reflection.context_count == 9u && reflection.shader_reg_count == 4u);
    free(json);
    return 0;
}

static int build_bound_metadata(char *out, size_t out_size, const char *bindings)
{
    int written = snprintf(out, out_size,
                           "{\"version\":14,\"target\":2,\"source_stage\":5,\"machine_code_size\":48,"
                           "\"hardware_stage\":5,\"unresolved_fields\":1,\"address32_hi\":0,"
                           "\"user_sgpr_count\":4,"
                           "\"context_registers\":[{\"offset\":452,\"value\":9}],"
                           "\"shader_registers\":[{\"offset\":8,\"value\":0}],"
                           "\"input_semantics\":[],\"output_semantics\":[],"
                           "\"descriptor_bindings\":[%s],"
                           "\"base_vertex_user_data_dword\":2,"
                           "\"is_indexed_draw_user_data_dword\":3}",
                           bindings);
    return written > 0 && (size_t)written < out_size;
}

static int test_psbc_reflection_descriptor_bindings(void)
{
    /* Malformed: the pinned emitter always writes all six fields. */
    static const char malformed[] =
        "{\"version\":14,\"target\":2,\"source_stage\":1,\"machine_code_size\":44,"
        "\"hardware_stage\":1,\"unresolved_fields\":1,\"address32_hi\":0,"
        "\"user_sgpr_count\":4,"
        "\"context_registers\":[{\"offset\":433,\"value\":128}],"
        "\"shader_registers\":[{\"offset\":72,\"value\":0}],"
        "\"input_semantics\":[],\"output_semantics\":[],"
        "\"descriptor_bindings\":[{\"set\":0,\"binding\":0}],"
        "\"base_vertex_user_data_dword\":2,\"is_indexed_draw_user_data_dword\":3,"
        "\"linkage\":{\"ge_cntl\":{\"offset\":603,\"value\":1},"
        "\"stages_en\":{\"offset\":725,\"value\":1},"
        "\"user_vgpr_en\":{\"offset\":610,\"value\":0}}}";
    static const openagc_shader_binding_decl decls[2] = {
        { 0u, 0u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u },
        { 0u, 2u, OPENAGC_SHADER_BINDING_UNIFORM_BUFFER, 16u }
    };
    static const openagc_shader_texture_decl tex_decls[1] = {
        { 0u, 1u, OPENAGC_GRAPHICS_FORMAT_RGBA8_UNORM }
    };
    openagc_psbc_reflection reflection;
    openagc_shader_artifact_desc desc;
    openagc_shader_resource_binding resources[3];
    openagc_shader_texture_binding textures[2];
    openagc_shader_resource_binding mapped_resources[3];
    openagc_shader_texture_binding mapped_textures[2];
    char json[1024];
    uint32_t mapped_resources_count = 0u;
    uint32_t mapped_textures_count = 0u;
    uint32_t json_size;

    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)malformed,
                                                  (uint32_t)(sizeof(malformed) - 1u),
                                                  &reflection),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    /* Typed bindings: one UBO at 0, a sampled image at 1, a UBO at 2. */
    CHECK(build_bound_metadata(json,
                               sizeof(json),
                               "{\"set\":0,\"binding\":0,\"type\":1,\"array_size\":1,"
                               "\"offset\":0,\"stride\":16},"
                               "{\"set\":0,\"binding\":1,\"type\":2,\"array_size\":1,"
                               "\"offset\":4,\"stride\":16},"
                               "{\"set\":0,\"binding\":2,\"type\":1,\"array_size\":1,"
                               "\"offset\":8,\"stride\":16}"));
    json_size = (uint32_t)strlen(json);
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json, json_size,
                                                  &reflection),
           OPENAGC_OK);
    CHECK(reflection.descriptor_binding_count == 3u);
    CHECK(reflection.empty_descriptor_bindings == 0u);
    CHECK(reflection.descriptor_bindings[0].binding == 0u);
    CHECK(reflection.descriptor_bindings[0].type ==
          OPENAGC_PSBC_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    CHECK(reflection.descriptor_bindings[1].type ==
          OPENAGC_PSBC_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    CHECK(reflection.descriptor_bindings[2].binding == 2u);
    CHECK(reflection.descriptor_bindings[2].stride == 16u);

    memset(&desc, 0, sizeof(desc));
    desc.code_size = reflection.machine_code_size;
    desc.bindings = decls;
    desc.binding_count = 2u;
    desc.textures = tex_decls;
    desc.texture_count = 1u;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc), OPENAGC_OK);
    /* A missing declaration and an extra declaration are both INTEGRITY. */
    desc.binding_count = 1u;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc),
           OPENAGC_ERROR_INTEGRITY);
    desc.binding_count = 2u;
    desc.texture_count = 0u;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc),
           OPENAGC_ERROR_INTEGRITY);

    /* Typed map: metadata order wins, both slots resolve. */
    memset(resources, 0, sizeof(resources));
    memset(textures, 0, sizeof(textures));
    resources[0].set = 0u;
    resources[0].binding = 2u;
    resources[1].set = 0u;
    resources[1].binding = 0u;
    textures[0].set = 0u;
    textures[0].binding = 1u;
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 2u, textures, 1u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_OK);
    CHECK(mapped_resources_count == 2u && mapped_textures_count == 1u);
    CHECK(mapped_resources[0].binding == 0u);
    CHECK(mapped_resources[1].binding == 2u);
    CHECK(mapped_textures[0].binding == 1u);
    /* Missing slot, duplicate slot, and an undeclared slot are all refused. */
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 1u, textures, 1u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    resources[2] = resources[1];
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 3u, textures, 1u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    resources[2].binding = 7u;
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 3u, textures, 1u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    /* A buffer supplied for a sampled-image slot does not resolve it. */
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 2u, NULL, 0u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_ERROR_INVALID_ARGUMENT);

    /* Storage bindings and arrays are refused by the cross-check and map. */
    CHECK(build_bound_metadata(json, sizeof(json),
                               "{\"set\":0,\"binding\":0,\"type\":3,\"array_size\":1,"
                               "\"offset\":0,\"stride\":16}"));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_OK);
    memset(&desc, 0, sizeof(desc));
    desc.code_size = reflection.machine_code_size;
    desc.bindings = decls;
    desc.binding_count = 1u;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    EXPECT(openagc_psbc_reflection_map_resources(&reflection, resources, 0u, NULL, 0u,
                                                 mapped_resources, mapped_textures,
                                                 &mapped_resources_count,
                                                 &mapped_textures_count),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    CHECK(build_bound_metadata(json, sizeof(json),
                               "{\"set\":0,\"binding\":0,\"type\":2,\"array_size\":2,"
                               "\"offset\":0,\"stride\":16}"));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_OK);
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc),
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);

    /* Schema violations: duplicate slot, out-of-range binding, zero array. */
    CHECK(build_bound_metadata(json, sizeof(json),
                               "{\"set\":0,\"binding\":0,\"type\":1,\"array_size\":1,"
                               "\"offset\":0,\"stride\":16},"
                               "{\"set\":0,\"binding\":0,\"type\":1,\"array_size\":1,"
                               "\"offset\":0,\"stride\":16}"));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_ERROR_INVALID_ARGUMENT);
    CHECK(build_bound_metadata(json, sizeof(json),
                               "{\"set\":0,\"binding\":128,\"type\":1,\"array_size\":1,"
                               "\"offset\":0,\"stride\":16}"));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_ERROR_OUT_OF_RANGE);
    CHECK(build_bound_metadata(json, sizeof(json),
                               "{\"set\":0,\"binding\":0,\"type\":1,\"array_size\":0,"
                               "\"offset\":0,\"stride\":16}"));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_ERROR_OUT_OF_RANGE);

    /* Empty bindings still require zero declarations. */
    CHECK(build_bound_metadata(json, sizeof(json), ""));
    EXPECT(openagc_psbc_metadata_parse_reflection((const uint8_t *)json,
                                                  (uint32_t)strlen(json), &reflection),
           OPENAGC_OK);
    CHECK(reflection.empty_descriptor_bindings == 1u &&
          reflection.descriptor_binding_count == 0u);
    memset(&desc, 0, sizeof(desc));
    desc.code_size = reflection.machine_code_size;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc), OPENAGC_OK);
    desc.bindings = decls;
    desc.binding_count = 1u;
    EXPECT(openagc_psbc_reflection_check_artifact_desc(&reflection, &desc),
           OPENAGC_ERROR_INTEGRITY);
    return 0;
}

static int test_psbc_fixture_digests(void)
{
    static const char vert_path[] = "tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin";
    static const char frag_path[] = "tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin";
    FILE *handle;
    uint8_t vert[64];
    uint8_t frag[64];
    size_t vert_n;
    size_t frag_n;
    uint8_t digest[32];
    static const uint8_t vert_sha[32] = {
        0xda, 0x3d, 0xb5, 0xeb, 0x80, 0xb2, 0x23, 0x4c, 0xd4, 0xee, 0x28, 0x5a, 0xb1, 0xc2,
        0xef, 0x31, 0xd6, 0xf1, 0x8d, 0x66, 0xfe, 0x9d, 0xcf, 0x9b, 0xc5, 0x44, 0x37, 0x8f,
        0x0d, 0xb6, 0x7d, 0x2f
    };
    static const uint8_t frag_sha[32] = {
        0x28, 0xc5, 0x6f, 0x1c, 0xaa, 0xb7, 0xa7, 0x71, 0x9d, 0x09, 0xc8, 0x59, 0xb6, 0x90,
        0x77, 0xda, 0x00, 0x86, 0xb9, 0x5b, 0x17, 0x5f, 0x52, 0x90, 0x6c, 0x36, 0x63, 0x9b,
        0x30, 0xa0, 0x79, 0x81
    };

    handle = fopen(vert_path, "rb");
    if (handle == NULL) {
        handle = fopen("../tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin", "rb");
    }
    CHECK(handle != NULL);
    vert_n = fread(vert, 1u, sizeof(vert), handle);
    fclose(handle);
    CHECK(vert_n == 44u);

    handle = fopen(frag_path, "rb");
    if (handle == NULL) {
        handle = fopen("../tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin", "rb");
    }
    CHECK(handle != NULL);
    frag_n = fread(frag, 1u, sizeof(frag), handle);
    fclose(handle);
    CHECK(frag_n == 48u);

    openagc_sha256(vert, (uint32_t)vert_n, digest);
    CHECK(memcmp(digest, vert_sha, 32) == 0);
    openagc_sha256(frag, (uint32_t)frag_n, digest);
    CHECK(memcmp(digest, frag_sha, 32) == 0);
    return 0;
}

static int test_psbc_pgm_patch_and_eop(void)
{
    uint8_t *json = NULL;
    uint32_t json_size = 0u;
    openagc_psbc_reflection reflection;
    uint32_t words[64];
    uint32_t count;
    const uint64_t code_va = UINT64_C(0x20001000);

    CHECK(load_file("tests/fixtures/psbc_smoke/smoke.vert.metadata.json", &json, &json_size) ||
          load_file("../tests/fixtures/psbc_smoke/smoke.vert.metadata.json", &json, &json_size));
    EXPECT(openagc_psbc_metadata_parse_reflection(json, json_size, &reflection), OPENAGC_OK);
    EXPECT(openagc_psbc_reflection_patch_pgm_va(&reflection, code_va + 1u),
           OPENAGC_ERROR_OUT_OF_RANGE);
    EXPECT(openagc_psbc_reflection_patch_pgm_va(&reflection, code_va), OPENAGC_OK);
    CHECK(reflection.shader_values[0] == (uint32_t)(code_va >> 8));
    CHECK(reflection.shader_values[1] == (uint32_t)(code_va >> 40));
    count = openagc_psbc_reflection_encode_register_program_eop(&reflection, 1u, UINT64_C(0x3000),
                                                                words);
    CHECK(count == openagc_psbc_reflection_register_program_dwords(&reflection) +
                       OPENAGC_PM4_EOP_WITH_NOP_WORDS);
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    /* Graphics SET_SH pairs alone + EOP (Step O vehicle, no SET_CONTEXT). */
    {
        static const uint32_t offsets[] = { 72u, 73u, 74u, 75u };
        uint32_t values[4];
        uint32_t sh_words[OPENAGC_PM4_GRAPHICS_SH_EOP_WORDS(4u)];
        uint32_t sh_count;

        values[0] = reflection.shader_values[0];
        values[1] = reflection.shader_values[1];
        values[2] = reflection.shader_values[2];
        values[3] = reflection.shader_values[3];
        sh_count = openagc_pm4_encode_graphics_sh_eop(offsets, values, 4u, 1u, UINT64_C(0x3000),
                                                      sh_words);
        CHECK(sh_count == OPENAGC_PM4_GRAPHICS_SH_EOP_WORDS(4u));
        CHECK(sh_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(sh_words[1] == 72u && sh_words[2] == values[0]);
        CHECK(sh_words[sh_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    }
    /* SET_CONTEXT pairs alone + EOP (Step P vehicle: no linkage, no SET_SH). */
    {
        static const uint32_t ctx_offsets[] = { 433u, 451u, 519u };
        static const uint32_t ctx_values[] = { 128u, 4u, 0u };
        uint32_t ctx_words[OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(3u)];
        uint32_t ctx_count;

        ctx_count = openagc_pm4_encode_graphics_context_eop(
            ctx_offsets, ctx_values, 3u, 1u, UINT64_C(0x3000), ctx_words);
        CHECK(ctx_count == OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(3u));
        CHECK(ctx_count == 33u);
        CHECK(ctx_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(ctx_words[1] == 433u && ctx_words[2] == 128u);
        CHECK(ctx_words[3] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(ctx_words[4] == 451u && ctx_words[5] == 4u);
        CHECK(ctx_words[6] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(ctx_words[7] == 519u && ctx_words[8] == 0u);
        CHECK(ctx_words[ctx_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    }
    /* SET_CONTEXT then graphics SET_SH + EOP (Step Q vehicle: no linkage). */
    {
        static const uint32_t ctx_offsets[] = { 433u, 451u, 519u };
        static const uint32_t ctx_values[] = { 128u, 4u, 0u };
        static const uint32_t sh_offsets[] = { 72u, 73u, 74u, 75u };
        uint32_t sh_values[4];
        uint32_t combo_words[OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(3u, 4u)];
        uint32_t combo_count;

        sh_values[0] = reflection.shader_values[0];
        sh_values[1] = reflection.shader_values[1];
        sh_values[2] = reflection.shader_values[2];
        sh_values[3] = reflection.shader_values[3];
        combo_count = openagc_pm4_encode_graphics_context_sh_eop(
            ctx_offsets, ctx_values, 3u, sh_offsets, sh_values, 4u, 1u, UINT64_C(0x3000),
            combo_words);
        CHECK(combo_count == OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(3u, 4u));
        CHECK(combo_count == 45u);
        CHECK(combo_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(combo_words[1] == 433u && combo_words[2] == 128u);
        CHECK(combo_words[9] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(combo_words[10] == 72u && combo_words[11] == sh_values[0]);
        CHECK(combo_words[combo_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] ==
              OPENAGC_PM4_EOP_HEADER);
    }
    /* smoke.frag SET_CONTEXT ×9 + graphics SET_SH ×4 + EOP (Step T vehicle). */
    {
        static const uint32_t frag_ctx_offsets[] = { 452u, 453u, 435u, 436u, 438u,
                                                     440u, 515u, 143u, 784u };
        static const uint32_t frag_ctx_values[] = { 0u, 9u, 128u, 128u, 32768u,
                                                    0u, 16u, 15u, 0u };
        static const uint32_t frag_sh_offsets[] = { 8u, 9u, 10u, 11u };
        static const uint32_t frag_sh_values[] = { 0u, 0u, 36438017u, 4u };
        uint32_t frag_words[OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(9u, 4u)];
        uint32_t frag_count;
        uint32_t host_words[64];
        uint32_t host_count;
        openagc_psbc_reflection frag_reflection;
        uint8_t *frag_json = NULL;
        uint32_t frag_json_size = 0u;

        frag_count = openagc_pm4_encode_graphics_context_sh_eop(
            frag_ctx_offsets, frag_ctx_values, 9u, frag_sh_offsets, frag_sh_values, 4u,
            1u, UINT64_C(0x3000), frag_words);
        CHECK(frag_count == OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(9u, 4u));
        CHECK(frag_count == 63u);
        CHECK(frag_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(frag_words[1] == 452u && frag_words[2] == 0u);
        CHECK(frag_words[27] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(frag_words[28] == 8u && frag_words[29] == 0u);
        CHECK(frag_words[frag_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] ==
              OPENAGC_PM4_EOP_HEADER);

        CHECK(load_file("tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &frag_json,
                        &frag_json_size) ||
              load_file("../tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &frag_json,
                        &frag_json_size));
        EXPECT(openagc_psbc_metadata_parse_reflection(frag_json, frag_json_size,
                                                      &frag_reflection),
               OPENAGC_OK);
        free(frag_json);
        CHECK(frag_reflection.has_linkage == 0u);
        host_count = openagc_psbc_reflection_encode_register_program_eop(
            &frag_reflection, 1u, UINT64_C(0x3000), host_words);
        CHECK(frag_count == host_count);
        CHECK(memcmp(frag_words, host_words, (size_t)frag_count * sizeof(uint32_t)) == 0);
    }
    /* Linkage SET_CONTEXT ×3 + EOP (Step R vehicle: verified metadata only). */
    {
        uint32_t link_words[OPENAGC_PM4_GRAPHICS_LINKAGE_EOP_WORDS];
        uint32_t link_count;

        CHECK(reflection.has_linkage == 1u);
        link_count = openagc_pm4_encode_graphics_linkage_eop(
            reflection.linkage_ge_cntl_offset, reflection.linkage_ge_cntl_value,
            reflection.linkage_stages_en_offset, reflection.linkage_stages_en_value,
            reflection.linkage_user_vgpr_en_offset, reflection.linkage_user_vgpr_en_value,
            1u, UINT64_C(0x3000), link_words);
        CHECK(link_count == OPENAGC_PM4_GRAPHICS_LINKAGE_EOP_WORDS);
        CHECK(link_count == 33u);
        CHECK(link_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(link_words[1] == 603u && link_words[2] == 131200u);
        CHECK(link_words[3] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(link_words[4] == 725u && link_words[5] == 65536u);
        CHECK(link_words[6] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(link_words[7] == 610u && link_words[8] == 0u);
        CHECK(link_words[link_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] ==
              OPENAGC_PM4_EOP_HEADER);
    }
    /* Full host snapshot + EOP (Step S: ctx + SH + linkage, no DRAW). */
    {
        static const uint32_t ctx_offsets[] = { 433u, 451u, 519u };
        static const uint32_t ctx_values[] = { 128u, 4u, 0u };
        static const uint32_t sh_offsets[] = { 72u, 73u, 74u, 75u };
        uint32_t sh_values[4];
        uint32_t full_words[OPENAGC_PM4_GRAPHICS_CONTEXT_SH_LINKAGE_EOP_WORDS(3u, 4u)];
        uint32_t full_count;
        uint32_t host_count;

        sh_values[0] = reflection.shader_values[0];
        sh_values[1] = reflection.shader_values[1];
        sh_values[2] = reflection.shader_values[2];
        sh_values[3] = reflection.shader_values[3];
        CHECK(reflection.has_linkage == 1u);
        full_count = openagc_pm4_encode_graphics_context_sh_linkage_eop(
            ctx_offsets, ctx_values, 3u, sh_offsets, sh_values, 4u,
            reflection.linkage_ge_cntl_offset, reflection.linkage_ge_cntl_value,
            reflection.linkage_stages_en_offset, reflection.linkage_stages_en_value,
            reflection.linkage_user_vgpr_en_offset, reflection.linkage_user_vgpr_en_value,
            1u, UINT64_C(0x3000), full_words);
        CHECK(full_count == OPENAGC_PM4_GRAPHICS_CONTEXT_SH_LINKAGE_EOP_WORDS(3u, 4u));
        CHECK(full_count == 54u);
        host_count = openagc_psbc_reflection_encode_register_program_eop(
            &reflection, 1u, UINT64_C(0x3000), words);
        CHECK(full_count == host_count);
        CHECK(memcmp(full_words, words, (size_t)full_count * sizeof(uint32_t)) == 0);
        CHECK(full_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(full_words[1] == 433u && full_words[2] == 128u);
        CHECK(full_words[9] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(full_words[10] == 72u && full_words[11] == sh_values[0]);
        CHECK(full_words[21] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(full_words[22] == 603u && full_words[23] == 131200u);
        CHECK(full_words[full_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] ==
              OPENAGC_PM4_EOP_HEADER);
    }
    /* Vert + frag host snapshot + EOP (Step U: Step S body + Step T body). */
    {
        static const uint32_t vert_ctx_offsets[] = { 433u, 451u, 519u };
        static const uint32_t vert_ctx_values[] = { 128u, 4u, 0u };
        static const uint32_t vert_sh_offsets[] = { 72u, 73u, 74u, 75u };
        static const uint32_t frag_ctx_offsets[] = { 452u, 453u, 435u, 436u, 438u,
                                                     440u, 515u, 143u, 784u };
        static const uint32_t frag_ctx_values[] = { 0u, 9u, 128u, 128u, 32768u,
                                                    0u, 16u, 15u, 0u };
        static const uint32_t frag_sh_offsets[] = { 8u, 9u, 10u, 11u };
        static const uint32_t frag_sh_values[] = { 0u, 0u, 36438017u, 4u };
        uint32_t vert_sh_values[4];
        uint32_t combo_words[OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(3u, 4u, 9u, 4u)];
        uint32_t combo_count;
        uint32_t host_words[128];
        uint32_t host_count;
        openagc_psbc_reflection frag_reflection;
        uint8_t *frag_json = NULL;
        uint32_t frag_json_size = 0u;

        vert_sh_values[0] = reflection.shader_values[0];
        vert_sh_values[1] = reflection.shader_values[1];
        vert_sh_values[2] = reflection.shader_values[2];
        vert_sh_values[3] = reflection.shader_values[3];
        CHECK(reflection.has_linkage == 1u);
        combo_count = openagc_pm4_encode_graphics_vert_frag_eop(
            vert_ctx_offsets, vert_ctx_values, 3u, vert_sh_offsets, vert_sh_values, 4u,
            reflection.linkage_ge_cntl_offset, reflection.linkage_ge_cntl_value,
            reflection.linkage_stages_en_offset, reflection.linkage_stages_en_value,
            reflection.linkage_user_vgpr_en_offset, reflection.linkage_user_vgpr_en_value,
            frag_ctx_offsets, frag_ctx_values, 9u, frag_sh_offsets, frag_sh_values, 4u, 1u,
            UINT64_C(0x3000), combo_words);
        CHECK(combo_count == OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(3u, 4u, 9u, 4u));
        CHECK(combo_count == 93u);
        CHECK(combo_words[0] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(combo_words[1] == 433u && combo_words[2] == 128u);
        CHECK(combo_words[9] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(combo_words[10] == 72u && combo_words[11] == vert_sh_values[0]);
        CHECK(combo_words[21] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(combo_words[22] == 603u && combo_words[23] == 131200u);
        CHECK(combo_words[30] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_CONTEXT_REG, 3u, 0u));
        CHECK(combo_words[31] == 452u && combo_words[32] == 0u);
        CHECK(combo_words[57] == openagc_pm4_header3(OPENAGC_PM4_OP_SET_SH_REG, 3u, 0u));
        CHECK(combo_words[58] == 8u && combo_words[59] == 0u);
        CHECK(combo_words[combo_count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] ==
              OPENAGC_PM4_EOP_HEADER);

        CHECK(load_file("tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &frag_json,
                        &frag_json_size) ||
              load_file("../tests/fixtures/psbc_smoke/smoke.frag.metadata.json", &frag_json,
                        &frag_json_size));
        EXPECT(openagc_psbc_metadata_parse_reflection(frag_json, frag_json_size,
                                                      &frag_reflection),
               OPENAGC_OK);
        free(frag_json);
        CHECK(frag_reflection.has_linkage == 0u);
        host_count = openagc_psbc_reflection_encode_register_program(&reflection, host_words);
        host_count += openagc_psbc_reflection_encode_register_program(
            &frag_reflection, host_words + host_count);
        openagc_pm4_encode_eop_with_nops(UINT64_C(0x3000), 1u, host_words + host_count);
        host_count += OPENAGC_PM4_EOP_WITH_NOP_WORDS;
        CHECK(combo_count == host_count);
        CHECK(memcmp(combo_words, host_words, (size_t)combo_count * sizeof(uint32_t)) == 0);
    }
    free(json);
    return 0;
}

int main(void)
{
    if (test_psbc_vert_register_program() != 0 || test_psbc_frag_register_program() != 0 ||
        test_psbc_fixture_json_extract() != 0 ||
        test_psbc_reflection_descriptor_bindings() != 0 ||
        test_psbc_fixture_digests() != 0 || test_psbc_pgm_patch_and_eop() != 0) {
        return 1;
    }
    puts("OpenAGC PSBC adapter tests passed");
    return 0;
}
