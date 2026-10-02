# Button Text Icon

## Intent and problem

Allow a `Button` icon slot to render a translated text glyph when no texture icon
is available, so icon fonts and theme strings can replace small texture assets.

## Behavior contract

- A real `Button.icon` or theme icon always takes precedence.
- A non-empty per-node `text_icon` overrides the theme string `text_icon`.
- Text icons use independent `text_icon_font` and `text_icon_font_size` items.
- The shaped glyph participates in alignment, translation, RTL layout,
  `icon_max_width`, expanded sizing, and minimum-size calculation.
- Empty node and theme values preserve stock text-only behavior.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- The modern port replaces the legacy font-file-name and hexadecimal-codepoint
  conversion with Godot's current translated text shaping APIs.

## Consolidated source

- `personal/main` commit:
  `bf3bddf9cd610bf4d8ecea2b7455755b8f3a3bf0`.

## Dependencies

- `ui.theme-string-items`

## Important source areas

- `scene/gui/button.*`
- `scene/theme/default_theme.cpp`
- `doc/classes/Button.xml`
- `tests/scene/test_button.cpp`

## Conflict guidance

Retain texture-icon priority and use the current `TextParagraph` shaping path.
When upstream changes alignment or minimum-size logic, update both drawing and
measurement together. Do not restore the legacy codepoint parser.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*text glyphs as fallback icons*"
```

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
