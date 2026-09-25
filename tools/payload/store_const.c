/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step C: bounded compute store-const proof for FW9.40.
 *
 * Contract (docs/hardware-evidence.md): one submit, one 1-thread
 * dispatch that stores a constant via flat_store_dword, one EOP fence,
 * finite deadline, no flat_load, no retry, no VideoOut. One log line.
 *
 * Packet layout: include/openagc/pm4_compute_fw940.h
 * Shader: tools/shaders/store_const.s (embedded binary below).
 */

#include "openagc/pm4_compute_fw940.h"
#include "openagc/store_const_code.h"

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
#define OPENAGC_DEST_OFF 0x1000u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30

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

/* Assembled from tools/shaders/store_const.s (gfx1013); see store_const_code.h. */

static const char openagc_log_path[] = "/data/prosperoai/openagc-store-const.log";

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
    uint32_t *destination = NULL;
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t code_va, destination_va, marker_va, ib_va, cb_va;
    uint32_t words[OPENAGC_PM4_COMPUTE_STORE_WORDS];
    uint32_t dest_lo, dest_hi;
    int gc_fd;
    int completed = 0;
    int matched;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-store-const: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-store") != 0) {
        return openagc_logf("openagc-store-const: map failed\n") == 0 ? 0 : 1;
    }

    code = arena + OPENAGC_CODE_OFF;
    destination = (uint32_t *)(arena + OPENAGC_DEST_OFF);
    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    code_va = (uint64_t)(uintptr_t)code;
    destination_va = (uint64_t)(uintptr_t)destination;
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    if ((code_va & 0xffu) != 0u) {
        return openagc_logf("openagc-store-const: code VA misaligned\n") == 0 ? 0 : 1;
    }

    memset(arena, 0xCC, OPENAGC_ARENA);
    memcpy(code, openagc_store_const_code, sizeof(openagc_store_const_code));
    *destination = 0u;
    *marker = 0u;

    openagc_pm4_encode_compute_store_const(code_va, destination_va, OPENAGC_EOP_SEQUENCE,
                                           marker_va, words);
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_PM4_COMPUTE_STORE_WORDS << 32) |
                    ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-store-const: /dev/gc unavailable\n") == 0 ? 0 : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-store-const: context query refused\n") == 0 ? 0 : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-store-const: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    matched = (*destination == OPENAGC_STORE_CONST_VALUE);
    dest_lo = (uint32_t)destination_va;
    dest_hi = (uint32_t)(destination_va >> 32);
    (void)openagc_logf("openagc-store-const: submit=ok completed=%d matched=%d "
                       "destination=%08x%08x value=%08x marker=%llx\n",
                       completed, matched, dest_hi, dest_lo, *destination,
                       (unsigned long long)*marker);
    return completed && matched ? 0 : 1;
}
