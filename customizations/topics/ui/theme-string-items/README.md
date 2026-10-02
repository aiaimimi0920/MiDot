# Theme String Items

## Intent and problem

Store semantic text and icon-font glyph defaults in `Theme` resources instead of
hard-coding them in individual controls.

## Behavior contract

- `Theme::DATA_TYPE_STRING` is a first-class item type with set, get, has,
  rename, list, clear, merge, variation, and serialization support.
- An explicitly stored empty string still counts as an existing theme item.
- `Control` and `Window` expose string lookup and local string overrides with the
  same precedence and cache invalidation rules as other theme item types.
- The Theme editor can create, import, edit, rename, and remove string items.
- Documentation generation identifies string theme items as `String`.

## Legacy origin

- Source repository: `Z:\project\mimi_godot`, branch `mimi`.
- Personal Material UI source commit:
  `36131c16630657d5161c2cd35e6cc78e0fef0626` (`update material_ui2`).
- The legacy monolith carried this feature as `DATA_TYPE_STR` with storage under
  `strs/`. The modern port preserves the behavior while using the explicit
  `DATA_TYPE_STRING` name and `strings/` paths consistently across current Godot
  Theme APIs.

## Consolidated source

- `personal/main` commit:
  `cdded424f1496efeff24e727c8d5b24d1c43baa1`.

## Dependencies

- `ui.dynamic-color-theme`

## Important source areas

- `scene/resources/theme.*`
- `scene/gui/control.*`
- `scene/main/window.*`
- `editor/scene/gui/theme_editor_plugin.*`
- `editor/doc/doc_tools.cpp`
- `tests/scene/test_theme.cpp`, `test_control.cpp`, and `test_window.cpp`

## Conflict guidance

Port strings through every generic `Theme::DataType` switch. Preserve explicit
empty-string existence, variation lookup, local-override precedence, property
paths under `strings/` and `theme_override_strings/`, and editor import behavior.
Do not silently map strings onto constants or metadata.

## Verification

Run the topic verifier, build with tests, and execute:

```powershell
.\bin\godot.windows.editor.dev.x86_64.console.exe --headless --test `
  --test-case="*String theme*"
```

Also execute the Control and Window `String theme overrides` cases.

## Upstream status

Active personal feature; no exact equivalent exists in the audited upstream.
