#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p /tmp/turnwave-native-test/music
python3 tools/demo_wav.py '/tmp/turnwave-native-test/music/Turnwave Test Tone.wav'
gcc -std=c11 -O2 -g -Wall -Wextra -Werror -Isource \
  source/wav.c source/catalog_json.c source/library.c tests/test_core.c -o /tmp/turnwave-native-test/test_core
/tmp/turnwave-native-test/test_core \
  '/tmp/turnwave-native-test/music/Turnwave Test Tone.wav' \
  /tmp/turnwave-native-test/music
gcc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Itests/mocks -Isource \
  source/audio.c source/wav.c tests/test_audio_mock.c -o /tmp/turnwave-native-test/test_audio
/tmp/turnwave-native-test/test_audio '/tmp/turnwave-native-test/music/Turnwave Test Tone.wav'
if command -v pkg-config >/dev/null && pkg-config --exists libcurl; then
  gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Isource \
    $(pkg-config --cflags libcurl) source/catalog_net.c
  echo 'PASS: libcurl HTTP module host syntax check'
else
  echo 'SKIP: libcurl development headers unavailable for host syntax check'
fi
python3 -m py_compile design/render.py tools/demo_wav.py
printf '%s\n' 'Host tests passed; hardware output requires a Switch 2 test.'
gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Itests/mocks -Isource source/main.c source/audio.c
printf '%s\n' 'PASS: console UI/audio C syntax against libnx-compatible API mock'
