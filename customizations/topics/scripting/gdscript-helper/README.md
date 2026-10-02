# GDScript Helper API

## Intent and problem

Expose the editor's GDScript parser, completion, and symbol lookup to scripts
used by external tooling and editor automation.

## Behavior contract

- Validate source text and retrieve structured errors and functions.
- Request completion options and hints for source text and an optional owner.
- Look up symbols and return structured lookup result kinds.

## Legacy origin

- Original Godot line: 4.2 development history.
- Initial import and fixes:
  `a99bfbe5fb9a43d94c21c959a8a34914e8517def` and
  `5b6b9edfdf58c5878690190c3b056e040b039df9`.
- The final legacy files also contain the compatible adjustments from
  `e28a77fd8db08e009c3efe282f53533e0d1b21cf`.

## Consolidated source

- `personal/main` commit:
  `6c29e2c8bed2e64cfbf8fd5310b518f56386fac3`.

## Dependencies

None.

## Important source areas

- `modules/gdscript/gdscript_helper.*`
- `modules/gdscript/register_types.cpp`

Structured `Script` documentation is maintained independently by
`scripting.script-documentation-api` so either compatibility surface can be
rebased and repaired without coupling unrelated conflicts.

## Conflict guidance

Adapt to current parser and completion result types. Keep returned dictionaries
stable for script callers; do not expose stale raw pointers or editor-only
objects whose lifetime is shorter than the result.

## Verification

Run the topic verifier, compile the GDScript module, and execute validation,
completion, and lookup smoke cases from GDScript.

## Upstream status

Active personal feature; modern upstream has no `GDScriptHelper` class.
