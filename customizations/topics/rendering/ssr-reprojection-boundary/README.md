# SSR history reprojection boundary

Reject history coordinates behind the previous camera before perspective
division. Clamp each screen-edge margin to nonnegative before multiplication:
two negative margins at an out-of-image corner must not produce positive
validity and sample the clamp-to-edge history texture.

Positive-w, in-image projection and fading retain their original arithmetic.
The change does not clear history, disable SSR, or change roughness filtering.

## Replay boundary

One authoritative personal/main commit, with trailer:

```text
Godot-Patch-Topic: rendering.ssr-reprojection-boundary
```

The original implementation at 9d709eec4f81199c5b4732e572ffdabda8800dfe divides
by w unconditionally and multiplies signed margins. Preserve both guards in
servers/rendering/renderer_rd/shaders/effects/screen_space_reflection.glsl when
rebasing. There are no other topic dependencies. This is a local patch; no
upstream acceptance is claimed.

## Verification

`scripts/verify-topic.ps1 -Topic rendering.ssr-reprojection-boundary` checks
source shape only. Sibling RoleNPR's `tests/character_ssr_motion.gd` captures
real native SSR and a private replay of the exact native GLSL on the same GPU
inputs. `tests/analyze_ssr_motion.py` requires every replay RGBA value, after
RGBA16F RTZ storage conversion, to equal the native attachment before checking
UV and w validity. This is diagnostic replay, not an independent CPU ray tracer.

The 9d709ee negative control exposes four diagonal cuts and a reverse-camera
case. The candidate rejects all invalid history samples over 16 captures,
retaining significant static, small-motion and immediate recovery reflections.
Static reflection, half-size history, resize and default Silver Wolf captures
remain unchanged. Evidence and limitations are recorded in sibling RoleNPR's
docs/NPR_SSR_REPROJECTION_BOUNDARY.md. Matching committed editor and templates
are separate acceptance gates. Half-size motion and multiview are not covered
by the initial candidate results.

Build recipe: SCons 4.8.1, platform=windows, target=editor/template_release/
template_debug, dev_build=no, tests=no, debug_symbols=no, use_static_cpp=no,
accesskit=no, d3d12=no, angle=no, extra_suffix=postlight,
object_prefix=postlight_, -j6. Preserve existing validated export artifacts.
