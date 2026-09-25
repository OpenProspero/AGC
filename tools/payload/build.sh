#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
#
# Build an OpenAGC evidence payload without the official Windows/Linux SDK
# toolchain: a host clang with an ELF linker and the ps5-payload-sdk sysroot
# and linker script are enough. Nothing from the console is required.
#
# usage: build.sh <source.c> <output.elf> [freestanding|sdk]
#
#   freestanding  syscall-only payload with its own _start; links no SDK libs
#   sdk           links the SDK crt/libc/libkernel stub and expects main()
#
# environment:
#   PS5_PAYLOAD_SDK  path to the ps5-payload-sdk checkout (required)
#   HOST_CLANG       clang to use (default: clang from PATH)
#   HOST_LLD         ld.lld to use (default: ld.lld from PATH)
#   PAYLOAD_TARGET   ELF target triple (default x86_64-sie-ps5; use
#                    x86_64-unknown-freebsd13.0 if the host clang rejects it)

set -euo pipefail

SOURCE="${1:-}"
OUTPUT="${2:-}"
MODE="${3:-freestanding}"

if [ -z "$SOURCE" ] || [ -z "$OUTPUT" ]; then
    echo "usage: $0 <source.c> <output.elf> [freestanding|sdk]" >&2
    exit 2
fi
if [ -z "${PS5_PAYLOAD_SDK:-}" ]; then
    echo "PS5_PAYLOAD_SDK must point at the payload SDK" >&2
    exit 2
fi

CLANG="${HOST_CLANG:-clang}"
LLD="${HOST_LLD:-ld.lld}"
TARGET="${PAYLOAD_TARGET:-x86_64-sie-ps5}"
COMMON="--target=$TARGET -ffreestanding -fno-stack-protector -fno-plt -fPIE -O2"

case "$MODE" in
freestanding)
    "$CLANG" $COMMON -nostdlib -c "$SOURCE" -o "$OUTPUT.o"
    "$LLD" -m elf_x86_64 -pie -T "$PS5_PAYLOAD_SDK/ldscripts/elf_x86_64.x" \
        --eh-frame-hdr -z max-page-size=0x4000 --hash-style=gnu \
        -o "$OUTPUT" "$OUTPUT.o"
    ;;
sdk)
    "$CLANG" $COMMON -isystem "$PS5_PAYLOAD_SDK/target/include" \
        -c "$SOURCE" -o "$OUTPUT.o"
    "$LLD" -m elf_x86_64 -pie -T "$PS5_PAYLOAD_SDK/ldscripts/elf_x86_64.x" \
        --eh-frame-hdr -z max-page-size=0x4000 --hash-style=gnu \
        -o "$OUTPUT" "$PS5_PAYLOAD_SDK/target/lib/crt1.o" "$OUTPUT.o" \
        -L "$PS5_PAYLOAD_SDK/target/lib" -lc -lkernel_web -lSceLibcInternal \
        -lSceNet
    ;;
*)
    echo "unknown mode: $MODE" >&2
    exit 2
    ;;
esac

python3 "$(dirname "$0")/validate_elf.py" "$OUTPUT"
echo "built $OUTPUT ($(wc -c <"$OUTPUT") bytes)"
