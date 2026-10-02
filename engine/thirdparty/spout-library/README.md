# SpoutLibrary

- Upstream: https://github.com/leadedge/Spout2
- Version: SDK 2.007.017
- Release: https://github.com/leadedge/Spout2/releases/tag/2.007.017
- Source archive: `Spout-SDK-binaries_2-007-017_1.zip`
- Source archive SHA-256: `695F20E3505FA0DA51B2EB959AF359F5D9E2C914BB9676E9118D19F6A5424BF4`
- License: BSD-2-Clause; see `COPYING`
- Target: Windows x86_64

The module uses the prebuilt SpoutLibrary C++ interface distributed with the
SDK:

- `Binaries/x64/SpoutLibrary.h`
- `Binaries/x64/SpoutLibrary.lib`
- `Binaries/x64/SpoutLibrary.dll`

The `.lib` file is the import library for `SpoutLibrary.dll`. The Godot build
copies the DLL next to the generated executable so the module can load at
runtime.

The bundled library uses the SDK's dynamic-CRT (`MD`) binaries. Git normalizes
the text header's line endings; its tracked content otherwise matches the SDK
header. The checked-out files have these SHA-256 hashes:

- `SpoutLibrary.h`: `7E72293213F7366450AEB97E38333A80F971CEDE07C50919A810D339F6DB3B3B`
- `SpoutLibrary.lib`: `6C8CEBA65294FB76C5CCAD23C059551B92DDEF7D6D6665B39F6B2EC553EEE091`
- `SpoutLibrary.dll`: `31F12268169397BACD9048903D9AD711FAE58B788E90A860598B21B1DAAE62EB`
