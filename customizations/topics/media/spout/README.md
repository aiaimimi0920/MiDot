# Spout Texture Sharing

## Intent and problem

Expose Windows Spout texture sharing to Godot so a running project can exchange
GPU texture output with compatible desktop applications.

## Behavior contract

- The module is available only on supported Windows builds.
- Script bindings expose the legacy sender/receiver operations without changing
  unrelated rendering behavior on other platforms.

## Legacy origin

- Original Godot line: 4.2 development history.
- Personal integration and build fix commits:
  `a99bfbe5fb9a43d94c21c959a8a34914e8517def` and
  `5b6b9edfdf58c5878690190c3b056e040b039df9`.
- SpoutLibrary is vendored and keeps its upstream notices.

## External upstream

- Repository: <https://github.com/leadedge/Spout2>
- Release: `2.007.017`
- Binary archive: `Spout-SDK-binaries_2-007-017_1.zip`
- License: BSD-2-Clause
- Update policy: replace the header, import library, and DLL atomically using the
  same SDK/CRT variant because the C++ virtual interface is ABI-sensitive.

## Consolidated source

- `personal/main` commit:
  `3a33e14f21a1127b8dbd104786883b8b113938da`.

## Dependencies

No patch-stack topic dependency. The topic vendors the Windows x86_64 dynamic-CRT
SpoutLibrary 2.007.017 binary set.

## Important source areas

- `modules/spout/`
- `thirdparty/spout-library/`

## Conflict guidance

Keep the integration behind Windows feature checks. Adapt rendering handles to
the current RenderingDevice/RenderingServer contracts; never cast a modern
opaque handle based only on the old Godot 4.2 representation.

## Verification

Run the topic verifier, compile a Windows editor, and perform a sender/receiver
round trip with a current Spout-compatible application.

The automated lifecycle smoke validates class registration, the vendored DLL,
named-memory round trips, and repeated sender/receiver construction and release.
It also rejects ObjectDB, resource, and RID leak diagnostics:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  .\customizations\topics\media\spout\verification\run.ps1
```

The smoke does not replace the cross-process GPU texture round trip because that
requires a compatible Windows GPU, graphics context, and live Spout peer.

## Upstream status

Active external integration; no equivalent module exists in the inspected
upstream engine tree. The SDK was refreshed to 2.007.017 and the Godot wrapper
uses its current `GetSDKversion` API.
