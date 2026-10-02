# Godot Engine Workspace Instructions

## Upstream synchronization

- For `engine`, synchronize Godot upstream changes with rebase. Prefer the
  explicit sequence `git fetch origin master` followed by
  `git rebase origin/master` on the local personal-change branch. Use
  `git pull --rebase` only when that branch has the correct upstream tracking
  configuration.
- Do not use a routine `git pull` or merge `origin/master` into a personal
  branch, because either can create merge commits and make the personal patch
  history harder to replay and repair.
- Start synchronization only from a clean worktree. Never discard local work to
  make a rebase proceed.
- Rebase only local personal-change branches. Do not rewrite a shared or
  published branch without explicit user approval.
- If a rebase conflicts, inspect the currently replayed commit and resolve the
  conflict at that patch's topic boundary. If the correct resolution is not
  clear, use `git rebase --abort` and report the conflict instead of guessing.
- This repository may be shallow. If the required merge base is unavailable,
  deepen or unshallow the repository before rebasing; do not replace rebase
  with a merge as a workaround.

## Personal patch stack

- Keep `engine/master` as a pristine mirror of `origin/master`. Implement and
  test personal engine changes on `engine/personal/main`.
- Treat commits on `personal/main` as the source of truth. Each personal commit
  must represent one reviewable idea and include exactly one topic trailer
  declared by `customizations/stack.json`.
- Treat `customizations/patches/`, `customizations/series.txt`, and
  `customizations/stack.lock.json` as generated files. Fix commits rather than
  editing generated patches manually.
- After changing the personal commit stack, run
  `customizations/scripts/export-patches.ps1 -Replace`, followed by
  `customizations/scripts/verify-stack.ps1` and the relevant topic checks.
- Never make patch automation resolve, skip, reset, or clean a conflict
  automatically. Preserve the stopped Git operation for explicit review or
  abort it without discarding unrelated work.
