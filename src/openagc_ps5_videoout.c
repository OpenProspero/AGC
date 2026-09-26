/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* VideoOut setup and tile address adapted from PS5_Vulkan/src/demo_renderer.cpp
 * (Mihawk-99, GPL-3.0-or-later). CPU rasterization only. */
#include "openagc/ps5_videoout.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1920u
#define HEIGHT 1080u
#define FRAME_BYTES 0x1000000u
#define MEMORY_BYTES (FRAME_BYTES * 2u)

extern size_t sceKernelGetDirectMemorySize(void);
extern int sceKernelAllocateDirectMemory(int64_t, int64_t, size_t, size_t, int, int64_t *);
extern int sceKernelMapDirectMemory(void **, size_t, int, int, int64_t, size_t);
extern int sceKernelMunmap(void *, size_t);
extern int sceKernelReleaseDirectMemory(int64_t, size_t);
extern int sceSystemServiceHideSplashScreen(void);
extern int sceVideoOutOpen(int, int, int, const void *);
extern int sceVideoOutClose(int);
extern int sceVideoOutSetFlipRate(int, int);
extern int sceVideoOutSubmitFlip(int, int, uint32_t, int64_t);
extern int sceVideoOutWaitVblank(int);
extern int sceVideoOutIsFlipPending(int);
extern int sceVideoOutUnregisterBuffers(int, int);

typedef struct video_buffer {
    void *data;
    void *metadata;
    void *reserved0;
    void *reserved1;
} video_buffer;
typedef struct video_attribute { uint8_t reserved[80]; } video_attribute;
extern void sceVideoOutSetBufferAttribute2(video_attribute *, uint64_t, uint32_t,
                                           uint32_t, uint32_t, uint64_t, uint32_t, uint64_t);
extern int sceVideoOutRegisterBuffers2(int, int, int, video_buffer *, int,
                                       video_attribute *, int, void *);

struct openagc_ps5_videoout {
    int video;
    int64_t physical;
    uint8_t *mapped;
    uint32_t next_buffer;
    int allocated;
    int registered;
};

static size_t tile_offset(uint32_t x, uint32_t y)
{
    uint32_t intra = ((y << 4) & 0x70u) ^ ((y << 5) & 0xf00u) ^
                     ((y << 9) & 0x1000u) ^ ((y << 8) & 0x4000u) ^
                     ((x << 2) & 0xcu) ^ ((x << 5) & 0x380u) ^
                     ((x << 4) & 0x400u) ^ ((x << 6) & 0x800u) ^
                     ((x << 9) & 0xa000u);
    uint32_t block = (y >> 7) * ((WIDTH + 127u) >> 7) + (x >> 7);
    return ((size_t)block << 16) + intra;
}

static uint32_t pixel(openagc_color color)
{
    return 0xff000000u | ((uint32_t)color.b << 16) |
           ((uint32_t)color.g << 8) | color.r;
}

static void draw_rect(uint8_t *base, uint32_t x0, uint32_t y0,
                      uint32_t x1, uint32_t y1, openagc_color color)
{
    uint32_t foreground = pixel(color);
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            uint32_t *destination = (uint32_t *)(void *)(base + tile_offset(x, y));
            if (color.a == 255u) {
                *destination = foreground;
            } else if (color.a != 0u) {
                uint32_t background = *destination;
                uint32_t a = color.a, inverse = 255u - a;
                uint32_t r = ((foreground & 255u) * a + (background & 255u) * inverse + 127u) / 255u;
                uint32_t g = (((foreground >> 8) & 255u) * a + ((background >> 8) & 255u) * inverse + 127u) / 255u;
                uint32_t b = (((foreground >> 16) & 255u) * a + ((background >> 16) & 255u) * inverse + 127u) / 255u;
                *destination = 0xff000000u | (b << 16) | (g << 8) | r;
            }
        }
    }
}

static void flush_pixels(uint8_t *base)
{
#if defined(__x86_64__)
    for (size_t offset = 0; offset < FRAME_BYTES; offset += 64u)
        __asm__ volatile("clflush (%0)" : : "r"(base + offset) : "memory");
    __asm__ volatile("mfence" ::: "memory");
#else
    (void)base;
#endif
}

static int drain_flips(openagc_ps5_videoout *display)
{
    for (unsigned i = 0; i < 120u; ++i) {
        int pending = sceVideoOutIsFlipPending(display->video);
        if (pending <= 0) return pending;
        int result = sceVideoOutWaitVblank(display->video);
        if (result < 0) return result;
    }
    return 1;
}

openagc_result openagc_ps5_videoout_destroy(openagc_ps5_videoout *display)
{
    if (display == NULL) return OPENAGC_ERROR_INVALID_ARGUMENT;
    if (display->registered) {
        if (drain_flips(display) != 0) return OPENAGC_ERROR_BUSY;
        if (sceVideoOutUnregisterBuffers(display->video, 0) < 0)
            return OPENAGC_ERROR_BUSY;
    }
    if (display->video >= 0) sceVideoOutClose(display->video);
    if (display->mapped != NULL) sceKernelMunmap(display->mapped, MEMORY_BYTES);
    if (display->allocated) sceKernelReleaseDirectMemory(display->physical, MEMORY_BYTES);
    free(display);
    return OPENAGC_OK;
}

openagc_result openagc_ps5_videoout_create(openagc_ps5_videoout **out_display,
                                           int *out_platform_error)
{
    openagc_ps5_videoout *display;
    video_buffer buffers[2];
    video_attribute attribute;
    size_t pool_size;
    int result;
    if (out_display == NULL) return OPENAGC_ERROR_INVALID_ARGUMENT;
    *out_display = NULL;
    if (out_platform_error != NULL) *out_platform_error = 0;
    display = (openagc_ps5_videoout *)calloc(1u, sizeof(*display));
    if (display == NULL) return OPENAGC_ERROR_OUT_OF_MEMORY;
    display->video = -1;
    (void)sceSystemServiceHideSplashScreen();
    result = sceVideoOutOpen(0xff, 0, 0, NULL);
    if (result < 0) goto failed;
    display->video = result;
    pool_size = sceKernelGetDirectMemorySize();
    if (pool_size < MEMORY_BYTES || pool_size > INT64_MAX) {
        result = -1;
        goto failed;
    }
    result = sceKernelAllocateDirectMemory(0, (int64_t)pool_size, MEMORY_BYTES,
                                            0x200000u, 3, &display->physical);
    if (result < 0) goto failed;
    display->allocated = 1;
    result = sceKernelMapDirectMemory((void **)&display->mapped, MEMORY_BYTES, 0x33,
                                       0, display->physical, 0x200000u);
    if (result < 0) goto failed;
    memset(display->mapped, 0, MEMORY_BYTES);
    flush_pixels(display->mapped);
    flush_pixels(display->mapped + FRAME_BYTES);
    memset(buffers, 0, sizeof(buffers));
    buffers[0].data = display->mapped;
    buffers[1].data = display->mapped + FRAME_BYTES;
    memset(&attribute, 0, sizeof(attribute));
    (void)sceVideoOutSetFlipRate(display->video, 0);
    sceVideoOutSetBufferAttribute2(&attribute, UINT64_C(0x8000000022000000),
                                   0, WIDTH, HEIGHT, 0, 0, 0);
    result = sceVideoOutRegisterBuffers2(display->video, 0, 0, buffers, 2,
                                          &attribute, 0, NULL);
    if (result < 0) goto failed;
    display->registered = 1;
    *out_display = display;
    return OPENAGC_OK;
failed:
    if (out_platform_error != NULL) *out_platform_error = result;
    (void)openagc_ps5_videoout_destroy(display);
    return OPENAGC_ERROR_NOT_READY;
}

openagc_result openagc_ps5_videoout_present(openagc_ps5_videoout *display,
                                            const openagc_frame_view *frame,
                                            int *out_platform_error)
{
    uint8_t *base;
    int result;
    if (out_platform_error != NULL) *out_platform_error = 0;
    if (display == NULL || frame == NULL || frame->commands == NULL)
        return OPENAGC_ERROR_INVALID_ARGUMENT;
    if (frame->struct_size != sizeof(*frame)) return OPENAGC_ERROR_INCOMPATIBLE_VERSION;
    if (frame->width != WIDTH || frame->height != HEIGHT || frame->command_count > 4096u)
        return OPENAGC_ERROR_OUT_OF_RANGE;
    for (uint32_t i = 0; i < frame->command_count; ++i) {
        const openagc_command *command = &frame->commands[i];
        if (command->type == OPENAGC_COMMAND_CLEAR) continue;
        if (command->type != OPENAGC_COMMAND_RECTANGLE)
            return OPENAGC_ERROR_UNSUPPORTED_OPERATION;
        const openagc_rect *rect = &command->data.rectangle;
        if (!(rect->x >= 0.0f && rect->y >= 0.0f && rect->width > 0.0f &&
              rect->height > 0.0f && rect->x <= WIDTH && rect->y <= HEIGHT &&
              rect->width <= WIDTH - rect->x && rect->height <= HEIGHT - rect->y))
            return OPENAGC_ERROR_OUT_OF_RANGE;
    }
    result = drain_flips(display);
    if (result != 0) {
        if (out_platform_error != NULL) *out_platform_error = result;
        return OPENAGC_ERROR_NOT_READY;
    }
    base = display->mapped + display->next_buffer * FRAME_BYTES;
    memset(base, 0, FRAME_BYTES);
    for (uint32_t i = 0; i < frame->command_count; ++i) {
        const openagc_command *command = &frame->commands[i];
        if (command->type == OPENAGC_COMMAND_CLEAR) {
            draw_rect(base, 0u, 0u, WIDTH, HEIGHT, command->data.clear);
        } else {
            const openagc_rect *rect = &command->data.rectangle;
            uint32_t x0 = (uint32_t)rect->x, y0 = (uint32_t)rect->y;
            uint32_t x1 = (uint32_t)(rect->x + rect->width);
            uint32_t y1 = (uint32_t)(rect->y + rect->height);
            if ((float)x1 < rect->x + rect->width) ++x1;
            if ((float)y1 < rect->y + rect->height) ++y1;
            draw_rect(base, x0, y0, x1, y1, rect->color);
        }
    }
    flush_pixels(base);
    result = sceVideoOutSubmitFlip(display->video, (int)display->next_buffer, 1u,
                                   (int64_t)frame->frame_number);
    if (result < 0) {
        if (out_platform_error != NULL) *out_platform_error = result;
        return OPENAGC_ERROR_NOT_READY;
    }
    display->next_buffer ^= 1u;
    return OPENAGC_OK;
}
