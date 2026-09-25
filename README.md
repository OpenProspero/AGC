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
| Images and render state | Host-linear RGBA8/BGRA8 metadata with declared color-target/sampled usage, explicit logical owner/state transitions including copy-queue and shader-read ownership, scissor and clear recording, and deterministic **CPU** clear execution into bound host memory | Unavailable |
| Shader intake and pipelines | SHA-256-checked **unverified fixture** snapshots and host-only vertex/pixel/compute pipeline plans with exactly matched uniform-buffer and sampled-image descriptors; no compiler or executable shader | Unavailable |
| Shared frontend core (Vulkan/OpenGL) | Versioned native-to-backend translation, one staging/copy/transition path per device, CPU image and buffer upload/readback that preserves logical state and owner, and per-usage copy direction | Unavailable |
| Native tiling, executable shader pipelines, draws, rasterization, VideoOut | Not implemented | Not implemented |
| Vulkan 1.0 and OpenGL frontends | Host subsets over the shared core: enumeration, transfer, clears, render-pass and pipeline recording; draws refused; one host-simulated store-const compute dispatch | Unavailable |

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
RGBA8/BGRA8 images bound to same-device host memory, with declared
color-target and/or sampled usage. A graphics command buffer records
logical `UNDEFINED/HOST` to `COLOR_TARGET/GRAPHICS` ownership
transitions, one bound target, a bounded scissor, and RGBA8 clear
commands; an image may only enter a state its usage declares, so a
color-target-only image cannot reach `SHADER_READ/GRAPHICS` and a
sampled-only image cannot reach `COLOR_TARGET/GRAPHICS`. An image may
also be handed to the copy queue as `TRANSFER_DESTINATION/COPY` (an
upload target) or `TRANSFER_SOURCE/COPY` (a readback source), and the
copy path enforces that direction: a copy that overlaps image memory is
refused unless the image is copy-owned in the matching state, both when
the copy is recorded and again when the queue preflights the whole
submission. `apply_host_state` commits metadata only. After it succeeds,
a separate `openagc_graphics_command_buffer_execute_host` runs the
recorded clears as deterministic scissor-clipped **CPU** fills of the
bound host-linear image bytes: RGBA8 stores R,G,B,A and BGRA8 stores
B,G,R,A, row padding is never written, the reported `gpu_submitted`
stays 0, and the logical state/owner is unchanged. It creates no
graphics queue, GPU packet, fence, or display frame, and it draws
nothing. Unsupported formats, native tiling, scanout usage, depth,
multisampling, and presentation fail explicitly. Query
`openagc_graphics_get_capabilities` rather than inferring rendering
support from successful recording.

`include/openagc/shader.h` (include `<openagc/shader.h>`) adds an
immutable **structural-only** gfx1013 artifact/reflection intake and
vertex/pixel/compute pipeline-plan validator. It deep-copies caller
bytes and typed reflection, verifies the code SHA-256, and checks
stage linkage, color format, uniform-buffer bindings, sampled-image
declarations, ownership, capacity, and object lifetimes. A plan's
supplied uniform buffers and sampled images must match the reflected
binding slots exactly: buffers carry the shader-read usage and cover
the declared minimum, while sampled images carry the sampled usage,
sit in `SHADER_READ/GRAPHICS`, and match the declared format. Both
kinds are retained until the plan is destroyed. Host fixtures are
labeled `OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE`; they are **not**
compiled shaders. No PSBC/Mesa compiler or executable digest is
installed, so a claimed OpenGNM PSBC artifact returns
`OPENAGC_ERROR_NOT_READY`, and every accepted plan reports
`compiler_verified=0` and `gpu_executable=0`. There is no shader
command recording or execution, and nothing samples an image.
See the [pinned build-time compiler plan](docs/shader-toolchain.md)
before attempting any real artifact. The user-approved
[GitHub Actions compiler build](.github/workflows/build-psbc-host.yml)
is **manual-only**; it never runs on a push, and even a successful
private compiler artifact cannot unlock PS5 execution or runtime
shader intake.

`include/openagc/frontend.h` (include `<openagc/frontend.h>`) is the
shared core a Vulkan 1.0 or OpenGL frontend reuses instead of talking
to the driver directly. It translates published Vulkan and OpenGL
enumerants onto this backend — format, image usage, image layout,
buffer usage — and refuses everything the host core cannot represent:
sRGB and R8 images, `GENERAL`, `PREINITIALIZED`, `PRESENT_SRC_KHR`, the
depth read-only layout, storage or transfer-only images, and
storage-buffer usage. `openagc_frontend_device_create` builds one
staging allocation, one copy queue, and one transition recorder per
backend device. `openagc_frontend_image_upload` and
`openagc_frontend_image_readback` move bytes through that copy path
with **CPU** copies and restore the image's logical state and owner
afterwards, so neither frontend reimplements the ownership dance.
`openagc_frontend_image_copy_rect` copies a rectangle between two
same-format images the same way, row by row, and leaves both images in
the state and owner they had; a self copy and a format mismatch are
refused. `openagc_frontend_buffer_*` gives both frontends the same
buffer object over that path, with the copy direction enforced by the
translated usage, so a GL unpack buffer cannot be read and a pack
buffer cannot be written; `openagc_frontend_buffer_fill` writes a
repeating four-byte pattern into a copy-destination range.
Capabilities report `host_translation=1`, `host_image_copy=1`, and
`host_buffer_fill=1` with `gpu_execution=0`, `rasterization=0`, and
`presentation=0`. The staged plan for both frontends, including what
stays refused, is [docs/roadmap.md](docs/roadmap.md).

`include/openagc/vulkan.h` and `include/openagc/opengl.h` are the two
host frontends over that core. The Vulkan subset enumerates one
physical device whose only queue family is transfer, creates buffers,
images, and views, and records buffer copies, `vkCmdCopyImage`,
`vkCmdFillBuffer`, bounded `vkCmdUpdateBuffer` payloads, clears, and
layout transitions; fences and semaphores share the stage-2 timeline.
The OpenGL subset derives the same backend state from its own commands:
`glTexSubImage2D`/`glGetTexImage` are the upload and readback pair,
`glCopyTexSubImage2D` is the rect copy, an FBO attachment derives
`COLOR_TARGET/GRAPHICS`, sampling derives `SHADER_READ/GRAPHICS`,
`glClearBufferSubData` is the buffer fill, and
`glFinish`/`glFenceSync` publish the timeline a Vulkan fence uses. Both
record one color render pass, viewports, scissors, vertex and index
bindings, descriptor sets, push constants, blend state, and queries.
`vkCmdDraw`/`vkCmdDispatch` and `glDrawArrays`/`glDispatchCompute`
return `NOT_READY` from the shader compiler gate and rasterize nothing,
except the narrow host_store_const compute path (`1,1,1` →
`0xA5A5A5A5`) proven on console and asserted by
`test_openagc_equivalence`;
swapchains, the default framebuffer, presentation, depth testing, sRGB,
MSAA, and native tiling stay refused, and both frontends report
`gpu_execution=0` and `presentation=0`.
`tests/test_openagc_equivalence.c` asserts that equivalent Vulkan and
OpenGL work lands on the same backend state, the same bytes, and the
same pixels.

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
The architecture document also lists the FW9.40 console-aligned
DMA/EOP vectors (`pm4_fw940.h`, 31 dwords) and the compute store-const
dispatch (`pm4_compute_fw940.h`, 51 dwords), their evidence limits,
graphics frontend research, and a staged, opt-in qualification plan.
The host library never submits packets; separate SDK payloads proved
copy+EOP and one compute store. Draw/render packets remain unavailable.

## Status, direction, and provenance

[The architecture and qualification status](docs/architecture.md) describe the
separate backends, how the Vulkan 1.0 and OpenGL subsets map onto the shared
host core, and why no console capability is claimed. The staged plan for those
two frontends is [docs/roadmap.md](docs/roadmap.md). Public PS5_Vulkan and ps5-opengl
graphics architecture and public
OpenAGC PM4 interface facts were consulted as **knowledge references**;
FW9.40 empirical packet facts were read from the user's ProsperoAI notes.
No source, licensed assets, proprietary SDK content, binaries, firmware,
or keys were copied or made runtime dependencies. This implementation is
new OpenProspero-owned code; references do not imply endorsement or
hardware compatibility.

Copyright (C) 2026 OpenProspero. SPDX-License-Identifier:
GPL-3.0-or-later. See [LICENSE](LICENSE).
