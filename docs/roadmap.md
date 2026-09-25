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
4. `src/openagc_ps5_policy.c` stays fail-closed for unqualified
   firmware. Every new public symbol gets a deny stub there until a
   reviewed evidence path opens it; the host library never appears in
   a PS5 image. Deny-all is the gate while FW is unqualified — not the
   product end state.

## Where we are

| Layer | File | State |
| --- | --- | --- |
| UI recorder (version-1 original ABI) | `src/openagc.c` | Host recording only |
| Driver core: memory, buffers, copy queue, fences, reference PM4 | `src/openagc_gpu.c` | Host copy simulation |
| Graphics core: host-linear images, logical state/owner machine, CPU clear execution | `src/openagc_graphics.c` | Host metadata + CPU fills |
| Shader intake: structural artifacts and pipeline plans | `src/openagc_shader.c` | No compiler, nothing executes |
| **Shared frontend core** | `src/openagc_frontend.c` | Translation + shared image/buffer I/O and copy |
| Vulkan 1.0 subset frontend (host) | `src/openagc_vulkan.c` | Transfer, clear, render-pass and pipeline recording |
| OpenGL subset frontend (host) | `src/openagc_opengl.c` | Same backend, derived from GL commands |
| Fail-closed PS5 policy | `src/openagc_ps5_policy.c` | Denies every entry point |

Tests today: `openagc_host`, `openagc_gpu`, `openagc_graphics`,
`openagc_shader`, `openagc_frontend`, `openagc_vulkan`,
`openagc_opengl`, `openagc_equivalence`, `openagc_ps5_policy` (CTest).
Everything below builds on
`include/openagc/{driver,graphics,shader,frontend,vulkan,opengl}.h`.

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

Everything unsupported is refused with an explicit error: sRGB, R8, GL
`GL_RGB8`, transfer-only Vulkan images, `GENERAL`, `PREINITIALIZED`,
`PRESENT_SRC_KHR`, the depth read-only layout, storage images and
storage-buffer usage, unknown kinds.

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
5. `openagc_frontend_image_copy_rect` and
   `openagc_frontend_buffer_fill` — the rect copy between two
   same-format images (row by row through the staging allocation, both
   images restored) and the repeating four-byte pattern over a
   copy-destination range. Both frontends reach them through their own
   entry points rather than copying bytes themselves.

Evidence: `test_shared_heap_timeline_and_plan` creates more buffers
than the backend allocation cap, reuses a freed block, polls the
timeline, and builds one compute plan with `gpu_executable=0`.

## Stage 3 — Vulkan 1.0 subset frontend (host) (done)

Delivered in `include/openagc/vulkan.h`. The host subset enumerates one
physical device whose only queue family is transfer, creates buffers
and images through the shared frontend, records buffer copies,
`vkCmdCopyImage`, `vkCmdFillBuffer`, bounded `vkCmdUpdateBuffer`
payloads, and image layout transitions, and publishes a synchronous
fence when that recording is submitted. Graphics and compute families are absent, not emulated.
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
  layout transitions, buffer and image copies, fills, inline updates,
  clears, one color render pass, viewport, scissor, vertex and index
  bindings, and descriptor sets; `vkCmdDraw` and `vkCmdDispatch` return
  `NOT_READY` from the compiler gate and do not execute; dynamic
  rendering stays refused;
* fences/semaphores through the stage-2 timeline; no blocking waits.
  A semaphore wait that has not been reached returns `NOT_READY` and
  leaves the command buffer executable;
* `VK_KHR_swapchain`/surfaces refused until stage 7;
* format support derived from `openagc_frontend_translate_format`, so
  the Vulkan-visible format table and the backend table cannot diverge.

Evidence: `test_openagc_vulkan` runs a Vulkan-shaped resource and
transfer workload, including `vkCmdCopyImage`, `vkCmdFillBuffer`, and
`vkCmdUpdateBuffer`, plus negative tests for every refused entry point.
`test_openagc_equivalence` asserts the Vulkan result matches the OpenGL
result on the same backend. Hardware/CTS runs are out of scope for this
stage.

## Stage 4 — OpenGL-style frontend (host) (done)

Delivered in `include/openagc/opengl.h`. A context creates textures,
renderbuffers, and one color framebuffer on the shared frontend.
`glTexSubImage2D` and `glGetTexImage` use the image upload and readback
paths. Sampling derives `SHADER_READ/GRAPHICS`, an FBO color attachment
derives `COLOR_TARGET/GRAPHICS`, and `glFinish` publishes the same
timeline a Vulkan fence uses. The default framebuffer is refused.
`glDrawArrays` and `glDispatchCompute` use the same compiler gate as
Vulkan and return `NOT_READY` when a pass or compute plan is bound;
they do not rasterize or run a workgroup, except the shared
`host_store_const` path (`1,1,1`) that both frontends execute as a CPU
write. Pixel unpack accepts a write
and refuses a read; pixel pack does the opposite. Uniform buffers also
accept host `glBufferData`/`glGetBufferSubData` (direct memory I/O) while
`glClearBufferSubData` on a uniform stays refused.
`glCopyBufferSubData` is the same `openagc_frontend_buffer_copy` path as
`vkCmdCopyBuffer` (pixel pack/unpack only). A compute program is
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
  `glCopyTexSubImage2D` → the shared image rect copy,
  `glClearBufferSubData` → the shared buffer fill, sampling a bound
  texture → `SHADER_READ/GRAPHICS`, FBO attachment →
  `COLOR_TARGET/GRAPHICS`, `glMemoryBarrier` → recorded transition;
* `glFinish`/`glFenceSync` mapped onto the stage-2 frontend timeline,
  and deleting the bound draw target unbinding it rather than leaving
  the context with a stale pointer;
* refusals for everything with no backend meaning (geometry and
  compute draws, MSAA, depth testing, sRGB, default framebuffer/presentation).

This is where the "same backend" claim gets its strongest test: the GL
path must produce the **same** backend transitions as the Vulkan path
for equivalent work, asserted by shared helper checks in CTest.

## Stage 5 — executable pipelines (gated)

Blocked by two independent gates:

1. **Compiler / executable shaders**: build-time PSBC pin, typed
   reflection, host register-program encoder, and pin-checked
   `OPENGNM_PSBC` envelope intake exist
   (`OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256`, `psbc_metadata.h`,
   `pm4_graphics_fw940.h`, fixtures under `tests/fixtures/psbc_smoke/`).
   Accepted envelopes report `psbc_envelope=1` with
   `compiler_verified=0` and `gpu_executable=0`;
   `openagc_shader_artifact_require_compiler` still returns `NOT_READY`.
   Envelope metadata is retained on the artifact; a graphics plan with
   both stages as envelopes auto-attaches the host SET_CONTEXT/SET_SH
   register snapshot including vertex linkage context pairs (still
   non-executable). Attribute-less plans (`vertex_input_mask == 0`)
   record draws without a VBO and still stop at `NOT_READY`. Non-empty
   PSBC descriptor bindings remain unsupported. See
   [shader-toolchain.md](shader-toolchain.md).
2. **Draw/render PM4**: real render-target and draw packets have no
   independently verified FW9.40 evidence in this repository. Copy+EOP
   encoding is console-aligned via `pm4_fw940.h` (31 dwords observed).
   A separate compute store-const IB (`pm4_compute_fw940.h`, 51 dwords)
   completed once on console; Steps I–L extend that to 8-lane
   `store_span` chains (1/2/4/8 spans through
   `OPENAGC_PM4_COMPUTE_SPAN_MAX`) without graphics draws or
   `gpu_execution`. Steps D–H (WRITE_DATA and DMA+WRITE_DATA) are
   console-proven on FW9.40; Step M proves the host
   `OPENAGC_PM4_WRITE_DATA_MAX_ROWS` (=8) full-width clear window in
   one IB; Step N proves `MAX_COLS` (=2) × MAX_ROWS in one IB
   (`openagc_gpu_host_write_data_grid`). Host color/depth clears tile
   any scissor into ≤32×8 WRITE_DATA windows; dword-aligned fills ≤512
   bytes use the single-column vehicle; `openagc_gpu_host_dma_write_data`
   records copy-then-fill; `host_store_span` / `host_store_span_n` cover
   compute fills sized from the bound buffer up to SPAN_MAX.
   Step O proved graphics-bank SET_SH + EOP; Step P proved minimal
   SET_CONTEXT_REG ×3 + EOP (smoke.vert context_registers only, no
   linkage). Step Q proved SET_CONTEXT ×3 + graphics SET_SH ×4 + EOP
   in one IB (host snapshot order, no linkage). Step R proved linkage
   SET_CONTEXT ×3 + EOP (`ge_cntl` / `stages_en` / `user_vgpr_en` from
   smoke.vert metadata). Step S proved the full host-aligned register
   program (context + SH + linkage) + EOP in one IB. Draw/render packets
   remain unavailable.

Until both close, stages 3 and 4 must refuse draws and general
dispatches. A narrow exception exists on the host only: the
console-proven store-const compute blob may run through
`openagc_gpu_host_store_const` (PM4 encode + CPU write) via
`openagc_frontend_dispatch` (`host_store_const`, groups `1,1,1`) without
setting `compiler_verified` or `gpu_executable`, and without opening a
compute queue. Vulkan records the dispatch with
`openagc_frontend_dispatch_validate` and executes it on queue submit;
OpenGL keeps immediate dispatch. Render-pass load/clear pixel fills are
likewise deferred on Vulkan submit (validate + snapshot at record time)
while OpenGL framebuffer begin/clear stay immediate. If stages 5+ ever open
further, the pipeline/descriptor contract from stage 2 is the single
place both frontends plug into.

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
| `vkCmdCopyBuffer`, `glCopyBufferSubData` | `openagc_frontend_buffer_copy` | copy-source → copy-destination |
| `vkCmdCopyBufferToImage`, `glTexSubImage2D` | `openagc_frontend_image_upload` | transition → copy → transition |
| `vkCmdCopyImageToBuffer`, `glGetTexImage` | `openagc_frontend_image_readback` | transition → copy → transition |
| `vkCmdCopyImage`, `glCopyTexSubImage2D` | `openagc_frontend_image_copy_rect` | row copy with both ownership states |
| `vkCmdFillBuffer`, `glClearBufferSubData` | `openagc_frontend_buffer_fill` | ≤512 B aligned → WRITE_DATA rows + rem; else staging |
| `vkCmdCopyBuffer` then `vkCmdFillBuffer` (head) | `openagc_frontend_buffer_copy_then_fill` | Step H DMA+WRITE_DATA coalesce on submit |
| `glCopyBufferThenClearSubData` | same | immediate Step H composite |
| `vkCmdClearColorImage` / GL clear | `openagc_frontend_image_clear` | tiled ≤16×8 WRITE_DATA windows (Steps D–G) |
| `vkCmdClearDepthStencilImage` / GL depth clear | graphics depth clear | tiled ≤16×8 D24S8 WRITE_DATA windows |
| `vkCmdUpdateBuffer`, `glBufferSubData` | `openagc_frontend_buffer_upload` | host write; Vulkan defers to submit |
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
