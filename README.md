# OpenAGC

OpenProspero's original, GPL-3.0-or-later C99 foundation for a future GPU
driver. This release has a **host-testable driver core** for resource
ownership, bounded copy command buffers, queue/fence behavior, and
graphics-resource state recording. It is **not** a working PS5 GPU or
display driver.

| Capability | Host reference (`OpenAGC::openagc`) | PS5 policy (`OpenAGC::ps5_policy`) |
| --- | --- | --- |
| C99 ABI and typed validation | Versioned headers, explicit errors | Same symbols; fail closed |
| Memory and buffers | Owned host allocations, one-time binding, capacity and lifetime checks | Unavailable |
| Copy command buffers | Bounded host PM4 word recording and synchronous CPU copy simulation | Unavailable |
| Queues and fences | One copy queue, synchronous simulated completion, poll/reset | Unavailable |
| Images and render state | Host-linear RGBA8/BGRA8 metadata, explicit logical owner/state transitions, scissor and clear **recording only** | Unavailable |
| Shader intake and pipelines | SHA-256-checked **unverified fixture** snapshots and host-only vertex/pixel/compute pipeline plans; no compiler or executable shader | Unavailable |
| Native tiling, executable shader pipelines, draws, rasterization, VideoOut | Not implemented | Not implemented |
| Vulkan 1.0 and OpenGL frontends | Not implemented | Not implemented |

**Firmware 9.40 is not qualified.** Unknown firmware is not qualified either;
in fact no PS5 firmware has been qualified. The PS5 policy target returns
`OPENAGC_ERROR_UNSUPPORTED_FIRMWARE` before any allocation, ioctl, kernel
memory operation, VideoOut call, or GPU submission. The firmware numbers in the
context descriptor are untrusted diagnostic hints, not an authorization
mechanism. Neither target contains console graphics or video-output code.
Do not use this project to attempt hardware initialization or testing.

## Build and use

With a C99-capable GCC or recent MSVC toolchain and CMake:

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --build-config Debug --output-on-failure
```

The public C/C++-compatible header is `include/openagc/openagc.h` (include
`<openagc/openagc.h>`); the additive driver ABI is
`include/openagc/driver.h` (include `<openagc/driver.h>`). Link
`OpenAGC::openagc` in a host CMake project using `add_subdirectory`.
Create an `OPENAGC_BACKEND_HOST_REFERENCE` context, then a
`openagc_gpu_device` using `OPENAGC_GPU_DEVICE_DESC_INIT`. The driver
API allocates bounded host memory, binds typed copy-source/destination
buffers, records validated copies, and submits them to a **CPU-only**
copy queue with an explicit fence. `openagc_gpu_command_buffer_get_recording`
and `openagc_gpu_queue_get_last_submission` expose read-only, deterministic
PM4 snapshots with **synthetic host addresses**; those words must never be
sent to a console. `openagc_gpu_fence_poll` distinguishes unsignaled
(`OPENAGC_ERROR_NOT_READY`) from completed host simulation.

The additive graphics header `include/openagc/graphics.h` (include
`<openagc/graphics.h>`) defines versioned image and render-state descriptors.
It supports only single-layer, single-mip, single-sample **host-linear**
RGBA8/BGRA8 color-target images bound to same-device host memory.
A graphics command buffer records logical `UNDEFINED/HOST` to
`COLOR_TARGET/GRAPHICS` ownership transitions, one bound target, a
bounded scissor, and RGBA8 clear commands. `apply_host_state` commits
metadata only; it neither writes image pixels nor creates a graphics
queue, GPU packet, fence, or display frame. Unsupported formats, native
tiling, sampled/scanout usage, depth, multisampling, and presentation
fail explicitly. Query `openagc_graphics_get_capabilities` rather than
inferring rendering support from successful recording.

`include/openagc/shader.h` (include `<openagc/shader.h>`) adds an
immutable **structural-only** gfx1013 artifact/reflection intake and
vertex/pixel/compute pipeline-plan validator. It deep-copies caller
bytes and typed reflection, verifies the code SHA-256, and checks
stage linkage, color format, uniform-buffer bindings, ownership,
capacity, and object lifetimes. Host fixtures are labeled
`OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE`; they are **not** compiled
shaders. No PSBC/Mesa compiler or executable digest is installed, so
a claimed OpenGNM PSBC artifact returns `OPENAGC_ERROR_NOT_READY`,
and every accepted plan reports `compiler_verified=0` and
`gpu_executable=0`. There is no shader command recording or execution.
See the [pinned build-time compiler plan](docs/shader-toolchain.md)
before attempting any real artifact. The user-approved
[GitHub Actions compiler build](.github/workflows/build-psbc-host.yml)
is **manual-only**; it never runs on a push, and even a successful
private compiler artifact cannot unlock PS5 execution or runtime
shader intake.

The earlier host UI recorder remains available and ABI-compatible. Its
header provides `OPENAGC_CONTEXT_DESC_INIT(backend)`,
`OPENAGC_DEVICE_DESC_INIT(width, height, capacity)`,
`OPENAGC_CAPABILITIES_INIT`, and `OPENAGC_FRAME_VIEW_INIT`. Specify
`OPENAGC_BACKEND_HOST_REFERENCE` for the host implementation, then create a
context and device, record with `openagc_frame_begin`,
`openagc_frame_clear`, `openagc_frame_rect`, and `openagc_frame_present`.
Inspect `openagc_device_get_last_frame` to render the recorded clear/rectangle
commands **in your own host preview**. A successful host `present` only
finalizes the recording; it does not draw, display, or submit GPU work.

The host device accepts dimensions from 1 to 8192 in each direction, at most
16,777,216 pixels total, and 1 to 4096 commands per frame. Rectangles use
finite float coordinates and sizes, positive width/height, and must fit
entirely within the device. Failed validation or exhausted capacity does not
append a command. The view remains valid until the next successful frame
begin or device destruction. Destroy devices before their context; the
library returns explicit errors for invalid state or inputs. Calls on a
context or its devices must be serialized by the caller.

For the PS5 fail-closed object/library and a `-nostdinc` cross-build recipe,
see [docs/architecture.md](docs/architecture.md). **Never link the host
`openagc` target into a PS5 image**; use only `openagc_ps5_policy` there.
The architecture document also lists the exact FW9.40 **field-derived**
DMA/EOP vectors, their evidence limits, graphics frontend research, and a
staged, opt-in qualification plan. No packet in this repo has been submitted
to a PS5 by OpenAGC.

## Status, direction, and provenance

[The architecture and qualification status](docs/architecture.md) describe the
separate backends, future Vulkan/OpenGL layers, and why no console capability
is claimed. Public PS5_Vulkan and ps5-opengl graphics architecture and public
OpenAGC PM4 interface facts were consulted as **knowledge references**;
FW9.40 empirical packet facts were read from the user's ProsperoAI notes.
No source, licensed assets, proprietary SDK content, binaries, firmware,
or keys were copied or made runtime dependencies. This implementation is
new OpenProspero-owned code; references do not imply endorsement or
hardware compatibility.

Copyright (C) 2026 OpenProspero. SPDX-License-Identifier:
GPL-3.0-or-later. See [LICENSE](LICENSE).
