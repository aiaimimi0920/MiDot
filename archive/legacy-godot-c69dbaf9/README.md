# Legacy Godot Personal-History Archive

This directory preserves the old personal Godot history before it is rewritten
as a small topic-based patch stack.

## Protected source

- Source repository: `Z:\project\godot`
- Protected ref: `archive/personal-2024-c69dbaf9`
- Protected commit: `c69dbaf9e042080ace0bbc7a38a62d0aca86f29a`
- Prerequisite upstream base: `da0b1eb128a522bbef083b9f2a5cc2da6917c3d8`

The source worktree was detached at the protected commit. The archive branch was
created without checking it out, so the source worktree contents were not
changed.

## Contents

- `legacy-godot-c69dbaf9.bundle`: Git bundle containing the protected history
  after the prerequisite base. It intentionally retains interleaved historical
  commits for provenance and is not the maintained patch stack.
- `legacy-author-commits.tsv`: all 45 commits whose author identity matched one
  of the requested personal aliases.
- `raw-author-patches/`: one binary-safe `format-patch` archive per matched
  author commit. These are evidence, not patches to apply blindly.
- `excluded-commits.txt`: commits retained for evidence but explicitly excluded
  from the maintained functional stack.
- `archive-ref.txt` and `prerequisite-base.txt`: exact object references.

## Verification performed

The bundle was verified with `git bundle verify`. The source repository passed
`git fsck --full --strict`, and the protected branch resolved to the exact
detached HEAD object recorded above.

## Restore prerequisites

The bundle records the prerequisite base rather than duplicating all earlier
Godot history. Restore it into a repository that already contains
`da0b1eb128a522bbef083b9f2a5cc2da6917c3d8`.

Do not use this archive as the routine update mechanism. The maintained source
of truth is `engine/personal/main`; generated topic patches live under
`customizations/patches/`.
