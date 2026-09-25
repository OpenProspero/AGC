#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
#
# Build an OpenAGC evidence payload without the official Windows/Linux SDK
# toolchain: a host clang with an ELF linker and the ps5-payload-sdk sysroot
# and linker script are enough. Nothing from the console is required.
#
# usage: build.sh <source.c> <output.elf> [sdk|freestanding]
#
#   sdk           default: prospero-clang + SDK crt/libc (expects main())
#   freestanding  syscall-only payload with its own _start; links no SDK libs
#
# environment:
#   PS5_PAYLOAD_SDK  path to the ps5-payload-sdk checkout (required)
#   Prefer the SDK wrappers under $PS5_PAYLOAD_SDK/bin.

set -euo pipefail

SOURCE="${1:-}"
OUTPUT="${2:-}"
MODE="${3:-sdk}"

if [ -z "$SOURCE" ] || [ -z "$OUTPUT" ]; then
    echo "usage: $0 <source.c> <output.elf> [sdk|freestanding]" >&2
    exit 2
fi
if [ -z "${PS5_PAYLOAD_SDK:-}" ]; then
    echo "PS5_PAYLOAD_SDK must point at the payload SDK" >&2
    exit 2
fi

PROSPERO_CLANG="$PS5_PAYLOAD_SDK/bin/prospero-clang"
CLANG="${HOST_CLANG:-clang}"
LLD="${HOST_LLD:-ld.lld}"
TARGET="${PAYLOAD_TARGET:-x86_64-unknown-freebsd13.0}"
COMMON="--target=$TARGET -ffreestanding -fno-stack-protector -fno-plt -fPIE -O2"

# The SDK wrappers look up llvm-config; Homebrew llvm is not on PATH by default.
if [ -z "${LLVM_CONFIG:-}" ]; then
    for candidate in \
        /opt/homebrew/opt/llvm/bin/llvm-config \
        /usr/local/opt/llvm/bin/llvm-config \
        llvm-config; do
        if command -v "$candidate" >/dev/null 2>&1 || [ -x "$candidate" ]; then
            export LLVM_CONFIG="$candidate"
            break
        fi
    done
fi
export PATH="$PS5_PAYLOAD_SDK/bin:${PATH}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
INCLUDE="-I$ROOT/include"

case "$MODE" in
sdk)
    if [ -x "$PROSPERO_CLANG" ]; then
        "$PROSPERO_CLANG" -Wall -O2 $INCLUDE -o "$OUTPUT" "$SOURCE"
    else
        "$CLANG" $COMMON $INCLUDE -isystem "$PS5_PAYLOAD_SDK/target/include" \
            -c "$SOURCE" -o "$OUTPUT.o"
        "$LLD" -m elf_x86_64 -pie -e _start \
            -T "$PS5_PAYLOAD_SDK/ldscripts/elf_x86_64.x" \
            --eh-frame-hdr -z max-page-size=0x4000 --hash-style=gnu \
            -o "$OUTPUT" "$PS5_PAYLOAD_SDK/target/lib/crt1.o" "$OUTPUT.o" \
            -L "$PS5_PAYLOAD_SDK/target/lib" -lc -lkernel_web -lSceLibcInternal \
            -lSceNet
    fi
    ;;
freestanding)
    "$CLANG" $COMMON $INCLUDE -nostdlib -c "$SOURCE" -o "$OUTPUT.o"
    "$LLD" -m elf_x86_64 -pie -e _start \
        -T "$PS5_PAYLOAD_SDK/ldscripts/elf_x86_64.x" \
        --eh-frame-hdr -z max-page-size=0x4000 --hash-style=gnu \
        -o "$OUTPUT" "$OUTPUT.o"
    ;;
*)
    echo "unknown mode: $MODE" >&2
    exit 2
    ;;
esac

python3 "$(dirname "$0")/validate_elf.py" "$OUTPUT"
echo "built $OUTPUT ($(wc -c <"$OUTPUT") bytes)"
