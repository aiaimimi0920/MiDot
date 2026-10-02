# GIF ImageFrames Support

## Intent and problem

Load and save animated GIF data and convert frames into Godot animation
resources without requiring application-side GIF decoding.

## Behavior contract

- `ImageFrames` loads GIF files and byte buffers with frame durations.
- Frames can be edited and saved as static or animated GIF files.
- The editor can import GIF files as `SpriteFrames` or `AnimatedTexture`.

## Legacy origin

- Original Godot line: 4.2 development history.
- Personal commit:
  `c0c8a9ceab202642dcd43924fcc382ccbe1900ec`.
- giflib sources are vendored inside the module and retain their own license.

## External upstream

- Repository: <https://git.code.sf.net/p/giflib/code>
- Version/tag: `5.2.2`
- Commit: `44241952659c5db27da3d9db85d910c2b6904216`
- License: MIT
- Update policy: stay on compatible 5.2 maintenance releases; treat giflib 6.x
  as a separate API and file-format regression migration.

## Consolidated source

- `personal/main` commit:
  `7b94a4dde1f74c946acc0249b9fedfc32a980ab7`.

## Dependencies

No patch-stack topic dependency. The topic vendors giflib 5.2.2.

## Important source areas

- `modules/gif/`
- `ImageFrames`
- GIF `SpriteFrames` and `AnimatedTexture` importers

## Conflict guidance

Preserve frame timing, transparency, buffer loading, and both importer targets.
Use current `Image`, `SpriteFrames`, and import-plugin APIs instead of restoring
removed compatibility names.

## Verification

Run the topic verifier, compile the module, then round-trip static and animated
GIF fixtures and import one fixture into each supported resource type.

## Upstream status

Active personal feature; upstream Godot has generic image and frame APIs but no
equivalent GIF module in the inspected engine tree. The vendored giflib snapshot
is current at the compatible 5.2.2 maintenance release.
