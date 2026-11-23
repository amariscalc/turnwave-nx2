#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/music/Albums"
ffmpeg -nostdin -v error -f lavfi -i 'sine=frequency=523.25:sample_rate=48000:duration=3' -ac 2 -c:a pcm_s16le "$tmp/music/wave.wav"
encode() {
    ffmpeg -nostdin -v error -i "$tmp/music/wave.wav" -c:a "$1" "$tmp/music/$2"
}
encode flac flac.flac
encode alac alac.m4a
encode wavpack wavpack.wv
encode pcm_s16be aiff.aiff
encode libmp3lame mp3.mp3
encode libopus opus.opus
encode libvorbis vorbis.ogg
encode aac aac.m4a
encode wmav2 wma.wma
ffmpeg -nostdin -v error -f lavfi -i 'sine=sample_rate=96000:duration=3' -c:a flac -sample_fmt s32 "$tmp/music/high-res.flac"
ffmpeg -nostdin -v error -f lavfi -i 'color=red:size=48x32' -frames:v 1 "$tmp/cover.jpg"
ffmpeg -nostdin -v error -i "$tmp/music/mp3.mp3" -i "$tmp/cover.jpg" -map 0:a -map 1:v \
    -c copy -id3v2_version 3 -metadata album=Covers -metadata genre=Test -metadata replaygain_track_gain="-6 dB" -metadata replaygain_album_gain="-3 dB" "$tmp/music/Albums/art.mp3"
read -ra flags <<< "${HOST_FLAGS:-$(pkg-config --cflags sdl2 libavformat libavcodec libswresample libswscale libavfilter libavutil)}"
read -ra libs <<< "${HOST_LIBS:-$(pkg-config --libs sdl2 libavformat libavcodec libswresample libswscale libavfilter libavutil)}"
"${CXX:-c++}" -std=c++17 -Isrc "${flags[@]}" tests/playback.cpp src/{player,platform,decoder,playlist,skin,session,extras,library,details,tempo}.cpp "${libs[@]}" -o "$tmp/tests"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$tmp/tests" "$tmp/music"
