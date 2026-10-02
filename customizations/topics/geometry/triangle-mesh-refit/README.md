# Opt-in TriangleMesh face refit

Topic: `geometry.triangle-mesh-refit`.

Adds `TriangleMesh.refit_from_faces(faces)`. No existing caller is switched.
The tree must already be built and face count must remain unchanged. Invalid
count, unbuilt tree and non-finite input return false without mutation.

The method updates snapped face corners, original-input normals, leaf AABBs
and internal AABBs in child-before-parent order. It preserves partition,
original face ordering and surface IDs. It replaces the original welded vertex
mapping with independent corners so previously welded vertices may separate.
This can increase vertex memory. Refitting does not guarantee fresh-build
traversal speed, and equal-distance overlapping-face ties can select a different
face than a fresh partition. Callers must serialize updates and queries.

Runtime tests in RoleNPR:

- `tests/triangle_refit_regression.gd`: 10,321 checks, single/uneven trees,
  partition-reversing movement, normal/face/ray/segment equivalence, welded
  corners separating and rejoining, transactional rejection.
- `tests/pick_refit_benchmark.gd`: actual body and both hair meshes, 24 Skin
  and BlendShape updates, refitted-tree queries through the actual picker,
  independent world-triangle and GPU depth references. The test deliberately
  times native work separately; its preparation still builds a reference BVH.
  It does not prove an optimized production bridge or full frame-time gain.

`verify-topic.ps1` checks source contracts only, not execution. Full local
evidence and remaining integration work: `RoleNPR/docs/NPR_BVH_REFIT.md`.

## Direct indexed bridge

`update_from_indexed_surfaces(vertex_arrays, index_arrays, refit=false)` expands
paired packed arrays directly on the CPU, then builds/refits without a rendering
ArrayMesh, GPU upload/readback, or intermediate BVH. Invalid types, indices,
counts, non-finite referenced positions and empty geometry leave the tree intact.
An empty index array means no faces, not implicit sequential topology. A build
matches `create_from_faces` surface-ID semantics (all zero).

RoleNPR now selects this capability when available, retaining the legacy bridge
on old engines. Mesh/mask changes rebuild, and each partition is rebuilt after
at most eight refits. The age cap bounds partition lifetime, not traversal time.
`triangle_indexed_regression.gd` checks the API; the real-character
`pick_indexed_pipeline_benchmark.gd` compares the complete posed update with
the retained fallback and verifies periodic builds and independent query/depth
references. `dynamic_pick_regression.gd` runs on both new and old engines.
Production integration evidence: `RoleNPR/docs/NPR_BVH_INDEXED_PIPELINE.md`.
