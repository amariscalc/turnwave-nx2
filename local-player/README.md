# Turnwave Local

A native music player for Nintendo Switch 2 with a native 1280 × 720 glass-inspired interface and antialiased text. Turnwave plays audio files from the SD card.

Copy `switch/Turnwave/Turnwave.nro` to the SD card and open it from hbmenu. Put your audio in `/music`. Settings, history and saved playback position are stored in `/switch/Turnwave`.

## Controls

- **A** plays the highlighted track or opens the selected folder.
- **X** switches between the SD browser and the playback queue.
- **−** opens the actions menu for search, artists, albums, favorites, playlists, queue editing, lyrics, chapters and settings.
- **L/R** changes track, **ZL/ZR** changes volume, and **+** saves and exits.
- Touch the transport buttons or progress bar to control playback. Touch the large artwork to open track information.

Turnwave supports SD library indexing, ReplayGain, EQ presets, M3U8 playlists, embedded and folder JPEG covers, LRC lyrics, CUE chapters, bookmarks, local metadata overrides and pitch-preserving playback speed.

## Build

Install devkitPro, libounce and the Switch tools, then run `make switch` with `DEVKITPRO` set. This builds `build/Turnwave.nro`. The first build downloads and compiles the pinned FFmpeg codecs.

For desktop builds, install SDL2 and the FFmpeg development libraries and run `make desktop`. Run `bash tests/check-audio.sh` for the playback regression tests.
