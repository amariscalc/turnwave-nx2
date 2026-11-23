#!/usr/bin/env bash
set -euo pipefail
: "${DEVKITPRO:?Set DEVKITPRO to an installation with devkitA64 and libounce}"
root=$(cd "$(dirname "$0")/.." && pwd)
prefix="$root/build/codecs"
cache="$root/build/vendor"
version=7.1.5
mkdir -p "$cache" "$prefix"
cd "$cache"
archive="ffmpeg-$version.tar.xz"
if [[ ! -f $archive ]]; then curl -fL --retry 2 "https://ffmpeg.org/releases/$archive" -o "$archive"; fi
echo "de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f  $archive" | sha256sum -c -
tar xf "$archive"
cd "ffmpeg-$version"
export PATH="$DEVKITPRO/devkitA64/bin:$PATH"
./configure --prefix="$prefix" --target-os=none --arch=aarch64 --cpu=cortex-a78c \
  --cross-prefix=aarch64-none-elf- --enable-cross-compile --disable-everything \
  --disable-programs --disable-doc --disable-debug --disable-network --disable-autodetect \
  --disable-avdevice --enable-avfilter --enable-filter=atempo,abuffer,abuffersink --enable-swscale --disable-postproc --disable-pthreads \
  --disable-w32threads --disable-os2threads --enable-pic --enable-static --disable-shared \
  --extra-cflags="-D__SWITCH__ -D__SWITCH2__ -D_GNU_SOURCE -fPIC -mtp=soft -ftls-model=local-exec -I$DEVKITPRO/libounce/include" \
  --extra-ldflags="-mtp=soft -fPIE -specs=$DEVKITPRO/libounce/switch.specs -L$DEVKITPRO/libounce/lib" --extra-libs="-lnx -lm" \
  --enable-protocol=file --enable-demuxer=aac,aiff,ape,asf,caf,flac,matroska,mov,mp3,ogg,wav,wv \
  --enable-parser=aac,aac_latm,flac,mpegaudio,opus,vorbis \
  --enable-decoder='aac,aac_latm,alac,ape,flac,mjpeg,mp3,opus,vorbis,wavpack,wmalossless,wmav1,wmav2,pcm_*'
make -j"${JOBS:-4}"
make install
