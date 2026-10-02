# StyleBoxFlat Elevation

## Intent and problem

Provide consistent Material elevation shadows without requiring every theme to
manually reproduce three coordinated shadow layers.

## Behavior contract

- `dynamic_shadow` explicitly opts into Material shadow rendering.
- `elevation_level` selects a stable preset from level 0 through level 5.
- Each preset draws umbra, penumbra, and ambient components.
- With dynamic shadows disabled, stock `shadow_size` and `shadow_offset` behavior
  remains unchanged.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- Legacy symbols included `StyleBoxFlat::set_elevation_level` and per-level
  three-layer shadow parameters.

## Consolidated source

- `personal/main` commit:
  `6a22295cc77c619d44f8e248c4dbe5708b3773ab`.

## Dependencies

None.

## Important source areas

- `scene/resources/style_box_flat.*`
- `doc/classes/StyleBoxFlat.xml`
- `tests/scene/test_style_box_flat.cpp`

## Conflict guidance

Preserve the opt-in boundary. Upstream changes to `StyleBoxFlat::draw` must retain
stock shadow output when disabled and the three distinct Material components
when enabled. Keep preset values centralized rather than duplicated by callers.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*Material elevation presets*"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
