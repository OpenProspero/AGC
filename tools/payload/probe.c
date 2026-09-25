/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step A: freestanding, syscall-only evidence probe.
 *
 * It writes one line to a log file and to stdout, then exits. It opens no
 * device, issues no ioctl, maps no GPU memory, and submits no packet.
 * Its only purpose is to prove the build, push, and log-fetch path before
 * any device-facing step is attempted.
 */

#define SYS_EXIT 1
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6

#define O_WRONLY 1
#define O_CREAT 0x0200
#define O_TRUNC 0x0400

static long openagc_probe_syscall(long number, long a0, long a1, long a2)
{
    long result;

    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a0), "S"(a1), "d"(a2)
                     : "rcx", "r11", "memory");
    return result;
}

static const char probe_line[] =
    "openagc-probe: step A ok (toolchain+deploy, no device access)\n";

static const char *const probe_paths[] = {
    "/data/prosperoai/openagc-probe.log",
    "/data/openagc-probe.log",
    "/tmp/openagc-probe.log"
};

void _start(void)
{
    unsigned int i;

    for (i = 0u; i < sizeof(probe_paths) / sizeof(probe_paths[0]); ++i) {
        long fd = openagc_probe_syscall(SYS_OPEN, (long)probe_paths[i],
                                        O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd >= 0) {
            openagc_probe_syscall(SYS_WRITE, fd, (long)probe_line,
                                  (long)(sizeof(probe_line) - 1u));
            openagc_probe_syscall(SYS_CLOSE, fd, 0, 0);
        }
    }
    openagc_probe_syscall(SYS_WRITE, 1, (long)probe_line,
                          (long)(sizeof(probe_line) - 1u));
    openagc_probe_syscall(SYS_EXIT, 0, 0, 0);
}
