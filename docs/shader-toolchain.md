# Shader artifact provenance and build-time toolchain

OpenAGC's driver and runtime are original OpenProspero code. It does
**not** link Mesa, NIR, ACO, OpenGNM PSBC, OpenAGC/OpenAGC,
PS5_Vulkan, or ps5-opengl at runtime. A compiler may eventually be
used **outside** OpenAGC as a build-time producer of OpenProspero's own
shader artifacts, but no such tool is installed or trusted here.

## Exact research pins, not installed dependencies

The public
[ps5-opengl v0.3.0 release](https://github.com/blackbearreloaded/ps5-opengl/releases/tag/v0.3.0)
resolves to repository commit
`6cb291abea32281571c49705735046425cf000fd`. Its
[tag-scoped dependency manifest](https://github.com/blackbearreloaded/ps5-opengl/blob/v0.3.0/dependencies.json)
names these reproducible **source** inputs:

| Input | Release-tagged pin |
| --- | --- |
| OpenGNM PSBC source revision | `a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92` |
| OpenGNM declarations revision | `4b295ca54c82c83acf308d1c646a2dfa9ae57350` |
| PSBC patch SHA-256 | `a7c73aef3f1d51c47107976b03e68de5e99f044a97601a4adf3b89144fb2e848` |
| Patched compiler tree identifier | `a27cbecc8c11761da04af6b8e089905b93252e65` |
| Mesa source release | 26.2.0, archive SHA-256 `efd4bb08cdb7c365a812cd4e6c9202ab55b2f22cdcd13c7d6c4f9647b799a4ef` |

These are **not** the SHA-256 of a compiler executable. No compiler or
verified compiler binary digest is installed in the OpenAGC runtime.
The public
[PS5_Vulkan compiler milestone](https://github.com/mihawk-99/PS5_Vulkan/blob/main/docs/M5_PHASE_A.md)
describes PSBC code and typed metadata as distinct outputs; its older
metadata revision must not be assumed to match a future local build.
No source, patch, SDK archive, shader package, or licensed binary from
either reference project is copied into this repository. Before
building or distributing an external compiler, review every input's
license and preserve its required attribution separately.

## Current fail-closed contract

`include/openagc/shader.h` defines an OpenProspero-owned version-1
**structural** envelope. `openagc_shader_artifact_intake_host` accepts
`OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE` (zero metadata version,
zero toolchain release fields, empty source revision, zero
compiler-binary digest) and pin-checked
`OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` envelopes described below. It
verifies a SHA-256 of a deep copy of the supplied opaque bytes and
validates bounded gfx1013 reflection. This check establishes byte
integrity and shape only. Unverified CTest fixtures may contain
arbitrary test bytes, **not machine code or a compiled shader**.

An envelope claiming `OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` must
declare the v0.3.0 toolchain release, the source revision above,
metadata version 14, and an executable digest equal to
`OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256`. Another metadata
version is `INVALID_ARGUMENT`. The envelope must also carry the
compiler metadata object: `version`, `target` 2, `source_stage`
(vertex 1, pixel 5), `machine_code_size` equal to the payload,
register arrays, empty semantics, typed `descriptor_bindings`, and
stage linkage rules. A mismatch is `INTEGRITY`. Compute metadata is
refused (`UNSUPPORTED_OPERATION`). Bindings follow the pinned emitter
schema below; storage bindings and arrays are `UNSUPPORTED_OPERATION`.
When pin, reflection, and
OpenAGC descriptor cross-checks pass, intake **accepts** the artifact
as a host structural envelope (`psbc_envelope=1`) with
`compiler_verified=0` and `gpu_executable=0`.
`openagc_shader_artifact_require_compiler` still returns
`OPENAGC_ERROR_NOT_READY`. Shader capabilities keep
`compiler_available=0`. No shader command-recording or draw path is
opened. The freestanding PS5 policy rejects **every** shader entry
point before any hardware operation.

## User-approved manual GitHub Actions build

The user approved a **build-time-only** compiler build on GitHub Actions.
`.github/workflows/build-psbc-host.yml` has **only**
`workflow_dispatch`; a push cannot launch it. The pinned action revisions
check out this private repository with read-only permission.
`tools/build-pinned-psbc.sh` clones the exact public v0.3.0 release
outside OpenAGC, validates the tag commit and full patch SHA-256
**before fetching sources**, invokes that release's pinned
`tools/fetch-sources.py` and host compiler build, and independently
verifies the original source commits, patched Git tree, full Mesa archive
SHA-256, and a full SHA-256 of the patched tree listing. The resulting
private artifact additionally records the full SHA-256 of a packaged
archive of that exact patched tree. The host compiler builds against
the pinned PSBC tree's Mesa NIR/ACO sources; the full separately pinned
Mesa archive is also verified and bundled for provenance, not linked
into OpenAGC. Only the required external build scripts/patch and
compiler source archives are bundled, not unrelated upstream assets.

The resulting **external** `opengnm-psbc` CLI must compile OpenProspero's
original `tools/shaders/smoke.vert` and `smoke.frag` through
`glslangValidator` and `spirv-val`. It emits raw gfx1013 code and
compiler-produced metadata JSON. `tools/verify_pinned_psbc.py` refuses
empty/misaligned/oversized output and checks both stages' **observed**
metadata version against the patched compiler header's version 14,
target 2 (PS5), source stage, and actual raw code byte count. It then
records the executable's full SHA-256 and CLI-reported version, the
sources and smoke-output hashes, and `hardware_qualified=false`.
The private Actions artifact bundles the compiler, original smoke
sources/outputs, corresponding pinned source archives, patch, Mesa
archive, and license/notice texts. The manifest is written **last**:
any failed source, compiler, shader, metadata, license, or packaging
check prevents upload. No generated manifest or binary is committed
to OpenAGC.

The job uses Ubuntu 24.04 and records the actual host/compiler hash;
source pins are exact, but a different runner/compiler revision is
**not claimed** to produce a bit-identical executable. The artifact
is a build-time tool, not a runtime library or a PS5-qualified shader.
No Mesa/OpenGNM code is vendored here. Check the upstream license
texts in the artifact before redistributing a compiler binary.

## Pinned executable digest (build-time only)

OpenAGC records the verified private-artifact executable SHA-256 as
`OPENAGC_SHADER_PINNED_PSBC_EXECUTABLE_SHA256` in `include/openagc/shader.h`
(`2f2cbab5…12233fa9`, metadata version 14, revision
`a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92`). Smoke fixtures under
`tests/fixtures/psbc_smoke/` mirror the artifact output digests. Pinning
alone does **not** set `compiler_available`, `compiler_verified`, or
`gpu_executable`.

## Host register-program adapter (no DRAW)

`include/openagc/pm4_graphics_fw940.h` encodes public AMD
`SET_CONTEXT_REG` (0x69) and graphics `SET_SH_REG` (0x76) packets from
PSBC `context_registers` / `shader_registers` pairs, and from vertex
linkage fields (`ge_cntl`, `stages_en`, `user_vgpr_en`) when present.
Tests in `tests/test_openagc_psbc_adapter.c` exercise the smoke.vert /
smoke.frag pairs. `DRAW_INDEX_AUTO` (0x2D) is cited only; the adapter
never emits it. Step AD shows an NGG draw packet completing on console,
but no pixel was written; executable draw remains blocked.

### AGC linker records through the shared frontend

The public ps5-opengl v0.3.0 native runtime calls `sceAgcLinkShaders` with
the primitive type, then passes 34 linker-produced context records to its
context command and three uconfig records to its uconfig command before the
stage SH records. OpenAGC now accepts that record shape through
`openagc_frontend_pipeline_set_agc_linked_registers` after PSBC code is
bound. The Vulkan and OpenGL wrappers call this same core function. It
checks record counts and register-space bounds, copies the records, and
rebuilds the host program when code VAs change. The host snapshot orders
linker context, vertex and pixel context, linker uconfig, then vertex and
pixel SH records. The uconfig packets carry the GFX10 reset-filter-CAM bit.

The public PS5_Vulkan C1 triangle capture also gives a 16-register COLOR0
target table. `set_agc_target_registers` prepends that shape to the shared
host snapshot after linking, using a target base supplied by the caller.
The equivalence test uses the public defaults with a synthetic base. This
does not submit GPU commands, qualify a render target, or bypass PS5's
deny-all policy.

The equivalence test uses the linker and COLOR0 records from the public
PS5_Vulkan C1 triangle capture (commit `3a6f00df`, region 0, chunks
`0x5000`, `0x5100`, `0x6000`, `0x0400`) with a synthetic target base. This
proves the two host frontends encode the same captured shape. It does not
prove those values apply to FW9.40; no FW9.40 `sceAgcLinkShaders` capture is
owned. The adapter emits no DRAW, sets no executable capability, and the PS5
policy stub refuses it.

`openagc_frontend_agc_build_linear_target` uses the public target patch
formula with caller-supplied AGC defaults to make a one-sample linear
RGBA8/BGRA8 COLOR0 table. Both native format dialects enter the same
composer. It refuses unsupported formats, unaligned or overflowing GPU VAs,
and widths that cannot form 256-byte rows. The public C1 tiled default
produces `INFO=0x8028` for RGBA8 and `0x8828` for BGRA8 after format
patching; the host test checks both and the linear ATTRIB3 result. A target
write mask and the rest of a draw state are still separate, and no console
execution is qualified by this builder.

## Typed reflection and pin-checked intake (still non-executable)

`include/openagc/psbc_metadata.h` parses a typed `openagc_psbc_reflection`
from PSBC metadata (stage/size, register pairs, empty semantics, typed
descriptor bindings, optional user-data dwords, vertex linkage) and
cross-checks the caller OpenAGC descriptor.
Intake accepts a pin-checked `OPENGNM_PSBC` envelope as structural
(`psbc_envelope=1`) without enabling `gpu_executable`. Host register
program words can be encoded from a parsed reflection without claiming
console draw readiness.

Graphics plans auto-attach that host snapshot when both vertex and
pixel stages are `psbc_envelope` with retained metadata. Explicit
`openagc_frontend_pipeline_set_psbc_register_snapshot` (Vulkan/OpenGL
wrappers share the same path) remains available for re-attach.
`openagc_frontend_pipeline_patch_psbc_pgm_vas` rewrites
`SPI_SHADER_PGM_LO/HI` from 256-byte-aligned host code VAs (same
`>>8` / `>>40` encoding as console-proven compute) without claiming
`gpu_executable`. `openagc_frontend_pipeline_record_psbc_register_eop`
stores the register program plus the shared EOP trailer in the host
write snapshot (`gpu_submitted=0`). After `bind_psbc_code`, Vulkan
defers that Step-U-shaped record to queue submit when the pipeline is
bound; OpenGL records on `bind_program` via
`record_psbc_register_eop_if_bound`. Equivalence tests require identical
words (including linkage and identical PGM patches), attribute-less
draw → `NOT_READY` on both frontends, a 93-dword write view
(`69 + EOP`) after bind+submit/bind_program, and zero
`compiler_verified` / `gpu_executable`.

## Typed descriptor bindings (adapter, no execution)

The pinned opengnm-psbc patch
(`a7c73aef…`, revision `a92a1228…` — verified by full SHA-256 before
any fetch in `tools/build-pinned-psbc.sh`) emits one object per binding:

```json
{"set":N,"binding":N,"type":N,"array_size":N,"offset":N,"stride":N}
```

with `PsbcDescriptorType` NONE/UNIFORM_BUFFER/COMBINED_IMAGE_SAMPLER/
STORAGE_BUFFER/STORAGE_IMAGE = 0..4 and
`PSBC_MAX_DESCRIPTOR_BINDINGS` = 128.
`openagc_psbc_metadata_parse_reflection` parses all six fields as
required (the emitter always writes them), refuses duplicates,
`set != 0`, `binding >= 128`, unknown types, and `array_size == 0`.

`openagc_psbc_reflection_check_artifact_desc` then requires each
metadata binding to be backed by exactly one OpenAGC declaration of the
matching kind at the same `(set,binding)` — uniform buffer or combined
image sampler — with no missing and no extra declaration. Storage
bindings, `array_size != 1`, and any set other than 0 are
`UNSUPPORTED_OPERATION`. `openagc_psbc_reflection_map_resources`
performs the typed binding→OpenAGC resource map in metadata order and
refuses missing, duplicate, or undeclared slots. Nothing here flips
`compiler_available`, `compiler_verified`, or `gpu_executable`; draws
stay `NOT_READY` and the PS5 policy stays deny-all.

### Resource descriptor cites (next step, not yet implemented)

A V#/T#/S# encoder has no consumer until draws open, and two pieces still
lack a verified cite, so no descriptor is encoded yet. What **is**
verified, for whoever picks this up:

* Descriptor sizes in bytes, from the pinned patch's user-data sizing
  (`COMBINED_IMAGE_SAMPLER ? 48 : STORAGE_IMAGE ? 32 : 16`): buffer 16,
  image 32, combined image+sampler 48, storage image 32.
* Word layouts from `src/amd/registers/gfx10-rsrc.json` at the pinned
  revision: `SQ_BUF_RSRC_WORD1` `BASE_ADDRESS_HI[0,15]` `STRIDE[16,29]`
  `CACHE_SWIZZLE[30]` `SWIZZLE_ENABLE[31]`; `SQ_BUF_RSRC_WORD3`
  `DST_SEL_X[0,2]` `Y[3,5]` `Z[6,8]` `W[9,11]` `FORMAT[12,18]`
  `INDEX_STRIDE[21,22]` `ADD_TID_ENABLE[23]` `RESOURCE_LEVEL[24]`
  `OOB_SELECT[28,29]` `TYPE[30,31]`; `SQ_IMG_SAMP_WORD0..3` as listed
  there (clamp/LOD/filter/border fields).
* Raw-buffer convention from `src/amd/common/ac_descriptors.c`
  (`ac_build_raw_buffer_descriptor`): WORD0 = va, WORD1 = `(va >> 32) &
  0xffff` with STRIDE 0 and SWIZZLE_ENABLE 0, WORD2 = size in bytes,
  WORD3 = DST_SEL X/Y/Z/W, `OOB_SELECT=RAW` (= 3, from
  `SQ_BUF_RSRC_WORD3__OOB_SELECT`), FORMAT `GFX10_FORMAT_32_FLOAT` (= 22,
  same DB), RESOURCE_LEVEL 0.
* **Open, do not guess:** the numeric `SQ_SEL` values behind
  `DST_SEL_*` and where a driver is expected to place descriptors inside
  the shader's user SGPRs. Neither is in the pinned tree; the private
  Actions artifact's generated register headers carry the `SQ_SEL` enum.

## Remaining executable-shader gates

1. Pin + reflection + typed-binding envelope intake exist on the host.
   They do **not** flip `compiler_available` or `gpu_executable`, and
   `require_compiler` stays `NOT_READY`.
2. Binding *values* (V#/T#/S# contents, user SGPR placement) are not
   encoded; the verified cites and the two open questions are listed in
   the subsection above. Metadata `offset`/`stride` are retained but
   unused.
3. Enabling `gpu_executable` / draws still requires independently owned
   FW9.40 CB/DB or DRAW evidence and does **not** remove the deny-all
   PS5 policy. Native-app Stage 0 and any Stage 1 remain unapproved.
