# Button State Layers

## Intent and problem

Allow Material-style interaction overlays to be drawn independently of the base
`Button` style box, avoiding duplicated complete styles for every state.

## Behavior contract

- Optional focus, hover, pressed, and hover-pressed style boxes are drawn over
  the normal state rendering.
- Hover-pressed falls back to the pressed layer when its dedicated layer is not
  defined.
- Focus is used only when no hover or pressed layer has already been selected.
- Missing state layers preserve stock `Button` rendering.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).

## Consolidated source

- `personal/main` commit:
  `e3ac8905e66965a049dcfc19fb775e1d66091cda`.

## Dependencies

None.

## Important source areas

- `scene/gui/button.*`
- `doc/classes/Button.xml`
- `tests/scene/test_button.cpp`

## Conflict guidance

Keep base style selection and drawing intact. State layers are optional overlays,
not replacements. Preserve hover-pressed fallback and focus precedence when
upstream changes `Button` draw modes.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*resolves optional Material state layers*"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
