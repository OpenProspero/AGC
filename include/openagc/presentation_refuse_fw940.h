/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 OpenProspero */
#ifndef OPENAGC_PRESENTATION_REFUSE_FW940_H
#define OPENAGC_PRESENTATION_REFUSE_FW940_H

#include <stdint.h>

/*
 * Stage 6/7 fail-closed refuse contracts for FW9.40.
 *
 * Native tiling, scanout layouts, and presentation/VideoOut have no
 * independently owned firmware evidence in this repository. The working
 * driver must keep these paths refused until that evidence exists.
 * hardware_qualified stays false; presentation capability bits stay 0.
 *
 * These constants document the refuse surface; host APIs already return
 * UNSUPPORTED_OPERATION / NOT_READY for the corresponding calls.
 */

#define OPENAGC_PRESENTATION_REFUSE_API_VERSION 1u
#define OPENAGC_PRESENTATION_REFUSE_FW940_ID 0x9400008u

/* Stage 6: native / optimal tiling is refused (host-linear only). */
#define OPENAGC_NATIVE_TILING_SUPPORTED 0u
/* Stage 6: scanout / display-controller image usage is refused. */
#define OPENAGC_SCANOUT_USAGE_SUPPORTED 0u
/* Stage 7: swapchain / windowOut / flip is refused. */
#define OPENAGC_PRESENTATION_SUPPORTED 0u
/* Stage 7: no VideoOut evidence pin exists. */
#define OPENAGC_VIDEOOUT_EVIDENCE_PIN_COUNT 0u

#endif /* OPENAGC_PRESENTATION_REFUSE_FW940_H */
