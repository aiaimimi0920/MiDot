# Legacy Main-Screen Dock Cleanup

## Intent and problem

Prevent deprecated `EditorPlugin` main-screen compatibility wrappers from
surviving plugin removal. `LegacyMainScreenContainer` creates each wrapper
`EditorDock`, while `EditorDockManager::remove_dock()` only unregisters and
detaches it. Without owner-side cleanup, editor shutdown retained one dock and
its icon textures for every legacy main-screen plugin.

## Behavior contract

- Removing a legacy main-screen plugin unregisters its generated dock.
- The compatibility owner queues the detached dock for deletion.
- The plugin's `_dock` metadata is cleared so it cannot expose a stale object.
- The non-owning `EditorDockManager::remove_dock()` contract remains unchanged
  for callers that manage or reuse their own docks.

## Consolidated source

- `personal/main` commit:
  `661246efd04f9fb4c040e9ae48b987817414fb5f`.

## Dependencies

None.

## Important source areas

- `editor/editor_main_screen.cpp`
- `editor/docks/editor_dock_manager.cpp`
- Deprecated `EditorPlugin` main-screen compatibility lifecycle

## Conflict guidance

Preserve ownership rather than the exact deletion call: the component that
creates the compatibility dock must release it after manager unregistration.
Do not make `EditorDockManager::remove_dock()` delete arbitrary caller-owned
docks. If upstream removes this compatibility path, drop the topic instead of
recreating obsolete APIs.

## Verification

Run the static topic verifier and the minimal legacy-plugin shutdown smoke:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\customizations\topics\editor\legacy-main-screen-dock-cleanup\verification\run.ps1
```

The smoke loads and unloads one deprecated main-screen plugin, requires a clean
editor exit, rejects leaked `EditorDock`/texture diagnostics, and cleans only
its own project and newly leaked matching Godot processes.

## Upstream status

The leak reproduced in the upstream compatibility lifecycle used by existing
third-party plugins. Keep the topic until upstream releases equivalent
owner-side cleanup or removes the deprecated path.
