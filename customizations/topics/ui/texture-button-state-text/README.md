# TextureButton State Text

## Intent and problem

Let a `TextureButton` use translated text or icon-font glyphs for each visual
state when a texture is unavailable, reducing small icon-asset duplication.

## Behavior contract

- Normal, pressed, hover, disabled, and focused states have independent text.
- State-specific fallback follows the same state priority as texture selection.
- State text participates in minimum size, alignment, `icon_max_width`, theme
  font selection, translation, and layout direction.
- Texture rendering retains priority when a state texture is available.
- Theme colors can be static or resolved through semantic `ColorRole` values.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- Legacy symbols include `set_text_normal`, state-specific translated strings,
  and `text_icon_font_size`.

## Consolidated source

- `personal/main` commit:
  `fd3d3181aada4b3250148ccae5b45af2eb317bae`.

## Dependencies

- `ui.dynamic-color-theme`

## Important source areas

- `scene/gui/texture_button.*`
- `scene/theme/default_theme.cpp`
- `doc/classes/TextureButton.xml`
- `tests/scene/test_texture_button.cpp`

## Conflict guidance

Preserve texture-first behavior and existing stretch/click-mask semantics. Port
state selection to upstream draw modes instead of replacing the entire drawing
method. Keep translated text shaping and minimum-size calculation synchronized.

## Verification

Run the topic verifier, build with tests, and execute both focused cases:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*State text*"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
