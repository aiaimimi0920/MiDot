# Archived Changes Not Automatically Reapplied

## Root PNG branding

Commit `03e0afddabd9` replaces root `icon.png` and `logo.png` but leaves the SVG
assets unchanged. Current Godot stores generic logo assets under `misc/logo` and
also carries separate application and platform export resources. Replacing only
the two historical PNGs would produce inconsistent branding, so their exact
bytes are retained under `raw-assets/` without automatic replay.

## Viewport mouse-focus guard

Commits `cd5d03d9ea34` and `40f7ec6b34d6` add a null guard for
`gui.mouse_focus`. Current upstream already protects the equivalent state, so the
old hunk is upstream-absorbed rather than a maintained personal patch.
