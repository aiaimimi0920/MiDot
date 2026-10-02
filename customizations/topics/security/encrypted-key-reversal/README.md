# Encrypted-Key Byte Reversal

## Intent and problem

Make encrypted files and PCK payloads produced by the custom engine incompatible
with tools that assume stock Godot key ordering. The goal is to stop simple
off-the-shelf extraction using the configured key without requiring a separate
encryption implementation.

This is a format perturbation and obfuscation layer. Reversing bytes does not add
entropy to the AES-256 key, hide a key embedded in an export template from a
determined reverse engineer, or add authenticated encryption.

## Behavior contract

- `FileAccessEncrypted::open_and_parse()` reverses every valid 32-byte raw key
  before AES-CFB encryption or decryption.
- Raw-key and password-derived `FileAccess`, encrypted `ConfigFile`, encrypted
  PCK directory, per-file PCK, and script export paths share this behavior.
- Callers continue to provide the original configured key. They must not
  pre-reverse it.
- Read and write paths use the same transformation, so exports and runtime loads
  are symmetric when both use this custom engine/template.
- Files created by this topic are intentionally incompatible with an unmodified
  Godot build using the same raw key or password.
- Magic values, plaintext MD5 field, length, IV, padding, PCK layout, unencrypted
  files, and ZIP exports remain unchanged.

## Legacy origin

- Original repository: `Z:\project\godot-4-3-1`.
- Historical implementation: `cd5d03d9ea340f42f9ef91941026d7874c423a83`.
- Current legacy-branch implementation:
  `40f7ec6b34d6a3930facbbd3209956abf263a9b7`.
- The current implementation is distributed through the maintained topic patches
  and locked personal commit stack; no archived source checkout is required.

## Consolidated source

- `personal/main` commit:
  `728cc9a6bd8f293dbb79236d203e47ff308fddbf`.

## Dependencies

None.

## Important source areas

- `core/io/file_access_encrypted.*`
- `doc/classes/FileAccess.xml`
- `tests/core/io/test_file_access.cpp`
- PCK/export callers in `core/io/file_access_pack.cpp`, `core/io/pck_packer.cpp`,
  and `editor/export/editor_export_platform.cpp`

## Conflict guidance

Keep the transformation centralized in `FileAccessEncrypted::open_and_parse()`
so every raw, password, export, runtime, and tools override path stays symmetric.
Do not move reversal only into export preset parsing or compiled-key generation;
that would let one side encrypt with a different byte order than the other and
cause MD5 validation failures at runtime.

If upstream replaces the encrypted-file format or AES-CFB implementation,
preserve the public contract above and rebuild the format-discrimination test
against the new encryption boundary instead of blindly retaining the old hunk.

## Verification

Run the topic verifier, compile an editor with tests, then run both the focused
format test and the complete `FileAccess` group:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File ..\customizations\scripts\verify-topic.ps1 `
  -Topic security.encrypted-key-reversal

scons platform=windows target=editor dev_build=yes tests=yes
.\bin\godot.windows.editor.dev.x86_64.console.exe `
  --headless --test `
  --test-case="*Encrypted files reverse raw key bytes*"
.\bin\godot.windows.editor.dev.x86_64.console.exe `
  --headless --test --test-case="*FileAccess*"
```

The focused test writes with a fixed raw key and IV, verifies custom-engine
round-trip behavior, decrypts the raw ciphertext with the reversed key, and
proves that the original stock key order does not recover the plaintext.

For a release template, also export an encrypted PCK using the same configured
key compiled into that template and verify the exported game starts and loads an
encrypted resource. A custom editor paired with a stock export template is not a
valid configuration for this topic.

## Upstream status

Active personal security/obfuscation feature. It intentionally diverges from
stock Godot encrypted-file key ordering.
