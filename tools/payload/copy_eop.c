/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step B: bounded GPU copy and EOP proof for FW9.40.
 *
 * Contract (docs/hardware-evidence.md): one submit, one copy, one fence,
 * finite deadline, no retry, no queue-create, no dispatch, no flat_load,
 * no VideoOut. It writes exactly one log line and exits.
 *
 * The submitted IB is OpenAGC's own locked-down sequence plus the
 * operator's proven completion trailer:
 *   7 dwords  IT_DMA_DATA   (0xC0055002, 0x8C00C000, src lo/hi, dst lo/hi, size)
 *   8 dwords  action-based IT_RELEASE_MEM EOP (0xC0064900, 0x06703514, ...)
 *  16 dwords  NOP trailer  (0xC0001000, 0)
 * The destination bytes are compared on the CPU; the marker is polled with
 * a monotonic clock and a hard deadline.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

/* Sony-specific allocation entry points, resolved from the payload SDK stub. */
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
#define OPENAGC_COPY_BYTES 64u
#define OPENAGC_SOURCE_OFF 0x0000u
#define OPENAGC_DEST_OFF 0x1000u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u
#define OPENAGC_MARKER_SIZE 8u

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

static const char openagc_log_path[] = "/data/prosperoai/openagc-copy-eop.log";

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
    uint8_t *source = NULL;
    uint8_t *destination = NULL;
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    uint8_t *arena = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint32_t source_lo, source_hi, destination_lo, destination_hi;
    uint32_t marker_lo, marker_hi;
    uint64_t source_va, destination_va, marker_va, ib_va, cb_va;
    int gc_fd;
    int completed = 0;
    int matched;
    uint32_t i;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-copy-eop: allocate failed\n") == 0 ? 0 : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-eop") != 0) {
        return openagc_logf("openagc-copy-eop: map failed\n") == 0 ? 0 : 1;
    }

    source = arena + OPENAGC_SOURCE_OFF;
    destination = arena + OPENAGC_DEST_OFF;
    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    source_va = (uint64_t)(uintptr_t)source;
    destination_va = (uint64_t)(uintptr_t)destination;
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    memset(arena, 0, OPENAGC_ARENA);
    for (i = 0u; i < OPENAGC_COPY_BYTES; ++i) {
        source[i] = (uint8_t)(0x40u + i);
    }
    source_lo = (uint32_t)source_va;
    source_hi = (uint32_t)(source_va >> 32);
    destination_lo = (uint32_t)destination_va;
    destination_hi = (uint32_t)(destination_va >> 32);
    marker_lo = (uint32_t)marker_va;
    marker_hi = (uint32_t)(marker_va >> 32);

    ib[0] = 0xC0055002u;
    ib[1] = 0x8C00C000u;
    ib[2] = source_lo;
    ib[3] = source_hi;
    ib[4] = destination_lo;
    ib[5] = destination_hi;
    ib[6] = OPENAGC_COPY_BYTES;
    ib[7] = 0xC0064900u;
    ib[8] = 0x06703514u;
    ib[9] = 0x20000000u;
    ib[10] = marker_lo;
    ib[11] = marker_hi;
    ib[12] = OPENAGC_EOP_SEQUENCE;
    ib[13] = 0u;
    ib[14] = 0u;
    for (i = 0u; i < 16u; ++i) {
        ib[15u + i * 2u] = 0xC0001000u;
        ib[16u + i * 2u] = 0u;
    }

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)31u << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-copy-eop: /dev/gc unavailable\n") == 0 ? 0 : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-copy-eop: context query refused\n") == 0 ? 0 : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-copy-eop: submit refused\n") == 0 ? 0 : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    matched = memcmp(source, destination, OPENAGC_COPY_BYTES) == 0;
    (void)openagc_logf("openagc-copy-eop: submit=ok completed=%d matched=%d "
                       "source=%08x%08x destination=%08x%08x marker=%llx\n",
                       completed, matched, source_hi, source_lo, destination_hi,
                       destination_lo, (unsigned long long)*marker);
    return completed && matched ? 0 : 1;
}
