# External Upstream Dependency Audit

Audit date: 2026-08-31

## Scope

This audit distinguishes dependencies owned by a personal topic from libraries
owned by the official Godot tree. Official Godot third-party code follows the
normal `origin/master` rebase and must not be independently overwritten by this
patch stack.

Only four of the 21 topics own an external upstream dependency:

| Topic | Upstream | Applied revision | License | Update policy |
| --- | --- | --- | --- | --- |
| `media.gif-support` | [giflib](https://git.code.sf.net/p/giflib/code) | tag `5.2.2`, commit `44241952659c5db27da3d9db85d910c2b6904216` | MIT | Follow compatible 5.2 maintenance updates. Treat 6.x as a separate migration with file round-trip and importer/exporter regression coverage. |
| `media.spout` | [Spout2](https://github.com/leadedge/Spout2) | release `2.007.017` | BSD-2-Clause | Replace `SpoutLibrary.h`, `.lib`, and `.dll` together from the same architecture and CRT variant. Verify archive and file hashes. |
| `animation.spine-runtime` | [Spine Runtimes](https://github.com/EsotericSoftware/spine-runtimes) | branch `4.1`, release `4.1.56`, commit `77a5db0ec6d16331f5efbaa7662bba9355bd3424` | Spine Runtimes License Agreement | Stay on 4.1 while project assets are exported by Spine Editor 4.1. Upgrade runtime, editor, export pipeline, and assets together for a new major/minor line. |
| `ui.dynamic-color-theme` | [Material Color Utilities](https://github.com/material-foundation/material-color-utilities) | `cpp/` at commit `5b3618b16fdc3825e21d5679bafd144662088ea1` | Apache-2.0 | Pin an exact commit, retain only portability/build adaptations, and run color-output golden tests before accepting algorithm changes. |

## Applied refresh

- giflib moved from 5.2.1 to the compatible 5.2.2 maintenance release. The
  former local Windows include guard is now supplied by upstream.
- Spout moved from SDK 2.007.010 to 2.007.017 as one ABI-matched dynamic-CRT
  binary set. The wrapper was ported from removed `GetSpoutVersion` to
  `GetSDKversion`.
- Spine remained on the required 4.1 compatibility branch and moved to its
  latest audited maintenance revision, release 4.1.56.
- Material Color Utilities moved from an unpinned late-2023 snapshot to an exact
  2026-08-21 commit. Godot-specific changes are documented in the vendored
  `UPSTREAM.md`; light/dark/contrast output is protected by golden tests.

## Deliberately deferred major migrations

- giflib 6.x is not a maintenance replacement for 5.2.x.
- Spine 4.2 through 4.4 are not drop-in upgrades for 4.1-exported skeleton data.

These are migration projects, not routine dependency refreshes. They require
explicit compatibility work and representative project assets before the pinned
major/minor line can change.

## Remaining topics

The other 17 topics contain personal Godot behavior, configuration, tests, or
assets but do not own an independently tracked external repository. Their
upstream compatibility is maintained by rebasing `personal/main` onto the latest
Godot `origin/master`, resolving topic conflicts, and replay-verifying the stack.

