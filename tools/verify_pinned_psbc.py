#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Verify an external pinned PSBC build and package it only after real compilation."""

import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


RELEASE_COMMIT = "6cb291abea32281571c49705735046425cf000fd"
PSBC_COMMIT = "a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92"
OPENGNM_COMMIT = "4b295ca54c82c83acf308d1c646a2dfa9ae57350"
SPIRV_COMMIT = "0d25db97cb9b8f725e4c95e4553001710e7fc39d"
VULKAN_COMMIT = "b51f6b865c18fc5b33990d12f75e8dfd672cede6"
PATCH_SHA256 = "a7c73aef3f1d51c47107976b03e68de5e99f044a97601a4adf3b89144fb2e848"
PATCHED_TREE = "a27cbecc8c11761da04af6b8e089905b93252e65"
MESA_VERSION = "26.2.0"
MESA_SHA256 = "efd4bb08cdb7c365a812cd4e6c9202ab55b2f22cdcd13c7d6c4f9647b799a4ef"
METADATA_VERSION = 14
MAX_OPENAGC_CODE_BYTES = 65536


class VerificationError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise VerificationError(message)


def output(*args, cwd=None):
    return subprocess.run(
        args, cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def verify_release(upstream):
    require(upstream.is_dir(), f"release checkout missing: {upstream}")
    require((upstream / ".git").exists(), "release git checkout missing")
    require(output("git", "-C", str(upstream), "rev-parse", "HEAD") == RELEASE_COMMIT,
            "ps5-opengl v0.3.0 release commit mismatch")
    manifest_path = upstream / "dependencies.json"
    require(manifest_path.is_file(), "release dependency manifest missing")
    pins = json.loads(manifest_path.read_text(encoding="utf-8"))
    repos = pins["repositories"]
    expected = {
        "opengnm-psbc": PSBC_COMMIT,
        "opengnm": OPENGNM_COMMIT,
        "SPIRV-Headers": SPIRV_COMMIT,
        "Vulkan-Headers": VULKAN_COMMIT,
    }
    for name, revision in expected.items():
        require(repos[name]["revision"] == revision,
                f"release dependency pin changed: {name}")
    require(pins["mesa"]["version"] == MESA_VERSION, "Mesa version changed")
    require(pins["mesa"]["sha256"] == MESA_SHA256, "Mesa SHA-256 pin changed")
    require(pins["mesa"]["url"] == f"https://archive.mesa3d.org/mesa-{MESA_VERSION}.tar.xz",
            "Mesa source URL changed")
    require(pins["psbc_patch"]["sha256"] == PATCH_SHA256,
            "PSBC patch SHA-256 pin changed")
    require(pins["psbc_patch"]["patched_tree"] == PATCHED_TREE,
            "PSBC patched tree pin changed")
    patch = upstream / pins["psbc_patch"]["path"]
    require(patch.is_file() and digest(patch) == PATCH_SHA256,
            "tagged PSBC patch file SHA-256 mismatch")
    require((upstream / "LICENSE").is_file() and (upstream / "LICENSES/Mesa").is_dir(),
            "release license bundle missing")
    return pins


def verify_sources(upstream):
    verify_release(upstream)
    sources = upstream / "third_party"
    expected = {
        "opengnm-psbc": PSBC_COMMIT,
        "opengnm": OPENGNM_COMMIT,
        "SPIRV-Headers": SPIRV_COMMIT,
        "Vulkan-Headers": VULKAN_COMMIT,
    }
    for name, revision in expected.items():
        checkout = sources / name
        require(checkout.is_dir(), f"source checkout missing: {name}")
        require(output("git", "-C", str(checkout), "rev-parse", "HEAD") == revision,
                f"source checkout revision mismatch: {name}")
        require(subprocess.run(
            ["git", "-C", str(checkout), "diff", "--exit-code", "--quiet"],
            check=False
        ).returncode == 0, f"tracked source modified after checkout: {name}")
    psbc = sources / "opengnm-psbc"
    require(output("git", "-C", str(psbc), "write-tree") == PATCHED_TREE,
            "patched PSBC source tree mismatch")
    listing = subprocess.run(
        ["git", "-C", str(psbc), "ls-tree", "-r", "-z", PATCHED_TREE],
        check=True, capture_output=True
    ).stdout
    require(bool(listing), "patched PSBC tree listing empty")
    tree_listing_sha256 = hashlib.sha256(listing).hexdigest()

    mesa_archive = sources / f"mesa-{MESA_VERSION}.tar.xz"
    require(mesa_archive.is_file() and digest(mesa_archive) == MESA_SHA256,
            "full Mesa source archive SHA-256 mismatch")
    mesa_version_file = sources / f"mesa-{MESA_VERSION}/VERSION"
    require(mesa_version_file.is_file() and
            mesa_version_file.read_text(encoding="utf-8").strip() == MESA_VERSION,
            "Mesa extracted source version mismatch")
    require((psbc / "LICENSE").is_file() and (sources / "opengnm/LICENSE").is_file(),
            "compiler source license text missing")
    return {
        "ps5_opengl_release_commit": RELEASE_COMMIT,
        "opengnm_psbc_revision": PSBC_COMMIT,
        "opengnm_revision": OPENGNM_COMMIT,
        "spirv_headers_revision": SPIRV_COMMIT,
        "vulkan_headers_revision": VULKAN_COMMIT,
        "psbc_patch_sha256": PATCH_SHA256,
        "psbc_patched_tree": PATCHED_TREE,
        "psbc_patched_tree_listing_sha256": tree_listing_sha256,
        "mesa_version": MESA_VERSION,
        "mesa_archive_sha256": MESA_SHA256,
    }


def check_smoke_metadata(metadata, stage, code_size, header_version):
    expected_stage = {"vert": 1, "frag": 5}[stage]
    require(isinstance(metadata, dict), f"{stage}: metadata is not an object")
    for field, expected in (
        ("version", header_version),
        ("target", 2),
        ("source_stage", expected_stage),
        ("machine_code_size", code_size),
    ):
        value = metadata.get(field)
        require(type(value) is int and value == expected,
                f"{stage}: compiler metadata {field} expected {expected}, got {value!r}")
    for field in ("hardware_stage", "unresolved_fields"):
        value = metadata.get(field)
        require(type(value) is int and 0 <= value <= 0xFFFFFFFF,
                f"{stage}: compiler metadata {field} missing or invalid")


def verify_smoke(repo, upstream, smoke):
    psbc_header = upstream / "third_party/opengnm-psbc/libpsbc/psbc_compile.h"
    require(psbc_header.is_file(), "patched compiler metadata header missing")
    versions = re.findall(
        r"(?m)^\s*#define\s+PSBC_SHADER_METADATA_VERSION\s+(\d+)u\b",
        psbc_header.read_text(encoding="utf-8"),
    )
    require(len(versions) == 1 and int(versions[0]) == METADATA_VERSION,
            "actual patched compiler metadata schema version changed")
    checked = []
    for stage in ("vert", "frag"):
        source = repo / f"tools/shaders/smoke.{stage}"
        spirv = smoke / f"smoke.{stage}.spv"
        binary = smoke / f"smoke.{stage}.gfx1013.bin"
        metadata_file = smoke / f"smoke.{stage}.metadata.json"
        require(source.is_file() and spirv.is_file() and
                binary.is_file() and metadata_file.is_file(),
                f"{stage}: original source or compiler output missing")
        spirv_bytes = spirv.read_bytes()
        require(len(spirv_bytes) >= 20 and len(spirv_bytes) % 4 == 0 and
                spirv_bytes[:4] == b"\x03\x02\x23\x07",
                f"{stage}: invalid SPIR-V smoke output")
        code_size = binary.stat().st_size
        require(0 < code_size <= MAX_OPENAGC_CODE_BYTES and code_size % 4 == 0,
                f"{stage}: compiled raw code size invalid for OpenAGC intake")
        metadata = json.loads(metadata_file.read_text(encoding="utf-8"))
        check_smoke_metadata(metadata, stage, code_size, METADATA_VERSION)
        checked.append({
            "stage": stage,
            "source_sha256": digest(source),
            "spirv_sha256": digest(spirv),
            "raw_code_sha256": digest(binary),
            "metadata_sha256": digest(metadata_file),
            "raw_code_bytes": code_size,
            "metadata_version": metadata["version"],
            "target": metadata["target"],
            "source_stage": metadata["source_stage"],
            "hardware_stage": metadata["hardware_stage"],
            "unresolved_fields": metadata["unresolved_fields"],
        })
    return checked


def source_archive(checkout, revision, destination):
    tar_file = destination.with_suffix("")
    subprocess.run(
        ["git", "-C", str(checkout), "archive", "--format=tar",
         "-o", str(tar_file), revision],
        check=True,
    )
    subprocess.run(["xz", "-6", "-T2", str(tar_file)], check=True)
    require(destination.is_file(), f"source archive not created: {destination}")
    return digest(destination)


def package(repo, upstream, smoke, destination):
    pins = verify_sources(upstream)
    compiler = upstream / "third_party/opengnm-psbc/opengnm-psbc"
    library = upstream / "third_party/opengnm-psbc/libpsbc.a"
    require(compiler.is_file() and os.access(compiler, os.X_OK) and
            compiler.stat().st_size > 0 and library.is_file() and
            library.stat().st_size > 0, "real host compiler and libpsbc.a required")
    help_result = subprocess.run(
        [str(compiler), "-h"], check=True, capture_output=True, text=True, timeout=30
    )
    version_match = re.search(r"(?<!\d)0\.1\.0(?!\d)",
                              help_result.stdout + help_result.stderr)
    require(version_match is not None,
            "compiler CLI did not report pinned version 0.1.0")
    smoke_results = verify_smoke(repo, upstream, smoke)
    require(not destination.exists(), "refusing to overwrite an existing artifact")
    destination.mkdir(parents=True)

    binary_dir = destination / "bin"
    smoke_dir = destination / "smoke"
    licenses_dir = destination / "licenses"
    sources_dir = destination / "sources"
    for directory in (binary_dir, smoke_dir, licenses_dir, sources_dir):
        directory.mkdir()
    shutil.copy2(compiler, binary_dir / "opengnm-psbc")
    for stage in ("vert", "frag"):
        for suffix in ("", ".spv", ".gfx1013.bin", ".metadata.json"):
            name = f"smoke.{stage}{suffix}"
            origin = repo / "tools/shaders" / name if suffix == "" else smoke / name
            shutil.copy2(origin, smoke_dir / name)
    shutil.copy2(upstream / "LICENSE", licenses_dir / "ps5-opengl-LICENSE")
    shutil.copy2(upstream / "third_party/opengnm-psbc/LICENSE",
                 licenses_dir / "opengnm-psbc-LICENSE")
    shutil.copy2(upstream / "third_party/opengnm/LICENSE",
                 licenses_dir / "opengnm-LICENSE")
    shutil.copytree(upstream / "LICENSES", licenses_dir / "ps5-opengl-LICENSES")
    shutil.copy2(upstream / "toolchain/opengnm-psbc-ps5.patch",
                 sources_dir / "opengnm-psbc-ps5.patch")
    shutil.copy2(upstream / "dependencies.json", sources_dir / "dependencies.json")
    inputs_dir = sources_dir / "build-inputs"
    inputs_dir.mkdir()
    build_inputs = {}
    for relative_path in (
        "toolchain/build-opengnm-psbc.sh",
        "toolchain/opengnm-psbc-host.mak",
        "tools/fetch-sources.py",
    ):
        original = upstream / relative_path
        require(original.is_file(), f"pinned build input missing: {relative_path}")
        target = inputs_dir / original.name
        shutil.copy2(original, target)
        build_inputs[relative_path] = digest(target)
    shutil.copy2(upstream / f"third_party/mesa-{MESA_VERSION}.tar.xz",
                 sources_dir / f"mesa-{MESA_VERSION}.tar.xz")
    source_hashes = {}
    for name, revision in (
        ("opengnm-psbc", PSBC_COMMIT),
        ("opengnm", OPENGNM_COMMIT),
        ("SPIRV-Headers", SPIRV_COMMIT),
        ("Vulkan-Headers", VULKAN_COMMIT),
    ):
        name_path = sources_dir / f"{name}.tar.xz"
        checkout = upstream / "third_party" / name
        source_hashes[name] = source_archive(checkout, revision, name_path)
    source_hashes["opengnm-psbc-patched"] = source_archive(
        upstream / "third_party/opengnm-psbc", PATCHED_TREE,
        sources_dir / "opengnm-psbc-patched.tar.xz"
    )

    manifest = {
        "schema_version": 1,
        "verified": True,
        "build_time_only": True,
        "openagc_runtime_compiler_enabled": False,
        "hardware_qualified": False,
        "openagc_source_commit": output("git", "-C", str(repo), "rev-parse", "HEAD"),
        "source_pins": pins,
        "compiler": {
            "name": "opengnm-psbc",
            "cli_version": version_match.group(0),
            "executable_sha256": digest(binary_dir / "opengnm-psbc"),
            "executable_bytes": compiler.stat().st_size,
            "metadata_version_from_compiler_output": smoke_results[0]["metadata_version"],
            "build_host": platform.platform(),
            "gcc_version": output("gcc", "-dumpfullversion"),
        },
        "smoke_compiles": smoke_results,
        "bundled_source_archive_sha256": source_hashes,
        "bundled_build_input_sha256": build_inputs,
    }
    temporary = destination / "manifest.json.tmp"
    temporary.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    temporary.replace(destination / "manifest.json")
    print(f"Verified PSBC binary SHA-256: {manifest['compiler']['executable_sha256']}")
    print(f"Observed metadata version: {METADATA_VERSION}")


def main(argv):
    require(len(argv) >= 3, "usage: verify_pinned_psbc.py release|sources|package PATH...")
    action = argv[1]
    if action == "release":
        require(len(argv) == 3, "release requires UPSTREAM_DIRECTORY")
        verify_release(Path(argv[2]))
        print("Tagged release manifest and full patch SHA-256 verified")
    elif action == "sources":
        require(len(argv) == 3, "sources requires UPSTREAM_DIRECTORY")
        result = verify_sources(Path(argv[2]))
        print(f"Sources and full Mesa SHA-256 verified; patched tree listing SHA-256: "
              f"{result['psbc_patched_tree_listing_sha256']}")
    elif action == "package":
        require(len(argv) == 6,
                "package requires OPENAGC_ROOT UPSTREAM_DIRECTORY SMOKE_DIRECTORY ARTIFACT_DIRECTORY")
        package(*(Path(argument) for argument in argv[2:]))
    else:
        raise VerificationError(f"unknown verification action: {action}")


if __name__ == "__main__":
    try:
        main(sys.argv)
    except VerificationError as error:
        print(f"OpenAGC PSBC verification failed: {error}", file=sys.stderr)
        sys.exit(1)
