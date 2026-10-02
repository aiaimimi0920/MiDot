# `mimi_godot` Personal Change Audit

## Scope

- Source repository: `Z:\project\mimi_godot`
- Audited branch: `mimi`
- Worktree state at audit: clean
- Accepted personal identities: `vmjcv111`, `vmjcv666`, `aiaimimi0920`,
  `pingzi`, and `yamiyu`, matched against author name and email.

## Identity result

The repository contains 51 author-matching commits. All 51 use the author
identity `mimi <aiaimimi0920@gmail.com>`. No author commits were found for the
other four aliases. Another 86 commits only matched the committer-side history
or official synchronization paths and were not treated as personal changes.

## Migration result

Most net personal behavior was already represented by the existing 13-topic
stack. The remaining Material UI behavior was split into these eight reviewable
topics instead of preserving the old monolithic snapshots:

| Topic | `personal/main` source |
| --- | --- |
| `ui.color-role-transform` | `bb8d8c4c70023c73c05046ac385e4830d79424dd` |
| `ui.button-state-layers` | `e3ac8905e66965a049dcfc19fb775e1d66091cda` |
| `ui.stylebox-elevation` | `6a22295cc77c619d44f8e248c4dbe5708b3773ab` |
| `ui.texture-button-state-text` | `fd3d3181aada4b3250148ccae5b45af2eb317bae` |
| `ui.theme-string-items` | `cdded424f1496efeff24e727c8d5b24d1c43baa1` |
| `ui.button-text-icon` | `bf3bddf9cd610bf4d8ecea2b7455755b8f3a3bf0` |
| `ui.custom-icon-font` | `92f3590173207c59347d2a38b0d6c1b85018f254` |
| `ui.checkbox-state-text` | `23998d8bb0ede7b7a38f70a65d42a5a50fe8bb37` |

The primary legacy source is personal commit
`36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`). Its
13,000-plus-line, 159-file snapshot was used as intent evidence, not applied as a
single patch. Earlier `#cache code`, `#CACHE CODE`, and `# fix bug` commits were
treated as intermediate history and collapsed into the initial six behavior
modules.

A second control-inheritance and rendering-path review found that the optional
project icon font and the CheckBox/CheckButton state glyph matrix were not fully
covered by the first six topics. They were ported as the final two modules,
bringing the catalog to 21 topics.

## Explicit exclusions

- Official synchronization commits and commits that matched only by committer.
- Generated caches, editor state, imported artifacts, build products, and full
  source snapshots; this includes the `f7e62d6` cache/generated/upstream/third-
  party snapshot identified during the audit.
- Older implementations superseded by the final net Material UI behavior.
- Third-party code already carried by `ui.dynamic-color-theme`; it was not
  duplicated into any of the eight new topics.

## Porting policy

Each topic was ported to the current `origin/master` APIs, documented, given a
focused regression test, and committed with exactly one `Godot-Patch-Topic`
trailer. Generated patch files and the lock file are produced only by
`scripts/export-patches.ps1`.
