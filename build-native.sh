#!/usr/bin/env bash
# Build Turnwave's native NRO, always from clean compilation state.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CONTAINER="${TURNWAVE_BUILD_CONTAINER:-chromium-nx-diagnostico-build}"
NATIVE="$ROOT/native"
python3 "$NATIVE/design/render.py" "$NATIVE/build/ui"
# Remove all stale Docker objects, then copy exactly the current native source.
docker exec "$CONTAINER" bash -lc 'rm -rf /tmp/turnwave-native && mkdir -p /tmp/turnwave-native'
docker cp "$NATIVE/." "$CONTAINER:/tmp/turnwave-native/"
rm -f "$NATIVE/TurnwaveNX2.nro"
docker exec -e DEVKITPRO=/opt/devkitpro "$CONTAINER" bash -lc \
  'cd /tmp/turnwave-native && rm -f build/ui.o TurnwaveNX2.elf TurnwaveNX2.nacp TurnwaveNX2.nro && export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH && make && test -s TurnwaveNX2.nro'
docker cp "$CONTAINER:/tmp/turnwave-native/TurnwaveNX2.nro" "$NATIVE/TurnwaveNX2.nro"
printf '\nBUILT Turnwave NX2 0.6.0-dev: %s\n' "$NATIVE/TurnwaveNX2.nro"
sha256sum "$NATIVE/TurnwaveNX2.nro"
