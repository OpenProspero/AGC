# Architecture and qualification

## Trust boundary

`include/openagc/openagc.h` keeps the original version-1 UI recorder ABI.
`include/openagc/driver.h` adds a separate version-1 driver ABI with opaque
GPU-device, memory, buffer, command-buffer, queue, and fence handles.
`include/openagc/graphics.h` adds a version-1 graphics-resource ABI with
opaque image and render-state command-buffer handles. All four APIs
reject incorrect descriptor sizes and versions. `include/openagc/shader.h`
adds version-1 host-only gfx1013 artifact and pipeline-plan handles.
`src/openagc.c`
implements the host UI recorder; `src/openagc_gpu.c` implements the host
copy driver core; `src/openagc_graphics.c` records host-only image state;
`src/openagc_shader.c` deep-copies and validates unverified structural
artifacts using OpenProspero's C99 SHA-256 implementation.
The host components share context/device/memory lifetime through private
headers, so live graphics resources prevent premature allocation, device,
or context destruction.
`src/openagc_ps5_policy.c` implements **all** public symbols as a separate
freestanding, deny-all target. It has no allocator, device driver, VideoOut,
ioctl, filesystem, SDK, or graphics imports. Link **exactly one** target
into an application.

The current policy gate always rejects a PS5 context, including firmware 9.40,
an unknown version (0.0), and any other caller-claimed version. In the host
library it runs before allocation; in the PS5 policy library there is no
allocation or hardware path at all. A caller cannot opt in with a firmware
number. The policy target does not emulate rendering and never produces a
device handle. A successful host queue submit means only that CPU memory
was copied and a simulated fence signaled, **not** that a GPU accepted
commands. The UI, driver, and graphics capabilities all report
`gpu_execution=0` and `video_output=0`; graphics also reports
`rasterization=0`. Driver capabilities separately report host copy
simulation and reference packet encoding. Shader capabilities report
`compiler_available=0` and `gpu_execution=0`.

The host device checks bounded dimensions, pixel count, command capacity,
finite rectangle fields, positive sizes, and in-bounds geometry before
recording. Multiplication and counter overflow are checked explicitly.
Commands are stored in order; newly recorded entries are zero-filled to keep
unused union bytes deterministic. Failed operations preserve the current
frame. `begin` opens a recording; `present` closes it; a read-only view
exposes typed `CLEAR` and `RECTANGLE` commands to a separate host renderer.
The view is invalidated by the next successful `begin` or destruction.
There is no pixel buffer or windowing dependency in OpenAGC.

## Driver core: bounded resources and ownership

`openagc_gpu_device` requires an existing host context and an explicit
memory budget (default 16 MiB; maximum 64 MiB). Allocations are zeroed
host memory, at most 16 MiB each and 64 per device. Buffer descriptors
currently accept only copy-source and copy-destination usage, up to 128
buffers. Binding is one-time, four-byte aligned, in range, and
same-device. The memory allocation cannot be destroyed while buffers
reference it. A buffer cannot be destroyed while any recorded command
references it. Resetting or destroying a command buffer releases its
references, including after a validation/capacity failure. Destroying
a GPU device with any live child returns `OPENAGC_ERROR_BUSY`.
All calls on one context, its devices, and their objects require caller
serialization.

Only one **host copy** queue, 64 fences, and 32 command buffers per device
are available. Each command buffer is limited to 128 copies and 4096
recorded dwords. Each copy is nonempty, four-byte aligned and in bounds,
uses at most `0x1ffffc` bytes in **one** packet, and rejects overlap
even when two buffers alias one allocation. GPU-address overflow, size
overflow, cross-device handles, unbound memory, unsupported usage, and
capacity exhaustion return explicit errors before mutating the command
stream. Submissions require an executable nonempty command buffer and
an unsignaled same-device fence. The queue preflights the entire request,
then synchronously performs CPU copies in recorded order, attaches one
simulated EOP marker, and signals its fence. There is no pending GPU work
or blocking wait; `fence_poll` returns `NOT_READY` until a host submission
signals the fence. Queue submission snapshots expire on the next successful
submit or queue destruction; command recording snapshots expire on reset
or command-buffer destruction.

Every allocation and fence gets a deterministic page-spaced **synthetic**
48-bit address starting at `0x0000000100000000` per host GPU device. These
addresses are neither console virtual addresses nor mapped GPU memory.
The host can copy bytes into/out of allocations, but `DEVICE_LOCAL` memory,
graphics/compute queue creation, native-tiled images, shader modules, draw
commands, and presentation are explicitly unsupported. No success-shaped
no-op stands in for GPU rendering.

## Host-linear images and logical graphics state

`openagc/graphics.h` supports only RGBA8_UNORM and BGRA8_UNORM
single-layer, single-mip, single-sample **host-linear** color-target
descriptors. Width and height must each be 1..4096; explicit row pitch
must be at least width times four bytes and aligned to four. The complete
footprint is row pitch times height, capped at 16 MiB. Both products and
the memory-bound range are checked without 32-bit wrap. Up to 64 images
may be created per device; an image binds once to a same-device host
allocation. That allocation remains busy until the image is destroyed.
Depth/stencil, native tiling, sampled/scanout usage, mipmaps, arrays,
and multisampling are rejected rather than simulated.

Up to 32 graphics command buffers may each record 1..128 pointer-free
commands. The only supported logical ownership/state pairs are
`UNDEFINED/HOST` and `COLOR_TARGET/GRAPHICS`, with an explicit
transition in either direction. `GRAPHICS` is a **logical owner**, not
an available graphics queue. Recording a clear requires a bound image
already transitioned to color-target/graphics, an explicit
in-bounds positive scissor, and available capacity. Only one color
target is allowed per recording. Invalid order, stale state, unbound
memory, cross-device handles, unsupported owner/format, overflow,
and capacity failures leave the command stream unchanged. Each
recorded image is retained until reset/destruction, so in-use image
destruction returns `OPENAGC_ERROR_BUSY`.

`openagc_graphics_command_buffer_apply_host_state` preflights **all**
recorded images' state, owner, and generation before committing any
logical state. A stale concurrent recording, including an ABA
state change, is rejected transactionally. It **does not modify image
bytes, rasterize the clear, emit graphics PM4, signal a GPU fence, or
submit work**. Recording views expose deterministic image IDs and
typed transition/bind/scissor/clear commands with `gpu_submitted=0`.
They expire on reset or destruction. This explicit metadata-only model
is a foundation for later Vulkan/OpenGL translators, not a claim that
their graphics resource contracts are implemented.

## Structural shader intake and pipeline plans

`shader.h` accepts a versioned **unverified fixture** envelope for
target gfx1013. It copies 4..65,536 bytes of opaque four-byte-aligned
payload and up to eight sorted, unique uniform-buffer declarations
before validating their SHA-256 and reflection shape. A matching hash
checks byte integrity, **not** compiler origin or shader validity.
The only accepted descriptor set is zero, bindings are numbered 0..7,
and uniform minimum sizes are 4..4096 aligned bytes. Vertex fixtures
must declare position output; pixel fixtures must declare color output
at location zero (mask 1) with an RGBA8/BGRA8 export format and only varyings produced by
their vertex partner. Compute fixtures declare nonzero workgroups with
at most 1024 total invocations. Unsupported stages, binding kinds,
formats, malformed counts, bad digests and incompatible linkage return
explicit errors without retaining a partial artifact.

Host pipeline plans validate either a vertex/pixel pair with a bound,
logical graphics-owned color target of the exact pixel export format,
or one compute artifact without a target. Supplied uniform buffers must
be bound on the same device, have the **metadata-only**
`OPENAGC_GPU_BUFFER_SHADER_READ_BIT` usage, cover the declared minimum,
and match the reflection's binding set exactly. Plans deep-copy their
binding list and retain every artifact, buffer, and target image until
destruction. Thus attempts to destroy in-use resources return `BUSY`;
failed plans retain none. A device admits at most 64 structural
artifacts and 32 plans. There is **no shader command-recording entry
point**: every artifact and plan reports `compiler_verified=0`,
`gpu_executable=0`, and cannot be promoted by
`openagc_shader_artifact_require_compiler`.

The release-tagged, build-time-only
[compiler provenance plan](shader-toolchain.md) pins public source
revisions without vendoring or fetching them. No compatible compiler,
actual compiled artifact, local binary digest, or manifest is present.
The code checks a claimed PSBC provenance envelope for the published
source/release pin, then returns `NOT_READY` instead of accepting it.
Fixtures in CTest contain arbitrary test bytes, **not** AGC shader
code. No external Mesa/OpenGNM runtime is linked.

## PM4 field-derived host vectors

The strictly limited host encoder records the FW9.40-note raw
`IT_DMA_DATA` memory-to-memory form in seven dwords:
`0xC0055002`, `0x8C00C000`, source low/high, destination low/high,
and an aligned byte count (at most `0x1ffffc`). The trailing
action-based `IT_RELEASE_MEM` EOP form uses eight dwords:
`0xC0064900`, `0x06703514` (event 0x14, index 5, GCR 0x703, cache 3),
`0x20000000` (data selection 1), aligned marker low/high, a nonzero
32-bit sequence, zero, zero. It is followed by the two-dword NOP
`0xC0001000, 0`. Tests lock down all 17 words for one copy and fence.

These words are **field-derived fixtures, not independently captured
per-packet FW9.40 hardware goldens**. The user's local
`ProsperoAI-main/notes/re/940-gpu-empirics.md` reports that the raw
spoofer-layout DMA copy executed and that the **action-based** release
fence fired on physical FW9.40; the related `940-gc-ioctl.md` gives the
DMA field selectors. The latter note's earlier legacy EOP word order
is intentionally excluded because the later empirical note says it
did **not** fire. Public
[OpenAGC PM4 definitions](https://github.com/OpenAGC/OpenAGC/blob/main/include/agc_pm4.h)
and its
[release packet layout](https://github.com/OpenAGC/OpenAGC/blob/main/src/cb_builders.c)
provide factual field interpretation, not copied implementation.
Public OpenAGC's other-firmware copy control word is **not** substituted
for the FW9.40-observed form. No OpenAGC host packet has been tried on
console hardware, and the deny-all PS5 target has no packet encoder or
submission path.

## Freestanding PS5 integration

Build only `src/openagc_ps5_policy.c` for the SDK's
`--target=x86_64-sie-ps5`; do **not** compile or link `src/openagc.c` or
`src/openagc_gpu.c`, `src/openagc_graphics.c`,
`src/openagc_shader.c`, or `src/openagc_sha256.c` into a PS5 image.
The sole standard header required is
`<stdint.h>` through the public header. With the SDK's Clang cross-compiler,
LLVM archiver, and target include directory set as `PS5_CC`, `PS5_AR`, and
`PS5_SDK_TARGET_INCLUDE`, respectively, a PowerShell recipe is:

```powershell
$clangInclude = Join-Path (& $env:PS5_CC -print-resource-dir) 'include'
New-Item -ItemType Directory -Force .\out | Out-Null
& $env:PS5_CC --target=x86_64-sie-ps5 -std=c99 -O2 -ffreestanding -fno-builtin -fPIC -fPIE -fvisibility=hidden -nostdinc -isystem $clangInclude -isystem $env:PS5_SDK_TARGET_INCLUDE "-I$PWD\include" -Wall -Wextra -Werror -c .\src\openagc_ps5_policy.c -o .\out\openagc_ps5_policy.o
& $env:PS5_AR rcs .\out\libopenagc_ps5_policy.a .\out\openagc_ps5_policy.o
```

Both explicit include paths must come from the SDK/Clang toolchain, **not
host-system includes**. The shader-extended policy source cross-compiled
with the SDK's `x86_64-sie-ps5` Clang, with no undefined symbols under
`llvm-nm -u`; this version was not run on a console.
Recheck undefined imports after every change; no
missing symbol may be satisfied by the host library. This is build/link
evidence, not hardware operation or firmware qualification. In CMake integration,
configure the SDK toolchain with
`-DOPENAGC_PS5_POLICY_ONLY=ON`; this omits both host code and host tests and
builds only `OpenAGC::ps5_policy`. The CMake target adds
`-ffreestanding -fno-builtin` with Clang/GCC; the SDK toolchain must supply
its own `--target`, `-nostdinc`, and target include paths.

## Graphics frontends and presentation: research, not support

The public
[PS5_Vulkan overview](https://github.com/mihawk-99/PS5_Vulkan/blob/main/README.md)
separates command/resource/pipeline/descriptor work from swapchain and
VideoOut, and its
[headless milestone](https://github.com/mihawk-99/PS5_Vulkan/blob/main/docs/M5_PHASE_B.md)
shows why a fence must be independent of a display flip. The public
[ps5-opengl architecture](https://github.com/blackbearreloaded/ps5-opengl/blob/main/docs/architecture.md)
separates GL state tracking and shader compilation from a native
submission/presentation backend. These are **architectural research
references only**, not dependencies or code donors.
The public
[PS5_Vulkan compiler milestone](https://github.com/mihawk-99/PS5_Vulkan/blob/main/docs/M5_PHASE_A.md)
describes external PSBC output as raw code plus versioned typed metadata;
neither its compiler nor its shader package writer is used at runtime here.

Future original frontends can map Vulkan 1.0 buffer/image descriptors,
queue ownership, pipeline state, descriptors, semaphores and render passes
or OpenGL state and shader programs onto a separately qualified AGC
backend. The next missing primitives are **native** image layouts and
tiling beyond the host-linear metadata subset, an independently verified
build-time shader compiler/metadata adapter and executable pipeline
contract, real render-target/draw PM4, coherency/barriers, and an
independent presentation/VideoOut interface. They require their own
firmware-specific evidence and capability gates. This milestone
exposes none of Vulkan 1.0, OpenGL, shader execution, image rendering,
or VideoOut as supported.

## Firmware-9.40 proof gates: passive baseline only

**As of 2026-09-24, only a Stage 0 passive PAYLOAD baseline has been
observed.** An already-known-good resident service supplied a read-only
log window of 60.053 seconds, approximately 53 milliseconds beyond the
planned 60-second ceiling. The user reported that the UI stayed
responsive and that settings displayed firmware "9.40"; the exact
build ID remains unknown. A sanitized scan found no panic/trap or GPU
fault/hang/timeout marker and no `/dev/gc`, AGC, or VideoOut marker;
unrelated generic error lines were not counted as GPU faults. This is
**passive evidence with an incomplete build identity and a slight
duration overrun**, not a strict-duration pass or GPU qualification.
The private raw capture is not stored in this repository. No new
OpenAGC binary, ioctl, device access, VideoOut, kernel write, GPU
command, submission, or deployment was involved. Do not retest
automatically; any newly approved passive observation must use a
monotonic clock and a conservative 59-second deadline.

There is **no Stage 0 approval for a native-app PKG**. Payload
observations do not establish native-app permissions or behavior.
Stage 1 has not been designed or approved; review the passive baseline
before proposing any further diagnostic, and obtain separate explicit
approval per environment. Even a future read-only
device query would require its own reviewed, bounded design. A later
GPU copy/EOP proof would additionally require trusted firmware
identification, valid-memory/IB review, finite timeouts, an operator
recovery plan, and independently checked marker **and** destination
bytes with clean klogs. These are prerequisites, **not** permission
to submit packets or lift the firmware gate.

Any future plan must exclude `flat_load`, invalid indirect buffers,
queue-create/Ring paths known to disrupt the UI, and automatic retries.
No firmware is qualified for graphics or VideoOut. A trusted allowlist
must remain ahead of all kernel/device/video operations on every
entry path; caller-supplied firmware numbers are never proof.

## Attribution and license

Public OpenAGC packet field declarations and the two public graphics
architecture documents above were read for factual context. The
user-provided ProsperoAI notes supplied only reported FW9.40 interface
measurements; their compute/AI code was not used. No source code, asset,
shader, proprietary SDK content, firmware, key, or binary from those
projects was copied, vendored, linked, or made a runtime dependency.
All API and implementation code here was authored for OpenProspero.
Copyright (C) 2026 OpenProspero; GPL-3.0-or-later. See `LICENSE` for the
unaltered GNU GPL version 3 text.
