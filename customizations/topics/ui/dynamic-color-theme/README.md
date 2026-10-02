# Dynamic Color Theme

## Intent and problem

Allow a Godot theme to derive coordinated colors from a source color or texture
and refer to semantic Material color roles instead of hard-coded colors.

## Behavior contract

- `ColorScheme` generates light/dark Material color values with configurable
  contrast.
- `ColorRole` is available to scripts and theme resources.
- `Theme`, `Control`, and `Window` resolve color-role and color-scheme items,
  including overrides and caches.
- Style boxes and supported GUI controls update their effective colors when the
  scheme changes.
- The Theme editor can create, import, edit, and remove both data types.
- Color items use `<name>_role`, `<name>_scheme`, and the color multiplier
  `<name>_scale`. Style items use `<name>_scheme`; their roles and scales remain
  properties of the StyleBox resource.
- A local dynamic role with an available scheme takes precedence over a local
  static color. A local `STATIC` role suppresses inherited dynamic roles and
  follows the normal static-color fallback. Effective dynamic colors also count
  as present for `has_theme_color()`.
- Dynamic overrides, including `default_color_scheme` and custom item names,
  survive duplication and text/binary scene round-trips on derived controls and
  windows. Theme editing preserves absent, stored-null, and valid scheme items
  separately through import, undo/redo, and disk reload.
- Texture content changes synchronously invalidate the extracted seed. Dark and
  contrast changes reuse that seed. Non-finite inputs are rejected; unreadable
  textures retain the black fallback and transparent textures retain the MCU
  default seed. Valid-input Material outputs and `get_color(STATIC)` are unchanged.
- Static built-in StyleBoxes can reuse their source. Dynamic and scripted styles
  retain isolation; resolved node colors never overwrite authored resource colors.

## Legacy origin

- Original Godot line: 4.2 development history.
- Final successful series starts at
  `e28a77fd8db08e009c3efe282f53533e0d1b21cf`, branches through
  `8d2c17a83eac013a0fb67307848ce5ceb3d99f62`, and ends at
  `c69dbaf9e042080ace0bbc7a38a62d0aca86f29a`.
- Failed sibling `97d7ae0e1d32473068584d1ae9a0f4773e35a31c` is explicitly
  excluded.
- Google Material Color Utilities is vendored with its upstream license.

## External upstream

- Repository: <https://github.com/material-foundation/material-color-utilities>
- Imported directory: `cpp/`
- Commit: `5b3618b16fdc3825e21d5679bafd144662088ea1` (2026-08-21)
- License: Apache-2.0
- Local import adaptations are limited to relative includes, standard C++/MSVC
  portability, and replacing otherwise unused Abseil containers/formatting with
  standard-library equivalents. Material algorithm output is protected by
  `tests/modules/color_scheme/test_color_scheme.cpp`.

## Consolidated source

- `personal/main` commit:
  `5884522a3dd1b906d8d08086decc30dfa53e3202`.
- The September 2026 optimization stack adds 13 reviewable commits from
  `c6963d8` through `3a6537d06bca0cbcacfa8f70952ad9451deba199`.
  The task/commit mapping is in the [final plan](FINAL_OPTIMIZATION_PLAN.md).

## Dependencies

No patch-stack topic dependency. The topic vendors Material Color Utilities C++.
The current personal engine requires `module_color_scheme_enabled=yes`, including
when `modules_enabled_by_default=no`. Unsupported disable combinations fail during
SCons configuration, before compilation.

## Important source areas

- `modules/color_scheme/`
- `thirdparty/material-color-utilities/`
- `scene/resources/theme.*` and style boxes
- `scene/theme/`
- `scene/gui/` and `scene/main/window.*`
- `editor/scene/gui/theme_editor_plugin.*`
- Core enum and Variant registration

## Conflict guidance

This topic intentionally crosses many upstream-hot files. Preserve semantic
lookup order, override precedence, cache invalidation, resource change signals,
and static-color fallback. Port each layer to current Theme APIs; never replace a
whole modern source file with its Godot 4.2 version.

## Verification

The earlier review probes are maintained in
[dynamic_color_theme_review](../../../tests/dynamic_color_theme_review/).
They emit observations, not a replacement for the current assertion-based suite.

Run the topic verifier and a full editor build. Runtime checks must switch source
color, dark mode, and contrast, then verify live updates in controls, windows,
style boxes, default theme resources, and Theme editor serialization.

The [verification runner](../../../tests/verify-dynamic-color-theme.ps1) supports
`Cpp`, `Runtime`, `Editor`, `Benchmark`, and `Template` modes. Run from the workspace
root with a freshly built engine, for example:

```powershell
$editor = 'engine/bin/godot.windows.editor.dev.x86_64.exe'
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/verify-dynamic-color-theme.ps1 -GodotPath $editor -Mode Cpp -OutputPath .validation/dynamic-theme/cpp
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/verify-dynamic-color-theme.ps1 -GodotPath $editor -Mode Runtime -OutputPath .validation/dynamic-theme/runtime
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/verify-dynamic-color-theme.ps1 -GodotPath $editor -Mode Editor -OutputPath .validation/dynamic-theme/editor
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/verify-dynamic-color-theme.ps1 -GodotPath $editor -Mode Benchmark -OutputPath .validation/dynamic-theme/benchmark
rtk powershell.exe -NoProfile -ExecutionPolicy Bypass -File customizations/tests/verify-dynamic-color-theme.ps1 -GodotPath engine/bin/godot.windows.template_release.x86_64.exe -EditorPath $editor -Mode Template -OutputPath .validation/dynamic-theme/template-release
```

`Template` exports the fixture to a PCK beside a copy of the selected executable;
it does not require command-line path overrides in the template. Runtime images
and resource round-trips are written to the fixture's `output/` in editor runs,
and beside the copied executable in template runs. Each mode checks its completion
marker and exit status and cleans up its own Godot processes.

The [2026-09-23 validation report](VALIDATION_2026-09-23.md) records source and
binary identities, exact results, benchmark scope, and the generated patch-stack
checks. Large-image first extraction remains synchronous and unsampled.

## Upstream status

Active personal engine feature; no exact patch-equivalent commit exists in the
cached upstream master. Its Material Color Utilities snapshot is pinned to the
recorded upstream commit and verified by light/dark/contrast golden tests.
