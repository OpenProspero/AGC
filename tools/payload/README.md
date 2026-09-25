# Evidence payloads

These payloads exist to gather the firmware evidence described in
[../../docs/hardware-evidence.md](../../docs/hardware-evidence.md). They
are **not** part of the driver: they link no OpenAGC library code, the
host library never appears in a console image, and the PS5 policy
target stays deny-all.

## Hard rules

1. **Never push an artifact that fails `validate_elf.py`.** `build.sh`
   runs it after linking and `deploy.py` runs it again before opening a
   socket; a failure aborts the push. Do not bypass either check.
2. **Never send malformed or experimental input** to the loader. A
   deliberately malformed 64-byte ELF was pushed once as a "diagnostic"
   and aborted `elfldr.elf` on the console (fatal signal, `copyin ...
   nonsleeping lock`), forcing a console restart. The operator's console
   is not a test bench for loader robustness.
3. One push per run, no retries, no loops.
4. Attach the live kernel log (`--klog-port 3232`) *around* the push so
   the loader's own messages are captured even when it never answers on
   the client socket.
5. Step A (`probe.c`) must produce its log line before step B
   (`copy_eop.c`) is considered.

## Safety contract

A payload here may: open a log file, write one line, and (step B only)
open `/dev/gc`, copy 64 bytes inside its own direct-memory arena, poll
its own marker with a finite deadline, and compare bytes on the CPU.

A payload here must not: create queues or rings, dispatch compute,
issue `flat_load`, touch VideoOut, write kernel memory, use indirect
buffers, retry after a failure, or run a second submission.

## Build without the official toolchain

A host clang plus an ELF linker and the `ps5-payload-sdk` sysroot are
enough; no Windows or Linux SDK install is required. Verified with
Homebrew clang 22 and `ld.lld` on macOS:

```console
$ export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
$ export HOST_CLANG=$(brew --prefix llvm)/bin/clang
$ export HOST_LLD=$(brew --prefix lld)/bin/ld.lld
$ ./build.sh probe.c probe.elf freestanding
$ ./build.sh copy_eop.c copy_eop.elf sdk
```

`freestanding` payloads carry their own `_start` and link no SDK
libraries; `sdk` payloads use the SDK crt/libc and the libkernel stub,
which is what `copy_eop.c` needs for direct-memory allocation. If the
host clang rejects `x86_64-sie-ps5`, set
`PAYLOAD_TARGET=x86_64-unknown-freebsd13.0`.

## Deploy

```console
$ python3 deploy.py probe.elf <console-ip> --log /data/prosperoai/openagc-probe.log
```

One push, one wait, one log read, one bounded klog read. It never
retries and never restarts the loader.

## Order of operations

1. `probe.c` first. It proves the byte path with no device access at
   all; if its log line does not appear, nothing else may run.
2. `copy_eop.c` only after step A is proven, with firmware identity
   captured from the same boot session, and after the bounded design in
   `docs/hardware-evidence.md` has been reviewed.

## Toolchain result (2026-09-24)

| Payload | Mode | Result |
| --- | --- | --- |
| `probe.c` | freestanding | Builds: static PIE, 34,008 bytes |
| `copy_eop.c` | sdk | Builds: PIE with `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`, 140 relative relocations |

The `probe.elf` artifact was also uploaded to the console over FTP and
read back with a matching SHA-256, so the artifact path is proven; the
push path is not yet proven because the loader stopped accepting
connections during that session (see `docs/hardware-evidence.md`).
