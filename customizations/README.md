# Godot Personal Patch Stack

This catalog stores the reproducible representation of personal Godot engine
changes. In MiDot it is tracked by the workspace repository; an existing local
independent catalog checkout is also supported. `../engine` must always remain
an independent checkout of the official Godot repository, not flattened source
tracked by the workspace repository.

## Source of truth

- `../engine` branch `master` is the pristine upstream mirror.
- `../engine` branch `personal/main` is the authoritative ordered commit stack.
- `stack.json` describes stable topics and their dependencies.
- `patches/`, `series.txt`, `stack.lock.json`, and `personal-history.bundle` are generated from
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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\initialize-engine.ps1
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

`apply-patches.ps1` reconstructs source content using `git am`; its newly created
committer dates do not preserve original commit IDs. `initialize-engine.ps1`
instead fetches the official locked base and imports the incremental history
bundle, preserving every `sourceCommit` and the integration HEAD. It never
overwrites an existing checkout and always runs strict branch verification.

`apply-patches.ps1` and `update-engine.ps1` stop on conflicts. They never reset,
clean, skip, merge, or guess a resolution. Use `git am --abort` or
`git rebase --abort` to return to the pre-operation state.

## Generated files

- `patches/<topic-path>/*.patch`: `git format-patch --full-index --binary`
  output, ordered globally by filename prefix.
- `series.txt`: authoritative application order.
- `stack.lock.json`: exact upstream/head commits, source commit IDs, subjects,
  file paths, and SHA-256 values.
- `personal-history.bundle`: personal commits after the locked official base,
  with that base as a prerequisite; its SHA-256 is recorded in the lock file.
  It does not duplicate the complete upstream repository and is regenerated
  automatically after an upstream rebase. Empty stacks do not need a bundle.

The exporter builds a complete staging catalog before replacing generated
outputs. Existing outputs require the explicit `-Replace` switch.

## Exit codes

- `0`: success.
- `1`: verification failure.
- `2`: invalid arguments, configuration, or environment.
- `3`: unsafe repository state, such as a dirty worktree or wrong branch.
- `4`: patch application conflict/failure; `git am` state is preserved.
- `5`: rebase conflict/failure; rebase state is preserved.
