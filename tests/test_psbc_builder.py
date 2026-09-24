# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OpenProspero
"""Negative-only checks: no synthetic bytes may produce a verified compiler manifest."""

from pathlib import Path
import sys
import tempfile
import unittest

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import verify_pinned_psbc as verifier


class PinnedBuildChecks(unittest.TestCase):
    def test_missing_release_refuses_to_create_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            upstream = root / "upstream"
            upstream.mkdir()
            artifact = root / "artifact"
            with self.assertRaises(verifier.VerificationError):
                verifier.package(root, upstream, root / "smoke", artifact)
            self.assertFalse(artifact.exists())

    def test_wrong_compiler_metadata_rejected(self):
        good_shape = {
            "version": verifier.METADATA_VERSION,
            "target": 2,
            "source_stage": 1,
            "machine_code_size": 16,
            "hardware_stage": 3,
            "unresolved_fields": 0,
        }
        for field, invalid in (
            ("version", verifier.METADATA_VERSION - 1),
            ("target", 1),
            ("source_stage", 5),
            ("machine_code_size", 0),
            ("hardware_stage", None),
            ("unresolved_fields", -1),
        ):
            with self.subTest(field=field):
                metadata = dict(good_shape)
                metadata[field] = invalid
                with self.assertRaises(verifier.VerificationError):
                    verifier.check_smoke_metadata(
                        metadata, "vert", 16, verifier.METADATA_VERSION
                    )

    def test_boolean_is_not_a_metadata_integer(self):
        metadata = {
            "version": True,
            "target": 2,
            "source_stage": 1,
            "machine_code_size": 16,
            "hardware_stage": 3,
            "unresolved_fields": 0,
        }
        with self.assertRaises(verifier.VerificationError):
            verifier.check_smoke_metadata(metadata, "vert", 16, verifier.METADATA_VERSION)

    def test_published_checksums_are_full_sha256(self):
        for value in (verifier.PATCH_SHA256, verifier.MESA_SHA256):
            self.assertEqual(len(value), 64)
            self.assertTrue(all(character in "0123456789abcdef" for character in value))

    def test_workflow_is_manual_only_and_read_only(self):
        root = Path(__file__).resolve().parents[1]
        workflow_path = root / ".github/workflows/build-psbc-host.yml"
        workflow = yaml.load(workflow_path.read_text(encoding="utf-8"),
                             Loader=yaml.BaseLoader)
        self.assertEqual(list(workflow["on"]), ["workflow_dispatch"])
        self.assertEqual(workflow["permissions"], {"contents": "read"})
        self.assertEqual(list(workflow["jobs"]), ["psbc-host"])
        script = (root / "tools/build-pinned-psbc.sh").read_text(encoding="utf-8")
        self.assertIn("tools/verify_pinned_psbc.py", script)
        self.assertNotIn("verify-pinned-psbc.py", script)


if __name__ == "__main__":
    unittest.main()
