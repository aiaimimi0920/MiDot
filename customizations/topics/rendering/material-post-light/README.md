# Material-local HDR post-light stage

## Intent

NPR materials require nonlinear color operations after all direct lights have
accumulated, before native fog and tonemapping. Applying quantization once per
light, or disabling lights when a material uses nonlinear/signed gains, does not
preserve the authored color chain.

## Source and dependencies

Authoritative commits live on `engine/personal/main` with exactly one trailer:

```text
Godot-Patch-Topic: rendering.material-post-light
```

The exact source commit and binary patch hashes are recorded in generated
`stack.lock.json`. No dependency on another personal topic. Never hand-edit the
generated patch or use a prototype executable's old version string as source
provenance.

## Contract

- Forward+ advertises `HAS_MATERIAL_POST_LIGHT`; Mobile and Compatibility do not.
- Spatial `post_light()` runs once in the fragment color path after all lights,
  including the zero-light case, before fog/output composition. It is not a new
  GPU shader stage and does not execute in depth/normal-only passes.
- Writable `POST_LIGHT_COLOR` starts with the native merged material HDR result
  (albedo for unshaded materials).
- Read-only `POST_LIGHT_DIFFUSE` and `POST_LIGHT_SPECULAR` are raw direct-light
  accumulators before native albedo/AO/metallic scaling.
- Read-only `POST_LIGHT_DIRECT` is diffuse plus direct specular AFTER that native
  scaling. Do not assume raw and scaled domains are interchangeable.
- `MATERIAL_LIGHT_DATA` is a private vec4 for the current fragment invocation,
  initialized to zero before user fragment code, writable in fragment/light,
  read-only in post_light. It never aliases native color attachments or shares
  state across pixels.
- Fragment-authored varyings are readable in post_light, including function
  inputs, but remain non-writable there. They are already emitted as private
  fragment storage and must not consume vertex interpolation locations.
- Actual vertex varying limits are unchanged. Explicit post_light on an
  unsupported renderer fails rather than silently dropping the material stage.
- Separate-specular output preserves the original specular attachment and adds
  the post-color correction to diffuse. Their pre-screen-effect sum is correct;
  arbitrary subsequent SSS/SSR interactions are NOT certified by this contract.
- No-hook shaders retain the original guarded output expressions.

## Source boundaries and conflict guidance

The seven source files are ShaderLanguage header/implementation, ShaderTypes,
ShaderCompiler, ShaderPreprocessor, Forward+ compiler setup, and its fragment
GLSL template. During rebases preserve the placement relative to native material
scaling, fog, separate-specular merge, and both custom-light injection routes
(ordinary lights and Area lights). The private accumulator must be declared
before user fragment-stage helper functions and initialized once, not per lamp.

If upstream implements a corresponding stage, compare the domains, no-light
behavior, mutable data lifetime, read-only constraints and renderer gating before
replacing this patch. Never raise the hardware varying limit to accommodate
private fragment variables.

## Verification

Catalog verification is read-only source-shape checking, NOT GPU certification:

```powershell
powershell.exe -NoProfile -File customizations/scripts/verify-topic.ps1 -Topic rendering.material-post-light
```

Consumer GPU tests live in the sibling RoleNPR repository; they are not silently
downloaded or run by the patch catalog. Current local evidence covers 30 API
checks (real Directional/Omni/Spot, HDR MRT readback, fog, transparency, native
material scaling, private data and 40 private varyings), five required compiler
rejections, 96 body pairs and 96 two-pass hair pairs, plus default-image baseline
and real CSM/fill responses. Exact reports are documented in RoleNPR's
`docs/NPR_BODY_POST_LIGHT_MIGRATION.md` and `docs/NPR_HAIR_POST_LIGHT_MIGRATION.md`.

Matching Release templates, Area-light numeric coverage, cross-device validation,
and broader screen-space effects remain separate release gates.

## Isolated build recipe

Use the workspace's isolated SCons 4.8.1 environment. Keep validated `export/`
artifacts untouched. The same flags apply to editor/template_debug/template_release:

```text
platform=windows target=<target> dev_build=no tests=no debug_symbols=no
use_static_cpp=no accesskit=no d3d12=no angle=no
extra_suffix=postlight object_prefix=postlight_ -j6
```

Record the clean source commit, exact arguments/toolchain, artifact SHA-256 and
matched template type in a build manifest before promoting any executable.

## Headless import follow-up

Commit `c42de04a0fbe5a2ca34f705c9712f90b0289bacd` registers the post-light
entry point in Dummy material storage for uniform discovery. Dummy emits no GPU
output; this does not enable the stage on Mobile or Compatibility. Without this
registration, strict headless imports and exports rejected otherwise valid
Forward+ materials. RoleNPR's `post_light_headless_regression.gd` verifies both
fragment and post-light uniforms, and the full project headless import is also
required. GPU and unsupported-renderer gates remain separate.
