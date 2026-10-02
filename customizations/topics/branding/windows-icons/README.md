# Personal Windows Icons

## Intent and problem

Keep the personal editor and console executable branding when producing Windows
Godot binaries.

## Behavior contract

- Windows GUI/editor builds use the personal `godot.ico`.
- Windows console builds use the personal `godot_console.ico`.
- No non-Windows build output changes.

## Legacy origin

- Personal commit:
  `b6726b34e07bf528779c08a16abcf1448317ce7b`.

## Consolidated source

- `personal/main` commit:
  `11ae6bfcc714c375e399156a93e8fc4b466501aa`.

## Dependencies

None.

## Important source areas

- `platform/windows/godot.ico`
- `platform/windows/godot_console.ico`

## Conflict guidance

If upstream changes required icon sizes or resource metadata, regenerate the
personal artwork with those technical requirements rather than restoring an
obsolete resource script.

## Verification

Run the topic verifier, build both Windows executable variants, and inspect the
embedded icons at all Windows shell sizes.

## Upstream status

Active personal branding; intentionally not intended for upstream.
