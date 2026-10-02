# TextureButton Hit-Test Script API

## Intent and problem

Allow scripts to query the same `TextureButton` hit test used by GUI input.
A rectangular `Control` bounds check cannot reproduce click-mask-aware hit
testing for stretched or tiled button textures.

## Behavior contract

- `TextureButton.has_point(point)` is callable from scripts and returns a
  boolean.
- `point` uses coordinates relative to the button's origin.
- The result preserves `TextureButton`'s existing click-mask, stretch, and tile
  behavior; the topic does not introduce a second hit-test implementation.

## Legacy origin

- Original repository: `Z:\project\godot-4-3-1` on its local `4.4` branch.
- Original personal commit: `03e0afddabd971b56aa778a8eeb0a6ec8bf3f494`.
- The legacy change only added the ClassDB binding and had no documentation or
  regression test.

## Consolidated source

- `personal/main` commit:
  `5dd62618b83d4733ed52354b8bba8d75cf7e4b89`.

## Dependencies

None.

## Important source areas

- `scene/gui/texture_button.cpp`
- `doc/classes/TextureButton.xml`
- `tests/scene/test_texture_button.cpp`

## Conflict guidance

Preserve one public script method that delegates to the existing
`TextureButton::has_point()` override. If upstream exposes an equivalent
click-mask-aware query on `Control` or `TextureButton`, remove the duplicate
binding and mark this topic as upstream-absorbed rather than retaining two APIs.

## Verification

Run the topic verifier, compile an editor with tests enabled, and execute the
focused doctest filter:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File ..\customizations\scripts\verify-topic.ps1 `
  -Topic ui.texture-button-hit-test-api

scons platform=windows target=editor dev_build=yes tests=yes
.\bin\godot.windows.editor.dev.x86_64.console.exe `
  --headless --test --test-case="*has_point is exposed to scripts*"
```

The regression test must execute one test case and three assertions; a zero-test
filter result is not sufficient.

## Upstream status

Active personal feature; no equivalent script binding exists in the cached
upstream master.
