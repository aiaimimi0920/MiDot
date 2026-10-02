# TriangleMesh build allocation

Topic: `geometry.triangle-mesh-build`

## Contract

For `fc > 0` input triangles, the existing recursive full-binary BVH has
`fc` leaves and `fc - 1` internal nodes. Allocate `2 * fc - 1` scratch nodes
instead of `3 * fc`. Reserve `fc` entries in the temporary vertex-welding
HashMap to avoid repeated growth on typical indexed meshes. The reserve is
a capacity hint, not a limit: disconnected faces may require more entries.

No API, snapped coordinates, welding identity, normal calculation, triangle
order, partition algorithm, or query tie-breaking is changed. Requested node
count is reduced; physical allocator/RSS savings are not measured. Highly
duplicated geometry can over-reserve the hash map compared with incremental
growth. This is not a refit implementation or an all-mesh performance promise.

## Verification

`scripts/verify-topic.ps1 -Topic geometry.triangle-mesh-build` checks the source
contract only. Runtime evidence is maintained in the consuming RoleNPR project:

- `tests/triangle_build_capacity_regression.gd`: single/uneven/disconnected
  trees at 1, 2, 3, 17 and 257 triangles; 285 build/face-index/position checks.
- `tests/pick_skin_benchmark.gd`: actual body and two hair meshes, authored
  geometry and uniform/varied four/eight-bone skinning; independent triangle
  references, exact posed vertices, paired ray and face comparisons.
- `tests/dynamic_pick_regression.gd`: skin/blendshape changes, transformations,
  masks, replacement, wheel input and lazy invalidation.

See `RoleNPR/docs/NPR_BVH_BUILD_CAPACITY.md` for baseline/candidate identities,
timings, failure boundaries and promotion status. Do not interpret a successful
source-pattern check as a runtime or Release performance test.
