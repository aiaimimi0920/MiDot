# Viewport parent draw-order invalidation

Changing a viewport parent must invalidate the renderer's cached topological
ordering, so an active source viewport renders before its active consumer on the
next draw. Reassigning the same parent is a no-op. No activation toggle, target
recreation or history clear is required to establish the new dependency.

## Replay boundary

One authoritative personal/main commit, with trailer:

```text
Godot-Patch-Topic: rendering.viewport-parent-order
```

The change belongs in RendererViewport::viewport_set_parent_viewport. Preserve
the dirty assignment after changing parent when rebasing; do not alter the
topological algorithm as part of this fix. There are no other topic dependencies.
Parent validity, inactive ancestors, cycles and orphan cleanup remain caller
responsibilities and are not repaired by this change.

## Verification

`scripts/verify-topic.ps1 -Topic rendering.viewport-parent-order` checks the
setter source shape only. Sibling RoleNPR's `tests/native_viewport_order.gd`
observes two real native viewport render-target callbacks at completed draw-frame
boundaries. Each case requires exactly two callbacks in the expected order for
four consecutive frames. It covers independent ordering, reparenting, detach and
the opposite dependency without changing active state after warmup.

The archived 0395f4b editor fails reparenting; the candidate passes all four cases.
Default Silver Wolf rendering retains 180 state checks, 14 image checks and 36
byte-exact stage PNGs. Matching templates and committed build archives are
separate acceptance gates; source-shape verification is not runtime proof.

Build recipe: SCons 4.8.1, platform=windows, target=editor/template_release/
template_debug, dev_build=no, tests=no, debug_symbols=no, use_static_cpp=no,
accesskit=no, d3d12=no, angle=no, extra_suffix=postlight,
object_prefix=postlight_, -j6. Preserve existing validated export artifacts.
