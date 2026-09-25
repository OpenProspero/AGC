/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step V: IB dump of Step-U register program + EOP for FW9.40 (no DRAW).
 *
 * Contract (docs/hardware-evidence.md): same bounded submit as Step U
 * (vert+frag host register program + EOP), then write an
 * openagc-ib-dump log of the submitted dwords for host parse intake.
 * Omits CB/DB color binds and DRAW. Does not unlock Stage 5 CB pins.
 *
 * Packet layout: include/openagc/pm4_graphics_fw940.h
 * Dump format: include/openagc/pm4_ib_dump_fw940.h
 */

#include "openagc/pm4_graphics_fw940.h"
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
#define OPENAGC_VERT_CODE_OFF 0x0000u
#define OPENAGC_FRAG_CODE_OFF 0x0100u
#define OPENAGC_IB_OFF 0x2000u
#define OPENAGC_CB_OFF 0x2800u
#define OPENAGC_MARKER_OFF 0x3000u

#define OPENAGC_EOP_SEQUENCE 1u
#define OPENAGC_DEADLINE_SECONDS 30
#define OPENAGC_VERT_CTX_COUNT 3u
#define OPENAGC_VERT_SH_COUNT 4u
#define OPENAGC_FRAG_CTX_COUNT 9u
#define OPENAGC_FRAG_SH_COUNT 4u
#define OPENAGC_STEP_U_WORDS                                         \
    OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(OPENAGC_VERT_CTX_COUNT, \
                                             OPENAGC_VERT_SH_COUNT,  \
                                             OPENAGC_FRAG_CTX_COUNT, \
                                             OPENAGC_FRAG_SH_COUNT)

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

/* tests/fixtures/psbc_smoke/smoke.frag.gfx1013.bin (48 bytes). */
static const uint8_t openagc_smoke_frag_code[] = {
    0xf2, 0x02, 0x00, 0x7e, 0xff, 0x02, 0x02, 0x7e, 0x00, 0x00, 0x80, 0x3e,
    0x80, 0x02, 0x04, 0x7e, 0x0f, 0x18, 0x00, 0xf8, 0x00, 0x01, 0x02, 0x00,
    0x00, 0x00, 0x81, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf,
    0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf, 0x00, 0x00, 0x9f, 0xbf
};

/* smoke.vert.metadata.json — context_registers. */
static const uint32_t openagc_vert_ctx_offsets[OPENAGC_VERT_CTX_COUNT] = {
    433u, 451u, 519u
};
static const uint32_t openagc_vert_ctx_values[OPENAGC_VERT_CTX_COUNT] = {
    128u, 4u, 0u
};

/* smoke.vert.metadata.json shader_registers offsets; PGM patched at runtime. */
static const uint32_t openagc_vert_sh_offsets[OPENAGC_VERT_SH_COUNT] = {
    72u, 73u, 74u, 75u
};
static const uint32_t openagc_vert_sh_rsrc1 = 137101313u;
static const uint32_t openagc_vert_sh_rsrc2 = 8u;

/* smoke.vert.metadata.json linkage. */
#define OPENAGC_LINK_GE_CNTL_OFF 603u
#define OPENAGC_LINK_GE_CNTL_VAL 131200u
#define OPENAGC_LINK_STAGES_EN_OFF 725u
#define OPENAGC_LINK_STAGES_EN_VAL 65536u
#define OPENAGC_LINK_USER_VGPR_EN_OFF 610u
#define OPENAGC_LINK_USER_VGPR_EN_VAL 0u

/* smoke.frag.metadata.json — context_registers (linkage: null). */
static const uint32_t openagc_frag_ctx_offsets[OPENAGC_FRAG_CTX_COUNT] = {
    452u, 453u, 435u, 436u, 438u, 440u, 515u, 143u, 784u
};
static const uint32_t openagc_frag_ctx_values[OPENAGC_FRAG_CTX_COUNT] = {
    0u, 9u, 128u, 128u, 32768u, 0u, 16u, 15u, 0u
};

/* smoke.frag.metadata.json shader_registers offsets; PGM patched at runtime. */
static const uint32_t openagc_frag_sh_offsets[OPENAGC_FRAG_SH_COUNT] = {
    8u, 9u, 10u, 11u
};
static const uint32_t openagc_frag_sh_rsrc1 = 36438017u;
static const uint32_t openagc_frag_sh_rsrc2 = 4u;

static const char openagc_log_path[] =
    "/data/prosperoai/openagc-ib-dump-step-u.log";

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

static int openagc_write_ib_dump(int completed, const uint32_t *ib_words,
                                 uint32_t word_count)
{
    char buffer[4096];
    size_t used = 0u;
    uint32_t i;
    int n;

    n = snprintf(buffer, sizeof(buffer),
                 "openagc-ib-dump: tag=%s fw=0x%x completed=%d words=%u\nib",
                 OPENAGC_IB_DUMP_TAG_STEP_U, OPENAGC_IB_DUMP_FW940_ID, completed,
                 word_count);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        return -1;
    }
    used = (size_t)n;
    for (i = 0u; i < word_count; ++i) {
        n = snprintf(buffer + used, sizeof(buffer) - used, " %08x", ib_words[i]);
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
    uint8_t *vert_code = NULL;
    uint8_t *frag_code = NULL;
    uint32_t *ib = NULL;
    struct openagc_cb *cb = NULL;
    volatile uint64_t *marker = NULL;
    struct openagc_submit submit;
    struct timeval start;
    uint64_t vert_code_va, frag_code_va, marker_va, ib_va, cb_va;
    uint32_t words[OPENAGC_STEP_U_WORDS];
    uint32_t vert_sh_values[OPENAGC_VERT_SH_COUNT];
    uint32_t frag_sh_values[OPENAGC_FRAG_SH_COUNT];
    uint32_t word_count;
    int gc_fd;
    int completed = 0;
    off_t physical = 0;

    if (sceKernelAllocateMainDirectMemory(OPENAGC_ARENA, OPENAGC_ARENA, 1,
                                          &physical) != 0) {
        return openagc_logf("openagc-ib-dump-step-u: allocate failed\n") == 0 ? 0
                                                                              : 1;
    }
    if (sceKernelMapNamedDirectMemory((void **)&arena, OPENAGC_ARENA,
                                      OPENAGC_PROT_READ | OPENAGC_PROT_WRITE |
                                          OPENAGC_PROT_GPU_READ |
                                          OPENAGC_PROT_GPU_WRITE,
                                      OPENAGC_MAP_NO_COALESCE, physical,
                                      OPENAGC_ARENA, "openagc-ib-dump") != 0) {
        return openagc_logf("openagc-ib-dump-step-u: map failed\n") == 0 ? 0 : 1;
    }

    vert_code = arena + OPENAGC_VERT_CODE_OFF;
    frag_code = arena + OPENAGC_FRAG_CODE_OFF;
    ib = (uint32_t *)(arena + OPENAGC_IB_OFF);
    cb = (struct openagc_cb *)(arena + OPENAGC_CB_OFF);
    marker = (volatile uint64_t *)(arena + OPENAGC_MARKER_OFF);
    vert_code_va = (uint64_t)(uintptr_t)vert_code;
    frag_code_va = (uint64_t)(uintptr_t)frag_code;
    marker_va = (uint64_t)(uintptr_t)marker;
    ib_va = (uint64_t)(uintptr_t)ib;
    cb_va = (uint64_t)(uintptr_t)cb;

    if ((vert_code_va & 0xffu) != 0u || (frag_code_va & 0xffu) != 0u) {
        return openagc_logf("openagc-ib-dump-step-u: code_va misaligned\n") == 0
                   ? 0
                   : 1;
    }

    memset(arena, 0xCC, OPENAGC_ARENA);
    memcpy(vert_code, openagc_smoke_vert_code, sizeof(openagc_smoke_vert_code));
    memcpy(frag_code, openagc_smoke_frag_code, sizeof(openagc_smoke_frag_code));
    *marker = 0u;

    vert_sh_values[0] = (uint32_t)(vert_code_va >> 8);
    vert_sh_values[1] = (uint32_t)(vert_code_va >> 40);
    vert_sh_values[2] = openagc_vert_sh_rsrc1;
    vert_sh_values[3] = openagc_vert_sh_rsrc2;
    frag_sh_values[0] = (uint32_t)(frag_code_va >> 8);
    frag_sh_values[1] = (uint32_t)(frag_code_va >> 40);
    frag_sh_values[2] = openagc_frag_sh_rsrc1;
    frag_sh_values[3] = openagc_frag_sh_rsrc2;
    word_count = openagc_pm4_encode_graphics_vert_frag_eop(
        openagc_vert_ctx_offsets, openagc_vert_ctx_values, OPENAGC_VERT_CTX_COUNT,
        openagc_vert_sh_offsets, vert_sh_values, OPENAGC_VERT_SH_COUNT,
        OPENAGC_LINK_GE_CNTL_OFF, OPENAGC_LINK_GE_CNTL_VAL, OPENAGC_LINK_STAGES_EN_OFF,
        OPENAGC_LINK_STAGES_EN_VAL, OPENAGC_LINK_USER_VGPR_EN_OFF,
        OPENAGC_LINK_USER_VGPR_EN_VAL, openagc_frag_ctx_offsets,
        openagc_frag_ctx_values, OPENAGC_FRAG_CTX_COUNT, openagc_frag_sh_offsets,
        frag_sh_values, OPENAGC_FRAG_SH_COUNT, OPENAGC_EOP_SEQUENCE, marker_va, words);
    if (word_count != OPENAGC_STEP_U_WORDS) {
        return openagc_logf("openagc-ib-dump-step-u: encode size mismatch\n") == 0
                   ? 0
                   : 1;
    }
    memcpy(ib, words, sizeof(words));

    cb[0].header = ((uint64_t)ib_va << 32) | 0xC0023F00u;
    cb[0].ib_base = ((uint64_t)OPENAGC_STEP_U_WORDS << 32) | ((uint64_t)ib_va >> 32);

    gc_fd = open("/dev/gc", O_RDWR);
    if (gc_fd < 0) {
        return openagc_logf("openagc-ib-dump-step-u: /dev/gc unavailable\n") == 0
                   ? 0
                   : 1;
    }
    if (ioctl(gc_fd, OPENAGC_CONTEXT_QUERY, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-ib-dump-step-u: context query refused\n") == 0
                   ? 0
                   : 1;
    }

    submit.queue_type = 3u;
    submit.num_cbs = 1u;
    submit.cb_array = cb_va;
    gettimeofday(&start, NULL);
    if (ioctl(gc_fd, OPENAGC_SUBMIT_16, &submit) != 0) {
        close(gc_fd);
        return openagc_logf("openagc-ib-dump-step-u: submit refused\n") == 0 ? 0
                                                                              : 1;
    }

    while (openagc_elapsed_seconds(&start) < OPENAGC_DEADLINE_SECONDS) {
        if (*marker == (uint64_t)OPENAGC_EOP_SEQUENCE) {
            completed = 1;
            break;
        }
        usleep(1000);
    }
    close(gc_fd);

    /* Dump the submitted IB words (arena copy) for host parse intake. */
    if (openagc_write_ib_dump(completed, ib, OPENAGC_STEP_U_WORDS) != 0) {
        return 1;
    }
    return completed ? 0 : 1;
}
