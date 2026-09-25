# Architecture and qualification

## Trust boundary

`include/openagc/openagc.h` keeps the original version-1 UI recorder ABI.
`include/openagc/driver.h` adds a separate version-1 driver ABI with opaque
GPU-device, memory, buffer, command-buffer, queue, and fence handles.
`include/openagc/graphics.h` adds a version-1 graphics-resource ABI with
opaque image and render-state command-buffer handles. All five APIs
reject incorrect descriptor sizes and versions. `include/openagc/shader.h`
adds version-1 host-only gfx1013 artifact and pipeline-plan handles, and
`include/openagc/frontend.h` adds the version-1 shared layer that a
Vulkan or OpenGL frontend is meant to reuse.
`src/openagc.c`
implements the host UI recorder; `src/openagc_gpu.c` implements the host
copy driver core; `src/openagc_graphics.c` records host-only image state;
`src/openagc_shader.c` deep-copies and validates unverified structural
artifacts using OpenProspero's C99 SHA-256 implementation;
`src/openagc_frontend.c` translates native frontend descriptors and owns
one staging/copy/transition path per device.
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
`rasterization=0` together with `host_state_recording=1` and
`host_clear_simulation=1`, which describe host bookkeeping and CPU
pixel fills, not rendering. Driver capabilities separately report host copy
simulation and reference packet encoding. Shader capabilities report
`compiler_available=0` and `gpu_execution=0`. The version-1 capability,
image-info, artifact-desc/info, and pipeline-desc/info structs all
gained fields (clear simulation, image usage, and texture declarations
or counts), so a caller built against an earlier struct is rejected by
the `struct_size` check rather than misreading the following fields.

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
stream. Every copied range is additionally checked against the device's
images: bytes that belong to an image may be written only while that
image is `TRANSFER_DESTINATION/COPY` and read only while it is
`TRANSFER_SOURCE/COPY`. That check runs when the copy is recorded and
again while the queue preflights a submission, so a state change between
recording and submission fails the whole submission without transferring
a byte or signaling its fence. Host CPU access through
`memory_write`/`memory_read` is deliberately **not** policed by this
model: allocations are host memory, and the ownership rules bind the
GPU-shaped paths (the copy queue and the graphics commands), not the
host's own view of its allocations.
Submissions require an executable nonempty command buffer and
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
single-layer, single-mip, single-sample **host-linear** descriptors
whose usage mask declares a nonempty subset of color target and
sampled; that mask is reported per image and through
`openagc_graphics_get_capabilities`. Width and height must each be
1..4096; explicit row pitch
must be at least width times four bytes and aligned to four. The complete
footprint is row pitch times height, capped at 16 MiB. Both products and
the memory-bound range are checked without 32-bit wrap. Up to 64 images
may be created per device; an image binds once to a same-device host
allocation. That allocation remains busy until the image is destroyed.
Depth/stencil, native tiling, scanout usage, mipmaps, arrays,
and multisampling are rejected rather than simulated.

Up to 32 graphics command buffers may each record 1..128 pointer-free
commands. The five supported logical ownership/state pairs are
`UNDEFINED/HOST`, `COLOR_TARGET/GRAPHICS`, `SHADER_READ/GRAPHICS`,
`TRANSFER_SOURCE/COPY`, and `TRANSFER_DESTINATION/COPY`, with an
explicit transition between any two of them. A transition is refused
unless the destination state is one the image's usage declares:
`COLOR_TARGET` requires the color-target bit, `SHADER_READ` requires
the sampled bit, and the undefined and transfer states require none,
so a sampled-only image can never become a render target and a
color-target-only image can never become a sampling source. `GRAPHICS`
and `COPY` are **logical owners**, not
available queues. Recording a clear requires a bound image
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
They expire on reset or destruction. This explicit logical-state model,
together with the host-only CPU clear fill below, is a foundation for
later Vulkan/OpenGL translators, not a claim that
their graphics resource contracts are implemented. The command-buffer
lifecycle is `INITIAL`, `RECORDING`, `EXECUTABLE`, `HOST_APPLIED`,
then `HOST_EXECUTED`; reset returns any non-initial state to
`INITIAL`.

`openagc_graphics_command_buffer_execute_host` is the only entry point
that writes image bytes, and it runs only after `apply_host_state`
reaches `HOST_APPLIED`. It requires a bound target, at least one
recorded clear, and every referenced image still at the exact logical
generation committed by that apply; any mismatch, including an ABA
return to the same state and owner, returns `BAD_STATE` before a single
byte is written. It then replays the recording in order and fills each
clear's current scissor with CPU stores: RGBA8_UNORM receives R,G,B,A
and BGRA8_UNORM receives B,G,R,A, while bytes outside the scissor and
the row-pitch padding are never touched. Logical state and owner stay
unchanged, re-execution requires a reset, and the reported
`target_image_id`, `clear_count`, and `cleared_pixels` carry
`gpu_submitted=0`. This is a deterministic host fill, **not**
rasterization or draw execution: no shader runs, and there is no
depth/stencil, blending, PM4, fence, or display frame.

## Structural shader intake and pipeline plans

`shader.h` accepts a versioned **unverified fixture** envelope for
target gfx1013. It copies 4..65,536 bytes of opaque four-byte-aligned
payload plus up to eight sorted, unique uniform-buffer declarations
and up to eight sorted, unique sampled-image declarations before
validating their SHA-256 and reflection shape. A matching hash
checks byte integrity, **not** compiler origin or shader validity.
The only accepted descriptor set is zero, bindings are numbered 0..7
in one shared number space, so a texture and a uniform buffer may not
claim the same binding, and uniform minimum sizes are 4..4096 aligned
bytes. Sampled-image declarations carry an RGBA8/BGRA8 format; no
other format, and no depth, is accepted. Vertex fixtures must declare
position output; pixel fixtures must declare color output
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
and match the reflection's binding set exactly. Supplied sampled images
must be bound on the same device, carry the declared sampled usage,
hold the exact declared format, and be in `SHADER_READ/GRAPHICS`; they
match the reflected texture slots exactly, and one image may not fill
two slots. A texture slot declared by both stages must agree on its
format. Plans deep-copy their binding lists and retain every artifact,
buffer, texture, and target image until destruction. Thus attempts to
destroy in-use resources return `BUSY`; failed plans retain none. A
device admits at most 64 structural
artifacts and 32 plans. There is **no shader command-recording entry
point**: every artifact and plan reports `compiler_verified=0`,
`gpu_executable=0`, and cannot be promoted by
`openagc_shader_artifact_require_compiler`. A bound texture is
structural metadata: nothing reads a texel.

The release-tagged, build-time-only
[compiler provenance plan](shader-toolchain.md) pins public source
revisions without vendoring or fetching them. No compatible compiler,
actual compiled artifact, local binary digest, or manifest is present.
The code checks a claimed PSBC provenance envelope for the published
source/release pin, then returns `NOT_READY` instead of accepting it.
Fixtures in CTest contain arbitrary test bytes, **not** AGC shader
code. No external Mesa/OpenGNM runtime is linked.

## Shared frontend layer

`include/openagc/frontend.h` exists so that a Vulkan 1.0 frontend and an
OpenGL frontend share one implementation of everything both need. It is
a translation and ownership layer with no execution path of its own.

The translation functions accept published Vulkan and OpenGL enumerants
and return this backend's descriptors, so the accepted set lives in one
place and a frontend never re-encodes it. Vulkan image layouts map onto
the logical state/owner machine: `COLOR_ATTACHMENT_OPTIMAL` to
`COLOR_TARGET/GRAPHICS`, `SHADER_READ_ONLY_OPTIMAL` to
`SHADER_READ/GRAPHICS`, and the two transfer layouts to the copy-owned
states. `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` maps to
`DEPTH_TARGET/GRAPHICS`. `GENERAL`, `PREINITIALIZED`, `PRESENT_SRC_KHR`,
and the depth read-only layout are refused rather than approximated. OpenGL has no layout
enumerant, so a GL caller passes zero and derives states from the
commands it records; that asymmetry is why the shared layer exposes an
explicit transition entry point instead of a layout parameter
everywhere. Image usage maps to the color-target and sampled bits, and
a transfer-only Vulkan image is refused because the backend has no
usage bit it could declare.

`openagc_frontend_device` owns one staging allocation (4 KiB to 1 MiB),
one copy queue with its fence, and one bounded transition command
buffer per backend device, and it counts its images so the frontend
device cannot be destroyed underneath them. Upload and readback are the
only byte-moving paths: each reads the image's current logical state,
moves it to the matching copy-owned state, runs one **CPU** copy through
the staging allocation, and restores the original state and owner, so a
frontend never reimplements the ownership dance. Buffers are the same
object shape without the state machine: `openagc_frontend_buffer_*`
allocates one backend buffer, translates its usage, and moves bytes
through the same staging path, which is why a Vulkan transfer-destination
buffer and a GL unpack buffer behave identically while a pack buffer
refuses a write. `openagc_frontend_buffer_copy` is the copy a recorded
Vulkan command executes: the source must be a copy source, the
destination a copy destination, and both must belong to one frontend
device. `openagc_frontend_image_clear` fills a color target through the
graphics host clear and leaves the logical state unchanged. A Vulkan
image view is an identity 2D color view of one mip and one layer; it
does not allocate a second image, and the image stays alive until every
view is destroyed. Clears, presentation,
draws, and GPU execution do not exist here; frontend capabilities report
`host_translation=1` with `gpu_execution=0`, `rasterization=0`, and
`presentation=0`, and its format and usage masks come from the graphics
layer so the two cannot diverge. Images and buffers are placed in one
first-fit heap so they do not each consume a backend allocation.
`openagc_frontend_timeline_*` is the synchronous counter `VkFence`,
`VkSemaphore`, and `glFenceSync` share. `openagc_frontend_memory_*` is one host-visible slice of that heap.
A resource created unbound stays unusable until it is bound at a
256-byte offset; a dedicated resource cannot be bound again, overlapping
ranges are refused, and freeing a slice that still has a binding returns
`BUSY`. Device-local memory is not offered. A sampler records nearest filtering
and clamp-to-edge only; linear filtering is refused and no texel is read.
A render pass is one color target already in `COLOR_TARGET/GRAPHICS`,
plus an optional depth image already in `DEPTH_TARGET/GRAPHICS` and the
same size. That attachment is only a record: no depth test runs, and a
D24S8 image is refused as a color target. A depth `CLEAR` packs
`D24_UNORM_S8_UINT` (depth in the low 24 bits, stencil in the high byte)
through the same CPU fill as a color clear, limited by the scissor when
one is set. `LOAD` leaves those bytes. Beginning the pass does not draw. `LOAD` leaves the pixels; `CLEAR` writes the color first. A later clear inside the pass writes only the scissor. Viewport and scissor must fit the
color target; a rectangle outside it is `OUT_OF_RANGE`. A vertex binding records one
buffer with the vertex usage. A draw reads each attribute and discards the words; it does not rasterize. Rate 0 follows the vertex index and rate 1 follows `first_instance`. A divisor above 1 is refused. A graphics plan starts as a triangle list. Points, lines, and triangle strips or fans are the same primitive on both frontends; another topology is refused and the previous one stays. The primitive is not assembled. A push-constant block of at most 128 bytes is the same storage OpenGL writes with a uniform. An unaligned or oversized write is `OUT_OF_RANGE` and leaves the previous bytes. The draw reads that block and still does not run the shader. Blend factors are stored on that same plan, disabled by default with source one and destination zero. Enabling source-alpha over one-minus-source-alpha does not change a clear or a draw. Memory that is not bound is `BAD_STATE`. An indexed draw reads each stored index, adds the signed base vertex, and returns `OUT_OF_RANGE` when that vertex does not fit the stride. A graphics
plan can be bound into the pass and still cannot draw: `gpu_executable`
stays 0. A non-indexed draw without a bound vertex buffer, or without a vertex
stride and one attribute that fits in it, is `BAD_STATE`. Each location set in the vertex shader input mask needs four bytes inside an attribute. One span or several attributes may cover them. A float vector format (`R32` through `R32G32B32A32`, or OpenGL `GL_FLOAT` with 1..4 components) is that span: both frontends store the same format id. A gap or a format that does not fit the stride is `OUT_OF_RANGE` and leaves the previous input. A count that does not fit that stride is
`OUT_OF_RANGE`. The fetched words are not shaded.
A draw with a viewport, that buffer, and that plan calls
`openagc_shader_artifact_require_compiler` and returns `NOT_READY`
and writes no pixels. An indirect draw reads one 16-byte record from an indirect buffer: vertex count, instance count, first vertex, and first instance. It then uses the same fetch as a direct draw. Zero vertices or zero instances complete without
that call. An indexed draw without a bound index buffer is `BAD_STATE`. A positive
index count also needs the vertex buffer and its stride.
Vulkan `UINT16` and `UINT32`, and OpenGL `UNSIGNED_SHORT` and `UNSIGNED_INT`, share one element width. A 16-bit index is zero-extended before the base vertex is added. A positive index count that does not fit in the bound buffer is
`OUT_OF_RANGE`. A count that fits uses the same compiler gate and
fetches nothing. A compute dispatch passes the workgroup counts through the shared frontend.
A zero count completes without a launch. A count above 65535 is
`OUT_OF_RANGE`. A positive count on a compute plan returns `NOT_READY`
and runs no workgroup. A descriptor set is recorded only when it
holds a buffer, a sampled view, or a sampler; an empty set is
`BAD_STATE`. Recording it does not run a shader. A draw without them returns `BAD_STATE`.
`openagc_frontend_pipeline_*` and
`openagc_frontend_graphics_pipeline_create` are the only pipeline-plan
constructors either frontend may call. The graphics constructor intakes
a vertex and pixel pair against a color target that is already
`COLOR_TARGET/GRAPHICS` and owns those artifacts. It accepts the same
set-0 slot list as a compute plan. Both refuse a plan
that claims compiler verification or GPU execution. A reflected uniform
or sampled image is copied into that same plan: a declared binding with
no buffer is `INVALID_ARGUMENT`, and the retained buffer stays `BUSY`
until the plan is destroyed. A Vulkan descriptor set for a reflected plan must be created from a pipeline layout with the same slots and ranges. A set without that layout, or a layout whose range differs, is `BAD_STATE` before the compiler gate. OpenGL accepts that same layout. Dispatch and draw return `BAD_STATE` when any reflected slot is missing
or holds a different buffer, range, or image than the plan retained. A sampled
image needs the sampler named on the pipeline. Another sampler, or none, is `BAD_STATE`, and destroying that sampler is `BUSY`. A set can
hold every set-0 slot the plan names.
Occlusion and timestamp queries can be recorded by both frontends. Reading a result writes zero availability and returns `NOT_READY`. The plan still is not executable.

The staged plan for the two frontends — shared resource core,
Vulkan-shaped subset, OpenGL-shaped subset, and the gated stages for
executable pipelines, native layouts, presentation, and console
qualification — is [roadmap.md](roadmap.md).

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
`src/openagc_shader.c`, `src/openagc_frontend.c`, or `src/openagc_sha256.c`
into a PS5 image.
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
backend. The host core holds three of those contracts already: an
explicit ownership handoff between the copy and graphics paths, a
validated state rule for every GPU-shaped access, and an exact
descriptor match for the uniform-buffer and sampled-image bindings a
plan declares. The shared frontend layer above adds the fourth piece
both frontends need: one native-to-backend translation table and one
staging/copy/transition path per device. The staged plan, including
which stages stay refused and which gates they wait on, is
[roadmap.md](roadmap.md). The remaining missing
primitives are **native** image layouts and
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
monotonic clock and a conservative 59-second deadline. The passive
record, the build-path status, and the bounded copy/EOP design that must
be reviewed before any new hardware step are in
[hardware-evidence.md](hardware-evidence.md).

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
