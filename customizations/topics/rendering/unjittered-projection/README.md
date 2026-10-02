# Exact unjittered camera projection

## Intent and contract

Forward+ spatial vertex and fragment shaders can read
`UNJITTERED_PROJECTION_MATRIX` when `HAS_UNJITTERED_PROJECTION_MATRIX` is defined.
This is the renderer's depth-corrected projection before temporal jitter, not a
subtractive reconstruction from the jittered matrix. It is read-only and does
not replace the raster projection or alter native motion-vector generation.
Mobile and Compatibility do not advertise this capability; callers must guard
their use. Unconditional unsupported-renderer use is not a supported API.

The CPU/GPU scene UBO layouts both include the main and per-eye matrices.
Forward+ multiview selects `ViewIndex`; previous-frame storage uses previous
camera/per-eye projection inputs. This implementation is not XR certification.
The shared scene UBO grows by three matrices (192 bytes per current/previous
scene record with MAX_VIEWS=2); rebuild all dependent renderers/effects together.

Consumer shaders remain responsible for keeping depth texture coordinates in
the same mapping. Merely ignoring jitter during camera matching is insufficient.
Never replace exact camera identity tests with an epsilon to imitate this API.

## Source and replay boundaries

One topic trailer on the authoritative `engine/personal/main` commit:

```text
Godot-Patch-Topic: rendering.unjittered-projection
```

No dependency on another personal topic. Exact commit/patch hashes belong to
generated `stack.lock.json`; do not edit generated patches. On rebase, preserve
CPU/GPU UBO member order, current/previous initialization, eye selection and
renderer capability boundaries together. Do not silently treat the main camera
as every XR eye or add temporal jitter twice.

## Verification

`scripts/verify-topic.ps1 -Topic rendering.unjittered-projection` checks source
shape only. Consumer tests in sibling RoleNPR cover exact vertex/fragment GPU
matrix identity, stable UV reconstruction, offset-bypass negative control,
orthographic/frustum internal scaling, nearby-camera rejection, moving-camera
same-phase HDR/display repeat controls and unchanged default images. The
`run_post_light_rejections.ps1 -IncludeTemporal` consumer gate includes the two
read-only assignment rejections. These are distinct from complete temporal
quality, arbitrary render modes, XR or cross-device certification.

Build all three Windows targets with the existing isolated SCons 4.8.1 recipe:

```text
platform=windows target=<editor|template_debug|template_release>
dev_build=no tests=no debug_symbols=no use_static_cpp=no accesskit=no
d3d12=no angle=no extra_suffix=postlight object_prefix=postlight_ -j6
```

Validate matching template runtime behavior and archive clean commit identities
before promoting any build. Existing validated export artifacts remain intact.
