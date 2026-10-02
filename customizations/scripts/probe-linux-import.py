"""Bounded empty-project import check for the already identified Linux editor."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path, required=True)
    args = parser.parse_args()
    artifact = args.artifact.resolve()
    package = artifact / "package"
    binary = package / "godot.linuxbsd.editor.x86_64"
    manifest = json.loads((package / "manifest.json").read_text())
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if digest != manifest["files"][binary.name]:
        raise RuntimeError("Editor hash differs from the verified build manifest")
    root = artifact / "import-control"
    root.mkdir(exist_ok=False)
    project = root / "project"
    project.mkdir()
    (project / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="MiDot import control"\n'
        'run/main_scene="res://main.tscn"\n[rendering]\n'
        'renderer/rendering_method="gl_compatibility"\n[debug]\n'
        'settings/stdout/verbose_stdout=true\n', encoding="utf-8")
    (project / "main.tscn").write_text(
        '[gd_scene format=3]\n\n[node name="Main" type="Node"]\n', encoding="utf-8")
    user = root / "user"
    user.mkdir()
    environment = os.environ.copy()
    for name in ("HOME", "USERPROFILE", "APPDATA", "LOCALAPPDATA",
                 "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
        environment[name] = str(user)
    command = [str(binary), "--headless", "--path", str(project), "--import"]
    log = artifact / "empty-import.log"
    reason = "exited"
    cleanup_error = None
    started = time.monotonic()
    with log.open("xb") as stream:
        child = subprocess.Popen(command, env=environment, stdout=stream,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        try:
            while child.poll() is None:
                if time.monotonic() - started >= 180:
                    reason = "timeout"
                    break
                if log.stat().st_size > 8 * 1024 * 1024:
                    reason = "output_limit"
                    break
                time.sleep(0.05)
        finally:
            if child.poll() is None:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                except OSError as error:
                    cleanup_error = str(error)
            try:
                code = child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                code = None
                cleanup_error = "Root process exit remains unknown"
    raw = log.read_bytes()
    if len(raw) > 8 * 1024 * 1024:
        reason = "output_limit"
    text = raw.decode("utf-8", errors="replace")
    passed = (reason == "exited" and code == 0 and cleanup_error is None
              and not any(marker in text for marker in
                          ("SCRIPT ERROR:", "SHADER ERROR:", "ERROR:")))
    report = {
        "schema_version": 1, "probe": "empty_editor_import", "passed": passed,
        "engine_commit": manifest["engine_commit"], "engine_sha256": digest,
        "engine_version": manifest["engine_version"], "command": command,
        "seconds": time.monotonic() - started, "timeout_seconds": 180,
        "reason": reason, "exit_code": code, "cleanup_error": cleanup_error,
        "log_sha256": hashlib.sha256(raw).hexdigest(), "log_bytes": len(raw),
        "render_verified": False, "character_validation": False,
    }
    (artifact / "empty-import-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
