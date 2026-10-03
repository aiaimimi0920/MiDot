"""Check identity rejection and archive integrity; no compiler or game execution."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch


REPOSITORY = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "linux_template", REPOSITORY / "customizations/scripts/build-linux-template.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class TemplateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.engine = self.root / "engine"
        self.engine.mkdir()

    def git_output(self, command, cwd=None):
        args = command[1:]
        if args == ["rev-parse", "--show-toplevel"]:
            return str(self.engine)
        if args == ["rev-parse", "HEAD"]:
            return builder.ENGINE_COMMIT if cwd == self.engine else "catalog-fixture"
        if args == ["status", "--porcelain", "--untracked-files=no"]:
            return ""
        if args[0] == "ls-files":
            return ""
        raise AssertionError(command)

    def test_reviewed_catalog_and_every_patch_hash(self):
        with patch.object(builder, "output", side_effect=self.git_output), patch.object(builder.subprocess, "run") as ancestor:
            source = builder.verify_sources(REPOSITORY, self.engine)
        self.assertEqual(source["patch_count"], 56)
        self.assertEqual(source["engine_commit"], builder.ENGINE_COMMIT)
        self.assertIn(builder.UPSTREAM_COMMIT, ancestor.call_args.args[0])

    def test_engine_identity_drift_is_rejected(self):
        def output(command, cwd=None):
            if command[1:] == ["rev-parse", "HEAD"] and cwd == self.engine:
                return "0" * 40
            return self.git_output(command, cwd)
        with patch.object(builder, "output", side_effect=output), self.assertRaisesRegex(RuntimeError, "HEAD"):
            builder.verify_sources(REPOSITORY, self.engine)

    def test_modified_engine_is_rejected(self):
        def output(command, cwd=None):
            return " M main/main.cpp" if command[1] == "status" else self.git_output(command, cwd)
        with patch.object(builder, "output", side_effect=output), self.assertRaisesRegex(RuntimeError, "modified"):
            builder.verify_sources(REPOSITORY, self.engine)

    def test_parent_git_root_is_rejected(self):
        with patch.object(builder, "output", return_value=str(self.root)), self.assertRaisesRegex(RuntimeError, "independent"):
            builder.verify_sources(REPOSITORY, self.engine)

    def test_changed_catalog_is_rejected_before_git_or_compile(self):
        catalog = self.root / "customizations"
        catalog.mkdir()
        (catalog / "stack.lock.json").write_text("{}")
        with patch.object(builder, "output") as git, self.assertRaisesRegex(RuntimeError, "catalog hash"):
            builder.verify_sources(self.root, self.engine)
        git.assert_not_called()

    def test_changed_patch_is_rejected_before_git_or_compile(self):
        actual = builder.sha256
        def digest(path):
            return "0" * 64 if path.suffix == ".patch" else actual(path)
        with patch.object(builder, "sha256", side_effect=digest), patch.object(builder, "output") as git, self.assertRaisesRegex(RuntimeError, "patch hash"):
            builder.verify_sources(REPOSITORY, self.engine)
        git.assert_not_called()

    def real_git_fixture(self):
        subprocess.run(["git", "init", "-q", str(self.engine)], check=True)
        (self.engine / ".gitignore").write_text("/custom.py\n", encoding="utf-8")
        subprocess.run(["git", "add", ".gitignore"], cwd=self.engine, check=True)
        subprocess.run(["git", "-c", "user.name=Template fixture", "-c",
                        "user.email=fixture@example.invalid", "commit", "-qm", "fixture"],
                       cwd=self.engine, check=True)
        builder.reject_extra_inputs(self.engine)

    def test_real_git_untracked_module_is_rejected(self):
        self.real_git_fixture()
        module = self.engine / "modules/extra"
        module.mkdir(parents=True)
        (module / "config.py").write_text("# fixture module\n")
        self.assertEqual(builder.output(["git", "status", "--porcelain", "--untracked-files=no"], self.engine), "")
        with self.assertRaisesRegex(RuntimeError, "extra untracked or ignored"):
            builder.reject_extra_inputs(self.engine)

    def test_real_git_ignored_custom_config_is_rejected(self):
        self.real_git_fixture()
        (self.engine / "custom.py").write_text('target="editor"\n')
        self.assertEqual(builder.output(["git", "status", "--porcelain", "--untracked-files=no"], self.engine), "")
        with self.assertRaisesRegex(RuntimeError, "extra untracked or ignored"):
            builder.reject_extra_inputs(self.engine)

    def test_wrong_architecture_and_non_elf_are_rejected(self):
        binary = self.engine / builder.BINARY
        for raw in (b"MZ" + b"\0" * 62, self.elf(machine=183), b"\x7fELF"):
            binary.write_bytes(raw)
            with self.subTest(raw=raw[:6]), self.assertRaisesRegex(RuntimeError, "ELF"):
                builder.inspect_elf(binary)
        binary.write_bytes(self.elf())
        builder.inspect_elf(binary)

    @staticmethod
    def elf(machine=62):
        header = bytearray(64)
        header[:6] = b"\x7fELF\x02\x01"
        header[16:18] = (3).to_bytes(2, "little")
        header[18:20] = machine.to_bytes(2, "little")
        return bytes(header)

    def test_archive_and_all_package_hashes_survive_round_trip(self):
        destination = self.root / "artifact"
        destination.mkdir()
        binary = self.engine / builder.BINARY
        binary.write_bytes(self.elf() + b"fixture-only")
        for name in ("LICENSE.txt", "COPYRIGHT.txt"):
            (self.engine / name).write_text("fixture license")
        manifest = {"engine_commit": builder.ENGINE_COMMIT, "game_started": False}
        archive = builder.package_template(REPOSITORY, self.engine, destination, binary, manifest)
        self.assertEqual((destination / (archive.name + ".sha256")).read_text().split()[0], builder.sha256(archive))
        with tarfile.open(archive) as tar:
            prefix = builder.PACKAGE + "/"
            sums = tar.extractfile(prefix + "SHA256SUMS").read().decode().splitlines()
            for line in sums:
                digest, relative = line.split("  ", 1)
                self.assertEqual(digest, hashlib.sha256(tar.extractfile(prefix + relative).read()).hexdigest())
            self.assertEqual(tar.extractfile(prefix + "SOURCE_COMMIT").read().decode().strip(), builder.ENGINE_COMMIT)
            recorded = json.loads(tar.extractfile(prefix + "manifest.json").read())
            for relative, digest in recorded["files"].items():
                self.assertEqual(digest, hashlib.sha256(tar.extractfile(prefix + relative).read()).hexdigest())
            self.assertFalse(recorded["game_started"])


if __name__ == "__main__":
    unittest.main()
