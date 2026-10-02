/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#include "openagc/ps5_gpu.h"
#include "openagc/ps5_policy.h"

#include "ngg_smoke_tables.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int agc_module;
static int driver_module;
static int fail_open;
static int fail_symbol;
static int fail_init;
static int fail_submit;
static int fail_suspend;
static int complete_submit;
static int submit_count;
static int opened_count;
static int init_count;
static uint32_t firmware = 0x09400008u;
static volatile uint32_t *marker;
static uint8_t vertex_code[1024] __attribute__((aligned(256)));
static uint8_t fragment_code[1024] __attribute__((aligned(256)));
static uint8_t color[32u * 256u] __attribute__((aligned(256)));

typedef struct mock_description {
    void *words;
    uint32_t word_count;
    uint8_t flag;
    uint8_t padding[3];
} mock_description;

uint32_t op_ps5_system_firmware_version(void) { return firmware; }

static int32_t mock_init(uint32_t mode)
{
    assert(mode == 8u);
    ++init_count;
    return fail_init ? -17 : 0;
}

static int32_t mock_submit(void *request)
{
    const mock_description *desc = (const mock_description *)request;
    const uint32_t *words = (const uint32_t *)desc->words;
    uint32_t at = desc->word_count - OPENAGC_PM4_AGC_COMPLETION_WORDS;

    ++submit_count;
    assert(desc->flag == 0u && desc->padding[0] == 0u);
    assert(words[at] == OPENAGC_PM4_EOP_HEADER);
    assert(words[at + 1u] == OPENAGC_PM4_AGC_COMPLETION_CONTROL);
    assert(words[at + 2u] == OPENAGC_PM4_EOP_DATA_SEL);
    assert(words[at + 3u] == (uint32_t)(uintptr_t)marker);
    assert(words[at + 4u] == (uint32_t)((uintptr_t)marker >> 32));
    if (fail_submit) return -19;
    if (complete_submit) *marker = words[at + 5u];
    return 0;
}

static int32_t mock_suspend(void) { return fail_suspend ? -23 : 0; }

void *dlopen(const char *name, int flags)
{
    assert(flags == (RTLD_NOW | RTLD_LOCAL));
    if (fail_open) return NULL;
    if (strcmp(name, "libSceAgc.sprx") == 0) {
        ++opened_count;
        return &agc_module;
    }
    assert(strcmp(name, "libSceAgcDriver.sprx") == 0);
    ++opened_count;
    return &driver_module;
}

void *dlsym(void *module, const char *name)
{
    void *symbol = NULL;
    int32_t (*init)(uint32_t) = mock_init;
    int32_t (*submit)(void *) = mock_submit;
    int32_t (*suspend_point)(void) = mock_suspend;

    if (fail_symbol) return NULL;
    if (module == &agc_module && strcmp(name, "sceAgcInit") == 0)
        memcpy(&symbol, &init, sizeof(symbol));
    else if (module == &agc_module && strcmp(name, "sceAgcSuspendPoint") == 0)
        memcpy(&symbol, &suspend_point, sizeof(symbol));
    else if (module == &driver_module && strcmp(name, "sceAgcDriverSubmitDcb") == 0)
        memcpy(&symbol, &submit, sizeof(symbol));
    return symbol;
}

int dlclose(void *module)
{
    assert(module == &agc_module || module == &driver_module);
    --opened_count;
    return 0;
}

static void prepare_program(openagc_pm4_ngg_program *program)
{
    memset(program, 0, sizeof(*program));
    program->vertex_context = (openagc_pm4_ngg_table){
        OPENAGC_NGG_VERTEX_CONTEXT_COUNT, openagc_ngg_vertex_context_offsets,
        openagc_ngg_vertex_context_values };
    program->vertex_shader = (openagc_pm4_ngg_table){
        OPENAGC_NGG_VERTEX_SHADER_COUNT, openagc_ngg_vertex_shader_offsets,
        openagc_ngg_vertex_shader_values };
    program->linkage = (openagc_pm4_ngg_table){
        OPENAGC_NGG_LINKAGE_COUNT, openagc_ngg_linkage_offsets,
        openagc_ngg_linkage_values };
    program->fragment_context = (openagc_pm4_ngg_table){
        OPENAGC_NGG_FRAGMENT_CONTEXT_COUNT, openagc_ngg_fragment_context_offsets,
        openagc_ngg_fragment_context_values };
    program->fragment_shader = (openagc_pm4_ngg_table){
        OPENAGC_NGG_FRAGMENT_SHADER_COUNT, openagc_ngg_fragment_shader_offsets,
        openagc_ngg_fragment_shader_values };
    program->user_data = (openagc_pm4_ngg_table){
        OPENAGC_NGG_USER_DATA_COUNT, openagc_ngg_user_data_offsets,
        openagc_ngg_user_data_values };
    program->vertex_pgm_lo_slot = 0u;
    program->vertex_pgm_hi_slot = 1u;
    program->fragment_pgm_lo_slot = 0u;
    program->fragment_pgm_hi_slot = 1u;
    program->user_data_layout_slot = OPENAGC_NGG_USER_DATA_LAYOUT_SLOT;
    program->user_data_layout = OPENAGC_NGG_USER_DATA_LAYOUT;
}

static void prepare_draw(openagc_raster_gpu_draw *draw,
                         openagc_pm4_ngg_program *program,
                         uint32_t *context, uint32_t *uconfig)
{
    memset(draw, 0, sizeof(*draw));
    draw->struct_size = sizeof(*draw);
    draw->api_version = OPENAGC_RASTER_API_VERSION;
    draw->color_va = (uint64_t)(uintptr_t)color;
    draw->color_width = 32u;
    draw->color_height = 32u;
    draw->color_pitch_bytes = 128u;
    draw->viewport_x = 8u;
    draw->viewport_y = 8u;
    draw->viewport_width = 8u;
    draw->viewport_height = 8u;
    draw->viewport_y_down = 1u;
    draw->topology = OPENAGC_RASTER_TOPOLOGY_TRIANGLE_LIST;
    draw->topology_write = OPENAGC_RASTER_TOPOLOGY_WRITE_BOTH;
    draw->vertex_count = 3u;
    draw->gate_mask = OPENAGC_RASTER_GATE_ALL;
    draw->vertex_code_va = (uint64_t)(uintptr_t)vertex_code;
    draw->fragment_code_va = (uint64_t)(uintptr_t)fragment_code;
    draw->context_table = context;
    draw->context_table_va = (uint64_t)(uintptr_t)context;
    draw->uconfig_table = uconfig;
    draw->uconfig_table_va = (uint64_t)(uintptr_t)uconfig;
    draw->program = program;
}

int main(void)
{
    openagc_ps5_gpu *gpu = NULL;
    openagc_ps5_gpu_draw desc = OPENAGC_PS5_GPU_DRAW_INIT;
    openagc_ps5_gpu_submission info = OPENAGC_PS5_GPU_SUBMISSION_INIT;
    openagc_pm4_ngg_program program;
    openagc_raster_gpu_draw draw;
    uint32_t context[OPENAGC_PM4_NGG_TABLE_WORDS];
    uint32_t uconfig[OPENAGC_RASTER_UCONFIG_TABLE_WORDS];
    uint32_t words[OPENAGC_PS5_GPU_MAX_WORDS];
    uint64_t aligned_marker = 0u;
    int32_t platform_error = 0;

    /* Only an unreadable firmware identity is refused. */
    firmware = 0u;
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) ==
           OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
    assert(gpu == NULL && opened_count == 0);
    firmware = 0x04500000u;
    fail_open = 1;
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) == OPENAGC_ERROR_NOT_READY);
    assert(opened_count == 0);
    fail_open = 0;
    fail_symbol = 1;
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) ==
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    assert(opened_count == 0);
    fail_symbol = 0;
    fail_init = 1;
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) == OPENAGC_ERROR_NOT_READY);
    assert(platform_error == -17 && opened_count == 0);
    fail_init = 0;
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) == OPENAGC_OK);
    assert(gpu != NULL && opened_count == 2 && platform_error == 0);
    {
        openagc_ps5_gpu *other = NULL;
        assert(openagc_ps5_gpu_create(&other, &platform_error) ==
               OPENAGC_ERROR_BUSY);
        assert(other == NULL);
    }

    prepare_program(&program);
    prepare_draw(&draw, &program, context, uconfig);
    desc.draw = &draw;
    desc.words = words;
    desc.word_capacity = OPENAGC_PS5_GPU_MAX_WORDS;
    desc.marker = (volatile uint32_t *)&aligned_marker;
    desc.vertex_code = vertex_code;
    desc.vertex_code_bytes = sizeof(openagc_ngg_vert_code);
    desc.fragment_code = fragment_code;
    desc.fragment_code_bytes = sizeof(openagc_ngg_frag_code);
    desc.color = color;
    desc.color_bytes = sizeof(color);
    assert(sizeof(openagc_ngg_vert_code) <= sizeof(vertex_code));
    assert(sizeof(openagc_ngg_frag_code) <= sizeof(fragment_code));
    memcpy(vertex_code, openagc_ngg_vert_code, sizeof(openagc_ngg_vert_code));
    memcpy(fragment_code, openagc_ngg_frag_code, sizeof(openagc_ngg_frag_code));
    marker = desc.marker;

    draw.append_eop = 1u;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_INVALID_ARGUMENT);
    assert(submit_count == 0);
    draw.append_eop = 0u;
    desc.color_bytes = 128u * 32u;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_OUT_OF_RANGE);
    desc.color_bytes = sizeof(color);
    desc.word_capacity = 8u;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_INVALID_ARGUMENT);
    desc.word_capacity = OPENAGC_PS5_GPU_MAX_WORDS;
    fail_submit = 1;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_NOT_READY);
    assert(info.platform_error == -19 && info.submitted == 1u);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_ERROR_BUSY);
    fail_submit = 0;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_BUSY);
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_ERROR_NOT_READY);
    *marker = info.sequence;
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_OK);
    assert(info.platform_error == 0 && info.completed == 1u);
    fail_suspend = 1;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) ==
           OPENAGC_ERROR_NOT_READY);
    assert(info.platform_error == -23 && info.submitted == 1u);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_ERROR_BUSY);
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) == OPENAGC_ERROR_BUSY);
    fail_suspend = 0;
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_ERROR_NOT_READY);
    *marker = UINT32_MAX;
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_ERROR_INTEGRITY);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_ERROR_BUSY);
    *marker = info.sequence;
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_OK);
    assert(info.completed == 1u && info.submitted == 1u &&
           info.platform_error == 0);
    assert(openagc_ps5_gpu_wait(gpu, 0u, &info) == OPENAGC_ERROR_BAD_STATE);

    complete_submit = 1;
    assert(openagc_ps5_gpu_submit_draw(gpu, &desc, 0u, &info) == OPENAGC_OK);
    assert(info.completed == 1u && info.sequence == 3u);
    assert(info.word_count > OPENAGC_PM4_AGC_COMPLETION_WORDS);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_OK);
    assert(opened_count == 2 && init_count == 2);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_ERROR_BAD_STATE);
    assert(openagc_ps5_gpu_create(&gpu, &platform_error) == OPENAGC_OK);
    assert(opened_count == 2 && init_count == 2);
    assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_OK);
    {
        static const uint32_t firmwares[] = {
            0x06020000u, 0x09400008u, 0x10000000u
        };
        size_t i;

        for (i = 0u; i < sizeof(firmwares) / sizeof(firmwares[0]); ++i) {
            firmware = firmwares[i];
            assert(openagc_ps5_gpu_create(&gpu, &platform_error) == OPENAGC_OK);
            assert(openagc_ps5_gpu_destroy(gpu) == OPENAGC_OK);
        }
        firmware = 0u;
        gpu = NULL;
        assert(openagc_ps5_gpu_create(&gpu, &platform_error) ==
               OPENAGC_ERROR_UNSUPPORTED_FIRMWARE);
        assert(gpu == NULL);
    }
    puts("PS5 AGC submission contract: accepted/refused/timeout paths passed");
    return 0;
}
