# OpenAGC

OpenProspero's GPL-3.0-or-later C99 path to a **working PS5 GPU and
display driver**, with Vulkan 1.0 and OpenGL running natively on one
shared OpenAGC backend.

## Goal

Become a real PS5 GPU/display driver: firmware-independent submit,
executable pipelines, draws, and presentation — with host-testable
VK/GL frontends on the same core. The compiler gate and presentation
are tracked stages, not the product identity.

OpenAGC is **firmware-independent**: the same driver, policy and payloads
run on every PS5 firmware, and have been tested end-to-end on PS5 consoles
across firmware versions. Only an unreadable firmware identity (`0`) is
refused. Individual evidence records below say on which firmware a given
measurement was first taken.

## Current status (honest)

| Area | Status |
| --- | --- |
| Memory, buffers, copy queues, fences | Host simulation with explicit errors |
| Images / clears | Host-linear RGBA8/BGRA8; CPU clear fills; WRITE_DATA tiling ≤32×8 |
| Compute (narrow) | Console-proven `store_const` and `store_span` (1–8 lanes); the host CPU path stays the fallback |
| Shader intake | Unverified fixtures **and** pin-checked `OPENGNM_PSBC` envelopes (`psbc_envelope=1`) |
| Pipelines | Structural plans; shared host PSBC and AGC linker-register snapshots |
| Color-buffer bind | Nine-register linear RGBA8 bind (Step AB) and the public capture's 16-record set, both console round-trip proven |
| GPU rasterizer | `include/openagc/raster.h` composes one draw IB: scalar state, context/uconfig tables, ES/PS register program, colour bind, `DRAW_INDEX_AUTO` or `DRAW_INDEX_2`, EOP trailer, optional gate blocks. The shared core's capability reports `gpu_rasterization=1`, qualified by the Step AQ console pixel |
| First console DRAW | **Proven (Step AQ)**: submitted through the console's own AGC driver, the draw writes 64 dwords that are all the pinned pixel shader's `0xff0040ff`, exactly inside the viewport rectangle, with an empty guard scan and the nine colour-bind registers reading back as composed. The raw `0xC0108102` ioctl path has never produced a fragment; Step AQ omitted the EOP packet, so it did not establish completion |
| Native submit bridge | Optional `OpenAGC::ps5_gpu` takes a caller-owned GPU-visible draw on any PS5 firmware. A console run (first recorded on FW9.40) with the **OpenProspero native CRT and module loader** returned marker `1`, all 64 expected pixels, and a clean exit. This is one qualified payload draw, not a general GPU backend |
| Application preflight | `examples/ps5-native-smoke` is a separate OpenProspero C++ application with a runtime firmware-identity check and a log. It has not been run as an app on console; no GPU or VideoOut call is made |
| Application GPU diagnostic | `examples/ps5-native-gpu-smoke` compiles the bounded AGC bridge draw as an OpenProspero application with the opt-in app AGC loader and SDK UDP logging. A FW9.40 native-writer PKG reached `EXEC /app0/eboot.bin`, but produced no received UDP log or qualified marker/pixels; VideoOut is not used. This is not Vulkan/OpenGL |
| Draws through the frontends | Still `NOT_READY`: `vkCmdDraw`/`glDrawArrays` are not yet wired onto the encoder. The CPU paths (clears, store-const compute) are the fallback |
| Presentation / swapchain | Native GPU swapchain refused; experimental CPU VideoOut presenter added for QuickJS |
| `compiler_verified` / `gpu_executable` | Always **0** on accepted plans today |

## PS5 policy (capability gate, firmware-independent)

| Area | `OpenAGC::ps5_policy` |
| --- | --- |
| Same public symbols | Every public symbol is defined; host entry points stay fail-closed |
| Qualification record | `openagc_ps5_policy_qualification` / `openagc_ps5_policy_require` state the qualified capabilities for any valid firmware identity: copy+EOP, WRITE_DATA fills, compute stores, register programs, CB readback, IB dumps, the NGG program, and the draw |
| Draws | `OPENAGC_PS5_CAP_DRAW` qualified by Step AQ, with the marker caveat written into the header |
| Full host GPU library | Must **not** be linked into a PS5 image; the command recorder is built separately for the VideoOut presenter |

Every nonzero firmware identity (packed BCD from
`op_ps5_system_firmware_version()`) is qualified for the same capability
mask. Only an unreadable identity (`0`) answers `UNSUPPORTED_FIRMWARE`; a
request with unknown capability bits answers `INVALID_ARGUMENT`, and a
capability without console evidence answers `UNSUPPORTED_OPERATION`.
Firmware fields in descriptors are diagnostic hints, not authorization. The
SDK QuickJS host uses `openagc_ps5_videoout.c` to draw the recorder's frames on the CPU and submit
them to native VideoOut. This path has offline tests but awaits console
display confirmation.

## Stage gates (short)

See [docs/roadmap.md](docs/roadmap.md) for the full staged plan.
The external-application Vulkan 1.0 and OpenGL 3.3 ABI, including runtime
compilation of application-supplied shaders, is tracked separately in stage 9;
the existing host `openagc_vk_*` / `openagc_gl_*` subsets are not standard
drivers.

1. **Stages 1–4 (host)** — shared frontend core, VK/GL subsets, refuse
   unsupported ops. Done on host.
2. **Stage 5 — one of two gates closed.**
   - **Draw and colour bind**: closed on console. The shared encoder's draw
     writes the pinned pixel shader's colour into the caller's target when
     it is submitted through the AGC driver (`sceAgcDriverSubmitDcb` plus
     `sceAgcSuspendPoint`); rasterization is qualified.
   - **Compiler / executable shaders**: still gated. Pin-checked PSBC
     envelopes are intaken as structural (`psbc_envelope=1`),
     `require_compiler` returns `NOT_READY`, and nothing sets
     `gpu_executable`.
   - **Remaining work**: wire `vkCmdDraw` and `glDrawArrays` onto the
     shared encoder with the host CPU paths as the fallback.
3. **Stages 6–7** — native tiling / coherency and presentation: refused
   until separate evidence.

## Console evidence

Sanitized results only; raw captures stay off-repository. The per-step
records below were first taken on FW9.40; the driver has since been tested
end-to-end on PS5 consoles across firmware versions. Steps A–N proved
the copy, WRITE_DATA and compute vehicles; the raster campaign is:

- **AE–AH**: the AGC-shaped IB submits and retires with every program
  register readable, and writes no pixel — with either colour bind, either
  draw initiator, and the generic scissor and depth range written.
- **AI–AM**: with `PA_CL_VTE_CNTL` set the draw reaches fragment generation
  and stops retiring on the raw path; the legacy path never assembles
  because `VGT_PRIMITIVE_TYPE` never takes.
- **AO–AQ**: submitted through the AGC driver instead, the same words
  rasterize. The colour surface's hardware row stride is 256 bytes where a
  packed 32-pixel row is 128, which is what made the first runs look like a
  placement anomaly; with that understood the acceptance is exact.

The Step AQ payload left `append_eop=0` and emitted no marker packet;
its `completed=0` therefore cannot establish whether AGC supports that EOP.
Details and per-step artifacts: [docs/hardware-evidence.md](docs/hardware-evidence.md).
An opt-in follow-up submitted one draw with a distinct completion event from
[PS5_Vulkan's R90 runs](https://github.com/mihawk-99/PS5_Vulkan/blob/main/driver/ps5vk_queue.c).
On FW9.40 it returned marker `1`, exactly 64 expected shader pixels
and no out-of-bounds writes; the loader recorded a clean exit. The
bridge retains a pending submission on timeout or driver error, and
requires the caller to keep its GPU memory mapped until completion.
The full bridge also passed the same exact-pixel check with the
OpenProspero native CRT, validated runtime firmware query, and its AGC
module loader. Application-profile GPU and native VideoOut execution
remain unqualified.

## Build

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The optional native target is separate from the host implementation:
configure with `-DOPENAGC_PS5_GPU_NATIVE=ON` when cross-compiling
for PS5, link `OpenAGC::ps5_gpu`, and provide the SDK's dynamic-loader
and `op_ps5_system_firmware_version()` APIs. The native bridge now
requires the OpenProspero SDK's validated runtime query (not the
payload CRT's process SDK-version field); application-profile linkage
is available with the updated OpenProspero CRT, but GPU execution
from an application/PKG remains untested. It does not make the host
Vulkan/OpenGL frontends available on the console. `openagc_ps5_gpu`
allows one active bridge instance per process and keeps initialized
AGC modules resident until process exit, because the system AGC driver
refused their unload (observed on FW9.40). It submits a caller-owned raster draw; it is
not a shader compiler, memory allocator, swapchain, or complete GPU
driver.

Extra checks the stages expect:

```text
cmake -S . -B build-asan -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build-asan && ctest --test-dir build-asan
cmake -S . -B build-policy -DOPENAGC_PS5_POLICY_ONLY=ON && cmake --build build-policy
python3 tools/check_host_invariants.py
```

Headers (include as `<openagc/….h>`):

| Header | Role |
| --- | --- |
| `openagc.h` | Core ABI |
| `driver.h` | GPU device / copy / fence |
| `graphics.h` | Images and host clear path |
| `shader.h` | Artifact intake and pipeline plans |
| `frontend.h` | Shared VK/GL translation core |
| `raster.h` | GPU rasterizer: AGC-shaped draw composition and encoding |
| `ps5_policy.h` | Firmware-independent PS5 capability qualification record |
| `vulkan.h` / `opengl.h` | Host frontend subsets |
| `psbc_metadata.h` / `pm4*.h` | PSBC reflection and PM4 helpers |

The [Electron/DOM port track](https://github.com/OpenProspero/sdk/blob/main/docs/ELECTRON.md)
pins a matching FreeBSD Electron 36.3.1 source and patch set, cross-compiles
an ABI probe through the canonical SDK, and runs a host offscreen DOM/paint
smoke app. Electron is not yet cross-compiled or running on PS5.

Link `OpenAGC::openagc` on the host via `add_subdirectory`. For PS5
policy-only builds, use `-DOPENAGC_PS5_POLICY_ONLY=ON` and link only
`OpenAGC::ps5_policy`. Recipe notes: [docs/architecture.md](docs/architecture.md).

## Console payloads

`tools/payload/build.sh <source.c> <out.elf>` builds an evidence payload with
the payload SDK (extra defines through `PAYLOAD_CFLAGS`), `validate_elf.py`
runs after every link, and `deploy.py` performs one validated push and reads
the log back. Payloads that stall write their log only after their deadline,
so fetch with `--wait 45`; and give each payload its own log path, because a
later payload overwrites the previous file. Compute and copy payloads submit
through the raw ioctl; the raster payloads submit through the AGC driver.

## Shader / PSBC (host)

- Fixtures: `OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE` (structural only).
- Pin-checked envelopes: `OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` with
  matching `OPENAGC_SHADER_PINNED_PSBC_*` digest, revision, and metadata
  v14. Empty `descriptor_bindings` require zero OpenAGC
  bindings/textures; typed bindings must match the declared uniform
  buffers and combined image samplers exactly, and storage bindings or
  arrays stay `UNSUPPORTED_OPERATION`.
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
- Attribute-less plans (`vertex_input_mask == 0`) draw without a VBO;
  both frontends still stop at `NOT_READY`.
- After `bind_psbc_code`, both host frontends can intake the 34 context
  and three uconfig records emitted by AGC's linker through the shared
  `set_agc_linked_registers` path. These records are retained in host
  snapshots only; no linker capture or native draw is qualified.
- Both host frontends can prepend the public 16-record AGC COLOR0 target
  shape through `set_agc_target_registers` after linking. The shared core
  validates the register order and a nonzero caller-owned target base.
  The snapshot remains host-only; PS5 policy refuses the call.
- `openagc_frontend_agc_build_linear_target` derives those records for an
  aligned RGBA8/BGRA8 linear image from caller-supplied AGC defaults. It
  validates the 256-byte row pitch and address span once for both frontends.
- Build-time compiler job: [`.github/workflows/build-psbc-host.yml`](.github/workflows/build-psbc-host.yml)
  (manual-only). Details: [docs/shader-toolchain.md](docs/shader-toolchain.md).

## Frontends (host)

Vulkan and OpenGL share `frontend.h`: one staging/copy/transition path,
native→backend translation, and explicit refuse for unsupported formats
and layouts. Equivalence: `tests/test_openagc_equivalence.c` (including
WRITE_DATA grid clears, depth clears, and PSBC register snapshots).

The shared core reports `rasterization` from the rasterizer's qualification
pin and `gpu_execution=0`. The Vulkan and OpenGL capability structs
(`vulkan.h`, `opengl.h`) are still their own: one physical device with a
transfer queue family, `gpu_execution=0`, `presentation=0`, and no
rasterization field yet, which is part of the wiring left in stage 5.
Neither frontend submits a draw — that wiring, with the host CPU paths as
fallback, is what remains.

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

Public PS5_Vulkan / ps5-opengl and Mesa work is used for register and
runtime contracts. The next native stage will reuse compatible public
components where that speeds homebrew integration, with provenance and
licenses retained. No third-party source, SDK, firmware, or proprietary
blob is currently vendored or linked.
