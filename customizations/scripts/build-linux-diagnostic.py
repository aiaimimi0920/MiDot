"""Build a separate six-marker diagnostic; never a production or acceptance artifact."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tarfile
import time

ENGINE = "38b6ddee72e16d9d646057ee6ba533c122afc47c"
SOURCES = {
    "editor/editor_node.cpp": "faa21de3dd4a682b7e5233ac360d1ab8a39b926b0f2143dd7037c8951bc8119f",
    "main/main.cpp": "4ec346bd8387ceb46db01c554994c318b043da7dac36424d23f7349e33b29c23",
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(engine, *args):
    return subprocess.check_output(["git", *args], cwd=engine, text=True).strip()


def mark(label, indent):
    return indent + 'fprintf(stderr, "MIDOT_DIAGNOSTIC:' + label + '\\n"); fflush(stderr);\n'


def instrument(relative, original):
    text = original.decode("utf-8")
    if relative == "editor/editor_node.cpp":
        points = [
            ("\ttheme = EditorThemeManager::generate_theme();\n", "theme_return", "\t", False),
            ("\t_update_layouts_menu();\n}\n\nEditorNode::~EditorNode()", "constructor_end", "\t", False),
            ("\t\tcase NOTIFICATION_ENTER_TREE: {\n", "enter_tree", "\t\t\t", False),
            ("\t\t\t_begin_first_scan();\n", "begin_first_scan_return", "\t\t\t", False),
            ('\t\t\t\tEditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorNode::_execute_upgrades), CONNECT_ONE_SHOT);\n', "before_scan", "\t\t\t\t", False),
        ]
    else:
        points = [("\t\t\teditor_node = memnew(EditorNode);\n", "memnew_return", "\t\t\t", False)]
    for anchor, label, indent, _ in points:
        if text.count(anchor) != 1:
            raise RuntimeError(f"Non-unique diagnostic anchor: {label}")
        if label == "constructor_end":
            replacement = anchor.replace("\n}\n", "\n" + mark(label, indent) + "}\n")
        else:
            replacement = anchor + mark(label, indent)
        text = text.replace(anchor, replacement)
    # Explicit standard IO declaration; no logger, telemetry or runtime dependency.
    return ("#include <stdio.h>\n" + text).encode("utf-8")


def probe(binary, destination):
    root = destination / "import-control"
    root.mkdir()
    (root / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Diagnostic only"\n[rendering]\nrenderer/rendering_method="gl_compatibility"\n')
    user = root / "user"
    user.mkdir()
    env = os.environ.copy()
    for key in ("HOME", "USERPROFILE", "APPDATA", "LOCALAPPDATA", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
        env[key] = str(user)
    started = time.monotonic()
    reason = "exited"
    log = destination / "diagnostic-import.log"
    with log.open("xb") as stream:
        child = subprocess.Popen([str(binary), "--headless", "--path", str(root), "--import"], env=env, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            while child.poll() is None:
                if time.monotonic() - started >= 180 or log.stat().st_size > 8 * 1024 * 1024:
                    reason = "timeout_or_output_limit"
                    break
                time.sleep(0.05)
        finally:
            if child.poll() is None:
                os.killpg(child.pid, signal.SIGKILL)
            child.wait(timeout=5)
    raw = log.read_bytes()
    return {"reason": reason, "exit_code": child.returncode, "seconds": time.monotonic() - started,
            "log_sha256": digest(raw), "markers": [line for line in raw.decode(errors="replace").splitlines() if line.startswith("MIDOT_DIAGNOSTIC:")],
            "render_verified": False, "character_validation": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=2)
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--cache-dir", type=Path)
    args = parser.parse_args()
    engine = args.engine.resolve()
    destination = args.output.resolve()
    repository = Path(__file__).resolve().parents[2]
    lock = json.loads((repository / "customizations/stack.lock.json").read_text(encoding="utf-8-sig"))
    if git(engine, "rev-parse", "HEAD") != ENGINE or lock["integration"]["commit"] != ENGINE:
        raise RuntimeError("Unexpected locked engine identity")
    if git(engine, "status", "--porcelain", "--untracked-files=no"):
        raise RuntimeError("Refusing to instrument modified engine")
    changes = {}
    for relative, expected in SOURCES.items():
        original = (engine / relative).read_bytes()
        if digest(original) != expected:
            raise RuntimeError(f"Source hash mismatch: {relative}")
        changes[relative] = (original, instrument(relative, original))
    hashes = {name: {"original": digest(before), "instrumented": digest(after)} for name, (before, after) in changes.items()}
    if args.check_only:
        print(json.dumps(hashes, indent=2))
        return
    destination.mkdir(parents=True, exist_ok=False)
    flags = ["platform=linuxbsd", "target=editor", "arch=x86_64", "dev_build=no", "debug_symbols=no", "optimize=size", "lto=none", "accesskit=no", "wayland=no", "x11=yes", f"-j{args.jobs}"]
    cache_flags = ([f"cache_path={args.cache_dir.resolve()}", "cache_limit=3"]
                   if args.cache_dir else [])
    try:
        for relative, (_, after) in changes.items():
            (engine / relative).write_bytes(after)
        with (destination / "build.log").open("w") as stream:
            subprocess.run([sys.executable, "-m", "SCons", *flags, *cache_flags], cwd=engine, stdout=stream, stderr=subprocess.STDOUT, check=True)
    finally:
        for relative, (before, _) in changes.items():
            (engine / relative).write_bytes(before)
    package = destination / "package"
    package.mkdir()
    binary = package / "midot-diagnostic-six-markers.x86_64"
    shutil.copy2(engine / "bin/godot.linuxbsd.editor.x86_64", binary)
    for name in ("LICENSE.txt", "COPYRIGHT.txt"):
        shutil.copy2(engine / name, package / name)
    manifest = {"diagnostic_only": True, "render_verified": False, "character_validation": False,
                "catalog_commit": git(repository, "rev-parse", "HEAD"), "engine_commit": ENGINE,
                "upstream_commit": lock["upstream"]["commit"], "source_hashes": hashes, "build_flags": flags,
                "binary_sha256": digest(binary.read_bytes()), "binary_name": binary.name,
                "version": subprocess.check_output([str(binary), "--version"], text=True).strip()}
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with tarfile.open(destination / "midot-diagnostic-six-markers.tar.gz", "w:gz") as archive:
        archive.add(package, arcname="midot-diagnostic-six-markers")
    report = probe(binary, destination)
    (destination / "diagnostic-import-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
