# Color Role Transform

## Intent and problem

Represent reusable transformations of Material semantic colors without expanding
the global `ColorRole` enum or baking derived colors into every theme item.

## Behavior contract

- A `ColorRoleTransform` resolves either a semantic role from `ColorScheme` or a
  static color.
- Scaling, inversion, darkening, lightening, interpolation, and clamping run in
  a stable documented order.
- Interpolation targets may be static, another role, or another transform.
- Cyclic transform references fail safely instead of recursing indefinitely.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- The old change was a monolithic Material UI snapshot. This topic extracts its
  role-transformation intent into a current-Godot resource rather than copying
  an obsolete global enum design.

## Consolidated source

- `personal/main` commit:
  `bb8d8c4c70023c73c05046ac385e4830d79424dd`.

## Dependencies

- `ui.dynamic-color-theme`

## Important source areas

- `modules/color_scheme/color_role_transform.*`
- `modules/color_scheme/register_types.cpp`
- `modules/color_scheme/doc_classes/ColorRoleTransform.xml`
- `tests/modules/color_scheme/test_color_role_transform.cpp`

## Conflict guidance

Keep the transform as a resource layered on top of `ColorScheme`. Preserve
operation order, static-color operation, nested targets, and cycle rejection. Do
not reintroduce legacy enum values into the process-wide `ColorRole` enum.

## Verification

Run the topic verifier, build the editor with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*ColorRoleTransform*"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
