# Godot Personal Patch Catalog Instructions

- `../engine/personal/main` is the source of truth for personal engine changes.
  This repository stores their documented, generated, and verifiable patch
  representation.
- Keep `stack.json` topic IDs and paths stable. Group by modification intent,
  record dependencies explicitly, and require a topic README before export.
- Do not edit `patches/`, `series.txt`, or `stack.lock.json` manually. Change the
  corresponding commit on `personal/main`, then run
  `scripts/export-patches.ps1 -Replace`.
- Run `scripts/verify-stack.ps1` after any stack/configuration change and
  `tests/run.ps1` after changing patch-stack scripts.
- Upstream engine updates must use `scripts/update-engine.ps1`, which performs
  `fetch + rebase`; never replace it with a routine pull or merge.
- Never automate conflict guessing, skipping, hard resets, or worktree cleanup.
  Preserve a stopped `git am`/rebase for explicit resolution, or abort it.
- Keep all PowerShell compatible with Windows PowerShell 5.1 and write text as
  UTF-8 without BOM.
