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

## Bounded experiment: full-width multi-row WRITE_DATA (Step G)

**Question.** Do **two** FW9.40 `IT_WRITE_DATA` packets of **16** data
dwords each (full clear-tile width) at a non-contiguous 128-byte pitch,
followed by one shared EOP+NOP trailer, fill both rows correctly?

**Why it matters.** Host color clears and buffer fills of N×64 bytes need
full-width rows (Step E width × Step F chaining). Step F only proved
8-dword rows; Step G proves the host max row width on console.

**Payload (`tools/payload/write_data_wide_rows.c`).** One IB of
`OPENAGC_PM4_WRITE_DATA_STEP_G_EOP_WORDS` (64) dwords. Host
`openagc_frontend_buffer_fill` routes aligned multiples of 64 bytes
(≤512) through `openagc_gpu_host_write_data_rows` with 16 dwords/row.

**Status before push.** Encoding + host path ready; CTest updated.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data_wide_rows.elf`
(`e24b720ed60fa83b20ec007761508585e304cc8c73a1a24a15feb1e97bc60da8`,
111,216 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 rows=2 dwords=16 pitch=128 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 94 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves full-width multi-row CP WRITE_DATA on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: DMA + WRITE_DATA + EOP (Step H)

**Question.** Can one FW9.40 graphics-queue IB run **IT_DMA_DATA**
(64 bytes) then **IT_WRITE_DATA** (4 dwords over the destination start)
then the shared EOP+NOP trailer, with both the DMA tail and the write
head matching?

**Why it matters.** Host `vkCmdCopyBuffer` followed by `vkCmdFillBuffer`
/ a clear needs both packet families in one submit without inventing
CB/DB or draw packets. Steps B and D–G proved each side alone.

**Payload (`tools/payload/dma_write_eop.c`).** One IB of
`OPENAGC_PM4_DMA_WRITE_STEP_H_EOP_WORDS` (39) dwords. Host
`openagc_gpu_host_dma_write_data` encodes the same layout and applies
the CPU copy+fill simulation.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`dma_write_eop.elf`
(`42eeb61140c85cd71431e1ed9eedbbdc66068c414e7d5ac17a549c6d2a3ec84e`,
111,208 bytes):
`submit=ok completed=1 matched=1 dma_bytes=64 write_dwords=4 dma_tail=11111111 write_head=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 95 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves DMA+WRITE_DATA chaining on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: compute store-span (Step I)

**Question.** Does an 8-thread FW9.40 compute dispatch of the original
`store_span` kernel (`flat_store_dword` to `s2:s3 + tid*4`, PAI vaddr
pair / lane rules) write eight `0xA5A5A5A5` dwords and fire EOP?

**Why it matters.** Without citeable CB/draw packets, a multi-lane
compute store is the next native fill vehicle beyond one-dword
`store_const` (Step C) and CP WRITE_DATA (Steps D–H). It keeps
`gpu_execution=0` on the host while matching console PM4.

**Payload (`tools/payload/store_span.c`).** One IB of
`OPENAGC_PM4_COMPUTE_STORE_WORDS` (51) dwords with `NUM_THREAD_X=8`.
Host `openagc_gpu_host_store_span` / `host_store_span` artifact flag.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`store_span.elf`
(`43eb843c75b05e2629b72054c4358d3133cb0618267f74932693c55c75b4eabf`,
111,256 bytes):
`submit=ok completed=1 matched=1 lanes=8 head=a5a5a5a5 tail=a5a5a5a5 beyond=cccccccc marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 96 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves 8-lane compute flat_store fills on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: dual compute store-span (Step J)

**Question.** Can one FW9.40 IB run the Step I `store_span` preamble
once, then **two** USER_DATA+DISPATCH pairs (bases `dest` and
`dest+32`), then one EOP, filling 16 dwords / 64 bytes?

**Why it matters.** Host compute clears larger than 32 bytes need
chained dispatches without inventing CB/draw. Step I proved one span;
Step J proves multi-dispatch compute fill on the same queue.

**Payload (`tools/payload/store_span2.c`).** One IB of
`OPENAGC_PM4_COMPUTE_STORE_SPAN2_WORDS` (62) dwords. Host
`openagc_gpu_host_store_span2`.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`store_span2.elf`
(`c91f3ffc0b99e886de081c2a78871cd72614bdb0a7865aa48ad27fb2bc66382a`,
111,256 bytes):
`submit=ok completed=1 matched=1 lanes=16 head=a5a5a5a5 mid=a5a5a5a5 tail=a5a5a5a5 beyond=cccccccc marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 97 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves dual store_span chaining on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: N-span compute store (Step K)

**Question.** Can one FW9.40 IB share the Step I preamble, then run
**N** USER_DATA+DISPATCH pairs (bases `dest + i*32`, sample N=4 /
128 bytes), then one EOP, filling `N*8` dwords?

**Why it matters.** Host clears larger than 64 bytes need a general
N-span encoder (`1..OPENAGC_PM4_COMPUTE_SPAN_MAX`) without inventing
CB/draw. Step J proved dual; Step K proves the parameterized chain.

**Payload (`tools/payload/store_span4.c`).** One IB of
`OPENAGC_PM4_COMPUTE_STORE_SPAN4_WORDS` (84) dwords. Host
`openagc_gpu_host_store_span_n` / dispatch sizing from binding bytes.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`store_span4.elf`
(`0c1297e1ffd6aa1dcb71ee686f15fe0cc41ef9db08a6e8a03b80c6923f0947e7`,
111,256 bytes):
`submit=ok completed=1 matched=1 spans=4 lanes=32 head=a5a5a5a5 tail=a5a5a5a5 beyond=cccccccc marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 98 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves N-span store_span chaining on console (sample N=4). It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: max N-span compute store (Step L)

**Question.** Can one FW9.40 IB run `OPENAGC_PM4_COMPUTE_SPAN_MAX`
(=8) USER_DATA+DISPATCH pairs (bases `dest + i*32`), then one EOP,
filling 256 bytes / 64 dwords?

**Why it matters.** Host `host_store_span_n` clamps binding size to
this ceiling. Console must prove the max chain the host will encode
before any larger fill invents a new vehicle or CB/draw.

**Payload (`tools/payload/store_span8.c`).** One IB of
`OPENAGC_PM4_COMPUTE_STORE_SPAN_MAX_WORDS` (128) dwords. Host dispatch
sizes spans from binding bytes up to this max.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`store_span8.elf`
(`143e47e09fd5a1fa7a1f236c82e21f7fcad7ffa3172897404800c320aeaddf20`,
111,256 bytes):
`submit=ok completed=1 matched=1 spans=8 lanes=64 head=a5a5a5a5 tail=a5a5a5a5 beyond=cccccccc marker=1`,
exit 0. Live klog was attached across the push (no fault/hang/timeout
string in the drained window). Loader still accepted connections on
9021 afterward.
This proves the host SPAN_MAX compute fill chain on console. It does
**not** unlock CB/DB, draws, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all.

## Bounded experiment: host MAX_ROWS WRITE_DATA window (Step M)

**Question.** Can one FW9.40 IB run **eight** full-width (16-dword)
`IT_WRITE_DATA` packets at contiguous pitch 64, then one EOP, filling
the host 16×8 RGBA8 clear window (512 bytes)?

**Why it matters.** Host color/depth clears and buffer fills already
encode up to `OPENAGC_PM4_WRITE_DATA_MAX_ROWS` (=8) in one IB, but
console evidence stopped at Step G (2×16). Step M closes that gap so
the host clear ceiling is console-proven, not assumed.

**Payload (`tools/payload/write_data_max_rows.c`).** One IB of
`OPENAGC_PM4_WRITE_DATA_STEP_M_EOP_WORDS` (184) dwords. No shader,
no CB/DB, no draw.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data_max_rows.elf`
(`6404db2af9df8d537a98bde92c5e942642a6020a6359eb8f784aaa966ab245c3`,
111,216 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 rows=8 dwords=16 pitch=64 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 100 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves the host MAX_ROWS WRITE_DATA clear window on console. It
does **not** unlock CB/DB, draws, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate remains an independently owned FW9.40 capture of CB/DB bind or
draw packets — inventing those is still out of scope.

## Bounded experiment: multi-column WRITE_DATA grid (Step N)

**Question.** Can one FW9.40 IB run an **8×2** grid of full-width
(16-dword) `IT_WRITE_DATA` packets at pitch 128, then one EOP, filling
a 32×8 RGBA8 window (1024 bytes)?

**Why it matters.** Host color clears wider than 16 already tiled into
multiple single-column IBs (each with its own EOP). Step N proves a
multi-column grid in **one** IB so VK/GL clears of width 32×height≤8
share one PM4 snapshot (`openagc_gpu_host_write_data_grid`).

**Payload (`tools/payload/write_data_grid.c`).** One IB of
`OPENAGC_PM4_WRITE_DATA_STEP_N_EOP_WORDS` (344) dwords. No shader,
no CB/DB, no draw.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`write_data_grid.elf`
(`1e369345bf100a970c248ad1b2ada02b09ffa900aabd4edae8ae5603cbb3a97c`,
111,216 bytes):
`submit=ok completed=1 matched=1 destination=0000000200021000 rows=8 cols=2 dwords=16 pitch=128 value=a5a5a5a5 marker=1`,
exit 0. Live klog shows `GFX(pipe0) Game` for pid 101 and
`exit_value=0`, with no fault/hang/timeout marker in that capture.
Loader still accepted connections on 9021 afterward.
This proves the host MAX_COLS×MAX_ROWS WRITE_DATA clear grid on
console. It does **not** unlock CB/DB, draws, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all.

## Bounded experiment: graphics-bank SET_SH + EOP (Step O)

**Question.** Does one FW9.40 IB of **graphics-bank** `SET_SH_REG`
(opcode `0x76`, low_bits=0) for the four smoke.vert
`shader_registers` pairs — with `SPI_SHADER_PGM_LO/HI` patched to an
uploaded 256-byte-aligned code VA using the same `>>8` / `>>40`
encoding as console-proven compute — plus the shared EOP+NOP trailer,
complete with a fired marker and no GPU fault?

**Why it matters.** Host PSBC plans already encode SET_CONTEXT/SET_SH
snapshots and can patch PGM addresses. Compute proved SET_SH with the
compute-bank bit set; graphics-bank SET_SH (bit clear) is still
unowned on console. A positive Step O result is the smallest
graphics-adjacent register program OpenAGC can own **without**
inventing CB/DB bind or DRAW packets. A negative result retires
graphics SET_SH as a safe vehicle on this firmware. Either way it does
**not** unlock draws, tiling, VideoOut, or host `gpu_execution`.

**Why not SET_CONTEXT or DRAW yet.** SET_CONTEXT_REG can touch SPI and
geometry state that may interact with the compositor; DRAW and CB/DB
still lack an independently owned FW9.40 capture. Step O deliberately
omits both.

**Entry conditions.** Steps A–N proven on `fw=0x9400008`; host encoding
locked in `pm4_graphics_fw940.h` / `psbc_metadata.h` (PGM patch +
`openagc_pm4_encode_graphics_sh_eop`); payload ELF-validated; one push,
no retries.

**Payload (`tools/payload/set_sh_gfx_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_SH_EOP_WORDS(4)` dwords. Uploads
`smoke.vert.gfx1013.bin`, patches PGM, no SET_CONTEXT, no DRAW.

**Status before push.** Host path ready (`openagc_gpu_host_graphics_register_eop`,
frontend `patch_psbc_pgm_vas` / `record_psbc_register_eop` /
`bind_psbc_code`). Encoding locked in CTest.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_sh_gfx_eop.elf`
(`fbc50bc17d68f9833c710f63f94efae79daefb68bc1a2b7058f1430bc6a7783a`,
110,080 bytes):
`submit=ok completed=1 pairs=4 words=36 code_va=0000000200020000 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves graphics-bank SET_SH_REG + EOP on console with PGM patched
to uploaded smoke.vert code. It does **not** unlock CB/DB, DRAW,
SET_CONTEXT on console, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all. The next graphics-adjacent gate is a bounded
SET_CONTEXT-only IB (Step P); inventing CB/DB or DRAW remains out of
scope.

## Bounded experiment: SET_CONTEXT + EOP (Step P)

**Question.** Does one FW9.40 IB of **three** public-AMD
`SET_CONTEXT_REG` packets (opcode `0x69`) for the smoke.vert
`context_registers` pairs — offsets/values from the pin-checked PSBC
fixture, **without** linkage (`ge_cntl` / `stages_en` / `user_vgpr_en`),
SET_SH, DRAW, or CB/DB — plus the shared EOP+NOP trailer, complete with
a fired marker and no GPU fault?

**Why it matters.** Host PSBC plans already encode SET_CONTEXT snapshots.
Step O proved graphics-bank SET_SH + EOP. The remaining register-program
half used by host plans is SET_CONTEXT. Owning the minimal
context-register vehicle on console (without enabling geometry stages
via linkage) is the smallest next graphics-adjacent proof that does not
invent CB/DB or DRAW.

**Why not linkage / DRAW / CB yet.** Linkage writes `ge_cntl` and
`stages_en`, which enable geometry pipeline stages and may interact with
the compositor. DRAW and CB/DB still lack an independently owned FW9.40
capture. Step P deliberately omits all three.

**Entry conditions.** Steps A–O proven on `fw=0x9400008`; host encoding
locked in `pm4_graphics_fw940.h`
(`openagc_pm4_encode_graphics_context_eop`); payload ELF-validated; one
push, no retries.

**Payload (`tools/payload/set_context_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(3)` (=33) dwords. Pairs
`(433,128)`, `(451,4)`, `(519,0)` from smoke.vert metadata. No code
upload, no SET_SH, no linkage, no DRAW.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`).

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_eop.elf`
(`e6aba0877c4af0ee68aa0d4b1a9f6973017a845abe7943569e859e770ed27b54`,
111,216 bytes):
`submit=ok completed=1 pairs=3 words=33 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves minimal SET_CONTEXT_REG + EOP on console for the three
smoke.vert `context_registers` pairs. It does **not** unlock linkage
writes, SET_SH combination, CB/DB, DRAW, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded SET_CONTEXT + graphics SET_SH combination IB
(Step Q); inventing CB/DB or DRAW remains out of scope.

## Bounded experiment: SET_CONTEXT + graphics SET_SH + EOP (Step Q)

**Question.** Does one FW9.40 IB that concatenates the Step P
`SET_CONTEXT_REG` ×3 pairs with the Step O graphics-bank `SET_SH_REG`
×4 pairs (PGM patched to uploaded smoke.vert code) — same offsets/
values, **without** linkage, DRAW, or CB/DB — plus the shared EOP+NOP
trailer, complete with a fired marker and no GPU fault?

**Why it matters.** Host PSBC plans already emit SET_CONTEXT then
SET_SH in one snapshot (`openagc_psbc_reflection_encode_register_program`).
Steps O and P proved each half alone. Owning the combined vehicle on
console is the smallest proof that the host's full (non-linkage)
register program is a safe IB shape on this firmware, without inventing
CB/DB or DRAW.

**Why not linkage / DRAW / CB yet.** Unchanged from Step P: linkage
enables geometry stages; DRAW and CB/DB still lack an independently
owned FW9.40 capture.

**Entry conditions.** Steps A–P proven on `fw=0x9400008`; host encoding
locked in `pm4_graphics_fw940.h`
(`openagc_pm4_encode_graphics_context_sh_eop`); payload ELF-validated;
one push, no retries.

**Payload (`tools/payload/set_context_sh_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(3,4)` (=45) dwords. Context
pairs from Step P; SH pairs + code upload from Step O. No linkage,
no DRAW, no CB/DB.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`).

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_sh_eop.elf`
(`047cd712b0cab8b3da81bb05bb76684a641645dbff6b42089f9d27ad9f7c0d1b`,
109,936 bytes):
`submit=ok completed=1 ctx=3 sh=4 words=45 code_va=0000000200020000 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves the combined SET_CONTEXT_REG + graphics SET_SH_REG + EOP
IB on console in host snapshot order (no linkage). It does **not**
unlock linkage writes, CB/DB, DRAW, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded linkage-only SET_CONTEXT IB (Step R); inventing
CB/DB or DRAW remains out of scope.

## Bounded experiment: linkage SET_CONTEXT + EOP (Step R)

**Question.** Does one FW9.40 IB of **three** public-AMD
`SET_CONTEXT_REG` packets for the smoke.vert **linkage** pairs
(`ge_cntl` / `stages_en` / `user_vgpr_en` — offsets/values from the
pin-checked PSBC fixture metadata) plus the shared EOP+NOP trailer
complete with a fired marker and no GPU fault?

**Why it matters.** Host PSBC plans already append those three linkage
context pairs after context+shader registers
(`openagc_psbc_reflection_encode_register_program`). Steps P–Q proved
the non-linkage SET_CONTEXT vehicle and the combined ctx+SH snapshot.
Owning the linkage writes alone on console — same SET_CONTEXT opcode
and EOP trailer already proven — is the smallest proof that the host's
full register snapshot (including linkage) is a safe IB shape on this
firmware, without inventing CB/DB or DRAW.

**Why not DRAW / CB yet.** DRAW and CB/DB still lack an independently
owned FW9.40 capture. Step R deliberately omits both; it only writes
the three verified linkage context pairs.

**Entry conditions.** Steps A–Q proven on `fw=0x9400008`; host encoding
reuses `openagc_pm4_encode_graphics_context_eop` (Step P vehicle) with
linkage offsets/values from `smoke.vert.metadata.json`; payload
ELF-validated; one push, no retries.

**Payload (`tools/payload/set_context_linkage_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_CONTEXT_EOP_WORDS(3)` (=33) dwords. Pairs
`(603,131200)`, `(725,65536)`, `(610,0)` from smoke.vert linkage.
No code upload, no SET_SH, no DRAW, no CB/DB.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`).

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_linkage_eop.elf`
(`825641bb920a1961913231e8495d2d3c22f90de2c4b384224f57a57e83847853`,
110,040 bytes):
`submit=ok completed=1 pairs=3 words=33 marker=1`,
exit implied by completed marker (`exit_value=0` for pid 105). Live klog
was attached across the push (no fault/hang/timeout string in that
capture). Loader still accepted connections on 9021 afterward.
This proves linkage SET_CONTEXT_REG + EOP on console for the three
smoke.vert linkage pairs. It does **not** unlock CB/DB, DRAW, tiling,
VideoOut, or host `gpu_execution`. `hardware_qualified` stays **false**.
The `openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded full host-aligned register program IB (Step S:
context + SH + linkage + EOP); inventing CB/DB or DRAW remains out of
scope. Host plans may encode/record the full register snapshot; draws stay
`NOT_READY`.

## Bounded experiment: full host register program + EOP (Step S)

**Question.** Does one FW9.40 IB that concatenates the Step Q
SET_CONTEXT ×3 + graphics SET_SH ×4 pairs with the Step R linkage
SET_CONTEXT ×3 pairs — same offsets/values, host snapshot order
(context → SH → linkage), PGM patched to uploaded smoke.vert code —
plus the shared EOP+NOP trailer, complete with a fired marker and no
GPU fault?

**Why it matters.** Host PSBC plans already emit the full register
program including linkage
(`openagc_psbc_reflection_encode_register_program`). Steps O–R proved
each piece and the non-linkage combination. Owning the full host-aligned
IB on console is the smallest proof that the snapshot the frontends
record is a safe single-submit shape on this firmware, without inventing
CB/DB or DRAW.

**Why not DRAW / CB yet.** DRAW and CB/DB still lack an independently
owned FW9.40 capture. Step S deliberately omits both; it only submits
the verified register program the host already encodes.

**Entry conditions.** Steps A–R proven on `fw=0x9400008`; host encoding
locked in `pm4_graphics_fw940.h`
(`openagc_pm4_encode_graphics_context_sh_linkage_eop`); payload
ELF-validated; one push, no retries.

**Payload (`tools/payload/set_context_sh_linkage_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_CONTEXT_SH_LINKAGE_EOP_WORDS(3,4)` (=54) dwords.
Context + SH from Step Q; linkage from Step R; code upload from Step O.
No DRAW, no CB/DB.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`); Step S encoder byte-identical to
`openagc_psbc_reflection_encode_register_program_eop` for smoke.vert.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_sh_linkage_eop.elf`
(`59089231b4ec92feb2e6fcd3d46a868379350af61ce39494405886731795010e`,
110,088 bytes):
`submit=ok completed=1 ctx=3 sh=4 link=3 words=54 code_va=0000000200020000 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves the full host-aligned register program (SET_CONTEXT +
graphics SET_SH + linkage SET_CONTEXT) + EOP on console in host snapshot
order. It does **not** unlock CB/DB, DRAW, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded smoke.frag register program IB (Step T: context + SH
+ EOP, no linkage); inventing CB/DB or DRAW remains out of scope. Host
plans may treat the full **vertex** register snapshot as console-proven
for encode/record only; draws stay `NOT_READY`.

## Bounded experiment: smoke.frag SET_CONTEXT + graphics SET_SH + EOP (Step T)

**Question.** Does one FW9.40 IB that concatenates the nine smoke.frag
`context_registers` pairs with the four smoke.frag graphics-bank
`shader_registers` pairs (PGM patched to uploaded smoke.frag code) —
same offsets/values from the pin-checked PSBC fixture, **without**
linkage, DRAW, or CB/DB — plus the shared EOP+NOP trailer, complete with
a fired marker and no GPU fault?

**Why it matters.** Host PSBC graphics plans already concatenate
smoke.vert + smoke.frag register snapshots. Step S proved the full
vertex side on console; the pixel half (9 context + 4 SH, `linkage:
null`) has only been encoded on the host. Owning the frag ctx+SH vehicle
on console — same Step Q encoder, different verified pairs — closes the
pixel side of the Stage-5 register snapshot without inventing CB/DB or
DRAW.

**Why not linkage / DRAW / CB yet.** smoke.frag metadata has
`linkage: null`. DRAW and CB/DB still lack an independently owned
FW9.40 capture.

**Entry conditions.** Steps A–S proven on `fw=0x9400008`; host encoding
reuses `openagc_pm4_encode_graphics_context_sh_eop` (Step Q vehicle)
with smoke.frag pairs; payload ELF-validated; one push, no retries.

**Payload (`tools/payload/set_context_sh_frag_eop.c`).** One IB of
`OPENAGC_PM4_GRAPHICS_CONTEXT_SH_EOP_WORDS(9,4)` (=63) dwords. Context
and SH pairs from `smoke.frag.metadata.json`; code upload of
`smoke.frag.gfx1013.bin` (48 bytes). No linkage, no DRAW, no CB/DB.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`); Step T encoder byte-identical to
`openagc_psbc_reflection_encode_register_program_eop` for smoke.frag
(unpatched PGM LO/HI match fixture zeros until runtime patch).

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_sh_frag_eop.elf`
(`f7023696f475c461786439be9448fea6801c26281e404b56444f922afe336d1f`,
110,088 bytes):
`submit=ok completed=1 ctx=9 sh=4 words=63 code_va=0000000200020000 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves the smoke.frag SET_CONTEXT_REG ×9 + graphics SET_SH_REG ×4
+ EOP IB on console (pixel half of the Stage-5 register snapshot). It
does **not** unlock CB/DB, DRAW, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**. The
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded vert+frag combined register IB (Step U: Step S shape
+ Step T shape + EOP, still no DRAW/CB); inventing CB/DB or DRAW remains
out of scope. Host plans may treat both vertex and fragment register
snapshots as console-proven for encode/record only; draws stay
`NOT_READY`.

## Bounded experiment: vert + frag register program + EOP (Step U)

**Question.** Does one FW9.40 IB that concatenates the Step S vertex
register program (SET_CONTEXT ×3 + graphics SET_SH ×4 + linkage
SET_CONTEXT ×3, PGM patched to uploaded smoke.vert) with the Step T
fragment register program (SET_CONTEXT ×9 + graphics SET_SH ×4, PGM
patched to uploaded smoke.frag) — host snapshot order, **without**
DRAW or CB/DB — plus a single shared EOP+NOP trailer, complete with a
fired marker and no GPU fault?

**Why it matters.** Host graphics plans already concatenate
smoke.vert + smoke.frag register snapshots
(`openagc_frontend_pipeline_*` /
`openagc_psbc_reflection_encode_register_program` twice). Steps S and T
proved each half alone. Owning the combined vehicle on console is the
smallest proof that the full Stage-5 host register snapshot is a safe
single-submit shape on this firmware, without inventing CB/DB or DRAW.

**Why not DRAW / CB yet.** DRAW and CB/DB still lack an independently
owned FW9.40 capture. Step U deliberately omits both; it only submits
the verified vert+frag register programs the host already encodes.

**Entry conditions.** Steps A–T proven on `fw=0x9400008`; host encoding
locked in `pm4_graphics_fw940.h`
(`openagc_pm4_encode_graphics_vert_frag_eop`); payload ELF-validated;
one push, no retries. IB size
`OPENAGC_PM4_GRAPHICS_VERT_FRAG_EOP_WORDS(3,4,9,4)` (=93) dwords —
under the arena IB window used by Steps S/T (0x2000..0x2800).

**Payload (`tools/payload/set_context_sh_vert_frag_eop.c`).** One IB of
93 dwords. Vert pairs + code from Step S; frag pairs + code from
Step T (distinct 256-byte-aligned code VAs). No DRAW, no CB/DB.

**Status before push.** Host encoding locked in CTest
(`test_openagc_psbc_adapter`); Step U encoder byte-identical to
`encode_register_program(vert) + encode_register_program(frag) + EOP`.

**Observed result (2026-09-25, FW `0x9400008`).** One push of
`set_context_sh_vert_frag_eop.elf`
(`2b3b79b062f7dc14d559b465dca1a5f5267401c26748b6feee26f9be7dca9d4e`,
110,144 bytes):
`submit=ok completed=1 v_ctx=3 v_sh=4 link=3 f_ctx=9 f_sh=4 words=93 vert_va=0000000200020000 frag_va=0000000200020100 marker=1`,
exit implied by completed marker. Live klog was attached across the push
(no fault/hang/timeout string required for acceptance beyond marker=1
and loader still accepting). Loader still accepted connections on 9021
afterward.
This proves the combined host-aligned vert+frag register program
(SET_CONTEXT + graphics SET_SH + linkage, then frag SET_CONTEXT +
graphics SET_SH) + EOP on console in host snapshot order. It does
**not** unlock CB/DB, DRAW, tiling, VideoOut, or host `gpu_execution`.
`hardware_qualified` stays **false**. The `openagc_ps5_policy` target
stays deny-all. The next graphics-adjacent gate remains inventing neither
CB/DB nor DRAW until an independently owned FW9.40 capture exists; host
plans may treat the full Stage-5 vert+frag register snapshot as
console-proven for encode/record only; draws stay `NOT_READY`.

### Host CB capture intake (fail-closed scaffold)

**Question.** Can the host accept a CB/DB bind IB **only** when a
manifest + SHA-256 digest verifies the supplied dwords, without inventing
register values or DRAW packets?

**Status.** Scaffolded in `include/openagc/pm4_cb_capture_fw940.h` and
`openagc_gpu_host_cb_bind_from_capture` / frontend
`openagc_frontend_render_pass_bind_cb_capture`. Invent encode always
returns `UNSUPPORTED_OPERATION`. Evidence pin table count is **0** (no
owned FW9.40 CB/DB/DRAW dump in-repo). Structural verify+record may set
`capture_verified=1` with `evidence_qualified=0`, `gpu_submitted=0`;
`hardware_qualified` stays **false**; `gpu_executable` stays 0; deny-all
PS5 policy unchanged. DRAW captures remain `NOT_READY` even when the
digest matches. Console push of invent/CB remains out of scope until a
real cite is pinned.

## Bounded experiment: IB dump of Step U (Step V)

**Question.** Can a console payload re-submit the proven Step-U register
program + EOP and write an FTP-retrievable `openagc-ib-dump` log of the
exact submitted dwords, without adding CB/DB color binds or DRAW, so a
later independently owned CB capture can reuse the same dump vehicle?

**Design.** `tools/payload/ib_dump_step_u_eop.c` encodes the same Step-U
IB, submits once, polls the EOP marker, then writes
`/data/prosperoai/openagc-ib-dump-step-u.log` in the format defined by
`include/openagc/pm4_ib_dump_fw940.h`. Host
`openagc_ib_dump_parse` accepts `tag=step-u` as `REGISTER_EOP` with
`evidence_qualified=0`. One push, no retries. No invent CB values.

**Why it matters.** Stage 5 remains blocked on an independently owned
FW9.40 CB/DB or DRAW cite. Public Mesa/amdgpu/umr sources cite
`PACKET3_SET_CONTEXT_REG` and `CB_COLOR*` *offsets* but not a
PS5-owned dword *value* sequence safe to pin. Owning a dump vehicle on
console is the safest path to intake a real capture without inventing
registers.

**Why not CB / DRAW yet.** Unchanged: no independently owned FW9.40
CB/DB/DRAW IB exists in-repo or on the console FTP tree. Step V dumps
only the Step-U register program.

**Artifact.** `ib_dump_step_u_eop.elf`
(`b75eea10928df6eb8657365b4ab70a9081ed024cc50fc000d4d7d0b5198d5137`,
110,240 bytes): one push wrote
`/data/prosperoai/openagc-ib-dump-step-u.log` with
`tag=step-u fw=0x9400008 completed=1 words=93` and 93 hex dwords;
host `openagc_ib_dump_parse` accepted the log as `REGISTER_EOP` with
`evidence_qualified=0`. Loader still accepted connections on 9021
afterward.
This proves the IB dump vehicle on console for the Step-U register
program. It does **not** unlock CB/DB, DRAW, tiling, VideoOut, or host
`gpu_execution`. `hardware_qualified` stays **false**;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**.

### PSBC smoke → gfx10 register map (owned SPI/PA/DB_SHADER/CB_SHADER_MASK)

**Question.** Do the smoke.vert / smoke.frag `context_registers` (+ linkage)
offsets map to public Mesa/amdgpu gfx10 `CB_*` / `DB_*` / `PA_*` / `SPI_*`
names, and does that subset already constitute owned CB *bind* evidence?

**Mapping** (SET_CONTEXT_REG dword index = Linux `mmNAME` when
`NAME_BASE_IDX=1`; cite `gc_10_1_0_offset.h` + Mesa SET_CONTEXT_REG).
Host atlas: `include/openagc/pm4_context_regs_gfx10.h`.

| Source | offset | Public name | Notes |
| --- | --- | --- | --- |
| vert ctx | 433 | `SPI_VS_OUT_CONFIG` | console Steps P–U |
| vert ctx | 451 | `SPI_SHADER_POS_FORMAT` | |
| vert ctx | 519 | `PA_CL_VS_OUT_CNTL` | |
| vert linkage | 603 | `GE_CNTL` | Linux `mmGE_CNTL=0x225B` (low12=`0x25B`); PSBC `ge_cntl` |
| vert linkage | 725 | `VGT_SHADER_STAGES_EN` | PSBC `stages_en` |
| vert linkage | 610 | `GE_USER_VGPR_EN` | Linux `mm=0x2262` (low12=`0x262`); PSBC `user_vgpr_en` |
| frag ctx | 452 | `SPI_SHADER_Z_FORMAT` | console Step T/U |
| frag ctx | 453 | `SPI_SHADER_COL_FORMAT` | |
| frag ctx | 435 | `SPI_PS_INPUT_ENA` | |
| frag ctx | 436 | `SPI_PS_INPUT_ADDR` | |
| frag ctx | 438 | `SPI_PS_IN_CONTROL` | |
| frag ctx | 440 | `SPI_BARYC_CNTL` | |
| frag ctx | 515 | `DB_SHADER_CONTROL` | shader DB control — **not** `DB_*_BASE` |
| frag ctx | 143 | `CB_SHADER_MASK` | **only** smoke-owned `CB_*` |
| frag ctx | 784 | `PA_SC_SHADER_CONTROL` | |
| vert SH | 72–75 | `SPI_SHADER_PGM_{LO,HI,RSRC1,RSRC2}_VS` | SET_SH; PGM patched |
| frag SH | 8–11 | `SPI_SHADER_PGM_{LO,HI,RSRC1,RSRC2}_PS` | SET_SH; PGM patched |

**Absent from smoke (Stage 5 gap):** `CB_COLOR0_BASE` (792), `PITCH` (793),
`SLICE` (794), `VIEW` (795), `INFO` (796), `ATTRIB` (797),
`CB_TARGET_MASK` (142). Public *offsets* only — no owned *values*.

**Assessment.** PSBC-owned metadata + console-proven SET_CONTEXT is
owned evidence for the SPI/PA/GE/VGT/`DB_SHADER_CONTROL`/`CB_SHADER_MASK`
subset above. It is **not** owned CB *bind* (COLOR_BASE/pitch/tiling)
evidence. Do **not** pin a `CB_BIND` capture from smoke digests;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays 0.

## Bounded experiment: CB context-register readback (Step W)

**Question.** Can a console payload use public-cite `PACKET3_COPY_DATA`
(register→memory, gfx_v10 `emit_rreg` layout) to read the eight
COLOR_BASE-class offsets into a CPU-visible buffer and dump them as
`tag=ctxreg-cb`, without SETting CB binds or inventing values?

**Design.** `tools/payload/ctxreg_cb_dump_eop.c` encodes
`openagc_pm4_encode_copy_data_cb_probe_eop` (host-locked from Mesa sid.h
+ drm/amdgpu `gfx_v10_0_ring_emit_rreg`), submits once, polls EOP, writes
`/data/prosperoai/openagc-ib-dump-ctxreg-cb.log`. Host
`openagc_ib_dump_parse` accepts `tag=ctxreg-cb` as `CTXREG_CB` with
`evidence_qualified=0`. Probe order is fixed in
`openagc_gfx10_cb_probe_offsets`. One push, no retries. No invent CB
SET values; pin table stays empty until a real bind IB is owned.

**Why it matters.** Stage 5 is blocked on COLOR_BASE-class *values*, not
on opcode knowledge. Reading whatever the live FW9.40 context holds
(compositor residue or zeros) is the fail-closed path to own those
dwords without invention.

**Status.** Host encode + dump parse + atlas landed. Console push
recorded below.

**Artifact.** `ctxreg_cb_dump_eop.elf`
(`0c088bbcf4216dc2fbbdc8ef4482014b3e1d9e3f053b98c4cc8ca1702c910cd1`,
110,000 bytes): one push wrote
`/data/prosperoai/openagc-ib-dump-ctxreg-cb.log` with
`tag=ctxreg-cb fw=0x9400008 completed=0 words=8` and eight poison
`cccccccc` dwords (destination untouched). Host `openagc_ib_dump_parse`
accepts the log as `CTXREG_CB` with `evidence_qualified=0`. Loader still
accepted connections on 9021 afterward.
This proves the dump vehicle and that the public-cite `COPY_DATA`
register→memory encoding used here did **not** complete on the FW9.40
graphics submit path within the deadline. It does **not** unlock CB/DB
binds, DRAW, or owned COLOR_BASE values. `hardware_qualified` stays
**false**; `OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**. Do **not**
retry this relative-offset encoding. Step X tests the distinct absolute
aperture form (`CONTEXT_REG_START+offset`) under a separate cite.

## Bounded experiment: absolute COPY_DATA CB probe (Step X)

**Question.** Does the same public `PACKET3_COPY_DATA` control as Step W
complete when `src_lo` uses the absolute context aperture address
`PACKET3_SET_CONTEXT_REG_START (0xA000) + relative_offset` instead of the
relative SET_CONTEXT dword index alone?

**Hypothesis (public cite).** `gfx_v10_0_ring_emit_rreg` passes absolute
mm-mapped register dword addresses as `src_lo`. Context registers programmed
via `PACKET3_SET_CONTEXT_REG` are relative to
`PACKET3_SET_CONTEXT_REG_START` (`soc15d.h` `0x0000a000`). Step W used
relative indices (e.g. `CB_COLOR0_BASE=792`) and timed out; Step X uses
`0xA000+792` (=`0xA318`) for the same probe set. Control word unchanged
(`SRC_SEL=reg|DST_SEL=mem|WR_CONFIRM`). Not a blind retry of Step W.

**Design.** `tools/payload/ctxreg_abs_dump_eop.c` encodes
`openagc_pm4_encode_copy_data_cb_probe_abs_eop`, submits once, polls EOP,
writes `/data/prosperoai/openagc-ib-dump-ctxreg-abs.log`. Host
`openagc_ib_dump_parse` accepts `tag=ctxreg-abs` as `CTXREG_ABS` with
`evidence_qualified=0`. Same eight public offsets as Step W; no invent
CB SET values; pin table stays empty.

**Why it matters.** Stage 5 still needs owned COLOR_BASE-class *values*.
If absolute addressing completes, the dump may record live residue or
zeros without inventing binds. If it also times out, COPY_DATA
register→memory on this graphics submit path remains unproven and needs
a different public-cite path (not another offset tweak without review).

**Status.** Host encode + dump parse locked. Console push recorded below.

**Artifact.** `ctxreg_abs_dump_eop.elf`
(`d9cb3076acb9d0376329d0b75ef70229113221d355599c71c28e734f7ceb56ca`,
110,152 bytes): one push wrote
`/data/prosperoai/openagc-ib-dump-ctxreg-abs.log` with
`tag=ctxreg-abs fw=0x9400008 completed=1 words=8` and
`ib 00000000 00000000 00000000 00000000 00000000 00000000 ffffffff ffffffff`
(probe order: COLOR0_BASE/PITCH/SLICE/VIEW/INFO/ATTRIB = 0;
TARGET_MASK/SHADER_MASK = `0xffffffff`). Host `openagc_ib_dump_parse`
accepts the log as `CTXREG_ABS` with `evidence_qualified=0`. Loader still
accepted connections on 9021 afterward.
This proves absolute-aperture `COPY_DATA` register→memory on the FW9.40
graphics submit path and owns the eight readback dwords above. It does
**not** unlock CB/DB binds or DRAW: COLOR_BASE-class values are zero (no
usable bind to pin), and mask dwords alone are not a CB_BIND capture.
`hardware_qualified` stays **false**;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**. Do not invent non-zero
COLOR_BASE values from this dump.

## Bounded experiment: SET_CONTEXT + abs COPY_DATA round-trip (Step Y)

**Question.** Can one FW9.40 IB write smoke-owned context register values
via proven `PACKET3_SET_CONTEXT_REG`, then read them back with the Step-X
proven absolute `COPY_DATA` (`CONTEXT_REG_START+offset`) and recover the
same citeable non-default dwords?

**Hypothesis.** Step X proved absolute register→memory completes. Steps
P–U proved SET_CONTEXT of smoke SPI/PA/DB_SHADER/`CB_SHADER_MASK` pairs.
Combining them yields owned write→read evidence without inventing
`CB_COLOR0_BASE` binds. `CB_SHADER_MASK=15` from
`tests/fixtures/psbc_smoke/smoke.frag.metadata.json` is distinct from the
Step X live residue (`0xffffffff`), so a matching readback is unambiguous.

**Design.** `tools/payload/ctxreg_rt_dump_eop.c` encodes
`openagc_pm4_encode_ctxreg_rt_abs_eop` (six non-zero smoke.frag context
pairs, then six absolute COPY_DATA readbacks, then EOP), submits once,
polls EOP, writes `/data/prosperoai/openagc-ib-dump-ctxreg-rt.log` with
`tag=ctxreg-rt`. Host `openagc_ib_dump_parse` accepts `CTXREG_RT` with
`evidence_qualified=0`. No COLOR_BASE SET; pin table stays empty.

**Why it matters.** Round-trip ownership of smoke SPI/PA/DB_SHADER/
`CB_SHADER_MASK` values strengthens the register path used by host PSBC
plans. It does **not** close Stage 5: COLOR_BASE-class binds remain
unowned.

**Status.** Host encode + dump parse locked. Console push recorded below.

**Artifact.** `ctxreg_rt_dump_eop.elf`
(`f999c36ccec06c8012bdf946f8de91e688879b942d88b4644eab6ee6a5bdffe7`,
110,152 bytes): one push wrote
`/data/prosperoai/openagc-ib-dump-ctxreg-rt.log` with
`tag=ctxreg-rt fw=0x9400008 completed=1 words=6` and
`ib 00000009 00000080 00000080 00008000 00000010 0000000f`
(probe order: `SPI_SHADER_COL_FORMAT=9`, `SPI_PS_INPUT_ENA=128`,
`SPI_PS_INPUT_ADDR=128`, `SPI_PS_IN_CONTROL=32768`,
`DB_SHADER_CONTROL=16`, `CB_SHADER_MASK=15` — exact smoke.frag fixture
values; `CB_SHADER_MASK` distinct from Step X residue `0xffffffff`).
Host `openagc_ib_dump_parse` accepts the log as `CTXREG_RT` with
`evidence_qualified=0`. Loader still accepted connections on 9021 afterward.
This proves owned SET_CONTEXT → absolute COPY_DATA round-trip for the six
smoke-owned SPI/PA/DB_SHADER/`CB_SHADER_MASK` registers on FW9.40. It does
**not** unlock CB/DB binds or DRAW: no COLOR_BASE was written or claimed.
`hardware_qualified` stays **false**;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**.

## Bounded experiment: owned CB BASE + abs COPY_DATA round-trip (Step Z)

**Question.** Can one FW9.40 IB program `CB_COLOR0_BASE` and
`CB_COLOR0_BASE_EXT` from a known color-buffer VA inside the payload's
own direct-memory arena (public Mesa encoding `va >> 8` /
`(va >> 8) >> 32`), keep the smoke-owned `CB_SHADER_MASK`, then read the
eight CB probe offsets back with the Step-X-proven absolute
`COPY_DATA` and recover the programmed BASE/BASE_EXT?

**Why it matters.** Stage 5 needs owned COLOR_BASE-class *values*.
Steps X/Y proved absolute register readback and a smoke-owned
SPI/PA/DB_SHADER/`CB_SHADER_MASK` round-trip, but both left
`CB_COLOR0_BASE` zero or unowned. Step Z is the first submit that writes
a known, owned color-buffer VA into the CB base registers and has the CP
read the value back — the smallest owned BASE evidence that does not
invent render-target format/tiling or DRAW.

**Why not INFO/ATTRIB / DRAW yet.** `CB_COLOR0_INFO`/`ATTRIB2`/`VIEW`/
`TARGET_MASK` values are still unowned (no independently owned FW9.40
capture and no safe public cite for their *values*), and DRAW still has
no owned capture. Step Z writes BASE, BASE_EXT, and the
already-console-proven smoke `CB_SHADER_MASK` only. Pin table stays
empty; `evidence_qualified` stays 0.

**Payload contract (`tools/payload/ctxreg_cb_bind_eop.c`).**

May do: open `/dev/gc`; map one arena; place a 4 KiB zeroed color buffer
at a 256-byte-aligned VA; submit **one** IB of
`OPENAGC_PM4_CTXREG_CB_BIND_EOP_WORDS` (=78) dwords — SET_CONTEXT
`CB_COLOR0_BASE` + `BASE_EXT` + `CB_SHADER_MASK`, eight absolute
`COPY_DATA` reads, shared EOP+NOP trailer; poll the marker 30s; write
`/data/prosperoai/openagc-ib-dump-ctxreg-cb-bind.log` (one
`openagc-cb-bind-owned:` expect line with the owned VA/BASE/BASE_EXT,
then the `openagc-ib-dump:` block); CPU-check nothing else; exit.

Must not do: no DRAW/`DRAW_INDEX_AUTO`; no INFO/ATTRIB/VIEW/TARGET_MASK
SET; no shader; no `flat_load`; no second submit; no retry; no VideoOut;
no queue-create/ACB; no malformed ELF.

**Acceptance criteria.** `completed=1`, the readback BASE equals
`color_va >> 8` and BASE_EXT equals `(color_va >> 8) >> 32`, shader mask
equals 15, one log line set, no fault/hang/timeout in the live klog
window (TCP 3232), loader still accepting connections afterward.

**Status before push.** Host encode + dump parse locked in CTest
(`test_openagc_gpu`); `openagc_ib_dump_parse` skips the leading
owned-expect line and refuses text with no header;
`openagc_ib_dump_cb_bind_legacy_base_match` is fail-closed on zero
expected BASE or any mismatch. One push, no retries.

**Artifact.** `ctxreg_cb_bind_eop.elf` (built from revision `3f192cb`:
`78bc6c66fde1ab472f8001a61b41734a20412ded62f8827f43db51570ff7f425`,
110,152 bytes, ELF-validated). The console dump below is the record of the
single push; no re-push was performed for this review.

**Observed result (2026-09-25, FW `0x9400008`).** The dump
`/data/prosperoai/openagc-ib-dump-ctxreg-cb-bind.log` records
`tag=ctxreg-cb-bind fw=0x9400008 completed=1 words=8` and
`ib 02000240 00000000 00000000 00000000 00000000 00000000 ffffffff 0000000f`
against its owned expect line
`color_va=0000000200024000 base_lo=02000240 base_ext=00000000 shader_mask=0000000f`:
the CP readback of `CB_COLOR0_BASE` equals `color_va >> 8`, `BASE_EXT`
equals `(color_va >> 8) >> 32` (zero at this VA), and the smoke-owned
`CB_SHADER_MASK` reads 15, so the fail-closed owned-base match holds
(host regression fixture: `test_openagc_gpu`). `ATTRIB2`/`VIEW`/`INFO`/
`ATTRIB` read back zero (never written) and `TARGET_MASK` reads the
unwritten residue `ffffffff`. The reviewed klog windows (FTP snapshot
plus a live drain on 3232 after the fact) contain no
panic/fault/hang/timeout marker, and the loader still accepted
connections on 9021 afterward.
This proves owned SET_CONTEXT → absolute COPY_DATA round-trip for
`CB_COLOR0_BASE(+EXT)` on the FW9.40 graphics submit path. It does
**not** unlock CB/DB binds or DRAW: `INFO`/`ATTRIB2`/`VIEW`/
`TARGET_MASK` remain unowned, no DRAW initiator was submitted, and the
pin table stays empty. `hardware_qualified` stays **false**;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**. The next
graphics-adjacent gate is owning the remaining CB bind dwords from a
citeable source; inventing `INFO`/`ATTRIB` values or DRAW remains out of
scope.

**Correction (Step AB).** The second word this payload wrote and read
went to offset 793, which the atlas of the time called `BASE_EXT`. Per
`gc_10_1_0_offset.h` that offset is `CB_COLOR0_PITCH`, a hole on GFX10,
and `BASE_EXT` is 912. The value read back was zero because both the
write target and the claimed field are zero at this VA, so the dump does
**not** demonstrate a `BASE_EXT` round-trip. That claim is withdrawn;
what this dump still supports is the `CB_COLOR0_BASE` write/read path and
the `CB_SHADER_MASK` value under SET_CONTEXT. Step AB writes and reads
the real `BASE_EXT` at 912.

## Bounded experiment: GB tile-mode table readback (Step AA)

**Question.** Does one read-only absolute `COPY_DATA`
(`src_sel=0` mem-mapped register, the aperture Step X proved) of
`GB_ADDR_CONFIG` and `GB_TILE_MODE0..31` complete on the FW9.40
graphics submit path and return the console's own tile-mode table as
`tag=mmio-tilemode`?

**Why it matters.** Any color-target bind needs
`CB_COLOR0_ATTRIB.TILE_MODE_INDEX`, and that index is meaningful only
relative to the table firmware/driver programmed — console state the
host must not invent. Every capture so far read the submit's *own*
context. This is the first read of GPU configuration state that is not
the payload's own context, and it is purely read-only.

**Public cites.** `gc_10_1_0_offset.h`: `mmGB_ADDR_CONFIG=0x13DE`,
`mmGB_TILE_MODE0..31=0x13E4..0x1403`, all `BASE_IDX=0` (address used
as-is). `gc_10_1_0_sh_mask.h`: `GB_TILE_MODE0__ARRAY_MODE__SHIFT=2`
(mask `0x3C`), `PIPE_CONFIG=6`, `TILE_SPLIT=11`,
`MICRO_TILE_MODE_NEW=22` (mask `0x1C00000`), `SAMPLE_SPLIT=25`;
`GB_ADDR_CONFIG` `NUM_PIPES=0`, `PIPE_INTERLEAVE_SIZE=3`,
`MAX_COMPRESSED_FRAGS=6`, `NUM_SHADER_ENGINES=19`, `NUM_RB_PER_SE=26`.
drm/amdgpu `gfx_v10_0_ring_emit_rreg` establishes `PACKET3_COPY_DATA`
register→memory with an absolute mem-mapped source address.

**Why not write / DRAW.** A tile-mode index only makes a bind *valid*,
it does not make one safe; `CB_COLOR0_INFO`/`ATTRIB2`/`VIEW`/
`TARGET_MASK` values and DRAW stay out of scope. This IB writes no
register at all and submits no draw.

**Entry conditions.** Steps A–Z proven on `fw=0x9400008`; host encode +
parse + lookup locked in CTest; payload ELF-validated; one push, no
retries.

**Payload contract (`tools/payload/mmio_tilemode_dump_eop.c`).**

May do: open `/dev/gc`; map one arena; submit **one** IB of
`OPENAGC_PM4_MMIO_TILEMODE_PROBE_EOP_WORDS` (=222) dwords — 33 absolute
`COPY_DATA` reads (`GB_ADDR_CONFIG`, then `GB_TILE_MODE0..31` in index
order so a partial result still reads as a prefix) plus the shared
EOP+NOP trailer; poll the marker 30s; write
`/data/prosperoai/openagc-ib-dump-mmio-tilemode.log`; exit.

Must not do: no register write, no `SET_CONTEXT_REG`, no shader, no
DRAW, no `flat_load`, no second submit, no retry, no VideoOut, no
queue-create/ACB, no malformed ELF.

**Acceptance criteria.** `completed=1` with 33 non-poison dwords,
`GB_TILE_MODE` entries that yield at least one resolvable
(`ARRAY_MODE`, `MICRO_TILE_MODE_NEW`) index, no fault/hang/timeout in the
live klog window (TCP 3232), loader still accepting connections
afterward. A timeout (`completed=0`, poison) retires MMIO reads on this
submit path; it is a negative result, not a retry trigger.

**Status before push.** Host encode, `tag=mmio-tilemode` parse, and the
fail-closed `openagc_ib_dump_mmio_tilemode_lookup` are locked in
`test_openagc_gpu` (including completed=0 and wrong-kind refusals).
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays 0;
`evidence_qualified` stays 0. One push, no retries.

**Artifact.** `mmio_tilemode_dump_eop.elf`
(`5fd67bf50135fc39b9c90146d0fb57842c61ddc1cb16d0d807b7f657b96065f6`,
110,184 bytes, ELF-validated): exactly one push, klog attached across it,
no re-push afterward.

**Observed result (2026-09-25, FW `0x9400008`).** The dump
`/data/prosperoai/openagc-ib-dump-mmio-tilemode.log` records
`tag=mmio-tilemode fw=0x9400008 completed=0 words=33` with all 33 dwords
still `cccccccc` (destination untouched): the absolute `COPY_DATA` reads
of `GB_ADDR_CONFIG`/`GB_TILE_MODE0..31` did **not** complete on the
FW9.40 graphics submit path within the 30-second deadline. The loader
still accepted connections on 9021, FTP served the log, and the live
klog window contains no fault/hang/timeout/panic marker.
This retires the mem-mapped-register `COPY_DATA` read of the `0x13xx`
GB register block under the Step-X-equivalent encoding: the aperture
that works for `0xA000 + ctxreg` does not extend to those addresses. It
qualifies nothing and the tile-mode table stays unowned
(`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**;
`hardware_qualified` stays **false**).
**Do not retry this encoding.** Any next attempt must be a distinct,
reviewed public-cite path (a different `COPY_DATA` source-select or an
indexed read sequence), not another address tweak. Host parse and the
fail-closed index lookup stay locked for a future successful capture; a
`completed=0` dump never resolves an index.

## Bounded experiment: one point draw into an owned target (Step AC)

**Question.** Does one FW9.40 IB that runs the console-proven smoke.vert /
smoke.frag register program and a Step-AB linear color bind — plus the
minimum rasterizer state, a POINTLIST topology, and a single
`PACKET3_DRAW_INDEX_AUTO` of one vertex — actually rasterize, so that the
pixel shader's exported color appears in the target and nowhere else?

**Why it matters.** Everything OpenAGC owns so far is register programming
and copies. Rasterization is the gate the frontends' entire draw path
waits on, and this is the smallest draw the fixtures allow: the pinned
smoke.vert emits a constant `vec4(0,0,0,1)`, so three vertices are
degenerate and only a point primitive can cover a pixel. A point draw
exercises VS → VGT → PA → SC → SPI → PS → CB with one vertex and one
pixel, no vertex buffer, no depth buffer, and no second primitive.

**Cited state (Mesa at the pinned revision, all on top of CLEAR_STATE).**

* `PKT3_CLEAR_STATE` (0x12) + 0: `gfx10_init_gfx_preamble_state` emits it
  before any draw and relies on its documented defaults for everything it
  does not re-set (`sid.h` gives the opcode and
  `CIK_UCONFIG_REG_OFFSET = 0x30000`, which fixes every packet offset as
  the register's mm low 12 bits).
* Topology: `VGT_PRIMITIVE_TYPE` is a **uconfig** register on GFX10
  (`radeon_set_uconfig_reg(R_030908_VGT_PRIMITIVE_TYPE, ...)` in
  `si_state_draw.cpp`), i.e. `SET_UCONFIG_REG` (0x79) with offset
  `(0x030908 - 0x30000) >> 2 = 578`, value `DI_PT_POINTLIST` (1).
* Draw packet: `PKT3(PKT3_DRAW_INDEX_AUTO, 1)` + vertex count +
  `DI_SRC_SEL_AUTO_INDEX` (`use_opaque` is 0 for this draw).
* Color: the Step-AB nine-register linear RGBA8 bind, unchanged.
* Shaders: the Step-S/T/U program, PGMs patched from the uploaded code VAs.
* Rasterizer state (`si_create_rs_state`, `si_emit_viewport`,
  `ac_compute_guardband`, `si_emit_window_rectangles`,
  `si_emit_framebuffer_state`): viewport scale/offset for the drawn rect,
  `PA_SC_VPORT_ZMIN_0/ZMAX_0` = 0/1, guardband clip 4094.0f / discard 1.0f
  (the values `ac_compute_guardband` produces for this viewport with zero
  clip/discard distance, and no hardware screen offset), screen scissor =
  the drawn rect, window scissor = the framebuffer with
  `WINDOW_OFFSET_DISABLE`, `PA_SC_CLIPRECT_RULE = 0xffff` (the "no window
  rectangles" value), `PA_SC_EDGERULE` from the OpenGL-FBO branch,
  `PA_SU_POINT_SIZE`/`MINMAX` = 8 (one pixel in 1/8-pixel units, the cited
  non-per-vertex case), `PA_SC_MODE_CNTL_0` = alternate RBs per tile with
  MSAA and the per-viewport scissor off, `PA_CL_CLIP_CNTL` = GL clip space
  with linear attribute clipping, `PA_SU_VTX_CNTL` = half-pixel center,
  round-to-even, 1/256-pixel quantisation, `DB_Z_INFO`/`DB_STENCIL_INFO` =
  `Z_INVALID`/`STENCIL_INVALID` (no depth or stencil buffer is bound).

**Payload contract (`tools/payload/draw_point_eop.c`).**

May do: open `/dev/gc`; map one 256 KiB arena; upload the two smoke code
blobs; submit **one** IB of `OPENAGC_PM4_DRAW_POINT_EOP_WORDS` (=212)
dwords; poll the EOP marker with a 30-second monotonic deadline; read the
color window back on the CPU; count every other nonzero dword in the arena
and report it; write
`/data/prosperoai/openagc-ib-dump-draw-point.log`; exit.

Must not do: no second submit; no retry; no vertex buffer, index buffer, or
depth buffer; no second primitive; no shader other than the two pinned
smoke blobs; no VideoOut; no queue-create/ACB; no malformed ELF.

**Why this is bounded.** The draw is scissored (screen scissor) to a 16×16
rect inside a 32×32 linear target, the target sits inside a 256 KiB arena,
the shaders contain no memory instruction at all (pure ALU plus `exp`), no
depth buffer is bound, and the payload scans the whole arena afterwards and
reports any nonzero dword outside the drawn rect. A wrong topology,
viewport, or scissor yields no pixels rather than stray writes; a
rejected packet yields `completed=0` exactly like Steps W and AA.

**Acceptance criteria.** `completed=1`; at least one pixel equals the
shader's exported color (`0xff0040ff`: `SPI_SHADER_32_ABGR` into an
`8_8_8_8` UNORM target with `SWAP_STD`); every nonzero pixel in the window
equals that value; `outside=0`; no fault/hang/timeout in the live klog
window (TCP 3232); loader still accepting connections afterward.

**Status before push.** Draw-state composition, packet shapes, encode size,
and the fail-closed `openagc_ib_dump_draw_point_pixels_match` are locked in
`test_openagc_gpu` (wrong pixel, all-zero window, `completed=0`, wrong
kind, and NULL inputs all refuse). `hardware_qualified` stays false; the
pin tables stay empty. One push, no retries.

**Artifact.** `draw_point_eop.elf`
(`ebe7168d93ab0afb79c667df5d9c662e2cb770eaedc251f193fdf82c7abae379`,
111,424 bytes, ELF-validated by `tools/payload/validate_elf.py`).

**Observed result (2026-09-25, FW `0x9400008`): no pixel evidence, payload
defect.** One push was made; the loader accepted it and started the payload
as pid 123. The console's live klog shows the process receiving a fatal
signal **after the submit**:

```
# signal: 11 (SIGSEGV)   proc name: payload.elf
# reason: page fault (user read data, page not present)
# fault address: 0000000200080000
# rbx: 0000000200044000   r13: 0000000200044420
```

`0x0000000200044000` is the arena's color region and `0x0000000200080000`
is exactly one page past the end of the payload's own 256 KiB arena
mapping, i.e. the fault is in this payload's post-processing, not in the
GPU path: the acceptance scan iterated `OPENAGC_ARENA / 4` words *from the
colour pointer* instead of from the arena base and ran off the mapping
after the submit and the marker poll. The log file was therefore never
written (`/data/prosperoai/openagc-ib-dump-draw-point.log` does not exist,
FTP answers 550) and there is **no** record of the drawn pixels.

What this run does and does not establish:

* The 212-dword IB — CLEAR_STATE, the smoke draw state, the console-proven
  register program with patched PGMs, the Step-AB colour bind, the
  `SET_UCONFIG_REG` topology and one `PACKET3_DRAW_INDEX_AUTO` — was
  **submitted** and the process survived the submit and the marker poll.
* The console stayed healthy: no panic/trap/GPU-fault/hang/timeout marker
  in the klog window, the loader still accepted connections on 9021
  afterwards, FTP still served earlier logs, and the Syscore coredump of
  pid 123 completed normally. The GPU itself reported nothing.
* It does **not** establish that the draw rasterized, or even that the
  draw packet was accepted rather than dropped: both show up as the same
  missing evidence. `completed` and the pixel window are unknown.

**Do not re-push this artifact.** Per the repository's one-push-no-retry
rule, a second push was not attempted after the failure. The defect is
fixed in the payload and the faulting logic now lives in a shared, tested
helper (`openagc_pm4_draw_point_scan`, whose bounds are covered by
`test_openagc_gpu`, including the sanitizer-visible case that a window
buffer must be at least `view_w * view_h` words).

## Step AC iterations: five reviewed runs, no fragment yet

The fixed vehicle was re-designed and re-run four more times, each run a
single validated push with a reviewed, cited delta. The console stayed
healthy through all of them: no panic/trap/fault/hang/timeout marker,
loader still accepting, every log retrieved over FTP.

| Run | Change from the previous run | Result | What it established |
| --- | --- | --- | --- |
| AC-2 | Dropped the bare `CLEAR_STATE` (the kernel brackets its own clear-state block with `PREAMBLE_CNTL` and loads ASIC data inside it — `gfx_v10_0.c` — so a user-mode `CLEAR_STATE` would reset state it cannot restore); enabled `VPORT_SCISSOR_ENABLE` and wrote the per-viewport scissor; viewport and guardband as sequences; point size 8 px | `completed=1`, `pixels=0`, `outside=0` | The inherited context is a real driver baseline: `EDGERULE=0xaa959a6a`, `CLIP_CNTL=0x01000000`, `VTX_CNTL=0x2d`, `TILE_STEERING_OVERRIDE=0`, `MODE_CNTL_1=0` — and context state **persists across payload runs** (previous runs' scissors and point size are visible in the next run's baseline) |
| AC-3 | Added `PKT3_NUM_INSTANCES` (0x2F) with count 1 before the draw, plus `IA_MULTI_VGT_PARAM` and `VGT_GS_OUT_PRIM_TYPE` to the readback | `completed=1`, `pixels=0`, `outside=0` | The instance count is not the blocker |
| AC-4 | Added `IA_MULTI_VGT_PARAM = PRIMGROUP_SIZE(127) \| WD_SWITCH_ON_EOP` (cited composition), a **pre-draw readback probe** of the same 22 registers, and a guard-region scan up to the arena end | `completed=1`, `pixels=0`, `outside=0`, `guard=0` | The guard scan rules out writes landing elsewhere in the arena: the CB receives **nothing at all**. The probe also shows that two of the writes do not take: `PA_SC_MODE_CNTL_0.VPORT_SCISSOR_ENABLE` and `IA_MULTI_VGT_PARAM` read back as their previous values, i.e. those registers are write-protected/shadowed for this context |
| AC-5 | Added `CB_COLOR_CONTROL = MODE(CB_NORMAL) \| ROP3(0xcc)` and `CB_BLEND0_CONTROL = 0` | `completed=1`, `pixels=0`, `outside=0`, `guard=0` | See the CB-write gate below: the write was **taken** (the probe reads `00cc0010`) but the draw still produces no fragment |
| AC-6 | Adopted `PS5_Vulkan`'s `PA_SC_MODE_CNTL_0 = 0x23` with its whole one-sample MSAA block, `CB_COLOR_CONTROL = 0x00cc0011` (RB+ off) and an explicit `VGT_GS_OUT_PRIM_TYPE` | `completed=1`, `pixels=0` | The probe verifies every one of those writes; the hardware keeps MSAA plus the viewport scissor and manages the RB-alternation bit itself |
| AC-7 | Read `VGT_PRIMITIVE_TYPE` back through the uconfig aperture (`0xC000+578`) after the draw, and wrote `IA_MULTI_VGT_PARAM` with index 1 | `completed=1`, `pixels=0` | The read completes and returns **0**: the topology write never landed |
| AC-8 | Added the GFX10 CAM workaround bit (`PKT3_RESET_FILTER_CAM_S`) to the uconfig header | `completed=1`, `pixels=0` | Still 0: the CAM bit is not the reason |
| AC-9 | Tried the uconfig write with index 1, 2 and 4, each read back | `completed=1`, `pixels=0` | All four forms leave 0 |
| AC-10 | Built a uconfig table in the arena and issued the **`0x64` register-table load** exactly as `sceAgcDcbSetUcRegistersIndirect` does (payload `addr_lo, addr_hi, 0x80000000, count`; record = offset u16, value u32) | `completed=1`, `pixels=0`, readback 0 | Sony's own encoding does not land either |
| AC-11 | Wrote `SPI_SHADER_USER_DATA_GS_0..3` (SH offsets 140-143) with four distinctive words and read them back through the SH aperture `0x2C00 + offset` | `completed=1`, `pixels=0` | **The NGG vertex stage's user-data block is writable and reads back exactly**, and the SH aperture works: the prerequisite the NGG step needs is now verified. The run's table-load readback was lost to a destination overlap in the encoder, found in the log |
| AC-12 | Fixed that overlap (the probe and user-data readbacks no longer share a word) and re-ran | `completed=1`, `pixels=0`, `set_form=00000000 table_load=00000000`, `gs-user-data=a5a5a500 a5a5a501 a5a5a502 a5a5a503` | Clean confirmation of both: the uconfig space stays read-only for every form including Sony's table load, and the GS user-data block is reachable with exact per-index values |

**The CB colour-write gate (AC-5).** A fresh console context has
`CB_COLOR_CONTROL` = **0**, i.e. `MODE = CB_DISABLE`. Mesa only sets
`MODE(CB_NORMAL)` when the blend target mask is non-empty
(`si_state.c`), and `CB_DISABLE` is 0, so *any* context that has never
drawn has colour writes switched off at the CB. The AC-5 baseline line
records exactly that (`... 00000002 00000000 00000000 00000000 0000002d
00000000 00000000`, last two words = `CB_COLOR_CONTROL`, `CB_BLEND0_CONTROL`)
and the probe line records the fix (`00cc0010`). This is the first
*explained* reason the earlier runs could not produce a pixel, and any
future draw path must carry this register.

**State program verified (AC-6).** With the two public PS5 drivers as
citations — `PS5_Vulkan`'s `ps5vk_draw.c` (offsets "the register
database's MMIO over four, minus 0xA000", `PA_SC_MODE_CNTL_0 = 0x23` for
every draw, `CB_COLOR_CONTROL = 0x00cc0011` with RB+ off, the whole
one-sample MSAA block: `PA_SC_AA_CONFIG = 0xc000`, `DB_EQAA = 0x310000`,
the four `PA_SC_AA_SAMPLE_LOCS_*` at `0x622ae6ae`, `PA_SC_CENTROID_PRIORITY`
`0x32103210` and full coverage masks) and `ps5-opengl`'s own runtime
(`runtime_color_control = 0x00cc0010`, `runtime_target_mask = 0x0000000f`
and point/line defaults `0x00080008`) — the IB now carries all of it, and
the pre-draw probe proves the writes land: `CB_COLOR_CONTROL = 00cc0011`,
`PA_SC_MODE_CNTL_0 = 00000003` (the hardware keeps MSAA plus the viewport
scissor and manages the RB-alternation bit itself), `PA_SC_AA_CONFIG =
0000c000`, `DB_EQAA = 00310000`, `PA_SU_POINT_SIZE = 00400040`. Two writes
never take: `IA_MULTI_VGT_PARAM` (0 plain, 0 with index 1) and the RB
alternation bit. Pixels: still **0**, `outside=0`, `guard=0`.

**The primitive type is not writable from a user submission (AC-7…AC-10).**
The one piece the vehicle could not read back was the `SET_UCONFIG_REG`
write of `VGT_PRIMITIVE_TYPE`. `soc15d.h` gives
`PACKET3_SET_UCONFIG_REG_START = 0xC000` / `_END = 0xC400`, the same shape
as the context aperture `0xA000` that Step X proved, so a `COPY_DATA` read
at `0xC000 + 578` is a way for user mode to check its own topology write.
It completes (`completed=1`, no fault) and returns **0 after every write
form this project has tried**, each followed by its own read back:

| Form | Cite | Readback |
| --- | --- | --- |
| `SET_UCONFIG_REG` (0x79), plain offset 578 | Mesa's GFX10 `radeon_set_uconfig_reg` | `00000000` |
| 0x79 with index 1 / 2 / 4 in the offset word's top nibble | Mesa `radeon_opt_set_uconfig_reg_idx`; `soc15d.h` `PACKET3_SET_UCONFIG_REG_INDEX_TYPE` | `00000000` ×3 |
| 0x64 **register-table load**, 1 record `{578, DI_PT_POINTLIST}`, payload `addr_lo, addr_hi, 0x80000000, 1` | `sceAgcDcbSetUcRegistersIndirect`, word for word from PS5_Vulkan's golden captures and `tools/pm4_decode.py` (`TABLE_LOADS = {0x9f: context, 0x63: sh, 0x64: uconfig}`, records of 8 bytes: offset in the first u16, value in bytes 4..8) | `00000000` |

(All of these also carried the GFX10 CAM workaround bit,
`PKT3_RESET_FILTER_CAM_S`, which the kernel sets for every GFX-IP uconfig
write on GFX10+.) The read path works and the write path does not: **this
submission context cannot program the primitive type**, so the VGT
assembles from `DI_PT_NONE`. That is the whole of the missing
rasterization.

**Why the reference drivers are NGG-only.** Both public PS5 drivers reach
their draws through NGG, and `ps5-opengl`'s own source says so explicitly:
`options->ngg = shader->stage == PSBC_STAGE_VERTEX` and `options.ngg =
true`. In that path the rasterized topology comes from
`VGT_GS_OUT_PRIM_TYPE` — a **context** register, which every write of this
vehicle does reach (the AC-6 probe reads it back) — instead of the legacy
path's uconfig register. Their captured AGC stream also shows where an NGG
vertex stage's user data goes: `SET_SH_REG` at `0xb230`
(`SPI_SHADER_USER_DATA_GS_*`, "the NGG vertex stage"), and `ps5-opengl`
puts the metadata's `ngg_lds_layout` into the user-data dword that PSBC
declares for it. Any NGG step here must therefore write the GS user-data
block (SH offset 140 upwards) with the declared dwords, not the VS block.

**Consequence for the next gate.** The pinned smoke fixtures are legacy
(`stages_en = 0x10000`, no `PRIMGEN_EN`, `ngg_lds_layout: null`), so no
draw the current fixtures can express is reachable from a payload on this
firmware. The next draw attempt needs **NGG-compiled** stage fixtures from
the pinned `opengnm-psbc` build (build-time, manual workflow) and the
`GE_CNTL`/`VGT_SHADER_STAGES_EN` linkage they carry, plus the GS user-data
writes above. Until then rasterization stays unproven:
`hardware_qualified` is false, `gpu_executable` is 0, the pin tables are
empty, and the `openagc_ps5_policy` target stays deny-all.

**What the runs do not establish.** No fragment ever reaches the CB, and
the readback above names the reason: the VGT holds `DI_PT_NONE` because
this context cannot write its primitive type. Nothing here shows a
graphics shader stage executing either — the console-proven shader
execution is the compute path, with CPU-written code and the same PGM
patch encoding — so the VS/PS launch of an NGG pipeline is still
unexercised, and it is the next thing an NGG fixture would test.
Rasterization is therefore still **not** proven; `hardware_qualified`
stays **false**, `gpu_executable` stays 0, the pin tables stay empty, and
the `openagc_ps5_policy` target stays deny-all.

**Next gate (designed, waiting on a build-time artifact).** The same
point-draw vehicle, unchanged, with **NGG** stage fixtures: a vertex/pixel
pair whose PSBC metadata carries `ngg_lds_layout` and a
`VGT_SHADER_STAGES_EN` with `PRIMGEN_EN`, plus the `GE_CNTL` the pinned
compiler emits for an NGG pipeline. With those, the topology comes from
`VGT_GS_OUT_PRIM_TYPE` (writable here, verified by the AC-6 probe) and the
draw no longer needs the uconfig register this context cannot program.
Producing those fixtures requires a run of the manual
`.github/workflows/build-psbc-host.yml` job (or a local build of the
pinned `opengnm-psbc` with NGG enabled) and a review of the resulting
pinned digests; that is the dependency this experiment is now waiting on,
not a console push.

**A second diagnostic is available and safe** if the NGG fixtures are
delayed: the console-proven `store_const`/`store_span` kernel code as the
*vertex* shader of a legacy point draw, with its destination VA in the VS
user-data dwords (`SPI_SHADER_USER_DATA_VS_2/3`). A store landing proves
the graphics VS launch path; nothing landing proves it does not run. The
store kernel writes to the VA it is handed, so the payload must place that
VA in its own arena and must not rely on the ABI's base-vertex slot.

**Artifacts.** `draw_point_eop.elf` (first run,
`ebe7168d…c7abae379`), the AC-2…AC-9 binaries and their logs are archived
under `tools/payload/out/` (gitignored) with the deploy records; the
in-tree payload source, its tests, and the cited state program are the
reviewable artifact.

### Stage 6/7 refuse contracts (fail-closed scaffold)

**Status.** `include/openagc/presentation_refuse_fw940.h` documents
`OPENAGC_NATIVE_TILING_SUPPORTED=0`, `OPENAGC_SCANOUT_USAGE_SUPPORTED=0`,
`OPENAGC_PRESENTATION_SUPPORTED=0`, and
`OPENAGC_VIDEOOUT_EVIDENCE_PIN_COUNT=0`. Host image create already
refuses `NATIVE_OPTIMAL` / `SCANOUT`; `openagc_vk_create_swapchain`
returns `UNSUPPORTED_OPERATION`. No VideoOut path is opened.

## Atlas corrections found while designing Step AB

Designing the full color-bind words required re-reading the public
register map, and two earlier atlas entries were wrong. Both corrections
are recorded here rather than silently edited, because they change what
two recorded console dumps may be claimed to prove.

**Correction 1 — `CB_COLOR0_BASE_EXT`/`ATTRIB2` offsets.** The atlas had
read Mesa's SI-era names `R_028C64_CB_COLOR0_BASE_EXT` and
`R_028C68_CB_COLOR0_ATTRIB2` as offsets 793/794. `gc_10_1_0_offset.h`
gives, all `BASE_IDX=1`:

| Register | Offset | Value |
| --- | --- | --- |
| `mmCB_COLOR0_BASE` | 0x0318 | 792 |
| `mmCB_COLOR0_PITCH` | 0x0319 | 793 (hole on GFX10) |
| `mmCB_COLOR0_SLICE` | 0x031A | 794 (hole on GFX10) |
| `mmCB_COLOR0_VIEW` | 0x031B | 795 |
| `mmCB_COLOR0_INFO` | 0x031C | 796 |
| `mmCB_COLOR0_ATTRIB` | 0x031D | 797 |
| `mmCB_COLOR0_BASE_EXT` | 0x0390 | 912 |
| `mmCB_COLOR0_ATTRIB2` | 0x03B0 | 944 |
| `mmCB_COLOR0_ATTRIB3` | 0x03B8 | 952 |

Mesa's GFX10 emit block agrees: it writes `R_028C60 + 0..13` with zeros in
the PITCH/SLICE slots and puts `ATTRIB2`/`ATTRIB3` at `R_028EC0`/`R_028EE0`
(= 944/952). Consequence for recorded evidence: Step Z's readback index 1
was `CB_COLOR0_PITCH`, not `BASE_EXT`, so its zero is **not** a
`BASE_EXT` round-trip. That claim is withdrawn; the Step-Z dump still
proves BASE and `CB_SHADER_MASK`. `openagc_ib_dump_cb_bind_legacy_base_match`
now scores only those two, and the five index names are corrected in the
atlas.

**Correction 2 — GFX10 color tiling is not a `GB_TILE_MODE` index.**
Step AA was designed against the GFX6-8 rule that a color target needs
`CB_COLOR0_ATTRIB.TILE_MODE_INDEX`. The cited gfx9+ path does not use it
for color: `ac_descriptors.c` (`ac_init_gfx10_cb_surface`,
`ac_set_mutable_cb_surface_fields`) programs
`CB_COLOR0_ATTRIB3.COLOR_SW_MODE(surf->u.gfx9.swizzle_mode)`, where the
swizzle mode is a fixed enum (`addrtypes.h`: `ADDR_SW_LINEAR = 0`,
`ADDR_SW_256B_S = 1`, …), and the GFX10+ depth path likewise programs
`DB_Z_INFO.SW_MODE`. A linear color/depth bind therefore needs no
firmware table. Step AA's negative result stands as recorded, but it was
not on the critical path for a linear bind, and another attempt at that
read is **not** proposed.

## Bounded experiment: full linear color bind + readback (Step AB)

**Question.** Does one FW9.40 IB program all nine gc_10_1_0 color-bind
registers for an owned linear 32×32 RGBA8 UNORM target — BASE,
BASE_EXT (912), VIEW, INFO, ATTRIB, ATTRIB2 (944), ATTRIB3 (952),
`CB_TARGET_MASK`, `CB_SHADER_MASK` — with field compositions taken from
the cited gfx10 driver path, then read every one of them back with the
Step-X-proven absolute `COPY_DATA` and return the programmed words?

**Why it matters.** Stage 5's color-target gate needs owned bind words,
not owned offsets. Steps X/Y/Z owned the readback vehicle and one BASE
value; the dwords that describe format, tiling, slice, dimensions, and
write masks were unowned, and Step AA's attempt to derive the tile mode
from console state is a dead end (Correction 2). Composing those words
from public cites and proving a clean console round-trip is the smallest
step that owns the *set* the later DRAW work needs — without submitting a
DRAW, and therefore without exercising (or risking) the addressing.

**Cites.** Field shifts/masks: `gc_10_1_0_sh_mask.h`. Word composition:
Mesa `ac_descriptors.c` (`ac_init_cb_surface`, `ac_init_gfx10_cb_surface`,
`ac_set_mutable_cb_surface_fields`), `si_state.c`
(`si_set_framebuffer_state` register set, `cb_target_mask` for
`CB_TARGET_MASK`), `ac_formats.c` (`ac_translate_colorswap`). Enum values:
`registers/gfx10.json` (ColorFormat `COLOR_8_8_8_8` = 10, SurfaceEndian
`ENDIAN_NONE` = 0, SurfaceNumber `NUMBER_UNORM` = 0, SurfaceSwap
`SWAP_STD` = 0 / `SWAP_ALT` = 1) and `addrlib/inc/addrtypes.h`
(`ADDR_SW_LINEAR` = 0, `AddrResourceType` 2D = 1). The composed words for
this payload are

| Register | Word | Derivation |
| --- | --- | --- |
| `CB_COLOR0_BASE` | `(va >> 8)` | `ac_set_mutable_cb_surface_fields` |
| `CB_COLOR0_BASE_EXT` | `(va >> 8) >> 32` | same |
| `CB_COLOR0_VIEW` | 0 | `SLICE_START(0) \| SLICE_MAX(0) \| MIP_LEVEL(0)` |
| `CB_COLOR0_INFO` | `0x00028028` | `FORMAT(10)<<2 \| BLEND_CLAMP \| SIMPLE_FLOAT \| SWAP_STD` |
| `CB_COLOR0_ATTRIB` | 0 | `NUM_SAMPLES(0) \| NUM_FRAGMENTS(0)` |
| `CB_COLOR0_ATTRIB2` | `(31 << 14) \| 31` | `MIP0_WIDTH/HEIGHT/MAX_MIP` |
| `CB_COLOR0_ATTRIB3` | `0x01000000` | `RESOURCE_TYPE(2D=1) \| COLOR_SW_MODE(LINEAR=0)` |
| `CB_TARGET_MASK` | `0x0000000F` | `colormask << (4 * 0)` |
| `CB_SHADER_MASK` | `0x0000000F` | smoke-owned RGBA export mask |

BGRA8 differs only in `COMP_SWAP` (`SWAP_ALT`), locked in CTest.

**Why not DRAW / CB capture yet.** No DRAW is submitted, so this is a
*composition* round-trip: it proves the register set, the write path, and
the readback path, and it pins the exact words. Whether a bound linear
target also *addresses* correctly under a draw is a separate question with
its own failure modes (pitch alignment, swizzle semantics) and stays
gated. The pin table stays empty; `evidence_qualified` stays 0.

**Entry conditions.** Steps A–AA proven on `fw=0x9400008`; host composition
and encode locked in CTest; payload ELF-validated; one push, no retries.

**Payload contract (`tools/payload/ctxreg_cb_bind_full_eop.c`).**

May do: open `/dev/gc`; map one arena; place a zeroed 4 KiB, 256-byte
aligned color buffer (32×32 RGBA8 at a 128-byte, i.e. naturally aligned,
linear pitch); submit **one** IB of
`OPENAGC_PM4_CTXREG_CB_BIND_FULL_EOP_WORDS` (=105) dwords — nine
`SET_CONTEXT_REG` pairs, nine absolute `COPY_DATA` reads of the same
offsets, shared EOP+NOP trailer; poll the marker 30s; CPU-compare the nine
readback dwords against the composed expectation;
write `/data/prosperoai/openagc-ib-dump-ctxreg-cb-bind-full.log` (one
`openagc-cb-bind-full-owned:` expect line plus the `openagc-ib-dump:`
block); exit.

Must not do: no DRAW; no shader; no `flat_load`; no CMASK/FMASK/DCC
metadata words; no `CB_COLOR0_PITCH`/`SLICE` write; no second submit; no
retry; no VideoOut; no queue-create/ACB; no malformed ELF.

**Acceptance criteria.** `completed=1`, one log line set, and the nine
readback dwords equal the composed expectation (`match=1`); no
fault/hang/timeout in the live klog window (TCP 3232); loader still
accepting connections afterward.

**Status before push.** Composition, encoder words, `tag=ctxreg-cb-bind-full`
parse, and the fail-closed `openagc_ib_dump_cb_bind_full_match` are locked
in `test_openagc_gpu` (wrong kind, `completed=0`, zero BASE, and each
single-word mismatch refuse). `OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT`
stays 0; `hardware_qualified` stays false. One push, no retries.

**Artifact.** `ctxreg_cb_bind_full_eop.elf`
(`85ccfdf7c40c28a23186db931a124b563c0d5d1253bcdb583c9aa2cfc2e25edb`,
111,336 bytes, ELF-validated, built from this revision with the payload
SDK): exactly one push, klog attached across it, no re-push afterward.

**Observed result (2026-09-25, FW `0x9400008`).** The dump
`/data/prosperoai/openagc-ib-dump-ctxreg-cb-bind-full.log` records

```
openagc-cb-bind-full-owned: base_lo=02000240 base_ext=00000000 view=00000000
  info=00028028 attrib=00000000 attrib2=0007c01f attrib3=01000000
  target_mask=0000000f shader_mask=0000000f match=1
openagc-ib-dump: tag=ctxreg-cb-bind-full fw=0x9400008 completed=1 words=9
ib 02000240 00000000 00000000 00028028 00000000 0007c01f 01000000 0000000f 0000000f
```

for an arena color VA of `0x0000000200024000` (`base_lo` = `va >> 8`,
`base_ext` = 0 at this VA) and a 32×32 RGBA8 view. Every one of the nine
`COPY_DATA` readbacks equals the host composition for that VA and view —
locked as a CTest fixture — so the write path, the readback path, and the
composed words all agree on physical FW9.40. The reviewed klog windows
(live drain on 3232 around the push) contain `GFX(pipe0) Game` for pid
122, `exit_value=0`, and no panic/fault/hang/timeout marker; the loader
still accepted connections on 9021 and FTP 2120 served the log afterward.
This owns the gc_10_1_0 color-bind register *set* and its exact words for
a linear single-sample single-mip RGBA8 target. It does **not** unlock
CB/DB binds as executed state or DRAW: no `DRAW_INDEX_AUTO` was
submitted, so the composition's addressing behaviour is untested, and
`CB_COLOR0_PITCH`/`SLICE`/CMASK/FMASK/DCC base words were deliberately
not written. `hardware_qualified` stays **false**;
`OPENAGC_CB_CAPTURE_EVIDENCE_PIN_COUNT` stays **0**; the
`openagc_ps5_policy` target stays deny-all. The next graphics-adjacent
gate is a bounded attribute-less DRAW against an owned linear target with
its own dead-man design (finite submit, marker, CPU pixel comparison, no
retry); inventing CB capture pins remains out of scope.

## Step AD: the NGG draw and what it settles

### The pinned compiler now runs outside the CI image

`tools/build-pinned-psbc.sh` refuses to run off Ubuntu, so the pinned sources
were built directly: `git clone --branch v0.3.0` of ps5-opengl,
`tools/fetch-sources.py` and `--verify-psbc`, then `make` with a macOS host
copy of `toolchain/opengnm-psbc-host.mak` (clang, `-Dalloca=__builtin_alloca`,
`-include dlfcn.h -D_DARWIN_C_SOURCE=1`, `-DBLAKE3_USE_NEON=0`). Our own
`tools/verify_pinned_psbc.py sources` accepted the checkout afterwards: the
release commit, both patch digests and the Mesa archive SHA-256 all matched,
and `git diff` is clean — no source file was touched. The freshly built
compiler emits `smoke.frag` **byte-identical** to the CI-pinned fixture
(`sha256 28c56f1caab7a771…`), which is the evidence that this host build is
faithful for the fixtures it produced.

### The NGG vertex fixture and its cross-validation

`tools/shaders/smoke.tri.vert` (a viewport-covering triangle placed from
`gl_VertexIndex`) compiled with `--ngg --primitive-type triangle-list` gives
`hardware_stage 3` and a linkage block of `{GE_CNTL 603 = 0x00010080,
VGT_SHADER_STAGES_EN 725 = 0x00012010, SPI_SHADER_USER_VGPR_EN 610 = 0}`, an
ES/GS shader register set at SH 200/201 (PGM) and 138/139 (GS RSRC), and
`ngg_lds_layout {user_data_dword 2, value 512}`. Every one of those numbers
matches a decoded public capture of Sony's own NGG triangle draw
(`golden/c1-triangle` in PS5_Vulkan): the same 11 context offsets in the same
order with the same values, `stages_en = 0x00012010`, and the same SH 200/201
and 138/139 pairs carrying `0x422c0003`, `0x003fffff` and `0x0000ffff`. That
decode also settles two things this repository had wrong: the context table's
records are `{offset u32, value u32}`, and Sony's own draw does **not** put
GE_CNTL in the context table.

### What the console accepted

`tools/payload/draw_point_ngg_eop.c` (IB: `include/openagc/pm4_ngg_draw_fw940.h`,
tables from `tools/payload/gen_ngg_tables.py`) submits the NGG program as one
context table load plus the SH registers, then one `DRAW_INDEX_AUTO`. One push
per run, no retry, log over FTP. The best dump so far reads

```
openagc-ngg: 00012010 00010080 02000400 00000200 00000000 00000000 00000009 00000080 00000001
```

i.e. `VGT_SHADER_STAGES_EN = 0x00012010` (PRIMGEN_EN and ES_STAGE_REAL) and
`GE_CNTL = 0x00010080` both landed and read back, the ES program address is
the uploaded vertex code (`0x02000400` = `vert_code_va >> 8`), the LDS layout
dword reads `0x200` at SH 142, and the three registers that can only come from
the context **table** (`SPI_SHADER_COL_FORMAT = 9`, `SPI_PS_INPUT_ENA = 0x80`,
`GE_NGG_SUBGRP_CNTL = 1` at 0x2d3) all carry the pinned compiler's values — so the
one context table load writes the whole program, GE_CNTL included once it is
routed to the uconfig space. `completed=1`, no panic, no fault, no hang; the
klog shows only `GFX(pipe0) Game` and `exit_value=1`.

Two corrections came out of these runs. The uconfig space is **not**
read-only: GE_CNTL is a uconfig register (Mesa's `mmGE_CNTL 0x225b` with
`BASE_IDX 1`; the context offset 603 is a different register) and the plain
`SET_UCONFIG_REG` wrote it. And the ES PGM readback has to go through the SH
aperture (`0x2C00 + 200`); reading 200 through the context aperture returns
the unrelated context register of that index.

### The topology register still unresolved

`VGT_PRIMITIVE_TYPE` (uconfig index 0x242 = 578) reads back zero after every
form tried: the plain opcode, the indexed opcode radv uses for NGG
(`radeon_set_uconfig_reg_idx(..., idx = 1, ...)`, `SET_UCONFIG_REG_INDEX`
0x7a), the uconfig table load, and a `GRBM_GFX_INDEX = 0xe0000000` broadcast
first (the register Mesa's CAM-bug comment names). Every other register the
pinned compiler's NGG metadata names is now read back on the console with the
compiler's value, so the topology path remains unresolved. With no input
topology the primitive generator has nothing to assemble, which is consistent
with the zero pixels every NGG run has produced. ps5-opengl's runtime never writes this
register either — it hands the topology to `agc.link_shaders(...)`, i.e. the
AGC linker folds it into the linkage — so the next gate is either a linkage
slot that carries the input topology or a uconfig form the loader accepts.
It is not established whether the write is dropped or whether an indexed
uconfig register simply cannot be read back through an unindexed `COPY_DATA`;
both readings fit the dump. Rasterization is therefore still unproven,
`hardware_qualified` stays **false**, and the `openagc_ps5_policy` target
stays deny-all.

### Host correction and one bounded replay after the Step AD captures

Review of the encoded IB found a separate, deterministic error: the payload
sets `state.vertex_count = 3` for `smoke.tri.vert`, but the NGG encoder emitted
`DRAW_INDEX_AUTO` with a literal count of **1**. A triangle list cannot
assemble a triangle from that request, even if the topology register did
land. The encoder now emits `state.vertex_count`, and the host test checks
the packet against the requested count. The encoder also refuses a triangle
list whose count is not a nonzero multiple of three, matching the public
ps5-opengl draw-state contract at
`src/platform/ps5_agc_native_runtime.c` (commit
`6cb291abea32281571c49705735046425cf000fd`). Its native runtime hands
the primitive type to `sceAgcLinkShaders` rather than writing the topology
register itself.

On 2026-09-26, the probe log was readable and `/data/libkernel-dump.log`
still reported `fw=0x9400008`. One rebuilt SDK ELF passed
`validate_elf.py` (110,544 bytes; SHA-256
`ba2aededbc20090d2ae758ec337cf0f45dd36d9f22189b26920092ffe07488b8`)
and was pushed once. Live klog recorded the payload as pid 183, a graphics
client, then `exit_value=1`, with no panic, GPU fault, GPU hang, or ring
timeout marker in that window. FTP log retrieval reported `completed=1`,
`pixels=0`, `outside=0`, `guard=0`, `match=0`, and the same zero readback
for `VGT_PRIMITIVE_TYPE`. This is a negative rasterization result for the
corrected three-vertex draw; no retry was made. The next work is to compare
the linker-produced register tables and packet sequence against the public
native runtime. Neither the topology readback nor native Vulkan/OpenGL is
qualified by this run, and PS5 policy remains deny-all.

The public PS5_Vulkan C1 triangle capture (commit `3a6f00df`, region 0) now
gives exact comparison records: 34 linker context entries at `0x5000/0x5100`,
three uconfig entries at `0x6000`, and 16 COLOR0 entries at `0x0400`. The
shared host frontend accepts those tables, and both Vulkan and OpenGL encode
them identically in the equivalence test with a synthetic color address. The
capture has `CB_COLOR0_INFO=0x00008828` and
`CB_COLOR0_ATTRIB3=0x4dc6c000`, whereas the Step-AB linear bind used
`0x00028028` and `0x01000000`. The capture's target is tiled, so those words
are a comparison lead, not proof that either difference caused the FW9.40
zero-pixel result. A FW9.40 linker capture, a confirmed pixel, and a completed
fence are still missing; draw execution remains refused.

## What the two public PS5 drivers do that this submission does not

Reviewed on 2026-09-26 against PS5_Vulkan (`mihawk-99/PS5_Vulkan`, `main`:
`driver/ps5vk_draw.c`, `driver/ps5vk_queue.c`, `docs/HARDWARE_FINDINGS.md`,
`docs/PROBE_MILESTONES.md`) and ps5-opengl (`blackbearreloaded/ps5-opengl`,
`6cb291ab`). Their frames render exact pixels on a console, so the
differences below are the candidate causes of the Step AL/AM stall.

**Their draw stream.** PS5_Vulkan's per-draw words are one indirect context
table - the 16 `CB_COLOR0` registers, 15 viewport/guard-band/scissor/
target-mask registers, the AGC linker's 34 context records and both shaders'
context registers - then the linked uniform table, one SH table and
`DRAW_INDEX_AUTO`. That is the shape this repository already encodes (Steps
AD-AL), and their register *values* for a single-sample target match ours
exactly: `PA_SC_MODE_CNTL_0` 0x23, `PA_SC_AA_CONFIG` 0xc000, `DB_EQAA`
0x310000, the 4x sample locations, the centroid priorities, the full AA
masks, `PA_SU_VTX_CNTL` 0x2d, `CB_COLOR_CONTROL` 0x00cc0011 and
`CB_TARGET_MASK` 0xf. The one value they differ on is the guard band: they
write 1.0 into all four `PA_CL_GB_*_ADJ` registers where this repository
writes what Mesa's `ac_compute_guardband` produces (4094.0 clip, 1.0
discard). **Writing 1.0 changed nothing** (one push, `completed=0`), so a
guard band that lets the rasterizer walk 4096x the viewport is not the
stall.

**Their submission.** `driver/ps5vk_queue.c` submits with
`sceAgcDriverSubmitDcb` and passes `sceAgcSuspendPoint` (its own comments:
"submit with sceAgcDriverSubmitDcb and pass sceAgcSuspendPoint", a suspend
point taking ~125 us), and its description carries a flag byte whose effect
varies with the submission mode. Every OpenAGC payload, and every
console-proven step in this repository, submits with the raw `0xC0108102`
ioctl and a 16-byte `{queue_type, num_cbs, cb_array}` description instead.
Compute and copy submissions through that raw path execute and retire, and
so do draws without the viewport transform, but no draw with fragments has
ever retired through it.

**Their wait-until-safe packet.** AGC's own stream (the C1 capture, decoded
in Steps AG-AI) begins with `PKT3 0x93`, and PS5_Vulkan emits the same
packet form (`ps5vk_marker_wait_words`: control `0x06000113`, address, value,
mask `0xffffffff`, poll interval `0x40`) after its colour-buffer barrier. Its
`HARDWARE_FINDINGS.md` records the one thing that packet must get right: "the
wait-until-safe packet must name the buffer the frame renders into". OpenAGC
submissions carry no such packet at all: every payload starts with state
writes. That is the cleanest structural difference left between a working
stream and ours.

**Next instruments, in order.** (1) Submit a raster draw through the AGC
driver path (`sceAgcDriverSubmitDcb` plus `sceAgcSuspendPoint`, resolved the
way ps5-opengl resolves its AGC entry points) instead of the raw ioctl, with
the same IB. (2) Add the `PKT3 0x93` wait-until-safe packet in the recorded
form, naming the target. (3) Only then revisit register values: with the
multisample block, the rasterizer state and the colour control already
matching a working driver word for word, what is left is packets and
submission, not registers.

## Step AM: NGG stalls with viewport transform; legacy retires

Three more single pushes, each with a 1x1 viewport and `PA_CL_VTE_CNTL` set:

| Variant | Change | Result |
| --- | --- | --- |
| Step AM | point primitive (`DI_PT_POINTLIST` + `VGT_GS_OUT_PRIM_TYPE` 0), one vertex | `completed=0` |
| Step AM | legacy (non-NGG) program, pinned `smoke.vert`/`smoke.frag` fixtures, point | **`completed=1`**, `pixels=0` |
| Step AM | `SPI_PS_INPUT_ENA`/`_ADDR` = 0 (interpolation modes off) | `completed=0` |

Together with the AC-era record these two paths now say something precise:

* The **legacy** path retires without a pixel, and it has done so
  across AC-2..AC-12, AC-11's user-data probe, and now Step AM - all of which
  depend on `VGT_PRIMITIVE_TYPE` (uconfig index 0x242), the register that
  has read back 0 in every write form tried. A masked readback remains
  possible, so the absence of a pixel does not prove the VGT never assembled.
* The **NGG** path stalls when viewport transform is enabled for either a
  triangle or a point. This is consistent with the draw advancing into
  primitive or fragment work, but EOP noncompletion alone does not prove a
  fragment was generated or locate the stall. Disabling colour writes, depth
  and stencil, multisampling, binning and interpolation modes does not make
  that draw retire.

Artifacts: `draw_raster_point.elf`
(`1d05452ccbad22e8eee65da9df418fe50c35950cec9e14cf35ec04055363c89e`),
`draw_legacy_eop.elf`
(`e245dd849549c9719e57ef17a29652bc69633727381e3b53828b66e539fb0785`,
log `/data/prosperoai/openagc-ib-dump-draw-legacy.log`) and
`draw_raster_pixel_nointerp.elf`
(`443963fafb2564329213968563271bf3f428ff12e162db285cea81ec195c7f4a`),
one push each, klog attached, console healthy, no fault/hang marker.

**Status.** `hardware_qualified` stays **false**, `gpu_executable` stays 0,
`OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the PS5 policy still refuses
`OPENAGC_PS5_CAP_DRAW`. The next instrument has to bound the unknown the
stall sits behind, and the two that do not risk a stray write are a capture
from this firmware and the public native runtime's own draw replayed with
its own shaders.

## Step AL: the stall is the draw itself, not the state writes

Four more single pushes, all on the 1x1 viewport with `PA_CL_VTE_CNTL` written:

| Variant | Added | Result |
| --- | --- | --- |
| Step AL | `CB_COLOR_CONTROL = CB_DISABLE` (pixel shader runs, no colour write) | `completed=0` |
| Step AL | `SPI_PS_INPUT_CNTL_0..31` from the capture (identity table, now written by every draw) | `completed=0` |
| Step AL | `DB_DEPTH_CONTROL = 0` (depth and stencil off, colour writes normal) | `completed=0` |
| Step AL | the same state with the PS-input and gate blocks moved *before* the pre-draw readbacks | `completed=0`, readbacks land |

The last row is the informative one. The readbacks are ordinary CP memory
writes that have landed in every stalled run, so moving the suspect state in
front of them turns them into a progress marker: with `PA_CL_VTE_CNTL`,
`DB_DEPTH_CONTROL`, `PA_SC_MODE_CNTL_0`, `PA_SC_AA_CONFIG`, `DB_EQAA` and the
32 pixel-shader input controls all written *before* the readbacks, and the
readbacks still carrying the pinned values, those writes demonstrably
executed. The submission therefore stops in the remaining tail - colour
bind, `NUM_INSTANCES`, `DRAW_INDEX_AUTO`, EOP. `CB_DISABLE` (no colour write)
and a depth/stencil-off, single-sample, non-binning pipeline still stop it.
The readbacks bound the stalled packet range, but do not prove which GPU
stage waits inside the draw.

**Status.** `hardware_qualified` stays **false**, `gpu_executable` stays 0,
`OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the PS5 policy still refuses
`OPENAGC_PS5_CAP_DRAW`. The two remaining instruments are the legacy
(non-NGG) vertex path with the viewport transform on - the fragment path is
shared, the vertex path is not - and a capture taken on *this* firmware
rather than the public one.

## Step AK: a one-pixel draw still stalls, with binning and MSAA off

Three more single pushes, all with `PA_CL_VTE_CNTL` written and the payload's
viewport reduced to 1x1 (`OPENAGC_VIEW_W/H=1`, so the screen, viewport and
generic scissors are 1x1 as well):

| Variant | Added | Result |
| --- | --- | --- |
| Step AK | nothing | `completed=0`, `wait=30s`, window `1x1` |
| Step AK | `PA_SC_BINNER_CNTL_0 = 0` (binning off, now a scalar state pair) | `completed=0` |
| Step AK | `PA_SC_MODE_CNTL_0 = 2`, `PA_SC_AA_CONFIG = 0`, `DB_EQAA = 0` (MSAA off, gate bits 8-10) | `completed=0` |

So the stall is not fragment volume, not binning and not the multisample
path: one pixel's worth of coverage is enough to stop the submission. What
the three runs do establish is *where* the pipeline stops moving. With
`PA_CL_VTE_CNTL` clear, the vertex positions are not transformed, the
triangle lands outside every scissor and the IB retires with no fragment;
with it set, the triangle covers the 1x1 viewport and the IB never retires.
That is the first real difference in behaviour any OpenAGC draw has produced,
and it points at the fragment path after rasterization (pixel shader launch
or the RB write to a host-mapped target) rather than at the vertex stage.

**Note for the next run.** A payload that stalls writes its log only after
its 30-second deadline, so a deploy that fetches the log 15 s after the push
reads the *previous* run's file. Every stalled run here was recovered by
re-fetching after the deadline; `--wait 40` is the correct fetch setting for
this shape.

**Status.** `hardware_qualified` stays **false**, `gpu_executable` stays 0,
`OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the PS5 policy still refuses
`OPENAGC_PS5_CAP_DRAW`. Candidate next instruments: the legacy (non-NGG)
vertex path with the viewport transform on, and a variant that distinguishes
pixel shader launch from the RB write (for example the console-proven compute
store kernel as the pixel stage's export target, or a target address the
console's own driver has already written).

## Step AJ: the fragment-gate bisect lands on PA_CL_VTE_CNTL

The gate block was made selectable (`openagc_raster_gpu_draw.gate_mask`, the
payload's `OPENAGC_GATE_MASK`) and bisected with single pushes:

| Variant | Gate mask | Result |
| --- | --- | --- |
| Step AI | all seven | `completed=0`, `wait=30s` |
| Step AJ | `PA_CL_VTE_CNTL` only (1) | `completed=0`, `wait=30s` |
| Step AJ | `PA_CL_VTE_CNTL` + the capture's 16-record bind | `completed=0`, `wait=30s` |

The minimal tested trigger is `PA_CL_VTE_CNTL` alone - the six
viewport-transform enables. The draw stops retiring with either of the two
tested color binds when viewport transform is enabled. That is consistent
with work reaching rasterization, but EOP noncompletion does not prove a
fragment was generated. Everything else in
the block (depth control, the two prim filters, NaN/Inf control, the
over-rasterization control, `PA_CL_NGG_CNTL`) is not the trigger. Artifacts:
`draw_raster_vte.elf` (`177fee95fc63ed5ef30609c793bd4d6a232066ef9e3c66ebd661a37de8311421`)
and `draw_raster_vte_tiled.elf`
(`59e4ab50345be5fcdf6d2cd08ad6a96ea5abeb248582a8121861495ad19c89f8`), one
push each, klog attached, no fault/hang marker and the loader still serving
after both.

Two notes for the next run. First, the dump of the second variant shows the
console's context had been reset (a fresh boot: `CB_COLOR_CONTROL` 0, the
screen scissor 0..16384, `PA_CL_CLIP_CNTL` 0x00090000), so the earlier
"parameters persist across runs" observation applies to a boot session, not
across one. Second, the stall is a property of the *submission*: the console
stayed healthy and the next push was accepted, so a stalled EOP is a usable
negative signal here rather than a hazard.

**Status.** `hardware_qualified` stays **false**, `gpu_executable` stays 0,
`OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, and the PS5 policy still refuses
`OPENAGC_PS5_CAP_DRAW`. The next instrument is a one-pixel draw (1x1
viewport and scissors) to separate "fragment generation" from "many
fragments" and, after that, the legacy VS path.

## Steps AG-AI: the public capture settles the packet sequence, and the fragment gates change behaviour

The public C1 capture (`mihawk-99/PS5_Vulkan`, `golden/c1-triangle/c1-triangle-1.json`,
commit `3a6f00df`, fetched and decoded rather than quoted) gives the whole
submission AGC itself emits for one triangle: `SET_UCONFIG 0x342`
(`SQ_THREAD_TRACE_USERDATA_2`, a trace marker), two `SET_UCONFIG` writes,
then per draw a **context table of 90 records**, a **uconfig table of 3**,
a **SH table of 10**, `SET_SH` user data at `0x8c` (GS) and `0x0c` (PS), and
**`DRAW_INDEX_AUTO` with `DI_SRC_SEL_AUTO_INDEX`** - the same initiator
OpenAGC has used all along, and the same three uconfig records OpenAGC
composes (`{0x25b,0x10080}`, `{0x262,0}`, `{0x242,4}` byte for byte).
That retires the indexed-draw hypothesis before it needed a console run.

Diffing the capture's 90 context records against the OpenAGC state found
three registers OpenAGC never wrote and the capture does:

* `PA_SC_GENERIC_SCISSOR_TL/BR` (0x090/0x091) - TL `0x80000000`, BR
  `(4096,4096)` for a full-screen target. A context that inherits the
  reset value (0,0)-(0,0) discards every fragment.
* `PA_SC_VPORT_ZMIN_0/ZMAX_0` (0x0b4/0x0b5) - 0 and 1.0.
* `CB_COLOR0_DCC_CONTROL` in the capture's 16-record target set (Step AF).

`include/openagc/pm4_context_regs_gfx10.h` now names all of these, the
shared encoder writes the generic scissor and the depth range, and the
offsets were re-verified against Mesa `gfx10.json` (the atlas had
0x090/0x091 and 0x0b4/0x0b5 unassigned; 0x094/0x095 are the per-viewport
scissor pair, as the atlas already had).

**Step AH (2026-09-26).** `draw_raster_eop.elf` rebuilt with the generic
scissor and the depth range
(`8ed91a920f46db168ca4b2fa5701f98a1b3ef77d5c6e3e67274c0b8b896f9490`),
pushed once: `completed=1`, `pixels=0`, `guard=0` - those two writes alone
do not change the zero-fragment result.

**Step AI (2026-09-26).** The same payload with a second block of
registers a driver always initializes because each one can drop fragments
silently: `PA_CL_VTE_CNTL` (the six viewport-transform enables),
`DB_DEPTH_CONTROL` (no depth test or write, no colour-write override),
`PA_SU_PRIM_FILTER_CNTL`, `PA_SU_SMALL_PRIM_FILTER_CNTL`,
`PA_CL_NANINF_CNTL`, `PA_SU_OVER_RASTERIZATION_CNTL` and `PA_CL_NGG_CNTL`,
all neutral (`bcf5bbaf97f04cc025d1f036b4ca6ee3f65d3d9b9b80acde8553aa2555536c5b`).
Pushed once: **the IB no longer retires** - `completed=0`, `wait=30s`, the
program readbacks (which precede the draw and the EOP) still land with the
pinned values, and the live klog shows the payload as a graphics client
with `exit_value=1` and no panic/fault/hang marker in the drained window.
Writing that block therefore moves the pipeline: one of those registers is
on the critical path, and the block needs bisecting before anything is
claimed about it. The console was left to the operator at this point; no
retry was made.

**Status.** `hardware_qualified` stays **false**, `gpu_executable` stays 0,
`OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the pin tables stay empty, and
the PS5 policy still refuses `OPENAGC_PS5_CAP_DRAW`.

## Step AE run 2 and Step AF: the AGC draw retires with two target binds

Two further single pushes were made, each one artifact, no retry.

**Step AE run 2 (2026-09-26).** The operator closed the console-side
application that had left the graphics ring in a timeout, and the same
AGC-shaped draw was pushed once
(`draw_raster_eop.elf`, 110,536 bytes, SHA-256
`12bbb43d7fe68f8fc1f19c790669c32a956d2e3770491e6e2d223fef0fc45d2d`). The
context query was accepted, the IB **submitted and retired**
(`completed=1`, exit_value=1), the console stayed clean, and the dump is

```
openagc-draw-raster-owned: color_va=0000000200044000 rect=8,8,8x8
  pixels=0 outside=0 guard=0 value=00000000 wait=0s match=0
openagc-probe: 00cc0011 00000000 00000003 00000000 00000002 0000c000 00310000 00400040
openagc-ngg: 00012010 00010080 02000400 00000200 00000000 00000000 00000009 00000080 00000001
```

Every register of the program landed and reads back with the pinned
compiler's value - `VGT_SHADER_STAGES_EN` 0x12010, **GE_CNTL 0x10080 read
through the uconfig aperture** (so the AGC-shaped uconfig table does load),
ES PGM 0x02000400 = `vert_code_va >> 8`, the LDS layout dword 0x200,
`SPI_SHADER_COL_FORMAT` 9, `SPI_PS_INPUT_ENA` 0x80,
`GE_NGG_SUBGRP_CNTL` 1 at 0x2d3 - and `VGT_PRIMITIVE_TYPE` still reads 0 through
that aperture, exactly as in Step AD. No pixel, and `guard=0` says nothing
was written anywhere in the arena.

**Step AF (2026-09-26).** A suspected structural difference from
public driver is the colour bind: OpenAGC's nine-word set binds a *linear*
8_8_8_8 surface, while the public native runtime
(`append_target_state`) and the C1 capture both write **16 COLOR0 records**
with `CB_COLOR0_DCC_CONTROL = 0x48` (0x31e is DCC_CONTROL, not an ATTRIB
alias: Mesa `gfx10.json`), `COLOR_SW_MODE 27`, `FMASK_SW_MODE 24`,
`RESOURCE_LEVEL 1` and the CMASK/DCC pipe-aligned bits. One IB therefore
drew the same triangle twice: pass A with the nine-word linear bind into a
32x32 target, pass B with the capture's 16-record bind into a 256B-pitch
64x256 target, both inside the payload's own arena, with the guard counting
nonzero dwords in every arena region that holds no target.

`draw_raster_ab_eop.elf` (111,328 bytes, SHA-256
`e68d11571dd0b91c14804aa3d38b5d0b7b3aa904813b3de3def99bf29480d8cf`) was
pushed once. `ib=659` dwords, two draws, one EOP; the dump records
`completed=1` and

```
openagc-draw-raster-ab-owned: a_pixels=0 a_outside=0 a_value=00000000
  b_nonzero=0 b_first=00000000 guard=0 ib=659 wait=0s match=0
openagc-cb-capture: 798=00000048 ... 944=000fc0ff 952=4dc6c000
```

so **neither bind received anything**: not the linear one, not the
capture-shaped one, and nothing landed anywhere else in the arena. Both
draws retired (the EOP fired once, exit_value=1, no fault/hang/timeout
marker in the live klog window; the loader still accepted connections on
9021 afterwards).

**Corrected after a source audit.** Step AF zero-initialized `draw` and never
set `draw.gate_mask`, so the shared encoder skipped the fragment-gate block.
It did not write `CB_DISABLE` after the initial normal-color state. Step AF
establishes that both draws retired with different target register sets, but
does not isolate either bind from the omitted fragment-gate state. The
original conclusion that the color bind had been eliminated as a cause is
withdrawn. `VGT_PRIMITIVE_TYPE` still read 0 in every tested write form,
but that readback alone does not locate the zero-pixel failure.
The A/B payload source now selects the full gate for a future build;
the Step AF ELF and result above remain the historical run.

**What remains open.** Whether that register write is dropped or only its
readback is masked is still not settled, and the next reviewed delta has
two candidates: (a) load the uconfig records one per table load (and with
`VGT_PRIMITIVE_TYPE` first) to test whether the loader consumes the whole
table; (b) reproduce the native runtime's *indexed* draw path
(`set_index_size` -> `VGT_INDEX_TYPE` with index type 2, `set_index_buffer`,
`set_index_count`, `draw_index`) instead of the auto-indexed
`DRAW_INDEX_AUTO` every OpenAGC draw has used so far. Neither is a retry of
anything above. `hardware_qualified` stays **false**, `gpu_executable`
stays 0, `OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the pin tables stay
empty, and the PS5 policy still refuses `OPENAGC_PS5_CAP_DRAW`.

## Step AE: the shared rasterizer's AGC-shaped draw (host locked, console refused at setup)

### What changed, and why

The public native runtime at the pinned release commit
(`blackbearreloaded/ps5-opengl`, `6cb291abea32281571c49705735046425cf000fd`,
`src/platform/ps5_agc_native_runtime.c`) settles the packet shape the Step-AD
vehicle got wrong. `sceAgcLinkShaders` writes 34 context records and **three
uconfig records**; `set_linkage_uc_state` then loads exactly those three with
`set_uc` (`sceAgcDcbSetUcRegistersIndirect`), writes the shader registers with
`set_sh`, the NGG user data at SH `0x8c` with `set_sh_direct`, and draws. The
C1 capture's uconfig records are `{0x25b GE_CNTL}`, `{0x262
SPI_SHADER_USER_VGPR_EN}`, `{0x242 VGT_PRIMITIVE_TYPE=4}` - and no
`VGT_SHADER_STAGES_EN`, which is a *context* record (`0x2d5`).

Two deterministic defects followed for Step AD:

1. its context table mixed the uconfig linkage records into the context table,
   so `SPI_SHADER_USER_VGPR_EN` (610) was written to an unrelated context
   register, and
2. `VGT_PRIMITIVE_TYPE` was written with the **indexed** packet and a
   `GRBM_GFX_INDEX` broadcast. Mesa `si_emit_draw_registers` (fetched from
   Mesa `main`) uses the **plain** `radeon_set_uconfig_reg(R_030908_...)` for
   `GFX_VERSION >= GFX10` and reserves the indexed form for GFX7-GFX9;
   `soc15d.h` gives `PACKET3_SET_UCONFIG_REG_INDEX_TYPE` as an index *type*
   (2), not a generic index.

The shared rasterizer (`include/openagc/raster.h`, `openagc_raster_encode_draw`)
now composes the whole draw: the proven scalar state, viewport/guardband/scissor
sequences, the context table with the vertex records, the one context linkage
record and the pixel records, an AGC-shaped uconfig table
`{GE_CNTL, USER_VGPR_EN, VGT_PRIMITIVE_TYPE = this draw's topology}`, the ES/PS
shader registers with the PGMs patched, the GS user-data dwords, the Step-AB
nine-word linear color bind, one `DRAW_INDEX_AUTO`, and the shared EOP trailer.
A uconfig index inside a context table is refused, a linkage block without
`GE_CNTL` is refused, and a triangle list that is not a multiple of three
vertices is refused. `tests/test_openagc_raster` locks the packet walk, the two
table contents, every refusal, and the `OPENAGC_RASTER_GPU_QUALIFIED` pin (0).

### The single push

`tools/payload/draw_raster_eop.c` (built from this revision, SDK mode,
`validate_elf.py` clean, 110,536 bytes, SHA-256
`9d508d71bfbc5f81cbdb8ac77fe327c31b2fb3853c7987b26154a62e5a35516b`) was pushed
**once** on 2026-09-26 through `tools/payload/deploy.py` with the live klog
attached. The payload opened `/dev/gc`, became a graphics client
(`### GFX(pipe0) Game ... pid:202`), and then the `0xC004812E` context query
was refused:

```
openagc-draw-raster: context query refused
### Warning: own_gfx_ring timeout pid=202.
# process pid=202, payload.elf calls exit() exit_value=0.
```

The payload's own fail-closed path returned before building the command buffer,
so **nothing was submitted**: no IB, no draw, no packet. The loader still
accepted connections afterwards and the log was retrieved over FTP 2120. Per
the one-push rule this run was not repeated; the graphics-ring timeout is a
console-side state the operator owns, not a payload defect, and the payload
cannot be scored as a rasterization result either way.

### What this does and does not establish

* It establishes the AGC-shaped IB host-side: the composed draw is a single
  448-dword submission whose uconfig table is byte-for-byte the shape the
  public native runtime loads and whose values are the pinned fixtures'
  (locked by CTest).
* It does **not** establish any console result for the draw: the submit never
  happened, so the zero-pixel question from Steps AC/AD is unchanged.
* `hardware_qualified` stays **false**, `gpu_executable` stays 0,
  `OPENAGC_RASTER_GPU_QUALIFIED` stays **0**, the evidence pin tables stay
  empty, and the next draw attempt needs a console whose graphics ring is
  healthy plus a fresh reviewed delta - not a retry of this artifact.
* The PS5 policy target is no longer a blanket deny: it publishes the
  qualification record for the one observed firmware
  (`openagc_ps5_policy_qualification`, `openagc_ps5_policy_require`) with the
  capabilities Steps B-AD proved, and names the draw as still refused. Nothing
  in that record is new evidence.

## Step AN: corrected ESGS ring register and normal color writes

Mesa's `gfx10.json` maps context index `0x2ab` to
`VGT_ESGS_RING_ITEMSIZE`; index `0x2d3` is `GE_NGG_SUBGRP_CNTL`. The latter
was mislabeled in the earlier NGG probes. A decoded PS5_Vulkan C1 draw
records `0x2ab=1`, while the pinned NGG fixture records `0x2ab=0`. The
shared rasterizer's optional fragment-gate block had a `CB_DISABLE` default;
it now uses `0x00cc0011`, and the host test verifies no later disable write
when that block is selected. The historical Step AN payload left that block
unselected, as documented below.

The Step AN payload copies the fixture's vertex context table and changes
only its `0x2ab` record to 1. The shared encoder then emits the draw and
reads `0x2ab` before the draw. One SDK ELF passed `validate_elf.py` (110,328
bytes, SHA-256 `a9e45a049cbb5a135fab8142907ccda88a39c752627f868438dc72a7b9a31b1c`)
and was pushed once to port 9021 on FW `0x9400008`; the log was retrieved
from `/data/prosperoai/openagc-ib-dump-draw-ring.log` on FTP port 2120.
The payload became a graphics client (pid 97), submitted the draw, and exited
without a klog GPU fault, panic, hang, or timeout marker in the captured
window. Its readbacks show `CB_COLOR_CONTROL=00cc0011`, the NGG program
registers, and `VGT_ESGS_RING_ITEMSIZE=1`. The acceptance result was
`completed=0`, `wait=30s`, `pixels=0`, `outside=0`, `guard=0`.

This establishes the correct ring-size register write and readback. It does
not establish a completed GPU draw or a pixel. A later source audit found
that Step AN's `OPENAGC_GATE_MASK` appeared only in the log: `draw` was
zero-initialized and its `gate_mask` was never assigned. The encoder therefore
skipped the fragment-gate block. Its `CB_NORMAL` readback came from the
initial scalar state. The payload now assigns the gate mask, but this new
binary has not been pushed. No second push was made.
The corrected ELF passes offline validation (110,328 bytes, SHA-256
`ed408447bc774d3021c9facdfe2c1eddfbf16e0735880554ee468bd67615ecd3`);
it is not hardware evidence.
The remaining fragment-stage stall needs a new instrument or a native FW9.40
capture before enabling `OPENAGC_PS5_CAP_DRAW`; both Vulkan and OpenGL keep
using the shared frontend core, and `gpu_executable` remains 0.
