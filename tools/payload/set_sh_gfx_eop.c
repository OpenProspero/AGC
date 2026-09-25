/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step O: graphics-bank SET_SH_REG + EOP for FW9.40 (no SET_CONTEXT, no DRAW).
 *
 * Contract (docs/hardware-evidence.md): one submit of smoke.vert shader
 * register pairs (PGM patched to uploaded code VA) plus the shared
 * EOP+NOP trailer. Proves graphics-bank SET_SH (low_bits=0) completes.
 * Does not unlock CB/DB, DRAW, or host gpu_execution.
 *
 * Packet layout: include/openagc/pm4_graphics_fw940.h
 */

#include "openagc/pm4_graphics_fw940.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

extern int sceKernelAllocateMainDirectMemory(size_t len, size_t alignment,
                                             int memory_type, off_t *physical);
extern int sceKernelMapNamedDirectMemory(void **address, size_t len,
                                         int protection, int flags,
                                         off_t physical, size_t alignment,
                                         const char *name);

#define OPENAGC_SUBMIT_16 0xC0108102u
#define OPENAGC_CONTEXT_QUERY 0xC004812Eu

#define OPENAGC_PAGE 0x4000u
#define OPENAGC_ARENA (128u * 1024u)
#define OPENAGC_CODE_OFF 0x0000u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#define OPENAGC_SH_PAIR_COUNT 4u
#define OPENAGC_STEP_O_WORDS OPENAGC_PM4_GRAPHICS_SH_EOP_WORDS(OPENAGC_SH_PAIR_COUNT)

#define OPENAGC_PROT_READ 0x01
#define OPENAGC_PROT_WRITE 0x02
#define OPENAGC_PROT_GPU_READ 0x10
#define OPENAGC_PROT_GPU_WRITE 0x20
#define OPENAGC_MAP_NO_COALESCE 0x400000

struct openagc_submit {
    uint32_t queue_type;
    uint32_t num_cbs;
    uint64_t cb_array;
};

struct openagc_cb {
    uint64_t header;
    uint64_t ib_base;
};

/* tests/fixtures/psbc_smoke/smoke.vert.gfx1013.bin (44 bytes). */
static const uint8_t openagc_smoke_vert_code[] = {
    0x03, 0x00, 0x8f, 0xbf, 0x80, 0x02, 0x00, 0x7e, 0xf2, 0x02, 0x02, 0x7e,
    0xcf, 0x08, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x81, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf
};

/* smoke.vert.metadata.json shader_registers offsets; PGM values patched at runtime. */
static const uint32_t openagc_sh_offsets[OPENAGC_SH_PAIR_COUNT] = {
    72u, 73u, 74u, 75u
};
static const uint32_t openagc_sh_rsrc1 = 137101313u;
static const uint32_t openagc_sh_rsrc2 = 8u;

static const char openagc_log_path[] = "/data/prosperoai/openagc-set-sh-gfx-eop.log";

static int openagc_log(const char *line)
{
    FILE *handle = fopen(openagc_log_path, "w");

    if (handle == NULL) {
        return -1;
    }
    fputs(line, handle);
    fclose(handle);
    return 0;
}

static int openagc_logf(const char *fmt, ...)
{
    char line[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    return openagc_log(line);
}

static int openagc_elapsed_seconds(const struct timeval *start)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (int)(now.tv_sec - start->tv_sec);
}

int main(void)
{
    uint8_t *arena = NULL;
    uint8_t *code = NULL;
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t code_va, marker_va, ib_va, cb_va;
    uint32_t words[OPENAGC_STEP_O_WORDS];
    uint32_t values[OPENAGC_SH_PAIR_COUNT];
    uint32_t word_count;
    int gc_fd;
    int completed = 0;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-set-sh-gfx-eop: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-set-sh") != 0) {
        return openagc_logf("openagc-set-sh-gfx-eop: map failed\n") == 0 ? 0 : 1;
    }

    code = arena + OPENAGC_CODE_OFF;
    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    code_va = (uint64_t)(uintptr_t)code;
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    if ((code_va & 0xffu) != 0u) {
        return openagc_logf("openagc-set-sh-gfx-eop: code_va misaligned\n") == 0 ? 0 : 1;
    }

    memset(arena, 0xCC, OPENAGC_ARENA);
    memcpy(code, openagc_smoke_vert_code, sizeof(openagc_smoke_vert_code));
    *marker = 0u;

    values[0] = (uint32_t)(code_va >> 8);
    values[1] = (uint32_t)(code_va >> 40);
    values[2] = openagc_sh_rsrc1;
    values[3] = openagc_sh_rsrc2;
    word_count = openagc_pm4_encode_graphics_sh_eop(
        openagc_sh_offsets, values, OPENAGC_SH_PAIR_COUNT, OPENAGC_EOP_SEQUENCE,
        marker_va, words);
    if (word_count != OPENAGC_STEP_O_WORDS) {
        return openagc_logf("openagc-set-sh-gfx-eop: encode size mismatch\n") == 0 ? 0 : 1;
    }
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_STEP_O_WORDS << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-set-sh-gfx-eop: /dev/gc unavailable\n") == 0 ? 0 : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-set-sh-gfx-eop: context query refused\n") == 0 ? 0 : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-set-sh-gfx-eop: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    (void)openagc_logf("openagc-set-sh-gfx-eop: submit=ok completed=%d "
                       "pairs=%u words=%u code_va=%016llx marker=%llx\n",
                       completed, OPENAGC_SH_PAIR_COUNT, OPENAGC_STEP_O_WORDS,
                       (unsigned long long)code_va, (unsigned long long)*marker);
    return completed ? 0 : 1;
}
