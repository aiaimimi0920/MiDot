"""Compile one locked Linux release export template; never publish or run a game."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile


ENGINE_COMMIT = "38b6ddee72e16d9d646057ee6ba533c122afc47c"
UPSTREAM_COMMIT = "5ec4857b340b6284a18b49b2eda462bd250f219a"
BINARY = "godot.linuxbsd.template_release.x86_64"
PACKAGE = "midot-linux-template-release-x86_64"
CATALOG_HASHES = {
    "stack.lock.json": "aee35863851b196c05dc07c6c30e2859abddf2bc42f025b2c10f37e20f2d00c3",
    "stack.json": "8a9975205ebe21a4a680f85fac99f84f4bc5f3ad0355ca7af1c0fee2a43c133d",
    "series.txt": "56f09e72fb608a87ad82cf2108a9da9508db6d5b90d75d667abad81a0d6f6bc7",
    "personal-history.bundle": "d5e00c7c7063383f53392b813e67af494aec24672c5a9d0e5af5a3e0ac49df80",
}


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def output(command, cwd=None):
    return subprocess.check_output(command, cwd=cwd, text=True, timeout=30).strip()


def reject_extra_inputs(engine):
    for flags in (["--others", "--exclude-standard"],
                  ["--others", "--ignored", "--exclude-standard"]):
        if output(["git", "ls-files", *flags], engine):
            raise RuntimeError("Refusing extra untracked or ignored engine build inputs")


def verify_sources(repository, engine, before_build=True):
    catalog = repository / "customizations"
    for name, expected in CATALOG_HASHES.items():
        if sha256(catalog / name) != expected:
            raise RuntimeError(f"Reviewed catalog hash differs: {name}")
    lock = json.loads((catalog / "stack.lock.json").read_text(encoding="utf-8-sig"))
    if (lock["integration"]["commit"] != ENGINE_COMMIT
            or lock["upstream"]["commit"] != UPSTREAM_COMMIT
            or lock["patchCount"] != 56 or len(lock["patches"]) != 56):
        raise RuntimeError("Unexpected fixed source identity or patch count")
    for patch in lock["patches"]:
        if sha256(catalog / patch["file"]) != patch["sha256"]:
            raise RuntimeError(f"Reviewed patch hash differs: {patch['file']}")
    if Path(output(["git", "rev-parse", "--show-toplevel"], engine)).resolve() != engine:
        raise RuntimeError("Engine must be an independent Git root")
    actual = output(["git", "rev-parse", "HEAD"], engine)
    if actual != ENGINE_COMMIT:
        raise RuntimeError("Engine HEAD differs from the reviewed source")
    if output(["git", "status", "--porcelain", "--untracked-files=no"], engine):
        raise RuntimeError("Refusing to build a modified engine")
    if before_build:
        reject_extra_inputs(engine)
    subprocess.run(["git", "merge-base", "--is-ancestor", UPSTREAM_COMMIT, actual],
                   cwd=engine, check=True, timeout=30)
    return {
        "catalog_commit": output(["git", "rev-parse", "HEAD"], repository),
        "engine_commit": actual, "upstream_commit": UPSTREAM_COMMIT,
        "stack_lock_sha256": CATALOG_HASHES["stack.lock.json"],
        "history_bundle_sha256": CATALOG_HASHES["personal-history.bundle"],
        "catalog_files_sha256": CATALOG_HASHES, "patch_count": 56,
    }


def inspect_elf(binary):
    if binary.is_symlink() or not binary.is_file():
        raise RuntimeError("Expected a regular release template binary")
    with binary.open("rb") as stream:
        header = stream.read(64)
    if (len(header) != 64 or header[:6] != b"\x7fELF\x02\x01"
            or int.from_bytes(header[16:18], "little") not in (2, 3)
            or int.from_bytes(header[18:20], "little") != 62):
        raise RuntimeError("Template must be a Linux x86_64 ELF executable")


def package_template(repository, engine, destination, binary, manifest):
    package = destination / "package"
    package.mkdir()
    shutil.copy2(binary, package / binary.name)
    for name in ("LICENSE.txt", "COPYRIGHT.txt"):
        shutil.copy2(engine / name, package / name)
    (package / "SOURCE_COMMIT").write_text(ENGINE_COMMIT + "\n", encoding="utf-8")
    provenance = package / "provenance"
    provenance.mkdir()
    for name in ("stack.lock.json", "stack.json", "series.txt"):
        shutil.copy2(repository / "customizations" / name, provenance / name)
    audits = repository / "customizations/audits"
    if audits.is_dir():
        shutil.copytree(audits, package / "audits")
    manifest["files"] = {path.relative_to(package).as_posix(): sha256(path)
                         for path in sorted(package.rglob("*")) if path.is_file()}
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    sums = [f"{sha256(path)}  {path.relative_to(package).as_posix()}"
            for path in sorted(package.rglob("*")) if path.is_file()]
    (package / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
    archive_path = destination / (PACKAGE + ".tar.gz")
    with tarfile.open(archive_path, "w:gz") as archive:
        archive.add(package, arcname=PACKAGE)
    (destination / (archive_path.name + ".sha256")).write_text(
        sha256(archive_path) + "  " + archive_path.name + "\n", encoding="utf-8")
    return archive_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=2)
    args = parser.parse_args()
    if platform.system() != "Linux":
        raise RuntimeError("Compile this template only on Linux")
    repository = Path(__file__).resolve().parents[2]
    engine = args.engine.resolve()
    destination = args.output.resolve()
    source = verify_sources(repository, engine)
    destination.mkdir(parents=True, exist_ok=False)
    flags = ["platform=linuxbsd", "target=template_release", "arch=x86_64",
             "dev_build=no", "debug_symbols=no", "optimize=size", "lto=none",
             "accesskit=no", "wayland=no", "x11=yes", "cxxflags=-include cfloat",
             f"-j{args.jobs}"]
    manifest = {
        "schema_version": 1, **source, "target": "template_release",
        "build_flags": flags, "compiler": output(["g++", "--version"]).splitlines()[0],
        "python": platform.python_version(),
        "scons": output([sys.executable, "-m", "SCons", "--version"]).splitlines()[:2],
        "engine_api_probe": "not_run", "render_verified": False,
        "character_validation": False, "game_exported": False, "game_started": False,
    }
    (destination / "source-report.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    with (destination / "build.log").open("w", encoding="utf-8") as stream:
        subprocess.run([sys.executable, "-m", "SCons", *flags], cwd=engine,
                       stdout=stream, stderr=subprocess.STDOUT, check=True)
    # SCons output is untracked/ignored; tracked sources must still be unchanged.
    verify_sources(repository, engine, before_build=False)
    binary = engine / "bin" / BINARY
    inspect_elf(binary)
    version = output([str(binary), "--version"])
    if ENGINE_COMMIT[:9] not in version:
        raise RuntimeError("Template version does not report the locked commit")
    manifest["engine_version"] = version
    archive = package_template(repository, engine, destination, binary, manifest)
    print(json.dumps({"archive": str(archive), "sha256": sha256(archive),
                      "engine_commit": ENGINE_COMMIT, "game_started": False}))


if __name__ == "__main__":
    main()
