# Texture Streaming Initialization Order

## Intent and problem

Ensure every texture-streaming project setting exists before a Forward renderer
reads it while assembling its initial shader definitions. The upstream module
registered these settings at the scene initialization level, after the rendering
server and its Forward renderer had already been constructed.

## Behavior contract

- Builds with the texture-streaming module register all
  `rendering/textures/streaming/*` settings from `RenderingServer::init()`.
- Builds without the module do not expose those settings.
- `TextureStreaming` consumes the registered project values without redefining
  them during scene-level module initialization.
- Forward+ and Mobile startup must not emit `Property not found` warnings for
  texture-streaming settings.

## Consolidated source

- `personal/main` commit:
  `6d03cb106e8e1bce95b8cdd1b844370687b6c5e7`.

## Dependencies

None.

## Important source areas

- `servers/rendering/rendering_server.cpp`
- `modules/texture_streaming/texture_streaming.cpp`
- Forward+ and Mobile renderer initialization

## Conflict guidance

Preserve the initialization boundary rather than the exact source location:
the settings must be defined after `ProjectSettings` exists and before either
Forward renderer calls `GLOBAL_GET`. If upstream moves renderer construction or
module registration earlier, prefer the new upstream lifecycle and drop this
patch when it provides the same ordering guarantee.

## Verification

Run the topic verifier, compile editor and Windows template targets, and launch
a Forward+ project in GUI mode. Reject any startup warning naming
`rendering/textures/streaming/*`.

## Upstream status

Godot upstream master contained the initialization-order defect at the locked
base commit. Keep this compatibility patch only until upstream provides an
equivalent early registration path.
