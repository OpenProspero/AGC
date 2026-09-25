# OpenAGC

OpenProspero's GPL-3.0-or-later C99 path to a **working PS5 GPU and
display driver**, with Vulkan 1.0 and OpenGL running natively on one
shared OpenAGC backend.

## Goal

Become a real PS5 GPU/display driver: qualified firmware submit,
executable pipelines, draws, and presentation — with host-testable
VK/GL frontends on the same core. Current gaps (compiler gate, missing
CB/DRAW evidence, presentation) are tracked stages, not the product
identity.

## Current status (honest)

| Area | Status |
| --- | --- |
| Memory, buffers, copy queues, fences | Host simulation with explicit errors |
| Images / clears | Host-linear RGBA8/BGRA8; CPU clear fills; WRITE_DATA tiling ≤32×8 |
| Compute (narrow) | Console-proven `store_const` and `store_span` (1–8 lanes) via host CPU path |
| Shader intake | Unverified fixtures **and** pin-checked `OPENGNM_PSBC` envelopes (`psbc_envelope=1`) |
| Pipelines | Structural plans; host SET_CONTEXT/SET_SH (+ vertex linkage) snapshot from PSBC |
| Vulkan / OpenGL | Shared frontend core; equivalent work lands on the same backend bytes |
| Attribute-less draws | Bind + draw recorded; still `NOT_READY` (no CB/DRAW / no `gpu_executable`) |
| Draws / general dispatch | Refused (`NOT_READY` from compiler gate) |
| Presentation / swapchain | Refused until a display path exists |
| `compiler_verified` / `gpu_executable` | Always **0** on accepted plans today |

## PS5 policy (fail-closed until qualified)

| Area | `OpenAGC::ps5_policy` |
| --- | --- |
| Same public symbols | Fail closed: `UNSUPPORTED_FIRMWARE` while FW is unqualified |
| Host OpenAGC library | Must **not** be linked into a PS5 image |
| FW9.40 | **Not** hardware-qualified yet; no submit path in policy |

Unknown firmware is not qualified either. Firmware fields in descriptors
are diagnostic hints, not authorization. Policy opens only when
evidence qualifies a path — deny-all is the gate, not the end state.

## Stage gates (short)

See [docs/roadmap.md](docs/roadmap.md) for the full staged plan.

1. **Stages 1–4 (host)** — shared frontend core, VK/GL subsets, refuse unsupported ops. Done on host.
2. **Stage 5** — still gated:
   - **Compiler / executable shaders**: pin-checked PSBC envelopes may be
     intaken as structural (`psbc_envelope=1`); host register programs
     include context, shader, and vertex linkage pairs; `require_compiler`
     still returns `NOT_READY`; nothing sets `gpu_executable`.
   - **Draw / CB/DB PM4**: no independently owned FW9.40 color-buffer or
     DRAW capture in-tree; public AMD opcodes alone are not enough.
3. **Stages 6–7** — native tiling / coherency and presentation: refused
   until separate evidence.

## Console evidence (FW9.40, host-aligned only)

Separate SDK payloads on console proved, among other steps:

- DMA + EOP copy (`pm4_fw940.h`, 31 dwords)
- WRITE_DATA fills through MAX_ROWS / MAX_COLS grid (Steps D–H, M, N)
- Compute `store_const` and `store_span` chains through SPAN_MAX (I–L)

The host library encodes aligned PM4 snapshots and simulates on CPU; it
**never** submits those words to a console from this tree until a
reviewed, evidence-backed path exists. Draw/render packets remain
unavailable.

## Build

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --build-config Debug --output-on-failure
```

Headers (include as `<openagc/….h>`):

| Header | Role |
| --- | --- |
| `openagc.h` | Core ABI |
| `driver.h` | GPU device / copy / fence |
| `graphics.h` | Images and host clear path |
| `shader.h` | Artifact intake and pipeline plans |
| `frontend.h` | Shared VK/GL translation core |
| `vulkan.h` / `opengl.h` | Host frontend subsets |
| `psbc_metadata.h` / `pm4_*_fw940.h` | PSBC reflection and PM4 helpers |

Link `OpenAGC::openagc` on the host via `add_subdirectory`. For PS5
policy-only builds, use `-DOPENAGC_PS5_POLICY_ONLY=ON` and link only
`OpenAGC::ps5_policy`. Recipe notes: [docs/architecture.md](docs/architecture.md).

## Shader / PSBC (host)

- Fixtures: `OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE` (structural only).
- Pin-checked envelopes: `OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` with
  matching `OPENAGC_SHADER_PINNED_PSBC_*` digest, revision, and metadata
  v14. Empty `descriptor_bindings` in metadata require zero OpenAGC
  bindings/textures. Non-empty bindings stay `UNSUPPORTED_OPERATION`.
- Accepted PSBC artifacts report `psbc_envelope=1`, still
  `compiler_verified=0` and `gpu_executable=0`.
- Intake retains envelope metadata; graphics create auto-attaches the
  host SET_CONTEXT/SET_SH snapshot (plus vertex linkage context pairs)
  when both stages are envelopes. Explicit
  `openagc_frontend_pipeline_set_psbc_register_snapshot`
  (VK/GL wrappers) remains available. Host can patch
  `SPI_SHADER_PGM_LO/HI` from 256-byte-aligned code VAs and record the
  register program + EOP into the write snapshot (still
  `gpu_submitted=0`). After `bind_psbc_code`, VK queue submit / GL
  `bind_program` record the Step-U-shaped IB (69 + EOP = 93 dwords).
  No DRAW packets.
- Attribute-less plans (`vertex_input_mask == 0`) draw without a VBO;
  both frontends still stop at `NOT_READY`.
- Build-time compiler job: [`.github/workflows/build-psbc-host.yml`](.github/workflows/build-psbc-host.yml)
  (manual-only). Details: [docs/shader-toolchain.md](docs/shader-toolchain.md).

## Frontends (host)

Vulkan and OpenGL share `frontend.h`: one staging/copy/transition path,
native→backend translation, and explicit refuse for unsupported formats
and layouts. Equivalence: `tests/test_openagc_equivalence.c` (including
WRITE_DATA grid clears, depth clears, and PSBC register snapshots).

Narrow compute exception on host only: `host_store_const` /
`host_store_span` with groups `1,1,1` (console-proven blobs), without
opening a compute queue or flipping `gpu_executable`.

## Docs

| Doc | Content |
| --- | --- |
| [architecture.md](docs/architecture.md) | Backends, PM4 layout, PS5 policy build |
| [roadmap.md](docs/roadmap.md) | Stages and refuse rules |
| [shader-toolchain.md](docs/shader-toolchain.md) | PSBC pin, reflection, intake gates |
| [hardware-evidence.md](docs/hardware-evidence.md) | Console step evidence and limits |

Public PS5_Vulkan / ps5-opengl docs and ProsperoAI notes were consulted
as **knowledge references** only. No third-party source, SDK, firmware,
or proprietary blob is vendored or linked.
