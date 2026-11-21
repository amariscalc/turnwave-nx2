#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m unittest discover -s tests -v
node --check chromium-bridge/sync.js
node tests/test_bridge_js.cjs
python3 -m py_compile chromium-bridge/install.py
bash -n build-native.sh
gcc -std=c11 -Wall -Wextra -fsyntax-only -I tests/mocks native/source/main.c
