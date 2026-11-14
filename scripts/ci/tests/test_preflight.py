import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "preflight", Path(__file__).resolve().parents[1] / "preflight.py")
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)


class PreflightTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.root = self.base / "repo"
        self.root.mkdir()
        self.output = self.base / "results"
        self.calls = self.base / "calls"
        for args in (("init", "-q"), ("-c", "user.name=Test", "-c", "user.email=test@example.com",
                                      "commit", "-q", "--allow-empty", "-m", "fixture")):
            subprocess.run(["git", *args], cwd=self.root, check=True, capture_output=True)
        binary = self.base / "bin"
        binary.mkdir()
        nix = binary / "nix"
        nix.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$PREFLIGHT_TEST_CALLS"\n'
                       'exit "${PREFLIGHT_TEST_EXIT:-0}"\n')
        nix.chmod(0o755)
        self.addCleanup(os.chdir, Path.cwd())
        self.enterContext(patch.object(preflight, "ROOT", self.root))
        self.enterContext(patch.dict(os.environ, {
            "PATH": str(binary) + os.pathsep + os.environ["PATH"],
            "PREFLIGHT_TEST_CALLS": str(self.calls), "PREFLIGHT_TEST_EXIT": "0",
        }))
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.enterContext(contextlib.redirect_stderr(io.StringIO()))

    def run_preflight(self, *targets):
        return preflight.main([*targets, "--output", str(self.output)])

    def test_nix_failure_stops_before_package_build_and_records_failure(self):
        os.environ["PREFLIGHT_TEST_EXIT"] = "23"
        self.assertEqual(self.run_preflight("nix"), 1)
        self.assertEqual(self.calls.read_text().splitlines(), ["flake check --print-build-logs"])
        report = json.loads((self.output / "summary.json").read_text())
        self.assertEqual(report["results"], {"nix": "failed (23)"})
        self.assertIn("flake check", (self.output / "nix.log").read_text())

    def test_nix_success_runs_checks_and_build_for_reported_commit(self):
        self.assertEqual(self.run_preflight("nix"), 0)
        self.assertEqual(self.calls.read_text().splitlines(), [
            "flake check --print-build-logs", "build --no-link --print-build-logs"])
        report = json.loads((self.output / "summary.json").read_text())
        self.assertEqual(report["commit"], subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=self.root, text=True).strip())
        self.assertFalse(report["dirty"])

    def test_untracked_source_cannot_pass_as_committed_build(self):
        (self.root / "new.cpp").write_text("// untracked source\n")
        with self.assertRaises(SystemExit) as raised:
            self.run_preflight("nix")
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse(self.calls.exists())

    def test_lint_allows_dirty_tree_and_records_it(self):
        (self.root / "new.cpp").write_text("// untracked source\n")
        self.assertEqual(self.run_preflight("lint"), 0)
        self.assertTrue(json.loads((self.output / "summary.json").read_text())["dirty"])

    def test_stale_output_is_rejected_without_overwriting_evidence(self):
        self.output.mkdir()
        previous = self.output / "summary.json"
        previous.write_text("previous run")
        with self.assertRaises(SystemExit):
            self.run_preflight("nix")
        self.assertEqual(previous.read_text(), "previous run")
        self.assertFalse(self.calls.exists())
