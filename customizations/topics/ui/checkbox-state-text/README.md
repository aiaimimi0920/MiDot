# Check Control State Text

## Intent and problem

Allow CheckBox and CheckButton to render their checked, unchecked, disabled,
radio, and mirrored states as icon-font glyphs while retaining Godot's texture
icons as a safe fallback.

## Behavior contract

- CheckBox has separate state strings and semantic colors for checkbox and radio
  modes, including disabled variants.
- CheckButton has separate left-to-right and mirrored right-to-left strings and
  colors, including disabled variants.
- A non-empty state string takes priority only when `text_icon_font` is valid and
  the resolved font size is positive. Otherwise the corresponding texture is
  used.
- State glyphs are translated, centered in the icon slot, included in minimum
  sizing, constrained by Button's icon sizing, and use theme color-role
  resolution.
- Default state strings are populated only when the project configured a custom
  icon font, so an unconfigured engine keeps stock Godot appearance.
- Missing state textures are handled safely instead of dereferencing an invalid
  `Ref`.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- Earlier personal toggle work was consolidated into the same net behavior. The
  port uses current Theme string, translation, font, RTL, and ColorRole APIs
  instead of copying the old TextParagraph cache implementation verbatim.

## Consolidated source

- `personal/main` commit:
  `23998d8bb0ede7b7a38f70a65d42a5a50fe8bb37`.

## Dependencies

- `ui.custom-icon-font`
- `ui.dynamic-color-theme`

## Important source areas

- `scene/gui/check_box.*`
- `scene/gui/check_button.*`
- `scene/theme/default_theme.cpp`
- `doc/classes/CheckBox.xml`
- `doc/classes/CheckButton.xml`
- `tests/scene/test_button.cpp`

## Conflict guidance

Preserve the exact state matrix: checkbox versus radio for CheckBox, and LTR
versus mirrored RTL for CheckButton, each with enabled and disabled variants.
Keep usable state text ahead of textures, but never suppress texture fallback
when text or font data is absent. Update drawing and minimum-size logic together
when upstream changes Button icon layout.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --test `
  --test-case="[SceneTree][CheckBox] resolves state text glyphs"
.\bin\godot.windows.editor.dev.x86_64.console.exe --test `
  --test-case="[SceneTree][CheckButton] resolves mirrored state text glyphs"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
