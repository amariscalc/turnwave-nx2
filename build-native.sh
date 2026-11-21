#!/usr/bin/env bash
# Build Turnwave's native NRO inside the already-running ounce-devkit container.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CONTAINER="${TURNWAVE_BUILD_CONTAINER:-chromium-nx-diagnostico-build}"
NATIVE="$ROOT/native"
python3 "$NATIVE/design/render.py" "$NATIVE/build/ui"
# The Chromium Docker container usually mounts only the Chromium source tree.
# Copy Turnwave's independent native project into it for a real libounce build.
docker exec "$CONTAINER" mkdir -p /tmp/turnwave-native
docker cp "$NATIVE/." "$CONTAINER:/tmp/turnwave-native/"
docker exec -e DEVKITPRO=/opt/devkitpro "$CONTAINER" bash -lc \
  'cd /tmp/turnwave-native && export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH && make'
docker cp "$CONTAINER:/tmp/turnwave-native/TurnwaveNX2.nro" "$NATIVE/TurnwaveNX2.nro"
printf '\nNative NRO: %s\n' "$NATIVE/TurnwaveNX2.nro"
