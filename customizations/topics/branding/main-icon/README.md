# Personal Main Icon

## Intent and problem

Apply the personal game/editor artwork to Godot's modern main PNG assets while
keeping this concern separate from Windows executable resource icons.

## Behavior contract

- `misc/logo/icon.png` is the audited 256x256 personal source artwork.
- `main/app_icon.png` is the 128x128 Lanczos-derived application icon consumed
  by the main build's generated icon header.
- The source asset SHA-256 is
  `CCC4BF24FCE0A2C532A40DBEBF6FC9D8DB9A57508CFEA656CA411C9C78357BCE`.
- The derived asset SHA-256 is
  `10C437C8270B5A703D9F7916BACBB5A86D5E8E0F6433484980058D7477320C8D`.
- Windows `.ico` resources remain owned by `branding.windows-icons`.

## Legacy origin

- Source artwork: `Z:\project\godot-4-4-1\icon.png`.
- The modern application asset is derived explicitly instead of restoring the
  obsolete root-level PNG layout.

## Consolidated source

- `personal/main` commit:
  `b5bbfc0f3dc2bf9aa6759960b18dc179bcbd673e`.
- PCK test size-bound follow-up for the larger custom artwork:
  `06972f99a32b7d963ac489f00b204ef45e1a4422`.

## Dependencies

None.

## Important source areas

- `misc/logo/icon.png`
- `main/app_icon.png`
- generated `main/app_icon.gen.h` during a build

## Conflict guidance

If upstream changes source dimensions, asset locations, or icon generation,
regenerate the derived personal asset from the audited 256x256 source. Do not
blindly restore old root-level icon paths.

## Verification

Run the topic hash verifier, build the editor, confirm icon-header generation,
visually inspect both PNG assets and the resulting application icon, and run the
PCKPacker test group. Its upper bound is derived from the actual source asset
sizes so branding changes cannot make the unrelated packer test fail spuriously.

## Upstream status

Intentional personal branding; not intended for upstream.
