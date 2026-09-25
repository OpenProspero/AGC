/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PSBC_METADATA_H
#define OPENAGC_PSBC_METADATA_H

#include "openagc/openagc.h"
#include "openagc/shader.h"

#include <stdint.h>
#include <string.h>

/*
 * Extract (offset,value) pairs from a PSBC metadata JSON array named
 * "context_registers" or "shader_registers". Only the subset needed for
 * host SET_CONTEXT_REG / SET_SH_REG encoding is parsed. Does not enable
 * OPENGNM_PSBC intake or emit DRAW packets.
 */

#define OPENAGC_PSBC_MAX_REGISTER_PAIRS 64u

static inline int openagc_psbc_hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int)(c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (int)(c - 'A') + 10;
    }
    return -1;
}

/* Decode a 64-char lowercase/uppercase hex SHA-256 into 32 bytes. */
static inline int openagc_psbc_decode_sha256_hex(const char *hex, uint8_t out[32])
{
    uint32_t i;

    if (hex == NULL || out == NULL) {
        return 0;
    }
    for (i = 0u; i < 32u; ++i) {
        int hi = openagc_psbc_hex_nibble(hex[i * 2u]);
        int lo = openagc_psbc_hex_nibble(hex[i * 2u + 1u]);

        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i] = (uint8_t)(((unsigned)hi << 4) | (unsigned)lo);
    }
    return hex[64] == '\0' ? 1 : 0;
}

/*
 * Locate the JSON array for `array_key` and collect objects of the form
 * {"offset":N,"value":M} (field order may vary; whitespace allowed).
 * Returns OPENAGC_OK and writes *out_count, or an error code.
 */
static inline openagc_result openagc_psbc_metadata_extract_register_pairs(
    const uint8_t *json, uint32_t size, const char *array_key, uint32_t *offsets,
    uint32_t *values, uint32_t max_pairs, uint32_t *out_count)
{
    uint32_t key_len = 0u;
    uint32_t index;
    uint32_t array_begin = 0u;
    uint32_t array_end = 0u;
    uint32_t count = 0u;
    int found_array = 0;

    if (out_count == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0u;
    if (json == NULL || size < 2u || array_key == NULL ||
        (max_pairs != 0u && (offsets == NULL || values == NULL))) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    while (array_key[key_len] != '\0') {
        key_len++;
    }
    for (index = 0u; index + key_len + 2u < size; ++index) {
        uint32_t cursor;

        if (json[index] != '"' || memcmp(json + index + 1u, array_key, key_len) != 0 ||
            json[index + 1u + key_len] != '"') {
            continue;
        }
        cursor = index + key_len + 2u;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != ':') {
            continue;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != '[') {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (found_array != 0) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        found_array = 1;
        array_begin = cursor + 1u;
        {
            uint32_t depth = 1u;
            uint32_t pos = array_begin;

            while (pos < size && depth != 0u) {
                if (json[pos] == '[') {
                    depth++;
                } else if (json[pos] == ']') {
                    depth--;
                }
                pos++;
            }
            if (depth != 0u) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            array_end = pos - 1u;
        }
        break;
    }
    if (found_array == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }

    index = array_begin;
    while (index < array_end) {
        uint32_t obj_end;
        uint32_t offset = 0u;
        uint32_t value = 0u;
        int have_offset = 0;
        int have_value = 0;
        uint32_t pos;

        while (index < array_end &&
               (json[index] == ' ' || json[index] == '\t' || json[index] == '\n' ||
                json[index] == '\r' || json[index] == ',')) {
            index++;
        }
        if (index >= array_end) {
            break;
        }
        if (json[index] != '{') {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        obj_end = index + 1u;
        {
            uint32_t depth = 1u;

            while (obj_end < array_end && depth != 0u) {
                if (json[obj_end] == '{') {
                    depth++;
                } else if (json[obj_end] == '}') {
                    depth--;
                }
                obj_end++;
            }
            if (depth != 0u) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
        }
        for (pos = index; pos + 8u < obj_end; ++pos) {
            uint32_t cursor;
            uint32_t parsed = 0u;
            const char *field = NULL;
            int *flag = NULL;

            if (json[pos] != '"') {
                continue;
            }
            if (pos + 8u < obj_end && memcmp(json + pos + 1u, "offset\"", 7u) == 0) {
                field = "offset";
                flag = &have_offset;
                cursor = pos + 8u;
            } else if (pos + 7u < obj_end && memcmp(json + pos + 1u, "value\"", 6u) == 0) {
                field = "value";
                flag = &have_value;
                cursor = pos + 7u;
            } else {
                continue;
            }
            (void)field;
            while (cursor < obj_end &&
                   (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                    json[cursor] == '\r')) {
                cursor++;
            }
            if (cursor >= obj_end || json[cursor] != ':') {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            cursor++;
            while (cursor < obj_end &&
                   (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                    json[cursor] == '\r')) {
                cursor++;
            }
            if (cursor >= obj_end || json[cursor] < '0' || json[cursor] > '9') {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            if (*flag != 0) {
                return OPENAGC_ERROR_INVALID_ARGUMENT;
            }
            while (cursor < obj_end && json[cursor] >= '0' && json[cursor] <= '9') {
                uint32_t digit = (uint32_t)(json[cursor] - '0');

                if (parsed > (0xffffffffu - digit) / 10u) {
                    return OPENAGC_ERROR_INVALID_ARGUMENT;
                }
                parsed = parsed * 10u + digit;
                cursor++;
            }
            *flag = 1;
            if (flag == &have_offset) {
                offset = parsed;
            } else {
                value = parsed;
            }
            pos = cursor;
        }
        if (have_offset == 0 || have_value == 0) {
            return OPENAGC_ERROR_INVALID_ARGUMENT;
        }
        if (count >= max_pairs) {
            return OPENAGC_ERROR_CAPACITY;
        }
        offsets[count] = offset;
        values[count] = value;
        count++;
        index = obj_end;
    }

    *out_count = count;
    return OPENAGC_OK;
}

/* Top-level JSON number for a unique key; fails if absent or duplicated. */
static inline int openagc_psbc_json_uint(const uint8_t *json, uint32_t size, const char *key,
                                         uint32_t *out)
{
    uint32_t index;
    uint32_t key_length = 0u;
    uint32_t value = 0u;
    int found = 0;

    while (key[key_length] != '\0') {
        key_length++;
    }
    for (index = 0u; index + key_length + 2u < size; ++index) {
        uint32_t cursor;

        if (json[index] != '"' || memcmp(json + index + 1u, key, key_length) != 0 ||
            json[index + 1u + key_length] != '"') {
            continue;
        }
        cursor = index + key_length + 2u;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != ':') {
            return 0;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] < '0' || json[cursor] > '9') {
            return 0;
        }
        if (found != 0) {
            return 0;
        }
        found = 1;
        value = 0u;
        while (cursor < size && json[cursor] >= '0' && json[cursor] <= '9') {
            uint32_t digit = (uint32_t)(json[cursor] - '0');

            if (value > (0xffffffffu - digit) / 10u) {
                return 0;
            }
            value = value * 10u + digit;
            cursor++;
        }
        index = cursor;
    }
    if (found == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

/* Optional number or null: present=1 and *out set, present=0 when null, -1 on error. */
static inline int openagc_psbc_json_optional_uint(const uint8_t *json, uint32_t size,
                                                  const char *key, uint32_t *out)
{
    uint32_t index;
    uint32_t key_length = 0u;
    int found = 0;
    int present = 0;
    uint32_t value = 0u;

    while (key[key_length] != '\0') {
        key_length++;
    }
    for (index = 0u; index + key_length + 2u < size; ++index) {
        uint32_t cursor;

        if (json[index] != '"' || memcmp(json + index + 1u, key, key_length) != 0 ||
            json[index + 1u + key_length] != '"') {
            continue;
        }
        cursor = index + key_length + 2u;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != ':') {
            return -1;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (found != 0) {
            return -1;
        }
        found = 1;
        if (cursor + 4u <= size && memcmp(json + cursor, "null", 4u) == 0) {
            present = 0;
            index = cursor + 4u;
            continue;
        }
        if (cursor >= size || json[cursor] < '0' || json[cursor] > '9') {
            return -1;
        }
        present = 1;
        value = 0u;
        while (cursor < size && json[cursor] >= '0' && json[cursor] <= '9') {
            uint32_t digit = (uint32_t)(json[cursor] - '0');

            if (value > (0xffffffffu - digit) / 10u) {
                return -1;
            }
            value = value * 10u + digit;
            cursor++;
        }
        index = cursor;
    }
    if (found == 0) {
        return -1;
    }
    if (present != 0) {
        *out = value;
    }
    return present;
}

static inline int openagc_psbc_json_array_is_empty(const uint8_t *json, uint32_t size,
                                                   const char *array_key)
{
    uint32_t key_len = 0u;
    uint32_t index;

    while (array_key[key_len] != '\0') {
        key_len++;
    }
    for (index = 0u; index + key_len + 2u < size; ++index) {
        uint32_t cursor;

        if (json[index] != '"' || memcmp(json + index + 1u, array_key, key_len) != 0 ||
            json[index + 1u + key_len] != '"') {
            continue;
        }
        cursor = index + key_len + 2u;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != ':') {
            return 0;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != '[') {
            return 0;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        return (cursor < size && json[cursor] == ']') ? 1 : 0;
    }
    return 0;
}

/* Nested linkage field: "name": {"offset":N,"value":M}. */
static inline int openagc_psbc_json_linkage_pair(const uint8_t *json, uint32_t size,
                                                 const char *name, uint32_t *offset,
                                                 uint32_t *value)
{
    uint32_t key_len = 0u;
    uint32_t index;

    while (name[key_len] != '\0') {
        key_len++;
    }
    for (index = 0u; index + key_len + 2u < size; ++index) {
        uint32_t cursor;
        uint32_t obj_end;
        uint32_t off = 0u;
        uint32_t val = 0u;
        int have_off = 0;
        int have_val = 0;
        uint32_t pos;

        if (json[index] != '"' || memcmp(json + index + 1u, name, key_len) != 0 ||
            json[index + 1u + key_len] != '"') {
            continue;
        }
        cursor = index + key_len + 2u;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != ':') {
            continue;
        }
        cursor++;
        while (cursor < size &&
               (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\n' ||
                json[cursor] == '\r')) {
            cursor++;
        }
        if (cursor >= size || json[cursor] != '{') {
            return 0;
        }
        obj_end = cursor + 1u;
        {
            uint32_t depth = 1u;

            while (obj_end < size && depth != 0u) {
                if (json[obj_end] == '{') {
                    depth++;
                } else if (json[obj_end] == '}') {
                    depth--;
                }
                obj_end++;
            }
            if (depth != 0u) {
                return 0;
            }
        }
        for (pos = cursor; pos + 8u < obj_end; ++pos) {
            uint32_t field_cursor;
            uint32_t parsed = 0u;
            int *flag = NULL;

            if (json[pos] != '"') {
                continue;
            }
            if (pos + 8u < obj_end && memcmp(json + pos + 1u, "offset\"", 7u) == 0) {
                flag = &have_off;
                field_cursor = pos + 8u;
            } else if (pos + 7u < obj_end && memcmp(json + pos + 1u, "value\"", 6u) == 0) {
                flag = &have_val;
                field_cursor = pos + 7u;
            } else {
                continue;
            }
            while (field_cursor < obj_end &&
                   (json[field_cursor] == ' ' || json[field_cursor] == '\t' ||
                    json[field_cursor] == '\n' || json[field_cursor] == '\r')) {
                field_cursor++;
            }
            if (field_cursor >= obj_end || json[field_cursor] != ':') {
                return 0;
            }
            field_cursor++;
            while (field_cursor < obj_end &&
                   (json[field_cursor] == ' ' || json[field_cursor] == '\t' ||
                    json[field_cursor] == '\n' || json[field_cursor] == '\r')) {
                field_cursor++;
            }
            if (field_cursor >= obj_end || json[field_cursor] < '0' ||
                json[field_cursor] > '9' || *flag != 0) {
                return 0;
            }
            while (field_cursor < obj_end && json[field_cursor] >= '0' &&
                   json[field_cursor] <= '9') {
                uint32_t digit = (uint32_t)(json[field_cursor] - '0');

                if (parsed > (0xffffffffu - digit) / 10u) {
                    return 0;
                }
                parsed = parsed * 10u + digit;
                field_cursor++;
            }
            *flag = 1;
            if (flag == &have_off) {
                off = parsed;
            } else {
                val = parsed;
            }
            pos = field_cursor;
        }
        if (have_off == 0 || have_val == 0) {
            return 0;
        }
        *offset = off;
        *value = val;
        return 1;
    }
    return 0;
}

typedef struct openagc_psbc_reflection {
    uint32_t version;
    uint32_t target;
    uint32_t source_stage;
    uint32_t hardware_stage;
    uint32_t machine_code_size;
    uint32_t unresolved_fields;
    uint32_t address32_hi;
    uint32_t user_sgpr_count;
    uint32_t context_count;
    uint32_t shader_reg_count;
    uint32_t context_offsets[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t context_values[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t shader_offsets[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t shader_values[OPENAGC_PSBC_MAX_REGISTER_PAIRS];
    uint32_t has_linkage;
    uint32_t linkage_ge_cntl_offset;
    uint32_t linkage_ge_cntl_value;
    uint32_t linkage_stages_en_offset;
    uint32_t linkage_stages_en_value;
    uint32_t linkage_user_vgpr_en_offset;
    uint32_t linkage_user_vgpr_en_value;
    uint32_t has_base_vertex_user_data;
    uint32_t base_vertex_user_data_dword;
    uint32_t has_indexed_draw_user_data;
    uint32_t is_indexed_draw_user_data_dword;
    uint32_t empty_input_semantics;
    uint32_t empty_output_semantics;
    uint32_t empty_descriptor_bindings;
} openagc_psbc_reflection;

/*
 * Parse typed reflection from PSBC metadata JSON. Does not produce OpenAGC
 * shader descriptors for runtime intake and does not enable gpu_executable.
 */
static inline openagc_result openagc_psbc_metadata_parse_reflection(
    const uint8_t *json, uint32_t size, openagc_psbc_reflection *out)
{
    int optional;
    openagc_result result;

    if (json == NULL || size < 2u || out == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (openagc_psbc_json_uint(json, size, "version", &out->version) == 0 ||
        openagc_psbc_json_uint(json, size, "target", &out->target) == 0 ||
        openagc_psbc_json_uint(json, size, "source_stage", &out->source_stage) == 0 ||
        openagc_psbc_json_uint(json, size, "hardware_stage", &out->hardware_stage) == 0 ||
        openagc_psbc_json_uint(json, size, "machine_code_size", &out->machine_code_size) == 0 ||
        openagc_psbc_json_uint(json, size, "unresolved_fields", &out->unresolved_fields) == 0 ||
        openagc_psbc_json_uint(json, size, "address32_hi", &out->address32_hi) == 0 ||
        openagc_psbc_json_uint(json, size, "user_sgpr_count", &out->user_sgpr_count) == 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    result = openagc_psbc_metadata_extract_register_pairs(
        json, size, "context_registers", out->context_offsets, out->context_values,
        OPENAGC_PSBC_MAX_REGISTER_PAIRS, &out->context_count);
    if (result != OPENAGC_OK) {
        return result;
    }
    result = openagc_psbc_metadata_extract_register_pairs(
        json, size, "shader_registers", out->shader_offsets, out->shader_values,
        OPENAGC_PSBC_MAX_REGISTER_PAIRS, &out->shader_reg_count);
    if (result != OPENAGC_OK) {
        return result;
    }
    if (out->context_count == 0u || out->shader_reg_count == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    out->empty_input_semantics =
        (uint32_t)openagc_psbc_json_array_is_empty(json, size, "input_semantics");
    out->empty_output_semantics =
        (uint32_t)openagc_psbc_json_array_is_empty(json, size, "output_semantics");
    out->empty_descriptor_bindings =
        (uint32_t)openagc_psbc_json_array_is_empty(json, size, "descriptor_bindings");
    if (out->empty_input_semantics == 0u || out->empty_output_semantics == 0u ||
        out->empty_descriptor_bindings == 0u) {
        /* Smoke fixtures are binding-less; non-empty arrays need a separate adapter review. */
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    optional = openagc_psbc_json_optional_uint(json, size, "base_vertex_user_data_dword",
                                               &out->base_vertex_user_data_dword);
    if (optional < 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    out->has_base_vertex_user_data = (uint32_t)optional;
    optional = openagc_psbc_json_optional_uint(json, size, "is_indexed_draw_user_data_dword",
                                               &out->is_indexed_draw_user_data_dword);
    if (optional < 0) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    out->has_indexed_draw_user_data = (uint32_t)optional;
    if (openagc_psbc_json_linkage_pair(json, size, "ge_cntl", &out->linkage_ge_cntl_offset,
                                       &out->linkage_ge_cntl_value) != 0 &&
        openagc_psbc_json_linkage_pair(json, size, "stages_en", &out->linkage_stages_en_offset,
                                       &out->linkage_stages_en_value) != 0 &&
        openagc_psbc_json_linkage_pair(json, size, "user_vgpr_en",
                                       &out->linkage_user_vgpr_en_offset,
                                       &out->linkage_user_vgpr_en_value) != 0) {
        out->has_linkage = 1u;
    }
    return OPENAGC_OK;
}

static inline openagc_result openagc_psbc_reflection_validate(
    const openagc_psbc_reflection *reflection, uint32_t expected_source_stage,
    uint32_t code_size)
{
    if (reflection == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (reflection->version != OPENAGC_SHADER_PINNED_PSBC_METADATA_VERSION ||
        reflection->target != 2u || reflection->source_stage != expected_source_stage ||
        reflection->machine_code_size != code_size) {
        return OPENAGC_ERROR_INTEGRITY;
    }
    if (reflection->context_count == 0u || reflection->shader_reg_count == 0u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    return OPENAGC_OK;
}

/*
 * Cross-check a caller-supplied OpenAGC artifact envelope against parsed
 * PSBC reflection. Empty descriptor_bindings require zero OpenAGC
 * bindings/textures. Non-empty PSBC bindings remain unsupported.
 */
static inline openagc_result openagc_psbc_reflection_check_artifact_desc(
    const openagc_psbc_reflection *reflection, const openagc_shader_artifact_desc *desc)
{
    if (reflection == NULL || desc == NULL) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if (desc->code_size != reflection->machine_code_size) {
        return OPENAGC_ERROR_INTEGRITY;
    }
    if (reflection->empty_descriptor_bindings == 0u) {
        return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
    }
    if (desc->binding_count != 0u || desc->texture_count != 0u) {
        return OPENAGC_ERROR_INTEGRITY;
    }
    return OPENAGC_OK;
}

#include "openagc/pm4_graphics_fw940.h"

/*
 * Host-only dword count for SET_CONTEXT_REG + SET_SH_REG (+ vertex linkage
 * context pairs when present). Never includes DRAW.
 */
static inline uint32_t openagc_psbc_reflection_register_program_dwords(
    const openagc_psbc_reflection *reflection)
{
    uint32_t count;

    if (reflection == NULL) {
        return 0u;
    }
    count = 3u * (reflection->context_count + reflection->shader_reg_count);
    if (reflection->has_linkage != 0u) {
        /* ge_cntl, stages_en, user_vgpr_en — three SET_CONTEXT_REG packets. */
        count += 9u;
    }
    return count;
}

/*
 * Host-only SET_CONTEXT_REG + SET_SH_REG snapshot from reflection.
 * Vertex linkage offsets/values (when has_linkage) append as context
 * packets from the same PSBC metadata object. Never emits DRAW.
 */
static inline uint32_t openagc_psbc_reflection_encode_register_program(
    const openagc_psbc_reflection *reflection, uint32_t *words)
{
    uint32_t cursor;

    if (reflection == NULL || words == NULL) {
        return 0u;
    }
    cursor = openagc_pm4_encode_psbc_context_pairs(
        reflection->context_offsets, reflection->context_values, reflection->context_count,
        words);
    cursor += openagc_pm4_encode_psbc_shader_pairs(
        reflection->shader_offsets, reflection->shader_values, reflection->shader_reg_count,
        words + cursor);
    if (reflection->has_linkage != 0u) {
        openagc_pm4_encode_set_context_reg(reflection->linkage_ge_cntl_offset, 1u,
                                           &reflection->linkage_ge_cntl_value, words + cursor);
        cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);
        openagc_pm4_encode_set_context_reg(reflection->linkage_stages_en_offset, 1u,
                                           &reflection->linkage_stages_en_value, words + cursor);
        cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);
        openagc_pm4_encode_set_context_reg(reflection->linkage_user_vgpr_en_offset, 1u,
                                           &reflection->linkage_user_vgpr_en_value,
                                           words + cursor);
        cursor += OPENAGC_PM4_SET_CONTEXT_WORDS(1u);
    }
    return cursor;
}

/*
 * Patch SPI_SHADER_PGM_LO/HI: OpenGNM/PSBC places them as the first two
 * shader_registers (values often 0 while unresolved_fields >= 1). Encoding
 * matches the console-proven compute path: LO = va>>8, HI = va>>40.
 * code_va must be 256-byte aligned. Does not set gpu_executable.
 */
static inline openagc_result openagc_psbc_reflection_patch_pgm_va(
    openagc_psbc_reflection *reflection, uint64_t code_va)
{
    if (reflection == NULL || reflection->shader_reg_count < 2u) {
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    }
    if ((code_va & 0xffu) != 0u) {
        return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    reflection->shader_values[0] = (uint32_t)(code_va >> 8);
    reflection->shader_values[1] = (uint32_t)(code_va >> 40);
    return OPENAGC_OK;
}

/*
 * Encode register program then the shared FW9.40 EOP+NOP trailer (no DRAW).
 * words must hold register_program_dwords + OPENAGC_PM4_EOP_WITH_NOP_WORDS.
 */
static inline uint32_t openagc_psbc_reflection_encode_register_program_eop(
    const openagc_psbc_reflection *reflection, uint32_t sequence, uint64_t marker_va,
    uint32_t *words)
{
    uint32_t cursor;

    if (reflection == NULL || words == NULL) {
        return 0u;
    }
    cursor = openagc_psbc_reflection_encode_register_program(reflection, words);
    openagc_pm4_encode_eop_with_nops(marker_va, sequence, words + cursor);
    return cursor + OPENAGC_PM4_EOP_WITH_NOP_WORDS;
}

#endif /* OPENAGC_PSBC_METADATA_H */
