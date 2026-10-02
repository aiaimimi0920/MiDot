# Legacy Personal Commit Disposition

This file records how personal commits from the legacy Godot repositories are
reduced into the maintained topic stack. The first archive contains 45
author-matched commits from `Z:\project\godot`; the second audit covers three
`aiaimimi0920` commits from `Z:\project\godot-4-3-1`. The maintained catalog
also incorporates audited personal commits and final worktree state from
`Z:\project\godot-4-4-1`.

## Maintained topics

| Topic | Legacy commits or final source state |
| --- | --- |
| `runtime.process-api` | `d654681d030b`, relevant fixes from `5b6b9edfdf58` |
| `scripting.gdscript-helper` | relevant paths from `a99bfbe5fb9a`, `5b6b9edfdf58`, and `e28a77fd8db0` |
| `media.gif-support` | `c0c8a9ceab20` |
| `media.spout` | relevant paths from `a99bfbe5fb9a` and `5b6b9edfdf58` |
| `animation.spine-runtime` | final `modules/spine_godot` tree after `35b77c26abb6`, `c0c8a9ceab20`, `27c49058371a`, and `05a507fa780f` |
| `ui.dynamic-color-theme` | final successful series `e28a77fd8db0..c69dbaf9e042`, excluding failed sibling `97d7ae0e1d32` |
| `branding.windows-icons` | `b6726b34e07b` |
| `ui.texture-button-hit-test-api` | `scene/gui/texture_button.cpp` portion of `03e0afddabd9` |
| `security.encrypted-key-reversal` | `core/io/file_access_encrypted.*` portions of `cd5d03d9ea34` and `40f7ec6b34d6` |
| `scripting.script-documentation-api` | structured `Script` documentation API from the audited `godot-4-4-1` source state |
| `media.gif-streaming-exporter` | legacy `GifExporter` behavior from `godot-4-4-1`, reimplemented on the maintained `ImageFrames` backend |
| `branding.main-icon` | audited `godot-4-4-1/icon.png`, plus the derived modern application icon |
| `build.profiles.fortune-wheel` | current `fortune_wheel_web` and `fortune_wheel_windows` profile state from `godot-4-4-1` |

Historical commits are not replayed one-for-one. A topic commit preserves the
final intended behavior while removing intermediate experiments, reversals, and
meaningless messages.

## Archived but not automatically reapplied

The following changes remain in the provenance archive but are not formal active
topics unless their original failure can still be reproduced on modern Godot:

- `97d7ae0e1d32`: explicitly marked `# cache code no success`; replaced by the
  successful sibling beginning at `8d2c17a83eac`.
- `b5dbcb2ea7ee`, `7f987aa5760a`, `2da99e9e8bfc`: net removal of
  `SurfaceUpgradeTool` initialization plus a stray comment. Modern Godot must not
  lose required surface-upgrade initialization without a reproducible reason.
- `c1115c453435`: historical `dev_build=yes` packaging workaround whose exact
  old build invocation is not part of current source behavior.
- Mixed small imports/fixes `ac12837bd91f`, `911562835a79`, `241f1d152dad`,
  `3c0d7f49afd1`, `e084aa23c1d7`, `821d01f0533e`, `4382edd8caf1`, and
  `68ced3da036d`: these touch unrelated upstream-hot code and tests. They require
  semantic reproduction against current Godot before activation; exact patches
  are retained in the archive.
- The deleted `lucky_wheel` profile from `godot-4-4-1`: it substantially
  duplicates the two maintained Fortune Wheel profiles and is not restored
  unless a distinct current target requires it.

## Upstream-absorbed changes

- The `scene/main/viewport.cpp` mouse-focus null guard from `cd5d03d9ea34` and
  `40f7ec6b34d6` already exists in current upstream logic and must not be replayed
  as a separate personal patch.

The 278 commits whose author is another contributor but whose committer is
`vmjcv` are not personal topics. They are upstream-history synchronization and
must not be exported as personal patches.
