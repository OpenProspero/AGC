/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PM4_AGC_COMPLETION_H
#define OPENAGC_PM4_AGC_COMPLETION_H

#include "openagc/pm4.h"

/*
 * Step AQ encoded no completion packet (append_eop was zero), so its
 * missing marker says nothing about AGC's handling of EOP. For native
 * graphics completion, PS5_Vulkan's R90 console runs use RELEASE_MEM
 * event 20, index 5 and cache actions 0x30c to flush colour/depth caches
 * and write the marker after the draw. An opt-in FW9.40 diagnostic
 * confirmed marker 1, all 64 expected pixels, and an empty guard;
 * that does not qualify arbitrary draws or frontend execution.
 */
#define OPENAGC_PM4_AGC_COMPLETION_WORDS 8u
#define OPENAGC_PM4_AGC_COMPLETION_CONTROL 0x0030c514u

static inline void openagc_pm4_encode_agc_completion(uint64_t marker,
                                                      uint32_t sequence,
                                                      uint32_t words[8])
{
    words[0] = OPENAGC_PM4_EOP_HEADER;
    words[1] = OPENAGC_PM4_AGC_COMPLETION_CONTROL;
    words[2] = OPENAGC_PM4_EOP_DATA_SEL;
    words[3] = (uint32_t)marker;
    words[4] = (uint32_t)(marker >> 32);
    words[5] = sequence;
    words[6] = 0u;
    words[7] = 0u;
}

#endif
