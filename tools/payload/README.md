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
   (`copy_eop.c`) is considered. Step C (`store_const.c`) requires A and
   B on the same firmware identity.

## Safety contract

A payload here may: open a log file, write one line; open `/dev/gc`;
submit **one** bounded IB inside its own direct-memory arena; poll its
own marker with a finite deadline; and compare bytes on the CPU.

Step B is copy+EOP only. Step C is one 1-thread compute store-const
(no loads) plus the same EOP trailer.

A payload here must not: create queues or rings, issue `flat_load`,
touch VideoOut, write kernel memory, use indirect buffers, retry after
a failure, or run a second submission.

## Build without the official toolchain

A host clang plus an ELF linker and the `ps5-payload-sdk` sysroot are
enough; no Windows or Linux SDK install is required. Verified with
Homebrew clang 22 and `ld.lld` on macOS:

```console
$ export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
$ export HOST_CLANG=$(brew --prefix llvm)/bin/clang
$ export HOST_LLD=$(brew --prefix lld)/bin/ld.lld
$ ./build.sh probe.c probe.elf sdk
$ ./build.sh copy_eop.c copy_eop.elf sdk
$ ./build.sh store_const.c store_const.elf sdk
```

`freestanding` payloads carry their own `_start` and link no SDK
libraries; `sdk` payloads use the SDK crt/libc and the libkernel stub,
which is what `/dev/gc` payloads need for direct-memory allocation. If the
host clang rejects `x86_64-sie-ps5`, set
`PAYLOAD_TARGET=x86_64-unknown-freebsd13.0`.

## Deploy

```console
$ export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
$ export LLVM_CONFIG=$(brew --prefix llvm)/bin/llvm-config
$ ./build.sh probe.c probe.elf sdk
$ nc <console-ip> 9021 < probe.elf
```

`prospero-deploy -h <ip> -p 9021 probe.elf` is equivalent (socat). One
push, no retries. Fetch `/data/prosperoai/openagc-probe.log` over FTP
2120 and attach klog on 3232 around the push when diagnosing.

## Order of operations

1. `probe.c` first. It proves the byte path with no device access at
   all; if its log line does not appear, nothing else may run.
2. `copy_eop.c` only after step A is proven, with firmware identity
   captured from the same boot session.
3. `store_const.c` only after steps A and B on the same firmware
   identity, and after the Step C design in
   `docs/hardware-evidence.md` has been reviewed.
4. `write_data.c` only after steps A–C on the same firmware identity,
   with encoding locked in `pm4_write_fw940.h`. One push, no retries.
5. `write_data_clear.c` (Step E, 16-dword clear tile) only after Step D
   on the same firmware. One push, no retries.
6. `write_data_rows.c` (Step F, multi-row WRITE_DATA) only after Step E.
   One push, no retries. Draw/CB remain out of scope.

## Toolchain result (2026-09-25)

| Payload | Mode | Result |
| --- | --- | --- |
| `probe.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,888 bytes; one nc push executed, log line written, exit 0 |
| `copy_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,208 bytes; one nc push: `submit=ok completed=1 matched=1 marker=1`, exit 0 |
| `store_const.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,208 bytes; one nc push: `submit=ok completed=1 matched=1 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data_clear.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 dwords=16 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data_rows.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 rows=2 dwords=8 pitch=64 marker=1`, exit 0 |

Firmware identity on the console: `fw=0x9400008` (9.40) from
`/data/libkernel-dump.log`.
