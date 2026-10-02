# Dynamic Color Theme: Code Review and Development Plan

Date: 2026-09-22

Scope: `personal/main` commit `5884522a3dd1b906d8d08086decc30dfa53e3202` (`ui.dynamic-color-theme`), including `ColorScheme`, `ColorRole`, Theme/Control/Window integration, StyleBox resolution, Theme Editor integration, the vendored Material Color Utilities implementation, and the topic tests.

## Review conclusion

The feature is structurally complete and the topic verifier passes, but it should not yet be treated as release-ready. The most important correctness issue is that dynamic role-backed colors can be returned by `get_theme_color()` while `has_theme_color()` reports false. This affects both `Control` and `Window`, and it can also prevent `_scale` colors from being applied. Theme color-role type removal/renaming helpers also mutate lookup maps without invalidating theme users when called directly. Texture-derived schemes do not observe later texture changes, so the advertised adaptive behavior stops after the first palette extraction.

The current tests prove the happy path for direct colors, schemes, role overrides, StyleBox resolution, fallback signals, and Material golden values. They do not cover the failure paths above, resource serialization, editor round trips, texture updates, or cache invalidation after type operations.

## Findings

### F-01 — Dynamic colors are invisible to `has_theme_color()` (high)

`Control::get_theme_color()` resolves a role item named `<color>_role` and a scheme item named `<color>_scheme` (`scene/gui/control.cpp:3919-3963`), but `Control::has_theme_color()` only checks the static color override and `DATA_TYPE_COLOR` theme map (`scene/gui/control.cpp:4225-4239`). `Window` has the same mismatch (`scene/main/window.cpp:2839-2913` and `scene/main/window.cpp:3097-3112`).

Consequences:

- A role-backed color can be readable while `has_theme_color()` returns `false`.
- `get_theme_color()` uses `has_theme_color(<name>_scale)` before applying a scale, so a role-backed scale can be silently skipped.
- Scripts and editor code cannot reliably feature-detect a dynamic color using the public `has_*` API.

Plan: make `has_theme_color()` account for the same effective resolution chain as `get_theme_color()`, including local overrides, role items, scheme availability, and inherited theme types. Add matching Control and Window regression tests, including a role-backed `<name>_scale`.

### F-02 — Direct color-role/scheme type mutations can skip invalidation (medium)

`Theme::remove_color_role_type()` and `Theme::rename_color_role_type()` directly mutate `color_role_map` without `_emit_theme_changed()` (`scene/resources/theme.cpp:1076-1087`). The corresponding color-scheme type removal uses freeze/propagate logic (`scene/resources/theme.cpp:1183-1196`), while color-scheme type rename also omits notification (`scene/resources/theme.cpp:1198-1205`).

The higher-level `remove_type()` and `rename_type()` wrappers emit a final theme change, so the common public path is covered. The per-data-type helpers are still callable from C++ and are used by the wrappers; calling them directly can leave cached controls and windows stale. The asymmetry also makes future editor or API changes easy to get wrong.

Plan: use the same freeze/propagate pattern for all four operations, disconnect any resource signals before discarding a scheme map, keep the outer wrapper notifications single-shot, and add tests that warm a Control/Window cache, mutate the type, then assert notification and lookup results.

### F-03 — Texture-derived schemes do not react to texture changes (high)

`ColorScheme::set_source_texture()` stores the texture and immediately computes a scheme (`modules/color_scheme/color_scheme.cpp:91-100`), but it never connects to the source texture's `changed` signal. A later `ImageTexture::set_image()` or equivalent resource update therefore leaves the palette stale until the texture is manually re-assigned.

Plan: connect and disconnect the texture resource signal using a dedicated callback, mark the scheme dirty, recompute on the main thread, and emit the normal resource change signal. Add a test that updates one texture in place and verifies the generated source/palette changes.

### F-04 — Full-image quantization runs synchronously during property mutation (medium)

`ColorScheme::_create_scheme_content()` copies every pixel into a `std::vector` and runs Celebi quantization (`modules/color_scheme/color_scheme.cpp:48-73`). The operation is invoked synchronously by `set_source_texture()`, `set_source_color()`, `set_dark()`, and `set_contrast_level()` (`modules/color_scheme/color_scheme.cpp:76-128`). Large source textures can block the main thread and create avoidable peak allocations.

Plan: define a bounded extraction policy (for example, downsample to a configurable maximum pixel count), cache extraction by texture identity/content revision, and document that palette generation is synchronous. If asynchronous generation is desired, add a generation token so stale worker results cannot overwrite newer settings.

### F-05 — Invalid or unreadable source textures silently become a black-color scheme (medium)

When a texture is empty, unavailable, or cannot be decompressed, `_create_scheme_content()` keeps `source_color`. `set_source_texture()` first resets `source_color` to `Color()` (`modules/color_scheme/color_scheme.cpp:96-99`), so failure can silently produce a black-derived palette without an error or status signal (`modules/color_scheme/color_scheme.cpp:50-68`).

Plan: retain the last valid extracted source, expose an extraction status/error, and define an explicit fallback policy in the API and docs. Add tests for empty, compressed, and failed-decompression textures.

### F-06 — Override precedence is implicit and currently favors role overrides (medium)

In both `Control` and `Window`, a role override is resolved before a static color override (`scene/gui/control.cpp:3939-3949`; `scene/main/window.cpp:2864-2874`). The current tests intentionally assert this behavior (`tests/scene/test_control.cpp:68-75`, `tests/scene/test_window.cpp:81-88`), but the topic contract only says to preserve override precedence and does not document why a role override should supersede an explicit static color override.

Plan: write the precedence table into the public Theme/Control/Window documentation, then test every combination: local static color, local role, inherited static color, inherited role, explicit scheme, default scheme, and fallback scheme. If static overrides are intended to win, change both classes together; if role overrides are intended to win, preserve the code and document it as a deliberate rule.

### F-07 — Control and Window duplicate the resolution implementation (medium)

Theme lookup, role resolution, scheme fallback, StyleBox cloning, signal wiring, and cache invalidation are implemented twice in `Control` and `Window`. The identical `has_theme_color()` omission in both classes demonstrates the maintenance risk.

Plan: first align behavior with shared regression tests; then extract a small internal helper for effective dynamic color and scheme resolution without changing the public API. Keep Control/Window-specific override storage and notification behavior local.

### F-08 — Serialization and editor behavior are under-tested (medium)

The implementation adds dynamic properties to Theme and Control/Window, but the topic tests do not perform a resource save/load round trip or exercise Theme Editor create/import/edit/remove behavior. The README behavior contract explicitly includes serialization and editor integration.

Plan: add text resource round-trip tests for Theme, ColorScheme, all three dynamic StyleBox classes, and Control/Window overrides. Add an editor smoke test that creates a role and scheme, edits them, removes them, saves, reloads, and checks that no stale item remains.

## Missing or incomplete behavior to decide

1. Whether a source texture is sampled once or remains live as its image changes.
2. Whether palette extraction is allowed to block the main thread for large textures.
3. Whether a static color override or a role override has higher precedence.
4. Whether failed texture extraction falls back to the previous valid palette, the explicit source color, or a generated black palette.
5. Whether `ColorScheme` needs an explicit extraction status and last-valid-source API.
6. Whether all dynamic colors should be reported by `has_theme_color()` or whether a separate `has_theme_dynamic_color()` API is preferred. The current implementation should still make the existing API internally consistent.

## Development phases

### Phase 0 — Establish the contract

- Record the precedence and fallback table in `doc/classes/Theme.xml`, `Control.xml`, `Window.xml`, and `ColorScheme.xml`.
- Decide live texture updates, failure fallback, and synchronous/asynchronous extraction.
- Keep the existing topic boundary and preserve the vendored Material algorithm snapshot.

### Phase 1 — Correctness fixes

- Fix `has_theme_color()` in Control and Window, including role-backed scale lookup.
- Add change propagation for color-role and color-scheme type helpers, while keeping the outer `remove_type()`/`rename_type()` notifications single-shot.
- Add source texture signal connection, disconnection, and stale-resource cleanup.
- Preserve authored StyleBox colors and verify explicit scheme versus default scheme precedence.

### Phase 2 — Regression coverage

- Add Control and Window tests for `has_theme_color()` parity and dynamic scale colors.
- Add cache invalidation tests after type removal and rename.
- Add live texture update and invalid texture fallback tests.
- Add Theme/ColorScheme/StyleBox/Control/Window serialization round trips.
- Add editor smoke coverage for create/import/edit/remove.

### Phase 3 — Performance and maintainability

- Bound or cache texture quantization work.
- Extract shared dynamic-color resolution helpers used by Control and Window.
- Audit signal connection/disconnection pairs and add assertions around resource lifetime.
- Add a stress test for repeated scheme changes and many controls sharing one scheme.

### Phase 4 — Verification and release gate

- Run `customizations/scripts/verify-topic.ps1 -Topic ui.dynamic-color-theme`.
- Build the editor with `MODULE_COLOR_SCHEME_ENABLED` and run the focused color-scheme, Theme, Control, and Window tests.
- Run a runtime smoke scenario covering source color changes, texture changes, light/dark mode, contrast, inherited themes, windows, StyleBoxes, default theme resources, and Theme Editor save/load.
- Confirm no generated graph or test artifacts are included in the patch stack.

## Verification performed for this review

- Topic verifier: passed (`ui.dynamic-color-theme`).
- Graphify code index: generated for `engine/core` with 12,526 nodes and 30,816 edges; used to locate the core object/theme relationships.
- Full editor build and runtime smoke suite: not run during this review.

## Recommended implementation order

Fix F-01, F-02, and F-03 first. They are externally observable correctness defects and can be covered with narrow regression tests. Then settle F-06's precedence contract, add serialization/editor coverage for F-08, and only afterward undertake the performance and helper extraction work in F-04 and F-07.
