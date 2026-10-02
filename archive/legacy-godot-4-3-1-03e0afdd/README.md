# Legacy Godot 4.3/4.4 Personal-History Archive

This directory preserves the three personal commits found in
`Z:\project\godot-4-3-1` before their useful behavior is rewritten as a
topic-based patch stack.

## Protected source

- Source repository: `Z:\project\godot-4-3-1`
- Current-line ref: `archive/personal-4.4-03e0afdd`
- Current-line commit: `03e0afddabd971b56aa778a8eeb0a6ec8bf3f494`
- Current-line prerequisite: `4d7c448a0a58ff4b14d4a06c4b1bb387b58f4508`
- Historical ref: `archive/personal-master-cd5d03d9`
- Historical commit: `cd5d03d9ea340f42f9ef91941026d7874c423a83`
- Historical prerequisite: `aa65940a8509fb880e2c666eb8a901525d9200ff`

The refs were created without checking them out, so the source worktree contents
were not changed.

## Contents

- `legacy-godot-4-3-1-03e0afdd.bundle`: Git bundle containing both protected
  personal lines after their prerequisite bases.
- `legacy-author-commits.tsv`: the three commits whose author and committer match
  `aiaimimi0920`.
- `raw-author-patches/`: one binary-safe `format-patch` archive per matched
  commit. These preserve evidence and are not applied blindly.
- `raw-assets/`: exact personal `icon.png` and `logo.png` bytes from the current
  legacy branch.
- `excluded-changes.md`: disposition of changes not active in the maintained
  stack.
- `archive-ref.txt`, `prerequisite-base.txt`, and `asset-sha256.txt`: exact
  object and asset identifiers.

## Maintained result

- The `TextureButton.has_point()` binding from `03e0afddabd9` is maintained as
  topic `ui.texture-button-hit-test-api`.
- The encrypted-key reversal from `cd5d03d9ea34` and `40f7ec6b34d6` is
  maintained as topic `security.encrypted-key-reversal`.
- The two PNG replacements are archived until a coherent modern PNG/SVG/platform
  branding set is defined.
- The Viewport null guard is already represented by current upstream behavior.

## Restore prerequisites

The bundle records prerequisite commits rather than duplicating the complete
Godot history. Restore it into a repository that already contains both commits
listed in `prerequisite-base.txt`.

Do not use this archive as the routine update mechanism. The maintained source
of truth is `engine/personal/main`; generated topic patches live under
`customizations/patches/`.
