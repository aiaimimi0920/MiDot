# Custom Icon Font

## Intent and problem

Let projects supply one icon font for the engine's text-glyph UI features without
hard-coding a font asset into the engine or replacing Godot's normal fallback
font.

## Behavior contract

- `gui/theme/custom_icon_font` accepts an optional `Font` resource path.
- ThemeDB loads the resource defensively. A missing, invalid, or wrong-typed
  resource leaves the icon font empty and never dereferences an invalid `Ref`.
- The loaded font is exposed through `ThemeDB.fallback_icon_font`, independently
  from the normal text fallback font.
- The default theme propagates the font to the text-icon items used by Button,
  TextureButton, OptionButton, MenuButton, and ColorPickerButton.
- With no configured font, stock Godot rendering remains unchanged.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- The legacy loader called `set_name()` before checking whether the resource was
  valid. This port preserves the feature while fixing that null-`Ref` failure.

## Consolidated source

- `personal/main` commit:
  `92f3590173207c59347d2a38b0d6c1b85018f254`.

## Dependencies

- `ui.button-text-icon`
- `ui.texture-button-state-text`

## Important source areas

- `scene/theme/theme_db.*`
- `scene/theme/default_theme.*`
- `doc/classes/ProjectSettings.xml`
- `doc/classes/ThemeDB.xml`
- `tests/scene/test_theme.cpp`

## Conflict guidance

Keep the icon font separate from `fallback_font`. Validate both resource
existence and type before assigning or naming it. When default-theme signatures
change upstream, continue passing the same optional font to every control that
owns a `text_icon_font` item; do not make a configured icon font mandatory.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --test `
  --test-case="[ThemeDB] Fallback icon font"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
