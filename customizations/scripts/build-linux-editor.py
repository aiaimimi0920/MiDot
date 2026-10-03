"""Build the verified Linux editor and package provenance, never publish a release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile


def output(command, cwd=None):
    return subprocess.check_output(command, cwd=cwd, text=True).strip()


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run_logged(command, log, cwd=None, env=None):
    with log.open("w", encoding="utf-8") as stream:
        subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=1)
    parser.add_argument("--expected-commit", required=True)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    engine = args.engine.resolve()
    destination = args.output.resolve()
    destination.mkdir(parents=True, exist_ok=False)
    lock_path = repository / "customizations/stack.lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8-sig"))
    actual = output(["git", "rev-parse", "HEAD"], engine)
    if actual != args.expected_commit or actual != lock["integration"]["commit"]:
        raise RuntimeError("Engine HEAD does not match the reviewed lock")
    if output(["git", "status", "--porcelain", "--untracked-files=no"], engine):
        raise RuntimeError("Refusing to build a modified engine")
    flags = ["platform=linuxbsd", "target=editor", "arch=x86_64", "dev_build=no",
             "debug_symbols=no", "optimize=size", "lto=none", "accesskit=no",
             "wayland=no", "x11=yes", f"-j{args.jobs}"]
    command = [sys.executable, "-m", "SCons", *flags]
    run_logged(command, destination / "build.log", cwd=engine)
    candidates = list((engine / "bin").glob("godot.linuxbsd.editor.x86_64"))
    if len(candidates) != 1 or not candidates[0].is_file():
        raise RuntimeError("Expected exactly one Linux editor binary")
    binary = candidates[0]
    version = output([str(binary), "--version"])
    if actual[:9] not in version:
        raise RuntimeError("Editor version does not report the locked commit")
    probe = destination / "probe"
    probe.mkdir()
    (probe / "project.godot").write_text('config_version=5\n[application]\nconfig/name="MiDot API probe"\n')
    shutil.copyfile(repository / "customizations/scripts/probe-linux-editor.gd", probe / "probe.gd")
    environment = os.environ.copy()
    for variable, folder in [("XDG_DATA_HOME", "data"), ("XDG_CONFIG_HOME", "config"), ("XDG_CACHE_HOME", "cache")]:
        directory = probe / folder
        directory.mkdir()
        environment[variable] = str(directory)
    run_logged([str(binary), "--headless", "--path", str(probe), "--script", "res://probe.gd"],
               destination / "engine-api.log", env=environment)
    probe_log = (destination / "engine-api.log").read_text(encoding="utf-8")
    if any(marker in probe_log for marker in ["SCRIPT ERROR:", "SHADER ERROR:", "ERROR:"]):
        raise RuntimeError("Engine API probe logged an error")
    reports = [json.loads(line) for line in probe_log.splitlines() if line.startswith('{"')]
    if len(reports) != 1 or reports[0].get("probe") != "npr_engine_api" or reports[0].get("missing") != []:
        raise RuntimeError("Engine API probe did not produce a successful report")
    package = destination / "package"
    package.mkdir()
    shutil.copy2(binary, package / binary.name)
    for name in ["LICENSE.txt", "COPYRIGHT.txt"]:
        shutil.copy2(engine / name, package / name)
    audits = repository / "customizations/audits"
    if audits.is_dir():
        shutil.copytree(audits, package / "audits")
    manifest = {
        "schema_version": 1,
        "catalog_commit": output(["git", "rev-parse", "HEAD"], repository),
        "upstream_commit": lock["upstream"]["commit"],
        "engine_commit": actual,
        "stack_lock_sha256": sha256(lock_path),
        "history_bundle_sha256": sha256(repository / "customizations" / lock["historyBundle"]["file"]),
        "patch_count": lock["patchCount"],
        "engine_version": version,
        "build_flags": flags,
        "compiler": output(["g++", "--version"]).splitlines()[0],
        "python": platform.python_version(),
        "scons": output([sys.executable, "-m", "SCons", "--version"]).splitlines()[:2],
        "engine_api_probe": "passed",
        "render_verified": False,
        "files": {str(path.relative_to(package)): sha256(path)
                  for path in sorted(package.rglob("*")) if path.is_file()},
    }
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    with tarfile.open(destination / "midot-linux-x86_64.tar.gz", "w:gz") as archive:
        archive.add(package, arcname="midot-linux-x86_64")
    print(json.dumps({"archive": str(destination / "midot-linux-x86_64.tar.gz"),
                      "engine_commit": actual, "render_verified": False}))


if __name__ == "__main__":
    main()
