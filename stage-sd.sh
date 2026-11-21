#!/usr/bin/env bash
# Create one unambiguous SD staging tree from BOTH freshly built NROs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CHROMIUM_REPO="${CHROMIUM_REPO:-$HOME/switch2-dev/chromium-nx-diagnostico}"
NATIVE="$ROOT/native/TurnwaveNX2.nro"
STAGE="$ROOT/staged-sd"
if [[ ! -s "$NATIVE" ]]; then
  echo "Missing native NRO; run ./build-native.sh first" >&2
  exit 1
fi
# The NACP metadata of the rebuilt native NRO contains this version.
if ! LC_ALL=C grep -aFq '0.6.0-dev' "$NATIVE"; then
  echo "Native NRO is outdated: expected version 0.6.0-dev. Rebuild first." >&2
  exit 1
fi
if [[ -n "${CHROMIUM_NRO:-}" ]]; then
  BROWSER="$CHROMIUM_NRO"
else
  mapfile -d '' -t CANDIDATES < <(find "$CHROMIUM_REPO/gx/dist-nx2" -type f -name '*.nro' -print0)
  if (( ${#CANDIDATES[@]} != 1 )); then
    echo "Expected one Chromium NRO, got ${#CANDIDATES[@]}; pass CHROMIUM_NRO=/full/path/to/the/gx/browser.nro" >&2
    exit 1
  fi
  BROWSER="${CANDIDATES[0]}"
fi
if [[ ! -s "$BROWSER" ]]; then
  echo "Missing Chromium NRO: $BROWSER" >&2
  exit 1
fi
# NRO sections may be compressed: searching strings in .nro is unreliable.
# Verify the linked, uncompressed ELF produced by the same Chromium build instead.
CONTAINER="${CHROMIUM_BUILD_CONTAINER:-chromium-nx-diagnostico-build}"
GX_ELF="${CHROMIUM_ELF:-/work/gx-nx2/gx.elf}"
if ! docker exec "$CONTAINER" test -s "$GX_ELF"; then
  echo "Cannot verify Chromium: missing linked ELF $GX_ELF inside Docker container $CONTAINER" >&2
  echo "Build Chromium first. If OUT differs, set CHROMIUM_ELF=/path/to/gx.elf." >&2
  exit 1
fi
if ! docker exec "$CONTAINER" grep -aFq 'tw-profile-v06.ext4' "$GX_ELF"; then
  echo "Chromium linked ELF lacks isolated profile support. Re-run install.py and rebuild Chromium." >&2
  exit 1
fi
if ! docker exec "$CONTAINER" grep -aFq 'NX2_CDM_DIAG' "$GX_ELF"; then
  echo "WARNING: linked Chromium ELF has no NX2_CDM_DIAG memory instrumentation." >&2
fi
# The candidate NRO should be newer than the modified host source.
if [[ "$CHROMIUM_REPO/gx/host_nx.c" -nt "$BROWSER" ]]; then
  echo "The browser NRO is older than gx/host_nx.c. Rebuild and stage again." >&2
  exit 1
fi
echo "Chromium linked ELF verified: isolated profile selected by Turnwave."
mkdir -p "$STAGE/switch/TurnwaveNX2" "$STAGE/switch/chromium-nx-diagnostico"
cp "$NATIVE" "$STAGE/switch/TurnwaveNX2/TurnwaveNX2.nro"
cp "$BROWSER" "$STAGE/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro"
printf '\nCOPY THESE TWO FILES from the staging tree to the SD card:\n'
printf '  %s\n' "$STAGE/switch/TurnwaveNX2/TurnwaveNX2.nro"
printf '  %s\n' "$STAGE/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro"
printf '\nChecksums:\n'
sha256sum "$STAGE/switch/TurnwaveNX2/TurnwaveNX2.nro" \
          "$STAGE/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro"
echo 'Do not overwrite or delete any .ext4 files on your SD card.'
