# Hardware evidence: passive record and the bounded next step

This document is the review artifact the qualification gate in
[architecture.md](architecture.md#firmware-940-proof-gates-passive-baseline-only)
requires before any hardware-facing step. It records what was observed
**read-only**, what was deliberately **not** done, and the exact bounded
experiment that may run once a build path exists.

Raw console captures are **not** stored in this repository. Only the
sanitized summary below is committed.

## Observed on 2026-09-24 (read-only)

The console at `192.168.1.20` was reached for **reading only**: a TCP
connect check on the payload-loader port (no bytes sent) and anonymous
FTP reads of `/data/klog/` and `/data/prosperoai/`.

| Observation | Result |
| --- | --- |
| Payload loader port accepts connections | Reachable |
| FTP log access (anonymous) | Reachable |
| `klog.log` plus three rotations (~1.8 MB total) | Fetched and scanned |
| Panic, fatal trap, GPU fault, GPU hang, ring timeout markers | **0 occurrences in all four windows** |
| `AgcCompositor.elf` in the memory report | Present, running (normal display path) |
| `elfldr.elf` bootstrap lines | Present (the operator's own loader setup) |
| `/data/prosperoai/prosperoai.log` (194 KB) | Fetched; shows the operator's own FW9.40 GPU bring-up (context query, gvm/pml4 discovery, DMA-verified markers) |
| Exact firmware build identity in these windows | **Not present**; boot banners have rotated out |

Limits of this evidence, stated plainly: the log windows are
operator-controlled, the firmware build identity is still unconfirmed,
and a clean klog from a passive window says nothing about GPU
qualification. In the first, read-only pass no OpenAGC payload was
authored, built, or sent. In the second pass two payloads were built and
pushed (see below); neither opened `/dev/gc`, issued an ioctl, mapped
GPU memory, or submitted a packet.

## Build-path status: resolved

The earlier blocker is gone. A host clang plus an ELF linker and the
`ps5-payload-sdk` sysroot build a console payload **without** the
official Windows/Linux SDK: `tools/payload/build.sh` compiles a
syscall-only probe and a direct-memory payload with Homebrew clang 22
and `ld.lld` on macOS. Both artifacts are recorded in
[tools/payload/README.md](../tools/payload/README.md).

Independent check of the artifact path: `probe.elf` was uploaded to the
console over FTP and read back with a matching SHA-256, so a payload
built on this host arrives on the device byte-identical.

## Push-path status: blocked, operator action required

Timeline observed on 2026-09-24 (all times local, UTC+2):

| Time | Event |
| --- | --- |
| 23:36 | Loader port reachable; klog and payload logs readable over FTP |
| 23:41 | Validated `probe.elf` push accepted; connection closed with no response |
| 23:43 | `probe.elf` uploaded to console storage over FTP, SHA-256 verified |
| 23:43 | `file:` URI load accepted; connection closed with no response |
| 23:44 | **A deliberately malformed 64-byte ELF was pushed as a diagnostic** |
| 23:44 | Loader port began refusing connections |
| 23:5x | The console reported `elfldr.elf` taking a fatal signal (`abort is called(system)`, thread `SceSpZeroConfMain`, `copyin: SceSpZeroConfMain has nonsleeping lock`) and needed a restart |

That malformed push was a mistake and it cost the operator a console
restart. It is now structurally prevented: `tools/payload/validate_elf.py`
checks magic, class, byte order, type, machine, program headers, entry
range, and relocation sections, `build.sh` runs it after linking, and
`deploy.py` refuses to open a socket for an artifact it rejects. No
malformed input may be sent again for any reason.

Nothing else was sent in this session: no ioctl, no `/dev/gc` open, no
GPU mapping, no packet, no retry.

Logging channels, measured precisely: a file fetched from
`/data/klog/klog.log` over FTP was byte-identical (171,932 bytes) across
nine minutes while the console was active, so **that fetch path is not a
live view**; the operator confirms the klog does update in the console
UI, and the TCP stream on 3232 does deliver new lines and is the channel
to use for diagnosis. `deploy.py` now attaches to it before the push.

## Bounded design: OpenAGC copy and EOP proof

**Question.** Does the shared FW9.40 sequence in
`include/openagc/pm4_fw940.h` (seven `IT_DMA_DATA`, eight action-based
`IT_RELEASE_MEM`, sixteen NOP dwords = 31 total), as locked down by
`tests/test_openagc_gpu.c` and emitted by `tools/payload/copy_eop.c`,
execute on physical FW9.40 with real addresses, and does its EOP
marker fire? The payload: one submit, a monotonic 30-second deadline,
a CPU byte comparison, and one log line.

**Why it matters.** Host vectors must match the console-proven IB.
A positive result is the prerequisite for any later draw or
render-target work; a negative result is equally useful because it
retires an assumption.

**Entry conditions (all required).**

1. A working payload build path (see above).
2. Firmware identity captured from the same boot session that runs the
   experiment.
3. The payload contains only the established `/dev/gc` open, memory
   mapping, and the single submit path already used by the operator's
   own bring-up.

**Payload contract.**

May do: open `/dev/gc`; allocate two small device-memory buffers;
fill the source with a known pattern; submit **one** IB of exactly
31 dwords; poll for the fence with a monotonic clock and a hard
30-second deadline; read back the destination; compare on the CPU;
write one log line with the result; exit.

Must not do: no `flat_load`; no queue-create or ring paths; no
ACB/const-IB dispatch; no VideoOut call; no kernel memory write outside
its own allocations; no indirect buffer; no second submission after a
failure; no automatic retry; no background thread.

**Acceptance criteria.** Destination bytes equal the source pattern
(CPU check), the EOP/fence completion is observed at least once, the
run produced exactly one log line, a klog captured after the run shows
no GPU fault/hang/timeout marker, the console UI remained responsive
(operator check), and no packet other than the 31 words was submitted.

**Observed result (2026-09-25, FW `0x9400008`).** All of the automated
criteria above held for one push of `copy_eop.elf`
(`371a4852f52369afcbed29df451b8e52c44839716910446be5e18edaf78228ba`):
`submit=ok completed=1 matched=1 marker=1`, exit 0, no fault markers in
the live klog window. Operator UI responsiveness is assumed from the
loader still accepting connections afterward; it was not separately
scored. This does **not** qualify draw, present, or Vulkan/OpenGL.

**Recovery plan.** Stop at the first anomaly; leave the console to the
operator; the operator's own notes record that a wedged ring does not
poison the next `/dev/gc` open, but the payload performs no recovery
action of its own.

**Evidence handling.** The raw log and any capture stay off-repository.
A sanitized result (payload hash, firmware identity if known, yes/no per
acceptance criterion) is recorded here, and
`hardware_qualified=true` may only be asserted after every criterion is
met and reviewed.

## Status

* Passive read-only review: done (this document).
* Payload build path: done. Prefer `prospero-clang` from
  `ps5-payload-sdk` with `LLVM_CONFIG` pointing at Homebrew llvm.
  `tools/payload/build.sh` defaults to SDK mode; freestanding remains
  available. `validate_elf.py` still runs after every link and before
  any push.
* Push path: **proven on 2026-09-25**. Build with the SDK, then
  `nc <host> 9021 < probe.elf` (same contract as `prospero-deploy` /
  socat). No SHUT_WR handshake is required.
* Step A (`probe.c`): **proven on 2026-09-25** with one SDK-linked
  push (110,888 bytes). Evidence:
  * stdout over the loader socket printed
    `openagc-probe: step A ok (toolchain+deploy, no device access)`;
  * `/data/prosperoai/openagc-probe.log` and `/data/openagc-probe.log`
    contain the same line (FTP 2120);
  * klog 3232 shows `# process pid=88, payload.elf calls exit() exit_value=0`.
* Firmware identity (same console): `fw=0x9400008` from
  `/data/libkernel-dump.log` (FW 9.40). VSH build path
  `W:\Build\J03247173\...` appears in the live klog window around the
  probe.
* Bounded copy/EOP experiment: **run once on 2026-09-25** after the
  NOP trailer length was corrected to eight pairs (16 dwords) so the
  IB matches the submitted count of 31. One validated `copy_eop.elf`
  push (111,208 bytes, SHA-256 recorded in the session notes). Result
  line from `/data/prosperoai/openagc-copy-eop.log`:
  `submit=ok completed=1 matched=1 ... marker=1`. Live klog shows
  `GFX(pipe0) Game` for pid 89 and `exit_value=0`, with no
  fault/hang/timeout marker in that capture. Destination bytes matched
  the source pattern on the CPU and the EOP marker fired once.
  `hardware_qualified` stays **false**: this proves the bounded copy
  and EOP path only. Draw packets, tiling, VideoOut, and Vulkan/OpenGL
  on console remain gated. The `openagc_ps5_policy` target stays
  deny-all.
* Everything downstream (draw packets, native tiling, presentation,
  Vulkan/OpenGL on console) remains gated and unapproved, and the
  `openagc_ps5_policy` target stays deny-all.

## Bounded design: compute store-const (Step C)

**Question.** Does a minimal FW9.40 compute path — `SET_SH_REG` (compute
bank) for PGM/RSRC/NUM_THREAD/USER_DATA, `DISPATCH_DIRECT` initiator
`0x41`, one-thread `flat_store_dword` of a constant to a known VA, then
the same 24-dword EOP+NOP trailer as Step B — complete with CPU-checked
bytes and a fired marker?

**Why it matters.** Copy+EOP alone does not prove shader launch. A
positive result is the first compute evidence OpenAGC owns; it does
**not** open draw/render PM4, compiler intake, or `gpu_execution` on
the host library. Empirics already report this class of dispatch on
9.40; Step C asks whether OpenAGC's own encoder and original kernel
reproduce it.

**Entry conditions.** Step A and Step B proven on the same firmware
identity (`fw=0x9400008`). Payload built with ps5-payload-sdk and
`validate_elf.py`.

**Payload contract (`tools/payload/store_const.c`).**

May do: open `/dev/gc`; map one arena; place a 256-byte-aligned
original gfx1013 store-const kernel; submit **one** IB of
`OPENAGC_PM4_COMPUTE_STORE_WORDS` (51) dwords; poll EOP 30s; CPU-check
one dword; one log line; exit.

Must not do: no `flat_load`; no acquire/context preamble beyond the
minimal SH+DISPATCH sequence; no second submit; no retry; no VideoOut;
no queue-create/ACB.

**Acceptance criteria.** Destination dword equals `0xA5A5A5A5`, marker
equals the sequence, one log line, no fault/hang/timeout in the live
klog window, loader still accepting connections afterward.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`store_const.elf`
(`4f6b44aa85064c0d4eb04493c39a33706c79c316c815a89e44482b4694af8584`,
111,256 bytes):
`submit=ok completed=1 matched=1 destination=... value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 90 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
This proves OpenAGC's own compute store-const path on console. It does
**not** qualify draws, tiling, VideoOut, Vulkan/OpenGL on console, or
host `gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all.

## Bounded experiment: CP WRITE_DATA fill (Step D)

**Question.** Does a minimal FW9.40 `IT_WRITE_DATA` (PM4 opcode `0x37`)
path write a known 32-bit pattern into a CPU-visible destination VA,
then fire the same action-based EOP+NOP trailer already proven in
Steps B and C?

**Why it matters.** Color clears and present paths need a CP write into
image memory that is not a DMA copy and not a shader store. A positive
WRITE_DATA result is the smallest graphics-adjacent CP packet OpenAGC
can own before any CB/DB setup or draw initiator. A negative result
retires WRITE_DATA as the clear vehicle on this firmware. Either way it
does **not** unlock draw/render PM4, tiling, VideoOut, compiler intake,
or host `gpu_execution`.

**Why not draw/clear CB yet.** No independently owned FW9.40 capture of
render-target bind, CB/DB register programs, or draw packets exists in
this repository. ProsperoAI empirics (research only) document DMA,
compute dispatch, and flat_store rules; they do **not** record a
passing color-clear or draw IB for OpenAGC to reproduce. Inventing
those packets is out of scope.

**Entry conditions (all required).**

1. Steps A, B, and C proven on the same firmware identity
   (`fw=0x9400008`).
2. WRITE_DATA packet layout locked from an **independent** source
   OpenAGC may cite (SPRX / public AMD type-3 WRITE_DATA facts, or a
   single console capture of a known-good IB), written into
   `include/openagc/pm4_write_fw940.h` with host unit tests for dword
   count and field placement — **before** any payload is authored.
3. Payload built with ps5-payload-sdk and `validate_elf.py`.
4. Design reviewed; one push, no retries.

**Locked encoding.** Public AMD `PACKET3_WRITE_DATA` (`0x37`) memory
write:

| dword | value |
| --- | --- |
| 0 | type-3 header `0xC0033700` (one data dword) |
| 1 | control `DST_SEL(5)\|WR_CONFIRM` = `0x00100500` |
| 2 | destination VA low, 4-byte aligned |
| 3 | destination VA high |
| 4 | data dword |
| 5..28 | shared `OPENAGC_PM4_EOP_WITH_NOP_WORDS` trailer |

Cite: drm/amdgpu PM4 `PACKET3_WRITE_DATA` / `WRITE_DATA_DST_SEL(5)` /
`WR_CONFIRM` ring-emit pattern. Host tests:
`test_openagc_gpu.c::test_write_data_words`.

**Payload contract (`tools/payload/write_data.c`).**

May do: open `/dev/gc`; map one arena; submit **one** IB of
WRITE_DATA (one dword) plus the shared
`OPENAGC_PM4_EOP_WITH_NOP_WORDS` trailer; poll EOP 30s; CPU-check
destination bytes; one log line; exit.

Must not do: no shader; no `flat_load`; no CB/DB register program; no
draw initiator; no second submit; no retry; no VideoOut; no
queue-create/ACB; no malformed ELF.

**Acceptance criteria.** Destination matches the written pattern,
marker equals the sequence, one log line, no fault/hang/timeout in the
live klog window (TCP 3232), loader still accepting connections
afterward.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data.elf`
(`b307bdc05208065d7e8e6ac81a37c54bdf4d722356ed521e70857eeef3c4d8bb`,
111,208 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 91 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves OpenAGC's own CP WRITE_DATA fill path on console. It does
**not** qualify draws, tiling, VideoOut, Vulkan/OpenGL on console, or
host `gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all.

## Bounded experiment: CP WRITE_DATA clear tile (Step E)

**Question.** Does the same FW9.40 `IT_WRITE_DATA` control with **16**
data dwords (a 4×4 RGBA8 clear tile, 64 bytes) write the pattern with
address increment, then fire the shared EOP+NOP trailer?

**Why it matters.** Host `vkCmdFillBuffer` / `glClearBufferSubData` for
small ranges and eventual color clears need a multi-dword CP write
without inventing CB/DB packets. Step D proved one dword; Step E proves
the clear-tile size OpenAGC uses as the host WRITE_DATA fill cap.

**Entry conditions.** Steps A–D proven on `fw=0x9400008`; encoding already
locked in `pm4_write_fw940.h` (`openagc_pm4_encode_write_data_fill_eop`);
payload ELF-validated; one push, no retries.

**Payload contract (`tools/payload/write_data_clear.c`).** One IB of
`OPENAGC_PM4_WRITE_DATA_CLEAR_EOP_WORDS` (44) dwords: 16 identical
`0xA5A5A5A5` data dwords + shared EOP trailer. No shader, no CB/DB, no
draw, no retry.

**Status before push.** Host routes dword-aligned fills ≤64 bytes through
`openagc_gpu_host_write_data(..., dword_count)`. CTest 9/9.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data_clear.elf`
(`99e5a5981a167b8a5244a9f252454dcf4e4036b4c6ff47d2c6891c570fef5746`,
111,216 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 dwords=16 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 92 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves multi-dword CP WRITE_DATA (4×4 clear tile) on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: multi-row WRITE_DATA (Step F)

**Question.** Do **two** FW9.40 `IT_WRITE_DATA` packets (8 dwords each)
at a non-contiguous 64-byte pitch, followed by one shared EOP+NOP
trailer, fill both rows correctly?

**Why it matters.** Host color clears with row pitch larger than the
scissor width need one WRITE_DATA per row. Step E proved contiguous
tiles; Step F proves chaining without inventing CB/DB packets.

**Payload (`tools/payload/write_data_rows.c`).** One IB of
`OPENAGC_PM4_WRITE_DATA_STEP_F_EOP_WORDS` (48) dwords. Host routes
multi-row scissors (width ≤16, height ≤8) through
`openagc_gpu_host_write_data_rows`.

**Status before push.** Encoding + host path ready; CTest 9/9.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data_rows.elf`
(`494f1e7cc1a1b3439630cfac7a7b63b6337406adf8663e909e9696efe8ef372e`,
111,216 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 rows=2 dwords=8 pitch=64 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 93 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves multi-row CP WRITE_DATA chaining on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.
