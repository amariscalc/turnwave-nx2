#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$ROOT/.." && pwd)"
CONTAINER="${TURNWAVE_BUILD_CONTAINER:-chromium-nx-diagnostico-build}"
python3 "$ROOT/design/render.py" "$ROOT/build/ui"
# Standalone build isolated from the legacy Chromium bridge.
docker exec "$CONTAINER" bash -lc 'rm -rf /tmp/turnwave-standalone && mkdir -p /tmp/turnwave-standalone/native'
docker cp "$ROOT/." "$CONTAINER:/tmp/turnwave-standalone/"
docker cp "$REPO/native/icon.jpg" "$CONTAINER:/tmp/turnwave-standalone/native/icon.jpg"
docker exec -e DEVKITPRO=/opt/devkitpro "$CONTAINER" bash -lc \
  'export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH; \
   cd /tmp/turnwave-standalone && rm -f TurnwaveNX2.elf TurnwaveNX2.nacp TurnwaveNX2.nro && make'
docker cp "$CONTAINER:/tmp/turnwave-standalone/TurnwaveNX2.nro" "$ROOT/TurnwaveNX2.nro"
mkdir -p "$ROOT/staged-sd/switch/TurnwaveNX2/music"
cp "$ROOT/TurnwaveNX2.nro" "$ROOT/staged-sd/switch/TurnwaveNX2/TurnwaveNX2.nro"
python3 "$ROOT/tools/demo_wav.py" "$ROOT/staged-sd/switch/TurnwaveNX2/music/Turnwave Test Tone.wav"
sha256sum "$ROOT/TurnwaveNX2.nro"
echo "NRO staged: $ROOT/staged-sd/switch/TurnwaveNX2/TurnwaveNX2.nro"
echo "Demo WAV staged: $ROOT/staged-sd/switch/TurnwaveNX2/music/Turnwave Test Tone.wav"
echo 'Copy the TurnwaveNX2 folder to your SD, keeping your previous NRO as a backup.'
