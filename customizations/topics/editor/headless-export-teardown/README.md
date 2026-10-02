# Headless Export Teardown Guard

## Intent and problem

Prevent deferred editor-filesystem work from re-entering editor-only code after
the `EditorNode` singleton has already been destroyed during a command-line
export failure. The old null guard returned `false`, which classified teardown
as GUI mode and allowed the pending update path to continue into dead editor
state.

## Behavior contract

- `EditorNode::is_cmdline_mode()` returns `true` when no editor singleton exists.
- Deferred editor-filesystem work stops once headless editor teardown begins.
- A failed command-line export reports its original export error and exits with
  code 1 instead of dereferencing destroyed editor state and exiting with 139.
- Normal live-editor behavior continues to use the singleton's `cmdline_mode`
  value.

## Consolidated source

- `personal/main` commit:
  `f5c1a46be5cc699a6e9ef6b78c30b2a61621a812`.

## Dependencies

None.

## Important source areas

- `editor/editor_node.cpp`
- `editor/file_system/editor_file_system.cpp`
- Command-line export error and editor teardown paths

## Conflict guidance

Preserve the lifecycle rule rather than the exact helper implementation: once
the editor singleton is gone, callers must not continue GUI-only filesystem,
documentation, or scene-group work. If upstream gains an explicit teardown
state or cancels all pending work before singleton destruction, prefer that
newer ownership model and drop this guard when it is redundant.

## Verification

Run the static topic verifier and the deterministic failed-export smoke:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\customizations\topics\editor\headless-export-teardown\verification\run.ps1
```

The smoke creates a controlled minimal project, intentionally exports to a
missing parent directory, requires exit code 1, rejects the former singleton
null diagnostic, and cleans only its own project and newly leaked matching Godot
processes.

## Upstream status

The defect reproduced on the locked upstream-based editor and was not caused by
an earlier personal patch. Keep the guard until upstream provides an equivalent
headless teardown boundary.
