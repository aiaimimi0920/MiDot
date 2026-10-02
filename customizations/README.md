# Godot Personal Patch Stack

This repository stores the reproducible representation of personal Godot
engine changes. It is intentionally separate from `../engine`, which remains a
checkout of the official Godot repository.

## Source of truth

- `../engine` branch `master` is the pristine upstream mirror.
- `../engine` branch `personal/main` is the authoritative ordered commit stack.
- `stack.json` describes stable topics and their dependencies.
- `patches/`, `series.txt`, and `stack.lock.json` are generated from
  `personal/main`. Do not edit generated patches by hand.
- Topic READMEs distinguish the historical legacy commits from the current
  consolidated source commit on `personal/main`.
- `audits/external_upstreams.md` records topic-owned external repositories,
  pinned revisions, licenses, and compatibility-aware update policies.

Each personal commit must contain exactly one trailer naming a topic declared in
`stack.json`:

```text
Godot-Patch-Topic: editor.example-optimization
```

Keep each commit at a stable, reviewable state. Split unrelated ideas even when
they touch the same Godot source file.

## Commands

Run commands from this repository with Windows PowerShell 5.1 or newer:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\preflight.ps1 -RequireIntegrationBranch
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\export-patches.ps1 -Replace
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify-stack.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\update-engine.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\run.ps1
```

To reconstruct the stack on a clean branch at the lock file's upstream commit:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\apply-patches.ps1 -EnginePath <path>
```

`apply-patches.ps1` and `update-engine.ps1` stop on conflicts. They never reset,
clean, skip, merge, or guess a resolution. Use `git am --abort` or
`git rebase --abort` to return to the pre-operation state.

## Generated files

- `patches/<topic-path>/*.patch`: `git format-patch --full-index --binary`
  output, ordered globally by filename prefix.
- `series.txt`: authoritative application order.
- `stack.lock.json`: exact upstream/head commits, source commit IDs, subjects,
  file paths, and SHA-256 values.

The exporter builds a complete staging catalog before replacing generated
outputs. Existing outputs require the explicit `-Replace` switch.

## Exit codes

- `0`: success.
- `1`: verification failure.
- `2`: invalid arguments, configuration, or environment.
- `3`: unsafe repository state, such as a dirty worktree or wrong branch.
- `4`: patch application conflict/failure; `git am` state is preserved.
- `5`: rebase conflict/failure; rebase state is preserved.
