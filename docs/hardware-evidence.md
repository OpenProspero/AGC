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

## Bounded design: OpenAGC copy and EOP proof (not yet run)

**Question.** Does OpenAGC's own sequence, as locked down by
`tests/test_openagc_gpu.c` (seven `IT_DMA_DATA` dwords, eight
action-based `IT_RELEASE_MEM` dwords, two NOP dwords), execute on
physical FW9.40 with real addresses, and does its EOP marker fire?
`tools/payload/copy_eop.c` implements exactly this run: the locked-down
words, the operator's proven 16-dword NOP trailer, one submit, a
monotonic 30-second deadline, a CPU byte comparison, and one log line.

**Why it matters.** The repository's PM4 vectors are field-derived
fixtures. A positive, independently checked result turns them into
observed behaviour and is the prerequisite for any later draw or
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
fill the source with a known pattern; submit **one** copy of OpenAGC's
exact 17 words; poll for the fence with a monotonic clock and a hard
59-second deadline; read back the destination; compare on the CPU;
write one log line with the result; exit.

Must not do: no `flat_load`; no queue-create or ring paths; no
ACB/const-IB dispatch; no VideoOut call; no kernel memory write outside
its own allocations; no indirect buffer; no second submission after a
failure; no automatic retry; no background thread.

**Acceptance criteria.** Destination bytes equal the source pattern
(CPU check), the EOP/fence completion is observed at least once, the
run produced exactly one log line, a klog captured after the run shows
no GPU fault/hang/timeout marker, the console UI remained responsive
(operator check), and no packet other than the 17 words was submitted.

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
* Payload build path: done and verified (`tools/payload/`, both modes,
  with a fail-closed ELF validator on build and before any push).
* Push path: loader was listening again on 2026-09-25. One validated
  `probe.elf` (34,008 bytes) was pushed to port 9021. The loader closed
  with an empty response and logged `socksrv.c:233:recv: Invalid argument`.
  `/data/prosperoai/openagc-probe.log` was not created. The loader kept
  accepting connections. No second push was sent. `copy_eop.c` was not run.
* Bounded copy/EOP experiment: designed and implemented
  (`tools/payload/copy_eop.c`), **not run**; it follows step A and the
  entry conditions above.
* Everything downstream (draw packets, native tiling, presentation,
  Vulkan/OpenGL on console) remains gated and unapproved, and the
  `openagc_ps5_policy` target stays deny-all.
