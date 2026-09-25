/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step W: COPY_DATA register→memory probe of CB_COLOR0_* (+ masks) + EOP.
 *
 * Contract (docs/hardware-evidence.md): one submit reads public gfx10
 * CB bind-class context offsets into a CPU-visible buffer, then dumps the
 * eight values as openagc-ib-dump tag=ctxreg-cb. Does not SET color binds,
 * does not invent CB values, does not DRAW. Encoding from public
 * Mesa/amdgpu gfx_v10 emit_rreg; console proof is this single push.
 *
 * Packet layout: include/openagc/pm4_copy_data_fw940.h
 * Probe offsets: include/openagc/pm4_context_regs_gfx10.h
 * Dump format: include/openagc/pm4_ib_dump_fw940.h
 */

#include "openagc/pm4_copy_data_fw940.h"
#include "openagc/pm4_ib_dump_fw940.h"

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
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
#define OPENAGC_DEST_OFF 0x1000u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#define OPENAGC_POISON 0xCCCCCCCCu

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

static const char openagc_log_path[] =
    "/data/prosperoai/openagc-ib-dump-ctxreg-cb.log";

static int openagc_log_bytes(const char *bytes, size_t length)
{
    FILE *handle = fopen(openagc_log_path, "w");

    if (handle == NULL) {
        return -1;
    }
    if (fwrite(bytes, 1, length, handle) != length) {
        fclose(handle);
        return -1;
    }
    fclose(handle);
    return 0;
}

static int openagc_log(const char *line)
{
    return openagc_log_bytes(line, strlen(line));
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

static int openagc_write_ctxreg_dump(int completed, const uint32_t *values,
                                     uint32_t word_count)
{
    char buffer[1024];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\nib",
                 OPENAGC_IB_DUMP_TAG_CTXREG_CB, OPENAGC_IB_DUMP_FW940_ID, completed,
                 word_count);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;
    for (i = 0u; i < word_count; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", values[i]);
        if (n < 0 || (size_t)n >= sizeof(buffer) - used) {
            return -1;
        }
        used += (size_t)n;
    }
    if (used + 1u >= sizeof(buffer)) {
        return -1;
    }
    buffer[used++] = '\n';
    buffer[used] = '\0';
    return openagc_log_bytes(buffer, used);
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
    uint32_t *dest = NULL;
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t dest_va, marker_va, ib_va, cb_va;
    uint32_t words[OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS];
    uint32_t word_count;
    uint32_t i;
    int gc_fd;
    int completed = 0;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-ctxreg-cb: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-ctxreg-cb") != 0) {
        return openagc_logf("openagc-ctxreg-cb: map failed\n") == 0 ? 0 : 1;
    }

    dest = (uint32_t *)(arena + OPENAGC_DEST_OFF);
    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    dest_va = (uint64_t)(uintptr_t)dest;
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    memset(arena, 0xCC, OPENAGC_ARENA);
    for (i = 0u; i < OPENAGC_GFX10_CB_PROBE_COUNT; ++i) {
        dest[i] = OPENAGC_POISON;
    }
    *marker = 0u;

    word_count = openagc_pm4_encode_copy_data_cb_probe_eop(
        dest_va, OPENAGC_EOP_SEQUENCE, marker_va, words);
    if (word_count != OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS) {
        return openagc_logf("openagc-ctxreg-cb: encode size mismatch\n") == 0 ? 0
                                                                               : 1;
    }
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_PM4_COPY_DATA_CB_PROBE_EOP_WORDS << 32) |
                    ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-ctxreg-cb: /dev/gc unavailable\n") == 0 ? 0
                                                                              : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-ctxreg-cb: context query refused\n") == 0 ? 0
                                                                                : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-ctxreg-cb: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    if (openagc_write_ctxreg_dump(completed, dest, OPENAGC_GFX10_CB_PROBE_COUNT) !=
        0) {
        return 1;
    }
    return completed ? 0 : 1;
}
