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
`openagc_ib_dump_cb_bind_owned_base_match` is fail-closed on zero
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

### Stage 6/7 refuse contracts (fail-closed scaffold)

**Status.** `include/openagc/presentation_refuse_fw940.h` documents
`OPENAGC_NATIVE_TILING_SUPPORTED=0`, `OPENAGC_SCANOUT_USAGE_SUPPORTED=0`,
`OPENAGC_PRESENTATION_SUPPORTED=0`, and
`OPENAGC_VIDEOOUT_EVIDENCE_PIN_COUNT=0`. Host image create already
refuses `NATIVE_OPTIMAL` / `SCANOUT`; `openagc_vk_create_swapchain`
returns `UNSUPPORTED_OPERATION`. No VideoOut path is opened.
