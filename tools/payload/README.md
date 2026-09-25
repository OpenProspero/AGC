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
7. `write_data_wide_rows.c` (Step G, 16-dword × multi-row) only after
   Step F. One push, no retries. Draw/CB remain out of scope.
8. `dma_write_eop.c` (Step H, DMA + WRITE_DATA + EOP) only after Steps
   B and G. One push, no retries. Draw/CB remain out of scope.
9. `store_span.c` (Step I, 8-lane compute flat_store) only after Step C.
   One push, no retries. Draw/CB remain out of scope.
10. `store_span2.c` (Step J, dual store_span) only after Step I. One
    push, no retries. Draw/CB remain out of scope.
11. `store_span4.c` (Step K, four store_span / 128 bytes) only after
    Step J. One push, no retries. Draw/CB remain out of scope.
12. `store_span8.c` (Step L, SPAN_MAX=8 / 256 bytes) only after Step K.
    One push, no retries. Draw/CB remain out of scope.
13. `write_data_max_rows.c` (Step M, MAX_ROWS=8 × 16 dwords / 512 bytes)
    only after Step G. One push, no retries. Draw/CB remain out of scope.
14. `write_data_grid.c` (Step N, 8×2 × 16 dwords / 1024 bytes) only after
    Step M. One push, no retries. Draw/CB remain out of scope.
15. `set_sh_gfx_eop.c` (Step O, graphics-bank SET_SH ×4 + EOP) only after
    Step N and host PGM-patch encoding is locked. One push, no retries.
    No SET_CONTEXT, no DRAW, no CB/DB.
16. `set_context_eop.c` (Step P, SET_CONTEXT ×3 + EOP) only after Step O
    and host `openagc_pm4_encode_graphics_context_eop` is locked. One
    push, no retries. smoke.vert `context_registers` only — no linkage,
    no SET_SH, no DRAW, no CB/DB.
17. `set_context_sh_eop.c` (Step Q, SET_CONTEXT ×3 + SET_SH ×4 + EOP)
    only after Steps O–P and host
    `openagc_pm4_encode_graphics_context_sh_eop` is locked. One push, no
    retries. Same proven pairs; no linkage, no DRAW, no CB/DB.
18. `set_context_linkage_eop.c` (Step R, linkage SET_CONTEXT ×3 + EOP)
    only after Step Q and host `openagc_pm4_encode_graphics_linkage_eop`
    is locked. One push, no retries. smoke.vert linkage pairs only —
    no context_registers, no SET_SH, no DRAW, no CB/DB.
19. `set_context_sh_linkage_eop.c` (Step S, ctx + SH + linkage + EOP)
    only after Steps Q–R and host
    `openagc_pm4_encode_graphics_context_sh_linkage_eop` is locked. One
    push, no retries. Full host-aligned register program; no DRAW, no
    CB/DB.
20. `set_context_sh_frag_eop.c` (Step T, smoke.frag ctx + SH + EOP)
    only after Step S and host
    `openagc_pm4_encode_graphics_context_sh_eop` is locked for the
    smoke.frag (9+4) pairs. One push, no retries. No linkage, no DRAW,
    no CB/DB.
21. `set_context_sh_vert_frag_eop.c` (Step U, vert Step S + frag Step T
    + EOP) only after Steps S–T and host
    `openagc_pm4_encode_graphics_vert_frag_eop` is locked. One push, no
    retries. Full host-aligned vert+frag register program; no DRAW, no
    CB/DB.
22. `ib_dump_step_u_eop.c` (Step V, dump Step-U IB words to
    `openagc-ib-dump-step-u.log`) only after Step U and host
    `openagc_ib_dump_parse` is locked. One push, no retries. Same
    register IB as Step U; no DRAW, no CB/DB; dump is not a CB pin.
23. `ctxreg_cb_dump_eop.c` (Step W, COPY_DATA readback of CB_COLOR0_* +
    masks → `openagc-ib-dump-ctxreg-cb.log`) only after host
    `openagc_pm4_encode_copy_data_cb_probe_eop` and `tag=ctxreg-cb` parse
    are locked. One push, no retries. Reads public offsets only; does
    not SET CB binds or invent values; dump is not a CB pin.
    **Console-negative** (`completed=0`, poison) — do not retry.
24. `ctxreg_abs_dump_eop.c` (Step X, absolute `CONTEXT_REG_START+offset`
    COPY_DATA src → `openagc-ib-dump-ctxreg-abs.log`) only after host
    `openagc_pm4_encode_copy_data_cb_probe_abs_eop` and `tag=ctxreg-abs`
    parse are locked. Distinct from Step W; one push, no retries. Same
    probe set; no invent CB binds.
25. `ctxreg_rt_dump_eop.c` (Step Y, SET_CONTEXT smoke-owned RT pairs +
    absolute COPY_DATA readback → `openagc-ib-dump-ctxreg-rt.log`) only
    after host `openagc_pm4_encode_ctxreg_rt_abs_eop` and `tag=ctxreg-rt`
    parse are locked. Distinct from Step X (writes before read);
    one push, no retries. No COLOR_BASE invent; dump is not a CB pin.
26. `ctxreg_cb_bind_eop.c` (Step Z, SET owned `CB_COLOR0_BASE`(+EXT) from
    an arena VA + smoke `CB_SHADER_MASK`, then absolute COPY_DATA CB
    probe → `openagc-ib-dump-ctxreg-cb-bind.log`) only after host
    `openagc_pm4_encode_ctxreg_cb_bind_abs_eop`,
    `openagc_ib_dump_cb_bind_owned_base_match`, and `tag=ctxreg-cb-bind`
    parse are locked. One push, no retries. No INFO/ATTRIB/VIEW/
    TARGET_MASK SET, no DRAW; the dump is not a CB_BIND pin.
    **Console result:** `completed=1`, readback BASE `02000240` =
    `color_va >> 8`, `BASE_EXT=0`, `CB_SHADER_MASK=15` — owned BASE
    round-trip proven; remaining bind dwords still unowned.
27. `mmio_tilemode_dump_eop.c` (Step AA, read-only absolute COPY_DATA of
    `GB_ADDR_CONFIG` + `GB_TILE_MODE0..31` → `openagc-ib-dump-mmio-tilemode.log`)
    only after host `openagc_pm4_encode_mmio_tilemode_probe_eop`,
    `openagc_ib_dump_mmio_tilemode_lookup`, and `tag=mmio-tilemode`
    parse are locked. One push, no retries. No register write, no
    SET_CONTEXT, no DRAW; the dump owns the tile-mode table, not a CB
    bind. A `completed=0` timeout is a negative result — do not re-push.
    **Console result:** `completed=0` with all 33 dwords poison
    (`cccccccc`) — the `0x13xx` GB register block is not readable
    through this COPY_DATA encoding; tile-mode table stays unowned.
    Do not retry this encoding.

## Toolchain result (2026-09-25)

| Payload | Mode | Result |
| --- | --- | --- |
| `probe.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,888 bytes; one nc push executed, log line written, exit 0 |
| `copy_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,208 bytes; one nc push: `submit=ok completed=1 matched=1 marker=1`, exit 0 |
| `store_const.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,208 bytes; one nc push: `submit=ok completed=1 matched=1 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data_clear.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 dwords=16 value=a5a5a5a5 marker=1`, exit 0 |
| `write_data_rows.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 rows=2 dwords=8 pitch=64 marker=1`, exit 0 |
| `write_data_wide_rows.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 rows=2 dwords=16 pitch=128 marker=1`, exit 0 |
| `dma_write_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,208 bytes; one nc push: `submit=ok completed=1 matched=1 dma_bytes=64 write_dwords=4 marker=1`, exit 0 |
| `store_span.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 lanes=8 marker=1`, exit 0 |
| `store_span2.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 lanes=16 marker=1`, exit 0 |
| `store_span4.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 spans=4 lanes=32 marker=1`, exit 0 |
| `store_span8.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,256 bytes; one nc push: `submit=ok completed=1 matched=1 spans=8 lanes=64 marker=1`, exit 0 |
| `write_data_max_rows.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 rows=8 dwords=16 pitch=64 marker=1`, exit 0 |
| `write_data_grid.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 matched=1 rows=8 cols=2 dwords=16 pitch=128 marker=1`, exit 0 |
| `set_sh_gfx_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,080 bytes; one nc push: `submit=ok completed=1 pairs=4 words=36 marker=1` |
| `set_context_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 111,216 bytes; one nc push: `submit=ok completed=1 pairs=3 words=33 marker=1` |
| `set_context_sh_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 109,936 bytes; one nc push: `submit=ok completed=1 ctx=3 sh=4 words=45 marker=1` |
| `set_context_linkage_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,040 bytes; one nc push: `submit=ok completed=1 pairs=3 words=33 marker=1` |
| `set_context_sh_linkage_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,088 bytes; one nc push: `submit=ok completed=1 ctx=3 sh=4 link=3 words=54 marker=1` |
| `set_context_sh_frag_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,088 bytes; one nc push: `submit=ok completed=1 ctx=9 sh=4 words=63 marker=1` |
| `set_context_sh_vert_frag_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,144 bytes; one nc push: `submit=ok completed=1 v_ctx=3 v_sh=4 link=3 f_ctx=9 f_sh=4 words=93 marker=1` |
| `ib_dump_step_u_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,240 bytes; one nc push: `openagc-ib-dump tag=step-u completed=1 words=93`; host parse `REGISTER_EOP` `evidence_qualified=0` |
| `ctxreg_cb_dump_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,000 bytes; one nc push: `openagc-ib-dump tag=ctxreg-cb completed=0 words=8` (poison `cccccccc`); host parse `CTXREG_CB` `evidence_qualified=0`; relative COPY_DATA src not console-proven — do not retry |
| `ctxreg_abs_dump_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,152 bytes; one nc push: `openagc-ib-dump tag=ctxreg-abs completed=1 words=8` (`00000000`×6 + `ffffffff`×2); host parse `CTXREG_ABS` `evidence_qualified=0`; absolute COPY_DATA proven; not a CB_BIND pin |
| `ctxreg_rt_dump_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,152 bytes; one nc push: `openagc-ib-dump tag=ctxreg-rt completed=1 words=6` (`00000009 00000080 00000080 00008000 00000010 0000000f`); host parse `CTXREG_RT` `evidence_qualified=0`; SET_CONTEXT→abs COPY_DATA round-trip proven; not a CB_BIND pin |
| `ctxreg_cb_bind_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,152 bytes; one nc push: `openagc-ib-dump tag=ctxreg-cb-bind completed=1 words=8` (`02000240 00000000 00000000 00000000 00000000 00000000 ffffffff 0000000f`); owned BASE/BASE_EXT round-trip proven; host parse `CTXREG_CB_BIND` `evidence_qualified=0`; not a CB_BIND pin |
| `mmio_tilemode_dump_eop.c` | sdk (`prospero-clang`) | Builds: FreeBSD PIE, 110,184 bytes; one nc push: `openagc-ib-dump tag=mmio-tilemode completed=0 words=33` (all poison `cccccccc`); GB `0x13xx` MMIO not readable via this COPY_DATA encoding — do not retry; tile-mode table stays unowned |

Firmware identity on the console: `fw=0x9400008` (9.40) from
`/data/libkernel-dump.log`.
