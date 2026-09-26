/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "openagc/ps5_videoout.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *mapped;
static int submitted;
static int opened;
static int registered;

size_t sceKernelGetDirectMemorySize(void) { return 64u * 1024u * 1024u; }
int sceKernelAllocateDirectMemory(int64_t a, int64_t b, size_t c, size_t d, int e, int64_t *out) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    *out = 0x200000;
    return 0;
}
int sceKernelMapDirectMemory(void **out, size_t n, int a, int b, int64_t c, size_t d) {
    (void)a; (void)b; (void)c; (void)d;
    mapped = calloc(1u, n);
    *out = mapped;
    return mapped == NULL ? -1 : 0;
}
int sceKernelMunmap(void *address, size_t n) { (void)n; free(address); mapped = NULL; return 0; }
int sceKernelReleaseDirectMemory(int64_t a, size_t b) { (void)a; (void)b; return 0; }
int sceSystemServiceHideSplashScreen(void) { return 0; }
int sceVideoOutOpen(int a, int b, int c, const void *d) {
    (void)a; (void)b; (void)c; (void)d; opened = 1; return 4;
}
int sceVideoOutClose(int a) { assert(a == 4); opened = 0; return 0; }
int sceVideoOutSetFlipRate(int a, int b) { (void)a; (void)b; return 0; }
int sceVideoOutSubmitFlip(int a, int b, uint32_t c, int64_t d) {
    (void)c; (void)d; assert(a == 4); submitted = b + 1; return 0;
}
int sceVideoOutWaitVblank(int a) { (void)a; return 0; }
int sceVideoOutIsFlipPending(int a) { (void)a; return 0; }
int sceVideoOutUnregisterBuffers(int a, int b) {
    (void)a; (void)b; registered = 0; return 0;
}
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t format, uint32_t tile,
                                    uint32_t width, uint32_t height, uint64_t option,
                                    uint32_t dcc, uint64_t clear) {
    (void)attribute; (void)option; (void)dcc; (void)clear;
    assert(format == UINT64_C(0x8000000022000000));
    assert(tile == 0 && width == 1920 && height == 1080);
}
int sceVideoOutRegisterBuffers2(int a, int b, int c, void *buffers, int count,
                                void *attribute, int category, void *option) {
    (void)a; (void)b; (void)c; (void)buffers; (void)attribute; (void)category; (void)option;
    assert(count == 2); registered = 1; return 0;
}

int main(void) {
    openagc_ps5_videoout *display = NULL;
    openagc_command commands[2];
    openagc_frame_view frame = OPENAGC_FRAME_VIEW_INIT;
    int platform_error = -1;
    assert(openagc_ps5_videoout_create(&display, &platform_error) == OPENAGC_OK);
    assert(display != NULL && opened && registered && platform_error == 0);
    memset(commands, 0, sizeof(commands));
    commands[0].type = OPENAGC_COMMAND_CLEAR;
    commands[0].data.clear = (openagc_color){10, 20, 30, 255};
    commands[1].type = OPENAGC_COMMAND_RECTANGLE;
    commands[1].data.rectangle = (openagc_rect){0, 0, 1, 1, {255, 0, 0, 255}};
    frame.width = 1920; frame.height = 1080;
    frame.command_count = 2; frame.commands = commands;
    frame.frame_number = 1;
    assert(openagc_ps5_videoout_present(display, &frame, &platform_error) == OPENAGC_OK);
    assert(submitted == 1 && platform_error == 0);
    assert(*(uint32_t *)(void *)mapped == 0xff0000ffu);
    assert(*(uint32_t *)(void *)(mapped + 4) == 0xff1e140au);
    commands[1].type = 99;
    assert(openagc_ps5_videoout_present(display, &frame, &platform_error) ==
           OPENAGC_ERROR_UNSUPPORTED_OPERATION);
    assert(submitted == 1);
    assert(openagc_ps5_videoout_destroy(display) == OPENAGC_OK);
    assert(!opened && !registered && mapped == NULL);
    return 0;
}
