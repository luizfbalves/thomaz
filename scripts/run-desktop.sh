#!/usr/bin/env bash
#
# Build thomaz for desktop and run it — always from the current source.
# Use this for UI iteration so you never run a stale binary.
#
#   ./scripts/run-desktop.sh            # build (incremental) + run
#   ./scripts/run-desktop.sh --clean    # wipe build_desktop, full rebuild, then run
#
# One-time deps (Debian/Ubuntu/WSL):
#   sudo apt install -y cmake build-essential libgl1-mesa-dev xorg-dev libcurl4-openssl-dev
set -euo pipefail

cd "$(dirname "$0")/.."

if [[ "${1:-}" == "--clean" ]]; then
    echo "[clean] Removing build_desktop ..."
    rm -rf build_desktop
fi

echo "[1/3] Merging Borealis runtime resources into resources/ ..."
cp -rn lib/borealis/resources/* resources/ 2>/dev/null || true

echo "[2/3] Configuring (desktop, SDL2)..."
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release

echo "[3/3] Building..."
cmake --build build_desktop -j"$(nproc)"

echo
echo ">>> Running ./build_desktop/thomaz (resources resolved from repo root)"
echo
exec ./build_desktop/thomaz
