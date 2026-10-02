# Third party libraries

The following lists C/C++ libraries which are bundled and used by GIF module.

## giflib
- Upstream: http://sourceforge.net/projects/giflib
- Version: 5.2.2
- Upstream ref: `refs/tags/5.2.2`
- Upstream commit: `44241952659c5db27da3d9db85d910c2b6904216`
- License: MIT

Files extracted from upstream source:
- gif_err.c
- gif_lib.h
- dgif_lib.c
- egif_lib.c
- gifalloc.c
- gif_hash.{c,h}
- gif_lib_private.h
- openbsd-reallocarray.c
- COPYING

The previous Windows include fix in `gif_hash.h` is part of upstream 5.2.2, so
this snapshot does not carry local changes.
