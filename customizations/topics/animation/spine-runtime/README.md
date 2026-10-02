# Spine Runtime Integration

## Intent and problem

Load, edit, animate, and render Spine skeleton assets as first-class Godot
resources and nodes.

## Behavior contract

- Spine atlas, skeleton data, animation state, tracks, bones, slots, skins, and
  events are exposed through Godot objects.
- `SpineSprite` renders the skeleton and participates in editor workflows.
- The module retains the final behavior after the historical runtime replacement
  and compile fixes.

## Legacy origin

- Original Godot line: 4.2 development history.
- Initial integration:
  `35b77c26abb6c0fee744e3c6e7924fdfd4b47fb4`.
- Runtime replacement and final fixes:
  `27c49058371ad5648aaafd4953151a9b99553fbe` and
  `05a507fa780f41cdb4966d53389be9b21ef1d3ac`.
- `c0c8a9ceab202642dcd43924fcc382ccbe1900ec` also contains a
  later `SpineSprite` adjustment included in the final legacy tree.

## External upstream

- Repository: <https://github.com/EsotericSoftware/spine-runtimes>
- Compatibility branch: `4.1`
- Release: `4.1.56`
- Commit: `77a5db0ec6d16331f5efbaa7662bba9355bd3424`
- License: Spine Runtimes License Agreement
- Update policy: follow 4.1 maintenance updates only. Runtime and Spine Editor
  export major/minor versions must match; a move to 4.2+ requires coordinated
  project-asset and export-pipeline migration.

## Consolidated source

- `personal/main` commit:
  `380a06d5f6639e714da91b092a04672e019bac67`.
- Rendering-boundary hardening follow-up:
  `dc4089cb4f97e3424d5cb12c07441b9a852eb2cd`.

## Dependencies

No patch-stack topic dependency. A valid Spine Editor license is required by the
vendored Spine Runtime license.

## Important source areas

- `modules/spine_godot/`
- `modules/spine_godot/spine-cpp/`
- Spine editor plugin, resources, nodes, and script bindings

## Conflict guidance

Keep vendor runtime changes separate conceptually from Godot glue. Preserve the
Spine license header. Adapt rendering, ResourceLoader, inspector, and editor APIs
to current Godot without replacing newer upstream ownership/lifetime rules.

## Verification

Run the topic verifier, compile an editor with the module, import a licensed
sample skeleton, render it, and exercise animation switching and editor reload.

The license-independent smoke suite validates class registration, empty
resource/node behavior, editor initialization, safe rejection of a truncated
Spine binary, synthetic atlas loading, and missing-texture error propagation:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\customizations\topics\animation\spine-runtime\verification\run.ps1
```

The synthetic atlas and SVG exercise only the atlas/texture boundary. This smoke
suite does not replace the licensed skeleton import/render test. No licensed
Spine asset is stored in this patch stack.

Repeat the suite under stress after renderer or lifecycle changes. Slot counts,
triangle indices, UV/vertex lengths, atlas textures, and rendering-server
availability must be validated before accessing runtime-owned buffers or RIDs.

## Upstream status

Active external integration; not patch-equivalent to cached upstream master.
The vendored runtime is current on the asset-compatible 4.1 maintenance line.
