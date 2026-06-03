#!/usr/bin/env bash
#
# Build thomaz for desktop (PC) — fast UI iteration without devkitPro or hardware.
#
# One-time dependencies (Debian/Ubuntu/WSL):
#   sudo apt install -y cmake build-essential libgl1-mesa-dev xorg-dev libcurl4-openssl-dev
#
# Then just run this script from anywhere:
#   ./scripts/build-desktop.sh
#   ./build_desktop/thomaz
#
# Uses the SDL2 driver (vendored, built from source) which avoids the GLFW
# Wayland/DBus dependencies. On WSL, WSLg provides the display.
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[1/3] Merging Borealis runtime resources into resources/ ..."
cp -rn lib/borealis/resources/* resources/ 2>/dev/null || true

echo "[2/3] Configuring (desktop, SDL2)..."
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release

echo "[3/3] Building..."
cmake --build build_desktop -j"$(nproc)"

echo
echo "Done. Run it with:  ./build_desktop/thomaz"
