/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step P: minimal SET_CONTEXT_REG + EOP for FW9.40 (no SET_SH, no DRAW).
 *
 * Contract (docs/hardware-evidence.md): one submit of the three
 * smoke.vert context_registers pairs (offsets/values from the pinned
 * PSBC fixture) plus the shared EOP+NOP trailer. Omits linkage
 * (ge_cntl/stages_en/user_vgpr_en), SET_SH, CB/DB, and DRAW.
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
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#define OPENAGC_CTX_PAIR_COUNT 3u
#define OPENAGC_STEP_P_WORDS \
    OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(OPENAGC_CTX_PAIR_COUNT)

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

/* smoke.vert.metadata.json context_registers — no linkage, no invention. */
static const uint32_t openagc_ctx_offsets[OPENAGC_CTX_PAIR_COUNT] = {
    433u, 451u, 519u
};
static const uint32_t openagc_ctx_values[OPENAGC_CTX_PAIR_COUNT] = {
    128u, 4u, 0u
};

static const char openagc_log_path[] = "/data/prosperoai/openagc-set-context-eop.log";

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
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t marker_va, ib_va, cb_va;
    uint32_t words[OPENAGC_STEP_P_WORDS];
    uint32_t word_count;
    int gc_fd;
    int completed = 0;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-set-context-eop: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-set-ctx") != 0) {
        return openagc_logf("openagc-set-context-eop: map failed\n") == 0 ? 0 : 1;
    }

    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    memset(arena, 0xCC, OPENAGC_ARENA);
    *marker = 0u;

    word_count = openagc_pm4_encode_graphics_context_eop(
        openagc_ctx_offsets, openagc_ctx_values, OPENAGC_CTX_PAIR_COUNT,
        OPENAGC_EOP_SEQUENCE, marker_va, words);
    if (word_count != OPENAGC_STEP_P_WORDS) {
        return openagc_logf("openagc-set-context-eop: encode size mismatch\n") == 0 ? 0
                                                                                    : 1;
    }
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_STEP_P_WORDS << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-set-context-eop: /dev/gc unavailable\n") == 0 ? 0
                                                                                    : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-set-context-eop: context query refused\n") == 0
                   ? 0
                   : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-set-context-eop: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    (void)openagc_logf("openagc-set-context-eop: submit=ok completed=%d "
                       "pairs=%u words=%u marker=%llx\n",
                       completed, OPENAGC_CTX_PAIR_COUNT, OPENAGC_STEP_P_WORDS,
                       (unsigned long long)*marker);
    return completed ? 0 : 1;
}
