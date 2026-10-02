# Selective surface geometry decode

Topic: `geometry.surface-decode`

## Contract

RenderingServer exposes `mesh_surface_get_geometry_arrays` and
`mesh_surface_get_geometry_blend_shape_arrays`. The base getter preserves
vertex, color, bones, weights and index slots in the existing ARRAY_MAX layout;
the morph getter preserves only vertices, with original shape order/count.
Other slots are null. Both return independent read-only snapshots of the mesh.

Compressed positions are decoded without allocating coupled normals/tangents.
The original full-array APIs retain their behavior. Mesh topology, attributes
used for rendering, LODs and morph coordinates are not modified. Invalid surface
and malformed blend-shape handling follows the existing getters. This does not
repair the separate compressed-morph writer stride issue.

## Verification

`scripts/verify-topic.ps1 -Topic geometry.surface-decode` checks source presence
only. Runtime validation in RoleNPR uses `tests/geometry_decode_regression.gd`:
actual deformed body/two hair meshes, compressed static and uncompressed morph
surfaces, 4/8 bone weights, 2D nonindexed positions, 65536/65537 vertex index
boundaries, authored morph positions and mutation isolation.

Source evidence: 286 checks, 12 reversed-order allocator samples and 36 warm
timing batches; native/fallback snapshot memory checks each 109; lifecycle 23;
dynamic picking 86; default 36 stage images exact. See
`RoleNPR/docs/NPR_GEOMETRY_SELECTIVE_DECODE.md` for identities and limits.
Editor allocator request peaks are not RSS, GPU memory, whole-BVH or FPS claims.
Matched export templates and packaging require separate validation.
