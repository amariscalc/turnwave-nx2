#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Generated UI assets are intentionally not committed to Git.
# Build them before running the resource-size regression test on a fresh clone.
echo "Generating Turnwave native UI resources..."
if ! python3 native/design/render.py native/build/ui; then
  echo "UI rendering failed. Install Python dependencies: sudo apt install python3-numpy python3-pil fonts-open-sans" >&2
  exit 1
fi
python3 -m unittest discover -s tests -v
node --check chromium-bridge/sync.js
node tests/test_bridge_js.cjs
node tests/test_eme_js.cjs
python3 -m py_compile chromium-bridge/install.py tools/audit_widevine.py tools/instrument_gmem.py
bash -n build-native.sh
gcc -std=c11 -Wall -Wextra -fsyntax-only -I tests/mocks native/source/main.c
