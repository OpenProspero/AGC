/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/pm4_ngg_draw_fw940.h"
#include "openagc/raster.h"

#include "ngg_smoke_tables.h"

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

#define COLOR_VA UINT64_C(0x0000000200024000)
#define VERTEX_CODE_VA UINT64_C(0x0000000200020000)
#define FRAGMENT_CODE_VA UINT64_C(0x0000000200020100)
#define CONTEXT_TABLE_VA UINT64_C(0x0000000200022000)
#define UCONFIG_TABLE_VA UINT64_C(0x0000000200023000)
#define BASELINE_VA UINT64_C(0x0000000200021000)
#define PROBE_VA UINT64_C(0x0000000200021100)
#define NGG_PROBE_VA UINT64_C(0x0000000200021200)
#define MARKER_VA UINT64_C(0x0000000200024000)

#define PACKET_OPCODE(header) (((header) >> 8) & 0xffu)
#define PACKET_DATA_DWORDS(header) ((((header) >> 16) & 0x3fffu) + 1u)
#define PACKET_WORDS(header) (PACKET_DATA_DWORDS(header) + 1u)

static void make_program(openagc_pm4_ngg_program *program)
{
    memset(program, 0, sizeof(*program));
    program->vertex_context.count = OPENAGC_NGG_VERTEX_CONTEXT_COUNT;
    program->vertex_context.offsets = openagc_ngg_vertex_context_offsets;
    program->vertex_context.values = openagc_ngg_vertex_context_values;
    program->vertex_shader.count = OPENAGC_NGG_VERTEX_SHADER_COUNT;
    program->vertex_shader.offsets = openagc_ngg_vertex_shader_offsets;
    program->vertex_shader.values = openagc_ngg_vertex_shader_values;
    program->linkage.count = OPENAGC_NGG_LINKAGE_COUNT;
    program->linkage.offsets = openagc_ngg_linkage_offsets;
    program->linkage.values = openagc_ngg_linkage_values;
    program->fragment_context.count = OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT;
    program->fragment_context.offsets = openagc_ngg_fragment_context_offsets;
    program->fragment_context.values = openagc_ngg_fragment_context_values;
    program->fragment_shader.count = OPENAGC_NGG_FRAGMENT_SHADER_COUNT;
    program->fragment_shader.offsets = openagc_ngg_fragment_shader_offsets;
    program->fragment_shader.values = openagc_ngg_fragment_shader_values;
    program->vertex_pgm_lo_slot = 0u;
    program->vertex_pgm_hi_slot = 1u;
    program->fragment_pgm_lo_slot = 0u;
    program->fragment_pgm_hi_slot = 1u;
    program->user_data.count = OPENAGC_NGG_USER_DATA_COUNT;
    program->user_data.offsets = openagc_ngg_user_data_offsets;
    program->user_data.values = openagc_ngg_user_data_values;
    program->user_data_layout_slot = OPENAGC_NGG_USER_DATA_LAYOUT_SLOT;
    program->user_data_layout = OPENAGC_NGG_USER_DATA_LAYOUT;
    program->di_primitive = OPENAGC_GFX10_DI_PT_TRILIST;
}

static void make_draw(openagc_raster_gpu_draw *draw,
                      openagc_pm4_ngg_program *program, uint32_t *context_table,
                      uint32_t *uconfig_table)
{
    memset(draw, 0, sizeof(*draw));
    draw->struct_size = (uint32_t)sizeof(*draw);
    draw->api_version = OPENAGC_RASTER_API_VERSION;
    draw->color_va = COLOR_VA;
    draw->color_width = 32u;
    draw->color_height = 32u;
    draw->color_pitch_bytes = 128u;
    draw->color_bgra = 0u;
    draw->viewport_x = 8u;
    draw->viewport_y = 8u;
    draw->viewport_width = 8u;
    draw->viewport_height = 8u;
    draw->topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;
    draw->topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw->vertex_count = 3u;
    draw->program = program;
    draw->vertex_code_va = VERTEX_CODE_VA;
    draw->fragment_code_va = FRAGMENT_CODE_VA;
    draw->baseline_va = BASELINE_VA;
    draw->probe_va = PROBE_VA;
    draw->ngg_probe_va = NGG_PROBE_VA;
    draw->context_table_va = CONTEXT_TABLE_VA;
    draw->context_table = context_table;
    draw->uconfig_table_va = UCONFIG_TABLE_VA;
    draw->uconfig_table = uconfig_table;
    draw->gate_mask = OPENAGC_RASTER_GATE_ALL;
    draw->append_eop = 1u;
    draw->sequence = 7u;
    draw->marker_va = MARKER_VA;
}

/* Walks the IB's type-3 packets and reports what it found. */
typedef struct packet_walk {
    uint32_t context_tables;
    uint32_t uconfig_tables;
    uint32_t context_table_records;
    uint32_t uconfig_table_records;
    uint32_t uconfig_writes;
    uint32_t uconfig_write_offset;
    uint32_t uconfig_write_value;
    uint32_t context_writes;
    uint32_t gs_out_prim_written;
    uint32_t gs_out_prim;
    uint32_t draw_auto;
    uint32_t draw_auto_vertices;
    uint32_t draw_auto_initiator;
    uint32_t draw_index_2;
    uint32_t draw_index_max_size;
    uint32_t draw_index_va_lo;
    uint32_t draw_index_va_hi;
    uint32_t draw_index_count;
    uint32_t draw_index_initiator;
    uint32_t index_type_set;
    uint32_t index_type_offset;
    uint32_t index_type_value;
    uint32_t generic_scissor_tl;
    uint32_t generic_scissor_br;
    uint32_t depth_range[2];
    uint32_t num_instances;
    uint32_t cb_base_written;
    uint32_t cb_base_value;
} packet_walk;

static uint32_t walk_ib(const uint32_t *words, uint32_t count, packet_walk *walk)
{
    uint32_t index = 0u;

    memset(walk, 0, sizeof(*walk));
    while (index < count) {
        uint32_t header = words[index];
        uint32_t opcode;
        uint32_t data;
        uint32_t i;

        if ((header >> 30) != 3u) {
            return 0u;
        }
        opcode = PACKET_OPCODE(header);
        data = PACKET_DATA_DWORDS(header);
        if (index + 1u + data > count) {
            return 0u;
        }
        if (opcode == OPENAGC_PM4_OP_CONTEXT_TABLE_LOAD) {
            ++walk->context_tables;
            walk->context_table_records = words[index + 4u];
        } else if (opcode == OPENAGC_PM4_OP_UCONFIG_TABLE_LOAD) {
            ++walk->uconfig_tables;
            walk->uconfig_table_records = words[index + 4u];
        } else if (opcode == OPENAGC_PM4_OP_SET_UCONFIG_REG) {
            for (i = 0u; i + 1u < data; i += 2u) {
                ++walk->uconfig_writes;
                walk->uconfig_write_offset = words[index + 1u + i];
                walk->uconfig_write_value = words[index + 2u + i];
            }
        } else if (opcode == OPENAGC_PM4_OP_SET_CONTEXT_REG) {
            /* One packet is an offset dword plus its contiguous values. */
            uint32_t first = words[index + 1u];
            uint32_t values = data - 1u;

            for (i = 0u; i < values; ++i) {
                uint32_t offset = first + i;

                ++walk->context_writes;
                if (offset == OPENAGC_GFX10_VGT_GS_OUT_PRIM_TYPE) {
                    walk->gs_out_prim_written = 1u;
                    walk->gs_out_prim = words[index + 2u + i];
                }
                if (offset == OPENAGC_GFX10_CB_COLOR0_BASE) {
                    walk->cb_base_written = 1u;
                    walk->cb_base_value = words[index + 2u + i];
                }
                if (offset == OPENAGC_GFX10_PA_SC_GENERIC_SCISSOR_TL) {
                    walk->generic_scissor_tl = words[index + 2u + i];
                }
                if (offset == OPENAGC_GFX10_PA_SC_GENERIC_SCISSOR_BR) {
                    walk->generic_scissor_br = words[index + 2u + i];
                }
                if (offset == OPENAGC_GFX10_PA_SC_VPORT_ZMIN_0) {
                    walk->depth_range[0] = words[index + 2u + i];
                }
                if (offset == OPENAGC_GFX10_PA_SC_VPORT_ZMAX_0) {
                    walk->depth_range[1] = words[index + 2u + i];
                }
            }
        } else if (opcode == OPENAGC_PM4_OP_DRAW_INDEX_2) {
            ++walk->draw_index_2;
            walk->draw_index_max_size = words[index + 1u];
            walk->draw_index_va_lo = words[index + 2u];
            walk->draw_index_va_hi = words[index + 3u];
            walk->draw_index_count = words[index + 4u];
            walk->draw_index_initiator = words[index + 5u];
        } else if (opcode == OPENAGC_PM4_OP_SET_UCONFIG_REG_INDEX) {
            ++walk->index_type_set;
            walk->index_type_offset = words[index + 1u];
            walk->index_type_value = words[index + 2u];
        } else if (opcode == OPENAGC_PM4_OP_DRAW_INDEX_AUTO) {
            ++walk->draw_auto;
            walk->draw_auto_vertices = words[index + 1u];
            walk->draw_auto_initiator = words[index + 2u];
        } else if (opcode == OPENAGC_PM4_OP_NUM_INSTANCES) {
            ++walk->num_instances;
        }
        index += 1u + data;
    }
    return index == count ? 1u : 0u;
}

/* Walks the IB for one SET_CONTEXT_REG record with this offset and value. */
static uint32_t ib_has_context_word(const uint32_t *words, uint32_t count,
                                    uint32_t offset, uint32_t value)
{
    uint32_t index = 0u;

    while (index < count) {
        uint32_t header = words[index];
        uint32_t data;
        uint32_t i;

        if ((header >> 30) != 3u) {
            return 0u;
        }
        data = PACKET_DATA_DWORDS(header);
        if (index + 1u + data > count) {
            return 0u;
        }
        if (PACKET_OPCODE(header) == OPENAGC_PM4_OP_SET_CONTEXT_REG) {
            uint32_t first = words[index + 1u];
            uint32_t values = data - 1u;

            for (i = 0u; i < values; ++i) {
                if (first + i == offset && words[index + 2u + i] == value) {
                    return 1u;
                }
            }
        }
        index += 1u + data;
    }
    return 0u;
}

static uint32_t table_has(const uint32_t *table, uint32_t records, uint32_t offset,
                          uint32_t value)
{
    uint32_t i;

    for (i = 0u; i < records; ++i) {
        if (table[2u * i] == offset && table[2u * i + 1u] == value) {
            return 1u;
        }
    }
    return 0u;
}

static int test_raster_encodes_ngg_draw(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    packet_walk walk;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t count;

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);
    memset(context_table, 0xcc, sizeof(context_table));
    memset(uconfig_table, 0xcc, sizeof(uconfig_table));

    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && count <= OPENAGC_RASTER_MAX_WORDS);
    CHECK(walk_ib(words, count, &walk) != 0u);

    /* One draw, three vertices, auto-indexed, one instance. */
    CHECK(walk.draw_auto == 1u && walk.draw_auto_vertices == 3u);
    CHECK(walk.draw_auto_initiator == OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX);
    CHECK(walk.num_instances == 1u);

    /* The context table: vertex records, the context linkage record and
     * the pixel records, and no uconfig index in it. */
    CHECK(walk.context_tables == 1u);
    CHECK(walk.context_table_records ==
          OPENAGC_NGG_VERTEX_CONTEXT_COUNT + 1u + OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT);
    CHECK(table_has(context_table, walk.context_table_records, 725u, 0x12010u));
    CHECK(table_has(context_table, walk.context_table_records, 143u, 15u));
    CHECK(OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE == 0x2abu);
    CHECK(OPENAGC_GFX10_GE_NGG_SUBGRP_CNTL == 0x2d3u);
    CHECK(table_has(context_table, walk.context_table_records,
                    OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE, 0u));
    CHECK(table_has(context_table, walk.context_table_records,
                    OPENAGC_GFX10_GE_NGG_SUBGRP_CNTL, 1u));
    CHECK(table_has(context_table, walk.context_table_records, 0x25bu,
                    0x10080u) == 0u);
    CHECK(table_has(context_table, walk.context_table_records, 0x262u,
                    0u) == 0u);

    /* The uconfig table: GRBM-free linkage records with this draw's
     * topology, loaded the way the public native runtime loads them. */
    CHECK(walk.uconfig_tables == 1u && walk.uconfig_table_records == 3u);
    CHECK(table_has(uconfig_table, 3u, OPENAGC_GFX10_UCONFIG_GE_CNTL, 0x10080u));
    CHECK(table_has(uconfig_table, 3u, 610u, 0u));
    CHECK(table_has(uconfig_table, 3u, OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE,
                    OPENAGC_GFX10_DI_PT_TRILIST));

    /* The plain form carries the same topology. */
    CHECK(walk.uconfig_writes == 1u);
    CHECK(walk.uconfig_write_offset == OPENAGC_GFX10_UCONFIG_VGT_PRIMITIVE_TYPE);
    CHECK(walk.uconfig_write_value == OPENAGC_GFX10_DI_PT_TRILIST);

    /* The NGG rasterized primitive is the triangle strip Sony's own NGG
     * triangle draw carries, and the colour bind is this target's. */
    CHECK(walk.gs_out_prim_written == 1u);
    CHECK(walk.gs_out_prim == OPENAGC_GFX10_GS_OUT_TRISTRIP);
    CHECK(walk.cb_base_written == 1u && walk.cb_base_value == (uint32_t)(COLOR_VA >> 8));
    CHECK(openagc_gfx10_fragment_gate_values[OPENAGC_GFX10_FRAGMENT_GATE_COUNT - 1u] ==
          openagc_gfx10_draw_cb_color_control_normal());
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_CB_COLOR_CONTROL,
                              openagc_gfx10_draw_cb_color_control_normal()) == 1u);
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_CB_COLOR_CONTROL,
                              OPENAGC_GFX10_CB_COLOR_CONTROL_DISABLE) == 0u);

    /* The trailer is the shared EOP+NOP block with this draw's marker. */
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS] == OPENAGC_PM4_EOP_HEADER);
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS + 5u] == draw.sequence);
    CHECK(words[count - OPENAGC_PM4_EOP_WITH_NOP_WORDS + 3u] == (uint32_t)MARKER_VA);

    /* A short IB buffer must leave all three caller-owned outputs untouched. */
    memset(words, 0xa5, sizeof(words));
    memset(context_table, 0xcc, sizeof(context_table));
    memset(uconfig_table, 0xdd, sizeof(uconfig_table));
    CHECK(openagc_raster_encode_draw(&draw, words, count - 1u) == 0u);
    CHECK(words[0] == 0xa5a5a5a5u && words[count - 1u] == 0xa5a5a5a5u);
    CHECK(context_table[0] == 0xccccccccu && uconfig_table[0] == 0xddddddddu);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    return 0;
}

static int test_raster_topology_forms(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    packet_walk walk;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t count;

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);

    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_INDIRECT_TABLE;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    CHECK(walk.uconfig_tables == 1u && walk.uconfig_writes == 0u);

    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_PLAIN_UCONFIG;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    CHECK(walk.uconfig_tables == 0u && walk.uconfig_writes == 1u);
    CHECK(walk.uconfig_write_value == OPENAGC_GFX10_DI_PT_TRILIST);

    /* A point list is a point in both spaces. */
    draw.topology = OPENAGC_RASTER_TOPOLOGY_POINT_LIST;
    draw.vertex_count = 1u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    CHECK(walk.uconfig_writes == 1u);
    CHECK(walk.uconfig_write_value == OPENAGC_GFX10_DI_PT_POINTLIST);
    CHECK(walk.gs_out_prim == OPENAGC_GFX10_GS_OUT_POINTLIST);
    CHECK(walk.draw_auto_vertices == 1u);

    /* A line list has pairs of vertices and rasterizes as a line strip. */
    draw.topology = OPENAGC_RASTER_TOPOLOGY_LINE_LIST;
    draw.vertex_count = 2u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(walk.uconfig_write_value == OPENAGC_GFX10_DI_PT_LINELIST);
    CHECK(walk.gs_out_prim == OPENAGC_GFX10_GS_OUT_LINESTRIP);
    CHECK(walk.draw_auto_vertices == 2u);
    draw.vertex_count = 3u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    return 0;
}

/* The FW9.40 probe must exercise context 0x2ab, which the public C1 draw
 * carries as 1. Context 0x2d3 is a different NGG control register. */
static int test_raster_native_ring_variant(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t vertex_values[OPENAGC_NGG_VERTEX_CONTEXT_COUNT];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t i;

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);
    memcpy(vertex_values, openagc_ngg_vertex_context_values, sizeof(vertex_values));
    for (i = 0u; i < OPENAGC_NGG_VERTEX_CONTEXT_COUNT; ++i) {
        if (openagc_ngg_vertex_context_offsets[i] ==
            OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE) {
            vertex_values[i] = 1u;
            break;
        }
    }
    CHECK(i < OPENAGC_NGG_VERTEX_CONTEXT_COUNT);
    program.vertex_context.values = vertex_values;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) > 0u);
    CHECK(table_has(context_table,
                    OPENAGC_NGG_VERTEX_CONTEXT_COUNT + 1u +
                        OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT,
                    OPENAGC_GFX10_VGT_ESGS_RING_ITEMSIZE, 1u));
    CHECK(table_has(context_table,
                    OPENAGC_NGG_VERTEX_CONTEXT_COUNT + 1u +
                        OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT,
                    OPENAGC_GFX10_GE_NGG_SUBGRP_CNTL, 1u));
    return 0;
}

/*
 * A second pass: no EOP, and the caller's own COLOR0 record set instead
 * of the nine-word linear bind.
 */
static int test_raster_explicit_bind_and_pass(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    packet_walk walk;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t capture_values[OPENAGC_GFX10_CB_CAPTURE_COUNT];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t count;

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);

    /* The capture set with this target's base and geometry. */
    CHECK(openagc_gfx10_cb_capture_words(COLOR_VA, 64u, 16u, capture_values) == 1u);
    CHECK(capture_values[0] == (uint32_t)(COLOR_VA >> 8));
    CHECK(capture_values[2] == OPENAGC_GFX10_CB_CAPTURE_INFO);
    CHECK(capture_values[4] == OPENAGC_GFX10_CB_CAPTURE_DCC_CONTROL);
    CHECK(capture_values[14] == ((15u) | (63u << 14)));
    CHECK(capture_values[15] == OPENAGC_GFX10_CB_CAPTURE_ATTRIB3);
    CHECK(openagc_gfx10_cb_capture_words(COLOR_VA + 4u, 64u, 16u, capture_values) == 0u);

    draw.cb_bind_offsets = openagc_gfx10_cb_capture_offsets;
    draw.cb_bind_values = capture_values;
    draw.cb_bind_count = OPENAGC_GFX10_CB_CAPTURE_COUNT;
    draw.append_eop = 0u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    CHECK(words[count - 1u] == OPENAGC_PM4_DI_SRC_SEL_AUTO_INDEX);
    CHECK(walk_ib(words, count, &walk) != 0u);
    CHECK(walk.draw_auto == 1u && walk.num_instances == 1u);
    CHECK(walk.cb_base_written == 1u && walk.cb_base_value == (uint32_t)(COLOR_VA >> 8));
    /* 28 scalar records, the viewport/guardband/scissor/depth sequences
     * and the caller's 16-record bind. */
    CHECK(walk.context_writes == OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 6u + 5u +
                                     2u + 2u + 2u +
                                     OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT +
                                     OPENAGC_GFX10_FRAGMENT_GATE_COUNT +
                                     OPENAGC_GFX10_CB_CAPTURE_COUNT);
    /* The capture-only records are the evidence this path was taken: the
     * nine-word bind never writes DCC_CONTROL. */
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_CB_COLOR0_DCC_CONTROL,
                              OPENAGC_GFX10_CB_CAPTURE_DCC_CONTROL) == 1u);
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_CB_COLOR0_ATTRIB3,
                              OPENAGC_GFX10_CB_CAPTURE_ATTRIB3) == 1u);
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_CB_COLOR0_ATTRIB,
                              0u) == 1u);

    /* A gate mask selects which of the block's registers are written. */
    draw.gate_mask = 0u;
    draw.append_eop = 1u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(walk.context_writes == OPENAGC_PM4_DRAW_POINT_STATE_COUNT + 6u + 5u +
                                     2u + 2u + 2u +
                                     OPENAGC_GFX10_SPI_PS_INPUT_CNTL_COUNT +
                                     OPENAGC_GFX10_CB_CAPTURE_COUNT);
    draw.gate_mask = 1u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u);
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_PA_CL_VTE_CNTL,
                              OPENAGC_GFX10_VTE_CNTL_VIEWPORT_TRANSFORM) == 1u);
    CHECK(ib_has_context_word(words, count, OPENAGC_GFX10_DB_DEPTH_CONTROL,
                              0u) == 0u);
    draw.gate_mask = OPENAGC_RASTER_GATE_ALL | 0x2000u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.gate_mask = OPENAGC_RASTER_GATE_ALL;

    /* A count above the capture set, or a count without its words, is
     * refused rather than emitted short. */
    draw.cb_bind_count = OPENAGC_RASTER_CB_BIND_MAX + 1u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.cb_bind_count = OPENAGC_GFX10_CB_CAPTURE_COUNT;
    draw.cb_bind_values = NULL;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    return 0;
}

static int test_raster_indexed_draw(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    packet_walk walk;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];
    uint32_t count;
    static const uint64_t index_va = UINT64_C(0x0000000200021c00);

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);
    draw.index_va = index_va;
    draw.index_count = 3u;
    draw.index_size = 2u;
    draw.index_max_size = 3u;

    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(openagc_raster_encode_draw(&draw, words, count) == count);
    CHECK(walk.draw_auto == 0u && walk.draw_index_2 == 1u);
    CHECK(walk.draw_index_max_size == 3u && walk.draw_index_count == 3u);
    CHECK(walk.draw_index_va_lo == (uint32_t)index_va);
    CHECK(walk.draw_index_va_hi == (uint32_t)(index_va >> 32));
    CHECK(walk.draw_index_initiator == OPENAGC_PM4_DI_SRC_SEL_DMA);
    CHECK(walk.index_type_set == 1u);
    CHECK(walk.index_type_offset ==
          (OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE |
           OPENAGC_PM4_UCONFIG_VGT_INDEX_TYPE_HDR(
               OPENAGC_GFX10_UCONFIG_VGT_INDEX_TYPE_INDEX)));
    CHECK(walk.index_type_value == 0u);

    draw.index_size = 4u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(walk.index_type_value == 1u);

    draw.index_size = 3u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.index_size = 2u;
    draw.index_max_size = 2u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.index_max_size = 3u;
    draw.index_count = 0u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.index_count = 4u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.index_count = 3u;
    draw.index_va = index_va + 1u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.index_va = 0u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);

    /* An auto-indexed draw leaves the index fields at zero. */
    draw.index_va = 0u;
    draw.index_count = 0u;
    draw.index_size = 0u;
    draw.index_max_size = 0u;
    count = openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS);
    CHECK(count > 0u && walk_ib(words, count, &walk) != 0u);
    CHECK(walk.draw_auto == 1u && walk.draw_index_2 == 0u && walk.index_type_set == 0u);
    return 0;
}

static int test_raster_refusals(void)
{
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t context_table[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig_table[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t words[OPENAGC_RASTER_MAX_WORDS];

    make_program(&program);
    make_draw(&draw, &program, context_table, uconfig_table);

    draw.vertex_count = 0u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.vertex_count = 2u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.vertex_count = 3u;

    draw.topology = 99u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;

    draw.topology_write = 99u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;

    draw.viewport_width = 31u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.viewport_width = 8u;
    draw.viewport_x = 0u;
    draw.viewport_y = 0u;
    draw.viewport_width = 0u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.viewport_width = 8u;

    draw.color_va = COLOR_VA + 4u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.color_va = COLOR_VA;
    draw.vertex_code_va = VERTEX_CODE_VA + 4u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.vertex_code_va = VERTEX_CODE_VA;
    draw.color_width = UINT32_MAX;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.color_width = 32u;
    draw.color_pitch_bytes = 132u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.color_pitch_bytes = 128u;
    draw.viewport_x = UINT32_MAX;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.viewport_x = 0u;
    draw.color_va = 0u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.color_va = COLOR_VA;

    program.vertex_pgm_lo_slot = 2u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    program.vertex_pgm_lo_slot = 0u;

    draw.context_table = NULL;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.context_table = context_table;
    draw.context_table_va = CONTEXT_TABLE_VA + 1u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.context_table_va = CONTEXT_TABLE_VA;
    draw.uconfig_table_va = CONTEXT_TABLE_VA + 16u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.uconfig_table_va = UCONFIG_TABLE_VA;
    draw.uconfig_table = NULL;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.uconfig_table = uconfig_table;

    draw.program = NULL;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.program = &program;

    /* A uconfig index in the vertex context table is a routing error. */
    {
        uint32_t bad_offsets[OPENAGC_NGG_VERTEX_CONTEXT_COUNT];

        memcpy(bad_offsets, openagc_ngg_vertex_context_offsets, sizeof(bad_offsets));
        bad_offsets[0] = OPENAGC_GFX10_UCONFIG_GE_CNTL;
        program.vertex_context.offsets = bad_offsets;
        CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
        program.vertex_context.offsets = openagc_ngg_vertex_context_offsets;
    }

    /* A linkage block without GE_CNTL is an incomplete NGG program. */
    {
        static const uint32_t context_only_offsets[1] = {725u};
        static const uint32_t context_only_values[1] = {0x12010u};
        openagc_pm4_ngg_table saved = program.linkage;

        program.linkage.count = 1u;
        program.linkage.offsets = context_only_offsets;
        program.linkage.values = context_only_values;
        CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
        program.linkage = saved;
    }

    CHECK(openagc_raster_encode_draw(&draw, words, 16u) == 0u);
    CHECK(openagc_raster_encode_draw(NULL, words, OPENAGC_RASTER_MAX_WORDS) == 0u);

    draw.struct_size = (uint32_t)sizeof(draw) - 4u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.struct_size = (uint32_t)sizeof(draw);
    draw.api_version = OPENAGC_RASTER_API_VERSION + 1u;
    CHECK(openagc_raster_encode_draw(&draw, words, OPENAGC_RASTER_MAX_WORDS) == 0u);
    draw.api_version = OPENAGC_RASTER_API_VERSION;
    return 0;
}

static int test_raster_init_macro(void)
{
    openagc_raster_gpu_draw draw = OPENAGC_RASTER_GPU_DRAW_INIT;

    CHECK(draw.struct_size == (uint32_t)sizeof(draw));
    CHECK(draw.api_version == OPENAGC_RASTER_API_VERSION);
    CHECK(draw.color_va == 0u && draw.color_width == 0u && draw.color_height == 0u);
    CHECK(draw.color_pitch_bytes == 0u && draw.color_bgra == 0u);
    CHECK(draw.viewport_x == 0u && draw.viewport_y == 0u);
    CHECK(draw.viewport_width == 0u && draw.viewport_height == 0u);
    CHECK(draw.topology == 0u && draw.topology_write == 0u);
    CHECK(draw.vertex_count == 0u && draw.program == NULL);
    CHECK(draw.vertex_code_va == 0u && draw.fragment_code_va == 0u);
    CHECK(draw.baseline_va == 0u && draw.probe_va == 0u && draw.ngg_probe_va == 0u);
    CHECK(draw.context_table_va == 0u && draw.context_table == NULL);
    CHECK(draw.uconfig_table_va == 0u && draw.uconfig_table == NULL);
    CHECK(draw.index_va == 0u && draw.index_count == 0u && draw.index_size == 0u);
    CHECK(draw.index_max_size == 0u);
    CHECK(draw.cb_bind_offsets == NULL && draw.cb_bind_values == NULL);
    CHECK(draw.cb_bind_count == 0u && draw.append_eop == 1u);
    CHECK(draw.gate_mask == OPENAGC_RASTER_GATE_ALL);
    CHECK(draw.sequence == 0u && draw.marker_va == 0u);
    return 0;
}

static int test_raster_capabilities(void)
{
    openagc_raster_capabilities capabilities = OPENAGC_RASTER_CAPABILITIES_INIT;

    EXPECT(openagc_raster_get_capabilities(&capabilities), OPENAGC_OK);
    CHECK(capabilities.pm4_draw_encoding == 1u);
    /* No console run has produced a pixel yet: the pin is the gate. */
    CHECK(capabilities.gpu_rasterization == OPENAGC_RASTER_GPU_QUALIFIED);
    CHECK(OPENAGC_RASTER_GPU_QUALIFIED == 0u);
    CHECK(capabilities.host_rasterization == 0u);
    CHECK(capabilities.evidence_pin_count == 0u);
    CHECK(capabilities.topology_write_forms == 3u);
    CHECK(capabilities.max_words == OPENAGC_RASTER_MAX_WORDS);
    EXPECT(openagc_raster_get_capabilities(NULL), OPENAGC_ERROR_INVALID_ARGUMENT);
    capabilities.struct_size -= 4u;
    EXPECT(openagc_raster_get_capabilities(&capabilities),
           OPENAGC_ERROR_INCOMPATIBLE_VERSION);
    return 0;
}

int main(void)
{
    if (test_raster_encodes_ngg_draw() != 0 ||
        test_raster_topology_forms() != 0 ||
        test_raster_native_ring_variant() != 0 ||
        test_raster_indexed_draw() != 0 ||
        test_raster_explicit_bind_and_pass() != 0 ||
        test_raster_refusals() != 0 ||
        test_raster_init_macro() != 0 ||
        test_raster_capabilities() != 0) {
        return 1;
    }
    return 0;
}
