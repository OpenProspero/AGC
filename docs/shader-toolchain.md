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
only `OPENAGC_SHADER_COMPILER_UNVERIFIED_FIXTURE` with zero metadata
version, zero toolchain release fields, empty source revision, and a
zero compiler-binary digest. It verifies a SHA-256 of a deep copy of
the supplied opaque bytes and validates bounded gfx1013 reflection.
This check establishes byte integrity and shape only. The positive
CTest fixtures contain arbitrary test bytes, **not machine code or a
compiled shader**.

An envelope claiming `OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` must at
least declare the v0.3.0 toolchain release, the source revision above,
a nonzero metadata version and an executable digest. Even when those
fields, the payload digest, and reflection shape pass, intake returns
`OPENAGC_ERROR_NOT_READY` with a null handle: a caller-supplied
fingerprint is not independent proof that a compiler is installed,
pinned, or that reflection came from its output. Shader capabilities
always report `compiler_available=0`, and every structural artifact
and host pipeline plan reports `compiler_verified=0` and
`gpu_executable=0`. No shader command-recording API exists. The
freestanding PS5 policy rejects **every** shader entry point before
any hardware operation.

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

## Later intake integration remains gated

1. A successful private build artifact supplies an observed tool
   version, executable SHA-256, metadata version, and original
   vertex/pixel smoke results. It does **not** enable the runtime's
   `OPENAGC_SHADER_COMPILER_OPENGNM_PSBC` intake by itself.
2. An original adapter must extract verified code and typed reflection
   from the exact compiler output, cross-check bindings, stage linkage,
   format and payload hash, and produce immutable OpenAGC descriptors.
   Review it and a pinned executable manifest separately; do not
   equate the current structural fixtures with compiled shaders.
3. Only then may a separately gated compiled-artifact intake be
   implemented. That still does **not** remove the FW9.40 deny-all
   policy or qualify Vulkan, OpenGL, draw packets, VideoOut, or native
   image layouts. Native-app Stage 0 and any Stage 1 remain unapproved.
