# Script Documentation API

## Intent and problem

Preserve the personal tooling contract that reads structured documentation from
an arbitrary `Script`, without coupling it to the separate `GDScriptHelper`
parser and completion surface.

## Behavior contract

- `Script.get_script_documentation_list()` returns an array of dictionaries.
- Each class record includes its name, inheritance, descriptions, tutorials,
  enums, constants, signals, variables, and methods where available.
- Nested argument records preserve names, types, enumerations, and default
  values; an undocumented signal argument type is represented as `Variant`.
- Documentation remains editor-only. Non-tools builds return an empty array.
- The dictionary schema remains compatible with the legacy personal API even
  when upstream changes its internal `DocData` representation.

## Legacy origin

- Audited personal source state: `Z:\project\godot-4-4-1`.
- The legacy behavior was adapted to the current `DocData::ClassDoc` model
  rather than copying obsolete documentation structures.

## Consolidated source

- `personal/main` commit:
  `dfb6dc26b39b98e68ad7a95980b78e379d96ad01`.

## Dependencies

None. The API works with every `Script` implementation that supplies a
documentation class and does not depend on `scripting.gdscript-helper`.

## Important source areas

- `core/object/script_language.*`
- `doc/classes/Script.xml`
- `tests/modules/gdscript/test_script_documentation.cpp`

## Conflict guidance

When upstream changes `DocData`, adapt fields into the documented legacy
dictionary schema. Do not expose editor-owned documentation objects or return
pointers whose lifetime is shorter than the array.

## Verification

Run the topic verifier, compile an editor build with tests, and run the focused
`Structured documentation is exposed to scripts` test case.

## Upstream status

Active personal compatibility API; current upstream does not expose the same
structured documentation list through `Script`.
