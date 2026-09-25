/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
/* Step A: SDK-linked evidence probe.
 *
 * Built with ps5-payload-sdk (prospero-clang). Writes one line to a log
 * file and stdout, then exits. Opens no device and submits no packet.
 * Proves the build, push (nc to elfldr), and log-fetch path before any
 * device-facing step is attempted.
 */

#include <stdio.h>

static const char probe_line[] =
    "openagc-probe: step A ok (toolchain+deploy, no device access)\n";

static const char *const probe_paths[] = {
    "/data/prosperoai/openagc-probe.log",
    "/data/openagc-probe.log",
    "/tmp/openagc-probe.log"
};

int main(void)
{
    unsigned int i;

    for (i = 0u; i < sizeof(probe_paths) / sizeof(probe_paths[0]); ++i) {
        FILE *fp = fopen(probe_paths[i], "w");

        if (fp != NULL) {
            fputs(probe_line, fp);
            fclose(fp);
        }
    }
    fputs(probe_line, stdout);
    fflush(stdout);
    return 0;
}
