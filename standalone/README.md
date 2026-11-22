# Turnwave NX2 — Native Engine 0.7

A browser-free, single-process Nintendo Switch 2 homebrew prototype for local PCM playback and read-only Apple Music catalog discovery.

**Status: host-unit-tested prototype, not yet verified on real Switch 2 hardware.** This is a standalone implementation; the legacy Chromium integration remains unchanged on `main`. The implemented features require no Chromium, Linux guest, EXT4 image, AirPlay or DRM module at runtime.

## Implemented

- One native `.nro`, built using **libounce**, using Horizon AudioOut (`audout:u`).
- Controller/touch UI, active playback clock, pause/resume, next track and automatic advancement.
- Four page-aligned PCM buffers, pumped without blocking the main event loop.
- Scans up to 160 WAV files in `/switch/TurnwaveNX2/music`.
- Strict RIFF/WAVE validation: 48 kHz, 16-bit, signed little-endian stereo PCM.
- Optional Apple Music **catalog metadata** search via the official HTTPS API, TLS peer/host verification and a signed developer JWT.
- Original six-second test WAV generator, host regression tests, mocked AudioOut lifecycle tests and source syntax checks.

**Not implemented:** Apple subscriber login on Switch 2, personal music library, protected full-song Apple Music playback, FairPlay/Widevine, MP3/AAC/ALAC/FLAC decoding or lossless Apple Music streaming. Catalog entries are not playable: displaying metadata does not grant audio decryption or playback rights.

## Build

Use the existing Switch 2 devkit container as a **compiler**, not as a runtime dependency.

```bash
cd ~/switch2-dev/turnwave-nx2
git switch feature/native-player-core
bash standalone/tests/check.sh
bash standalone/build.sh
```

The native build script performs a clean Docker compilation, produces `standalone/TurnwaveNX2.nro`, stages it as `standalone/staged-sd/switch/TurnwaveNX2/TurnwaveNX2.nro`, and generates a short original test file in `standalone/staged-sd/switch/TurnwaveNX2/music/`.

Back up the old `TurnwaveNX2.nro` before replacing it on the SD card. The standalone build does not need or modify any Chromium NRO, EXT4 disk, browser session or profile.

If the libounce toolchain or link libraries differ, capture the exact compiler/linker error; the desktop tests cannot validate the console linker.

## Controls

| Input | Action |
|---|---|
| L/R | Local music / Apple Music catalog |
| Up/Down | Select track |
| A | Play or pause/resume a local WAV |
| X | Next local track / catalog search |
| Y | Rescan local files / refresh catalog |
| B or + | Exit |
| Touch | Select a visible item or tab |

## Optional catalog search

Apple Music API requires a signed, time-limited **developer token** even for public catalog requests. Provision a developer JWT with Apple developer credentials outside the Switch and place **only the JWT** in:

`sdmc:/switch/TurnwaveNX2/developer-token.txt`

Do not place private signing keys or Apple Account passwords on the SD card. Do not commit JWTs, session data or private key material. Optionally, set a UTF-8 phrase in `sdmc:/switch/TurnwaveNX2/search.txt` (default: `Daft Punk`). If the libcurl build has no trusted CA store, place a vetted trust bundle in `sdmc:/switch/TurnwaveNX2/cacert.pem`; the application never disables TLS verification.

Press **R**, then **X**. Requests target the Spanish Apple Music catalog and never request protected audio. Full user-library access requires a Music User Token issued through an authorized workflow; a self-contained Horizon OS authorization path has **not** been demonstrated.

## Source map

- `source/main.c` — native UI, controls, panel rendering
- `source/audio.c` — AudioOut PCM buffer queue and transport controls
- `source/wav.c` — validated RIFF/WAV reader
- `source/library.c` — local SD music scan
- `source/catalog_json.c` — size-bounded Apple catalog JSON parser
- `source/catalog_net.c` — read-only HTTPS client using libcurl
- `design/render.py` — UI assets generated at build time
- `tools/demo_wav.py` — original six-second PCM hardware-test tone
- `tests/check.sh` — host regression suite

Runtime diagnostics: `sdmc:/switch/TurnwaveNX2/standalone.log`. No credentials are intentionally written to logs.

## Hardware acceptance gates

1. `tests/check.sh` passes; `build.sh` successfully links a new NRO.
2. Launch the NRO with kaiHEN / hbmenu, hear the test tone and confirm that the UI remains responsive.
3. Verify pause/resume, next-track behavior and app restart on the console.
4. Validate TLS catalog search only after audio works.
5. Investigate Apple subscriber authorization and protected streaming separately before promising full playback.

## Reference documentation

- [ounce-devkit](https://github.com/ouncebrew/ounce-devkit) — native Switch 2 toolchain.
- [AudioOut API](https://switchbrew.github.io/libnx/audout_8h.html) — native PCM output.
- [Apple Music API](https://developer.apple.com/documentation/applemusicapi) — catalog access.
- [MusicKit user authentication](https://developer.apple.com/documentation/applemusicapi/user-authentication-for-musickit) — Music User Token requirements.

Turnwave NX2 is an independent experimental project. Apple Music, Apple and Nintendo remain trademarks of their respective owners.
