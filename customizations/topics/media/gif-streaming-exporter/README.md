# GIF Streaming Exporter API

## Intent and problem

Preserve the stateful `GifExporter` workflow used by personal projects while
building the output on the maintained GIF module instead of carrying the old,
unsafe standalone encoder implementation.

## Behavior contract

- `begin_export(path, width, height, frame_delay, loop_count, bit_depth,
  dither)` begins one active export and rejects invalid dimensions or state.
- `write_frame(frame, background_color, frame_delay, bit_depth, dither)`
  requires matching dimensions, composites alpha correctly, and buffers the
  prepared frame. A zero delay uses the default from `begin_export`.
- Delays use legacy GIF hundredths of a second.
- Bit depths from 1 through 8 select uniform quantization; optional
  Floyd-Steinberg dithering is applied after alpha compositing.
- `end_export()` writes through `ImageFrames`, including the requested GIF loop
  count. A loop count of zero means infinite looping.
- Failed finalization leaves the exporter active so the failure is observable
  and does not silently discard state.

## Legacy origin

- Audited personal source state: `Z:\project\godot-4-4-1`.
- The old `gifanim`-based code was not copied because it mixed scalar and array
  deletion and computed alpha blending incorrectly. Only its public workflow
  and intended controls were retained.

## Consolidated source

- `personal/main` commit:
  `dca2b738065d7e602a621ed2d97bbcb984dc94ac`.

## Dependencies

- `media.gif-support`: supplies `ImageFrames`, GIF encoding, frame timing, and
  loop metadata support.

## Important source areas

- `modules/gif/gif_exporter.*`
- `modules/gif/image_frames.*`
- `modules/gif/register_types.cpp`
- `modules/gif/doc_classes/GifExporter.xml`
- `tests/modules/gif/test_gif_exporter.cpp`

## Conflict guidance

Keep the script-visible names, hundredth-of-a-second timing, alpha compositing,
quantization controls, and loop semantics stable. If upstream adds streaming
encoding, the backend may change, but the compatibility API and failure behavior
must remain explicit.

## Verification

Run the topic verifier, compile the GIF module with tests, and run all focused
`GifExporter` tests. The tests inspect round-trip pixels and delays plus the
encoded Netscape loop extension.

## Upstream status

Active personal compatibility API; current upstream does not provide this
stateful exporter class.
