# Roadmap: one OpenAGC backend, two frontends

This document is the handover plan for making **Vulkan 1.0 and OpenGL
run on the same OpenAGC backend**, with as much shared code as
possible. It records what exists, what each stage must deliver, which
evidence closes a stage, and which gates a stage may not cross.

Rules this roadmap obeys:

1. Nothing here claims console capability. Every host stage is a host
   implementation of a *contract*; console use requires the separate,
   firmware-specific approval process in
   [architecture.md](architecture.md#firmware-940-proof-gates-passive-baseline-only).
2. A capability is real only when a test proves the accepted **and**
   refused paths. Refusals are first-class results, never silent no-ops
   that look like success.
3. No reference project (PS5_Vulkan, ps5-opengl, Mesa, OpenGNM, public
   OpenAGC) is vendored, linked, or copied. Public work is read as
   architectural research only.
4. `src/openagc_ps5_policy.c` stays deny-all. Every new public symbol
   gets a fail-closed stub there, and the host library never appears in
   a PS5 image.

## Where we are

| Layer | File | State |
| --- | --- | --- |
| UI recorder (version-1 original ABI) | `src/openagc.c` | Host recording only |
| Driver core: memory, buffers, copy queue, fences, reference PM4 | `src/openagc_gpu.c` | Host copy simulation |
| Graphics core: host-linear images, logical state/owner machine, CPU clear execution | `src/openagc_graphics.c` | Host metadata + CPU fills |
| Shader intake: structural artifacts and pipeline plans | `src/openagc_shader.c` | No compiler, nothing executes |
| **Shared frontend core** | `src/openagc_frontend.c` | Translation + shared image I/O |
| Fail-closed PS5 policy | `src/openagc_ps5_policy.c` | Denies every entry point |

Tests today: `openagc_host`, `openagc_gpu`, `openagc_graphics`,
`openagc_shader`, `openagc_frontend`, `openagc_ps5_policy` (CTest).
Everything below builds on `include/openagc/{driver,graphics,shader,frontend}.h`.

## Target architecture

```
        ┌──────────────────────┐        ┌──────────────────────┐
        │  Vulkan frontend     │        │  OpenGL frontend     │
        │  vk* entry points    │        │  gl* entry points    │
        └──────────┬───────────┘        └──────────┬───────────┘
                   │  both call the same shared layer
        ┌──────────▼───────────────────────────────▼───────────┐
        │  Shared frontend core  (openagc/frontend.h)          │
        │  native↔backend translation, frontend device,        │
        │  image/buffer I/O, capability gating, refusal map    │
        └──────────┬───────────────────────────────┬───────────┘
                   ▼                               ▼
        ┌──────────────────────┐        ┌──────────────────────┐
        │ Driver core          │        │ Graphics core        │
        │ memory/buffer/queue/ │◄──────►│ image + logical      │
        │ fence/PM4 encoder    │        │ state & owner machine│
        └──────────┬───────────┘        └──────────┬───────────┘
                   └────────────┬───────────────────┘
                                ▼
                   ┌────────────────────────┐
                   │ AGC/PM4 backend        │  not implemented
                   │ (needs FW evidence)    │
                   └────────────────────────┘
```

**Dependency rule:** frontends depend on the shared core; the shared
core depends only on `driver.h` + `graphics.h` + `shader.h`. A frontend
must never call the driver core directly for something the shared core
already offers — that is how the two frontends stay one backend.

## Stage 1 — shared frontend core (done)

`include/openagc/frontend.h`, `src/openagc_frontend.c`,
`tests/test_openagc_frontend.c`.

* Native→backend translation for Vulkan and OpenGL enumerants:
  format, image usage, image layout, buffer usage, plus an advertised
  native-format list so no frontend re-encodes the table.
* One `openagc_frontend_device` per backend device: one staging
  allocation (4 KiB..1 MiB), one copy queue + fence, one bounded
  transition recorder, one image counter.
* `openagc_frontend_image_*`: create from a native descriptor, query,
  explicit transition, CPU upload/readback that **preserve** the
  image's logical state and owner across I/O.
* Capability gating: `host_translation=1`, `gpu_execution=0`,
  `rasterization=0`, `presentation=0`, accepted format/usage masks.

Everything unsupported is refused with an explicit error: sRGB, depth
testing, R8, GL `GL_RGB8`, transfer-only Vulkan images, `GENERAL`,
`PREINITIALIZED`, `PRESENT_SRC_KHR`, storage/vertex/index/indirect
buffer usage, unknown kinds.

## Stage 2 — shared resource core (done)

Extend the shared layer so both frontends get the same objects. The
backend allows only 64 allocations per device, so images and buffers
share one host-linear heap.

Delivered:

1. `openagc_frontend_buffer_*` — one buffer object over the same
   staging/copy path as images, with the translated usage enforcing
   copy direction.
2. A first-fit suballocator over one `openagc_gpu_memory` heap
   (256-byte blocks, adjacent free blocks coalesced). Images and
   buffers take blocks from it instead of one allocation each.
3. `openagc_frontend_timeline_*` — a monotonic host counter. `signal`
   publishes the next synchronous point and `poll` returns `NOT_READY`
   for a later value. It does not block and does not pretend a GPU
   submit is in flight.
4. `openagc_frontend_pipeline_*` — the only plan constructor the
   frontends use. A plan that reports `compiler_verified` or
   `gpu_executable` is refused.

Evidence: `test_shared_heap_timeline_and_plan` creates more buffers
than the backend allocation cap, reuses a freed block, polls the
timeline, and builds one compute plan with `gpu_executable=0`.

## Stage 3 — Vulkan 1.0 subset frontend (host)

Started in `include/openagc/vulkan.h`. The host subset enumerates one
physical device whose only queue family is transfer, creates buffers
and images through the shared frontend, records buffer copies and image
layout transitions, and publishes a synchronous fence when that recording
is submitted. Graphics and compute families are absent, not emulated.
A color render pass, viewport, scissor, vertex buffer, index buffer,
graphics plan, and descriptor set can be recorded on that transfer
pool. The set keeps every reflected set-0 slot. A vertex attribute must cover every location in the vertex shader input mask. A draw or dispatch
is `BAD_STATE` until each of those slots is present. `vkCmdDraw` and `vkCmdDispatch` then call
`openagc_shader_artifact_require_compiler` and return `NOT_READY`.
They do not rasterize or run a workgroup. Swapchains stay refused.
An identity 2D color image view and a CPU color clear of a color
target are recorded from a transfer command pool. Reset of that pool
drops the recording. `gpu_execution` and `presentation` stay 0.

Scope is a **conformance-shaped subset**, not a full ICD:

* instance/device enumeration with an honest `VkPhysicalDevice` surface:
  copy queue family only; graphics/compute families advertised as
  absent (not emulated silently);
* `VkDeviceMemory`/`VkBuffer`/`VkImage`/`VkImageView` over stages 1-2;
* command pools/buffers whose recording mirrors the host model:
  layout transitions, copies, clears, one color render pass, viewport,
  scissor, vertex and index bindings, and descriptor sets;
  `vkCmdDraw` and `vkCmdDispatch` return `NOT_READY` from the compiler
  gate and do not execute; dynamic rendering stays refused;
* fences/semaphores through the stage-2 timeline; no blocking waits.
  A semaphore wait that has not been reached returns `NOT_READY` and
  leaves the command buffer executable;
* `VK_KHR_swapchain`/surfaces refused until stage 7;
* format support derived from `openagc_frontend_translate_format`, so
  the Vulkan-visible format table and the backend table cannot diverge.

Evidence: a host test binary that runs a Vulkan-shaped resource and
transfer workload, plus negative tests for every refused entry point.
Hardware/CTS runs are out of scope for this stage.

## Stage 4 — OpenGL-style frontend (host)

Started in `include/openagc/opengl.h`. A context creates textures,
renderbuffers, and one color framebuffer on the shared frontend.
`glTexSubImage2D` and `glGetTexImage` use the image upload and readback
paths. Sampling derives `SHADER_READ/GRAPHICS`, an FBO color attachment
derives `COLOR_TARGET/GRAPHICS`, and `glFinish` publishes the same
timeline a Vulkan fence uses. The default framebuffer is refused.
`glDrawArrays` and `glDispatchCompute` use the same compiler gate as
Vulkan and return `NOT_READY` when a pass or compute plan is bound;
they do not rasterize or run a workgroup. Pixel unpack accepts a write
and refuses a read; pixel pack does the opposite. A compute program is
a host pipeline plan with `compiler_verified=0` and `gpu_executable=0`,
the same plan Vulkan creates. A graphics program is the same kind of
plan for a vertex and pixel pair whose color target is already
`COLOR_TARGET/GRAPHICS`; a sampled image is refused, and the plan
stays non-executable.
`test_openagc_equivalence` checks that a sampled texture and a
`SHADER_READ_ONLY_OPTIMAL` image, and a cleared FBO and a cleared Vulkan
color target, land on the same backend state and the same pixels, and
that both graphics plans share that non-executable state.
`gpu_execution` and `presentation` stay 0.

Deliverables:

* a context object with a state-tracking table, buffer/texture/
  renderbuffer objects, FBO/attachment binding, and program objects;
* an explicit **derivation table** mapping implicit GL commands onto
  backend transitions, e.g. `glTexSubImage2D` → transfer-destination
  write, `glReadPixels`/`glGetTexImage` → transfer-source read,
  sampling a bound texture → `SHADER_READ/GRAPHICS`, FBO attachment →
  `COLOR_TARGET/GRAPHICS`, `glMemoryBarrier` → recorded transition;
* `glFinish`/`glFenceSync` mapped onto the stage-2 frontend timeline;
* refusals for everything with no backend meaning (geometry and
  compute draws, MSAA, depth testing, sRGB, default framebuffer/presentation).

This is where the "same backend" claim gets its strongest test: the GL
path must produce the **same** backend transitions as the Vulkan path
for equivalent work, asserted by shared helper checks in CTest.

## Stage 5 — executable pipelines (gated)

Blocked by two independent gates:

1. **Compiler**: no pinned, verified PSBC/OpenGNM compiler is installed;
   `openagc_shader_artifact_require_compiler` always returns
   `NOT_READY`. See [shader-toolchain.md](shader-toolchain.md). Only
   after a reviewed build-time artifact, an original metadata adapter,
   and a pinned executable manifest may compiled-artifact intake exist.
2. **Draw/render PM4**: real render-target and draw packets have no
   independently verified FW9.40 evidence in this repository, and the
   current PM4 encoder is field-derived fixtures only.

Until both close, stages 3 and 4 must refuse draws and dispatches. If
they ever open, the pipeline/descriptor contract from stage 2 is the
single place both frontends plug into.

## Stage 6 — coherency, native layouts, tiling

The host core is host-linear only. Native tiling, swizzles, layout
conversion, and cache coherency rules need their own firmware evidence
and their own review; until then Vulkan images are linear/host-visible
and GL textures are linear, and both frontends must report that
capability honestly.

## Stage 7 — presentation

Vulkan swapchain and GL window-system integration need a display path
that does not exist here (no VideoOut, no flip, no display controller).
Presentation stays refused, and it is a separate interface with its own
approval, not a mode of the copy queue.

## Stage 8 — console qualification

Unchanged and untouched by any host stage: only a passive Stage 0
baseline has ever been observed, with an incomplete build identity.
Anything beyond that needs a reviewed, bounded design and separate
explicit approval per environment; no stage above may submit a packet
to a console. The passive record and the bounded first experiment (one
copy, one fence, finite deadline, no retries) are specified in
[hardware-evidence.md](hardware-evidence.md).

## Reuse map

| Frontend concept | Shared entry point | Backend contract |
| --- | --- | --- |
| `VkFormat`, GL internal format | `openagc_frontend_translate_format` | RGBA8/BGRA8 host-linear only |
| `VkImageUsageFlags`, GL texture target | `openagc_frontend_translate_image_usage` | color-target / sampled bits |
| `VkImageLayout`, GL state derivation | `openagc_frontend_translate_image_layout` | image state + owner machine |
| `VkBufferUsageFlags`, GL buffer target | `openagc_frontend_translate_buffer_usage` | copy source/destination, shader-read |
| `vkCreateImage`, `glTexImage2D` | `openagc_frontend_image_create` | bounded image + memory + buffer |
| `vkCmdCopyBufferToImage`, `glTexSubImage2D` | `openagc_frontend_image_upload` | transition → copy → transition |
| `vkCmdCopyImageToBuffer`, `glGetTexImage` | `openagc_frontend_image_readback` | transition → copy → transition |
| `vkCmdPipelineBarrier`, `glMemoryBarrier` | `openagc_frontend_image_transition` | usage-checked state change |
| `VkFence`, `VkSemaphore`, `glFenceSync` | stage 2 timeline wrapper | single-shot backend fence |
| `VkQueue` (copy family) | shared core staging path | one host copy queue |
| `VkPipeline`, descriptor sets, GL program | stage 2 plan bridge | structural shader plans |
| `VkDeviceMemory`, GL buffer storage | `openagc_frontend_memory_*` | host-visible heap slice |

## Design rules for later developers

* Add a capability in one place: header (versioned struct + INIT macro
  + doc comment), implementation, tests (accepted **and** refused), the
  PS5 policy stub, and the docs. Missing any of the five is an
  incomplete change.
* Every struct carries `struct_size` (and `api_version` where it is a
  descriptor); a layout change is rejected by older callers instead of
  being misread.
* Errors are explicit and specific: `UNSUPPORTED_OPERATION` for
  something this backend cannot represent, `BAD_STATE` for lifecycle
  misuse, `OUT_OF_RANGE` for values outside a validated range,
  `NOT_READY` for hardware that has not qualified.
* Host paths must stay deterministic: synchronous copies, no threads in
  the library, caller-serialized calls.
* Keep the C99/no-comment-noise style; comments explain *why*, not
  *what*.
* Run `ctest`, a sanitizer build, and the fail-closed policy build
  before calling a stage done. Verify the policy target defines every
  public symbol and has no undefined imports.
