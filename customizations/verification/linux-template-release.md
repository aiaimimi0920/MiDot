# Locked Linux release export template

The `Locked Linux release template` workflow builds only `template_release`
for Linux x86_64. It uses the same engine source as the verified Linux editor:
`38b6ddee72e16d9d646057ee6ba533c122afc47c`, based on upstream
`5ec4857b340b6284a18b49b2eda462bd250f219a` and the existing 56-patch catalog.
No engine source, generated catalog, editor probe or diagnostic build changes.

The workflow restores and verifies the original history, checks fixed catalog
and patch hashes, and uses the editor workflow's isolated SCons 4.10.1 wheel
pin. A fresh engine with no untracked or ignored inputs is required before
compilation; extra `custom.py` files or modules are refused without cleanup.
Compilation uses two jobs, no debug symbols or LTO, and size optimization.
The fixed Spine release source uses `FLT_MAX` without including its standard
header. The recorded `cxxflags=-include cfloat` compiler option supplies that
header without changing engine sources, patch identities or enabled modules.
The resulting ELF architecture and `--version` source suffix are checked.
CI does not export a project, launch a game, publish a release, sign or deploy.
PRs may be merged under the user's authorization after independent review and
the required checks succeed.

Download the `midot-linux-template-release-x86_64-38b6ddee72e16d9d646057ee6ba533c122afc47c`
Actions artifact from the successful run on the reviewed PR revision. It contains
`midot-linux-template-release-x86_64.tar.gz` and its archive SHA-256 sidecar.
After verifying the sidecar and extracting the archive, run `sha256sum -c SHA256SUMS`
inside the package. `SOURCE_COMMIT`, `manifest.json` and `provenance/` identify
the exact engine, upstream, catalog commit, lock/bundle hashes, all package file
hashes, compiler, Python, SCons and build flags. Compare those source identities
with the editor's manifest before using the pair; their binary hashes differ.

In that MiDot editor's Linux/BSD Export preset, set **Custom Template > Release**
to the extracted `godot.linuxbsd.template_release.x86_64` and use the normal
release export action. Use this custom template rather than a stock Godot one.
An embedded PCK provides one native executable; otherwise keep the exported
executable and matching PCK together. A debug export needs a separate matching
debug template, which this workflow does not build.

Template compilation and version identification do not validate a character,
rendering or native project startup. The manifest explicitly leaves
`render_verified`, `character_validation`, `game_exported` and `game_started`
false. The cloud project owner performs the actual Aster scene export and run.
