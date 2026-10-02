# Material Color Utilities upstream

- Repository: <https://github.com/material-foundation/material-color-utilities>
- Imported directory: `cpp/`
- Upstream commit: `5b3618b16fdc3825e21d5679bafd144662088ea1`
- Upstream commit date: 2026-08-21
- License: Apache-2.0; see `LICENSE`.

The import contains the 58 non-test C++ headers and sources from that commit.
Upstream test files are not vendored because Godot exercises the integration in
`tests/modules/color_scheme/`.

Godot keeps the sources directly under `thirdparty/material-color-utilities/`
instead of under the upstream `cpp/` prefix. Includes are therefore rewritten
from `cpp/<path>` to paths relative to each vendored source directory. The single
use of the non-portable `M_PI` macro is replaced with a local constant, and the
fallback ARGB literal uses an unsigned type to avoid an MSVC narrowing error.
The default viewing-condition initializer and `Vec3` values also use standard
C++ aggregate syntax instead of C compound literals. The WSMeans pixel-count map
uses `std::unordered_map` instead of Abseil's `flat_hash_map`, avoiding an
otherwise unused Abseil dependency. `HexFromArgb` uses a standard-library stream
instead of Abseil's `StrCat`, and a missing standard `<optional>` include is
explicitly provided. No algorithmic changes are made by these import adaptations.
