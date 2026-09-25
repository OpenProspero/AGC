#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Push one evidence payload to the console loader and fetch its log.

This is the bounded deploy step described in docs/hardware-evidence.md.
It performs exactly one push, never retries, and reads the log once after
a fixed wait. It does not restart the loader.

usage: deploy.py <payload.elf> <host> [--push-port 9021] [--ftp-port 2120]
                 [--log /data/prosperoai/openagc-probe.log] [--wait 5]
                 [--klog-port 3232] [--klog-seconds 10]
"""

import argparse
import base64
import os
import socket
import sys
import time
import urllib.request
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from validate_elf import validate  # noqa: E402 - local module by design


def push(path: str, host: str, port: int) -> int:
    # Same contract as prospero-deploy / netcat: write the ELF bytes and close.
    with open(path, "rb") as handle:
        payload = handle.read()
    print(f"push: {len(payload)} bytes to {host}:{port}")
    with socket.create_connection((host, port), timeout=30) as sock:
        sock.sendall(payload)
    print("push: closed")
    return 0


def fetch_log(host: str, port: int, remote: str) -> bytes:
    url = f"ftp://{host}:{port}{remote}"
    request = urllib.request.Request(url)
    password = "anonymous:test"
    import base64

    token = base64.b64encode(password.encode()).decode()
    request.add_header("Authorization", f"Basic {token}")
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return response.read()
    except Exception as exc:  # noqa: BLE001 - reported verbatim
        print(f"log: fetch failed: {exc}")
        return b""


def read_klog(host: str, port: int, seconds: float, sock: Optional[socket.socket] = None) -> bytes:
    """Drain the live klog stream, optionally on an already-open socket."""
    if sock is None:
        print(f"klog: connecting to {host}:{port}")
        try:
            sock = socket.create_connection((host, port), timeout=5)
        except OSError as exc:
            print(f"klog: connect failed: {exc}")
            return b""
    print(f"klog: draining for {seconds:.0f}s")
    chunks = []
    deadline = time.monotonic() + seconds
    try:
        sock.settimeout(1.0)
        while time.monotonic() < deadline:
            try:
                data = sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not data:
                break
            chunks.append(data)
    finally:
        sock.close()
    return b"".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("payload")
    parser.add_argument("host")
    parser.add_argument("--push-port", type=int, default=9021)
    parser.add_argument("--ftp-port", type=int, default=2120)
    parser.add_argument("--log", default="/data/prosperoai/openagc-probe.log")
    parser.add_argument("--wait", type=float, default=5.0)
    parser.add_argument("--klog-port", type=int, default=3232)
    parser.add_argument("--klog-seconds", type=float, default=10.0)
    parser.add_argument("--klog-out")
    args = parser.parse_args()

    if not os.path.exists(args.payload):
        print(f"missing payload: {args.payload}", file=sys.stderr)
        return 2

    ok, reason = validate(args.payload)
    print(f"validate: {reason}")
    if not ok:
        print("refusing to push an artifact that failed validation", file=sys.stderr)
        return 1

    # Hold the live kernel log open across the push so the loader's own
    # messages are captured even if it never answers on the client socket.
    klog_sock = None
    try:
        klog_sock = socket.create_connection((args.host, args.klog_port), timeout=5)
        print(f"klog: attached to {args.host}:{args.klog_port}")
    except OSError as exc:
        print(f"klog: attach failed: {exc}")

    push(args.payload, args.host, args.push_port)
    print(f"wait: {args.wait:.0f}s")
    time.sleep(args.wait)
    body = fetch_log(args.host, args.ftp_port, args.log)
    if body:
        print(f"log: {args.log}")
        sys.stdout.write(body.decode("utf-8", "replace"))
    else:
        print(f"log: {args.log} empty or missing")
    klog = read_klog(args.host, args.klog_port, args.klog_seconds, klog_sock)
    if klog:
        print(f"klog: {len(klog)} bytes")
        if args.klog_out:
            with open(args.klog_out, "wb") as handle:
                handle.write(klog)
    return 0


if __name__ == "__main__":
    sys.exit(main())
