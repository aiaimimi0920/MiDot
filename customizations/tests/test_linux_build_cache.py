"""Cache wiring must not replace source verification, builds or runtime probes."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def load_builder(kind):
    path = ROOT / "customizations/scripts" / f"build-linux-{kind}.py"
    spec = importlib.util.spec_from_file_location(f"builder_{kind}", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BuildReached(Exception):
    pass


class LinuxCacheTests(unittest.TestCase):
    def test_editor_optional_cache_preserves_build_flags(self):
        module = load_builder("editor")
        for cached in (False, True):
            with self.subTest(cached=cached), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                argv = ["build", "--engine", temp, "--output", str(root / "out"),
                        "--jobs", "2", "--expected-commit",
                        "38b6ddee72e16d9d646057ee6ba533c122afc47c"]
                if cached:
                    argv += ["--cache-dir", str(root / "cache")]
                with patch.object(sys, "argv", argv), patch.object(
                    module, "output", side_effect=[argv[8], ""]
                ), patch.object(module, "run_logged", side_effect=BuildReached) as run:
                    with self.assertRaises(BuildReached):
                        module.main()
                command = run.call_args.args[0]
                self.assertIn("-j2", command)
                self.assertIn("optimize=size", command)
                self.assertEqual("cache_limit=3" in command, cached)
                if cached:
                    self.assertIn(f"cache_path={root / 'cache'}", command)

    def test_diagnostic_restores_source_when_cached_build_fails(self):
        module = load_builder("diagnostic")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "probe.cpp"
            original = b"original source"
            source.write_bytes(original)
            argv = ["build", "--engine", temp, "--output", str(root / "out"),
                    "--cache-dir", str(root / "cache")]
            with patch.object(sys, "argv", argv), patch.object(
                module, "git", side_effect=[module.ENGINE, ""]
            ), patch.object(module, "SOURCES", {"probe.cpp": module.digest(original)}), patch.object(
                module, "instrument", return_value=b"instrumented"
            ), patch.object(module.subprocess, "run", side_effect=BuildReached) as run:
                with self.assertRaises(BuildReached):
                    module.main()
            self.assertEqual(source.read_bytes(), original)
            self.assertIn("cache_limit=3", run.call_args.args[0])
            self.assertIn(f"cache_path={root / 'cache'}", run.call_args.args[0])

    def test_workflows_isolate_caches_and_keep_verification(self):
        for kind in ("editor", "diagnostic"):
            with self.subTest(kind=kind):
                workflow = (ROOT / f".github/workflows/linux-{kind}.yml").read_text()
                self.assertIn("${{ github.job }}", workflow)
                self.assertIn("${{ steps.compiler.outputs.fingerprint }}", workflow)
                self.assertIn("customizations/stack.lock.json", workflow)
                self.assertIn("  push:\n    branches: [main]", workflow)
                restore_prefix = workflow.split("restore-keys: |", 1)[1].splitlines()[1]
                self.assertNotIn("stack.lock.json", restore_prefix)
                self.assertIn("build-linux-*.py", restore_prefix)
                self.assertIn('sha256sum "$(command -v g++)"', workflow)
                self.assertIn('--cache-dir "$RUNNER_TEMP/midot-scons"', workflow)
                self.assertIn("verify-stack.ps1", workflow)
                self.assertNotIn("cache-hit", workflow)
                self.assertNotIn("continue-on-error", workflow)
        editor = (ROOT / ".github/workflows/linux-editor.yml").read_text()
        self.assertIn("probe-linux-import.py", editor)


if __name__ == "__main__":
    unittest.main()
