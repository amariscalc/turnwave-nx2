# Third-party notices

- Turnwave links FFmpeg 7.1.5 (libavformat, libavcodec, libavutil and libswresample). The build uses LGPL components. Its upstream source is [ffmpeg-7.1.5.tar.xz](https://ffmpeg.org/releases/ffmpeg-7.1.5.tar.xz), SHA-256 `de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f`. The [build script](../scripts/build-codecs.sh) lists every configuration option. See [FFmpeg licensing](FFmpeg.md) and [LGPL 2.1](FFmpeg-LGPL-2.1.txt). Users may modify the libraries, relink the application, and debug those modifications. The application source and Makefile in this repository reproduce the link step.
- The Switch 2 platform backend uses libounce (libnx-compatible runtime). See [libounce notice](libounce.txt). The compiler runtime comes from devkitA64. See [newlib notices](newlib.txt).
- The embedded glyph tables and icon lettering derive from DejaVu Sans, Sans Condensed and Sans Mono. See [DejaVu notices](DejaVu.txt).
- The optional desktop build links the system SDL2 library.
