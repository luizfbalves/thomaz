#!/usr/bin/env bash
#
# Build thomaz.nro for Nintendo Switch.
#
# By default this runs the build inside the official devkitPro container
# (devkitpro/devkita64) — the same image the CI uses — so you only need Docker,
# not a local devkitPro install. The image already bundles libnx, deko3d, cmake
# and the switch portlibs (curl, mbedtls, libarchive, zstd, lz4, ...), so there
# is no dkp-pacman step.
#
# Usage:
#   ./scripts/build-switch.sh                              # build via Docker (recommended)
#   DEVKITPRO=/opt/devkitpro ./scripts/build-switch.sh     # build natively if you
#                                                          # already have devkitPro
#
# Output: build_switch/thomaz.nro  ->  copy to your SD card at /switch/thomaz.nro
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[*] Ensuring submodules (borealis, ...) are checked out..."
git submodule update --init --recursive

if [ -n "${DEVKITPRO:-}" ] && [ -d "${DEVKITPRO:-/nonexistent}" ]; then
    echo "[*] DEVKITPRO=$DEVKITPRO detected — building natively."
    echo "[1/3] Merging Borealis runtime resources into resources/ ..."
    cp -rn lib/borealis/resources/* resources/ 2>/dev/null || true
    echo "[2/3] Configuring (Switch, deko3d)..."
    cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
    echo "[3/3] Building thomaz.nro ..."
    make -C build_switch thomaz.nro -j"$(nproc)"
else
    echo "[*] No local devkitPro — building inside devkitpro/devkita64 (Docker)."
    command -v docker >/dev/null 2>&1 || {
        echo "ERROR: Docker not found. Install Docker, or set DEVKITPRO to build natively." >&2
        exit 1
    }
    # Same 3 steps as the CI (.github/workflows/build.yml), run in the container.
    # safe.directory: the bind-mounted repo is owned by your user, not root.
    docker run --rm -v "$PWD":/app -w /app devkitpro/devkita64 bash -lc '
        set -euo pipefail
        git config --global --add safe.directory /app
        echo "[1/3] Merging Borealis runtime resources into resources/ ..."
        cp -rn lib/borealis/resources/* resources/ 2>/dev/null || true
        echo "[2/3] Configuring (Switch, deko3d)..."
        cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
        echo "[3/3] Building thomaz.nro ..."
        make -C build_switch thomaz.nro -j"$(nproc)"'
fi

echo
echo "Done. Output: build_switch/thomaz.nro"
echo "Copy it to your SD card at: /switch/thomaz.nro"
