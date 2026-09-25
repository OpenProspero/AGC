#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Check the two host-side invariants that keep OpenAGC one driver.

1. Shared frontend core: `openagc_vulkan.c` and `openagc_opengl.c` may
   only call the shared core (`openagc_frontend_*`) plus the small
   lifecycle/intake allowlist below; anything else is a bypass.
2. Fail-closed PS5 policy: every externally linked `openagc_*` symbol
   declared in `include/openagc/*.h` has a definition in
   `openagc_ps5_policy.c`. Header `static inline` helpers are not
   external symbols and are excluded; undefined imports are a separate
   check (`llvm-nm -u` on the policy archive).

usage: check_host_invariants.py [repo_root]
"""

import glob
import os
import re
import sys

CALL = re.compile(r"\b(openagc_[a-z0-9_]+)\s*\(")
DECL = re.compile(r"^\s*(?:extern\s+)?[A-Za-z_][A-Za-z0-9_ \t*]*\b(openagc_[a-z0-9_]+)\s*\(", re.M)
STATIC = re.compile(r"^\s*static\b[^;{]*\b(openagc_[a-z0-9_]+)\s*\(", re.M)
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//.*")

# Frontend -> shared-core boundary: device/context lifecycle and shader
# artifact intake are shared API, not backend work the core already offers.
FRONTEND_ALLOWED = {
    "openagc_context_create",
    "openagc_context_destroy",
    "openagc_gpu_device_create",
    "openagc_gpu_device_destroy",
    "openagc_shader_artifact_intake_host",
    "openagc_shader_artifact_destroy",
}


def strip_comments(text: str) -> str:
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def read(path: str) -> str:
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def check_shared_core(root: str) -> list[str]:
    problems: list[str] = []
    for source, own_prefix in (("src/openagc_vulkan.c", "openagc_vk_"),
                               ("src/openagc_opengl.c", "openagc_gl_")):
        text = strip_comments(read(os.path.join(root, source)))
        for name in sorted(set(CALL.findall(text))):
            if name.startswith("openagc_frontend_") or name.startswith(own_prefix):
                continue
            if name in FRONTEND_ALLOWED:
                continue
            problems.append(f"{source} calls {name} instead of the shared core")
    return problems


def check_policy_symbols(root: str) -> list[str]:
    declared: set[str] = set()
    inline_only: set[str] = set()
    for path in sorted(glob.glob(os.path.join(root, "include/openagc/*.h"))):
        text = strip_comments(read(path))
        inline_only |= set(STATIC.findall(text))
        declared |= set(DECL.findall(text))

    policy = strip_comments(read(os.path.join(root, "src/openagc_ps5_policy.c")))
    defined = set(STATIC.findall(policy)) | set(DECL.findall(policy))

    external = declared - inline_only
    print(f"policy symbols: extern={len(external)} header_inline={len(inline_only)} "
          f"policy_defined={len(defined)}")
    return [f"openagc_ps5_policy.c does not define {name}"
            for name in sorted(external - defined)]


def main() -> int:
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    core_problems = check_shared_core(root)
    problems = core_problems + check_policy_symbols(root)

    print(f"shared core: {'ok' if not core_problems else 'bypass found'}")
    for problem in problems:
        print(f"  FAIL {problem}")
    print(f"host invariants: {'ok' if not problems else 'FAILED'}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
