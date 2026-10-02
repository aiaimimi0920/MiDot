# Fortune Wheel Build Profiles

## Intent and problem

Retain small export-template build configurations for the Fortune Wheel project
without mixing application-specific size choices into engine source changes.

## Behavior contract

- `fortune_wheel_web.gdbuild` describes the Web export-template feature set.
- `fortune_wheel_windows.gdbuild` describes the Windows export-template feature
  set.
- Both are valid modern `build_profile` JSON documents and contain the audited
  319 disabled classes.
- `CollisionPolygon2D`, `CollisionShape2D`, and the fallback text server remain
  available because the current project state requires them.
- These profiles target export templates; options such as `disable_3d` are not
  expected to validate against an editor target.
- The deleted, substantially duplicated `lucky_wheel` profile is intentionally
  not restored.

## Legacy origin

- Audited current files from `Z:\project\godot-4-4-1`, including its dirty
  worktree fixes rather than only the last committed profile revision.

## Consolidated source

- `personal/main` commit:
  `0824c11d5bb49da28ba3b9fd915ff4eb720fcb2d`.

## Dependencies

None.

## Important source areas

- `fortune_wheel_web.gdbuild`
- `fortune_wheel_windows.gdbuild`
- SCons `build_profile` loading and feature validation

## Conflict guidance

When upstream renames an option or class, regenerate the profiles with the
current build-profile editor or remove only entries proved obsolete. Do not
restore disabled collision or fallback-text entries from older revisions.

## Verification

Run the topic verifier, then load each file through current SCons using a
`template_release` target. Confirm the output reports `Using feature build
profile` without an invalid-option error.

## Upstream status

Project-specific build metadata; not intended for upstream.
