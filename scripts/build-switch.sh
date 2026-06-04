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

# The build steps, run either on the host (native) or inside the container.
# Configure is wrapped so a stale/incompatible CMake cache — e.g. left by a build
# at a different absolute path (host vs /app in Docker) — is wiped and
# regenerated instead of aborting with a "cache directory is different" error.
export STEPS='
set -euo pipefail
echo "[1/3] Merging Borealis runtime resources into resources/ ..."
cp -rn lib/borealis/resources/* resources/ 2>/dev/null || true
echo "[2/3] Configuring (Switch, deko3d)..."
configure() { cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON; }
if ! configure; then
    echo "[*] Stale/incompatible build_switch cache — wiping and reconfiguring..."
    rm -rf build_switch
    configure
fi
echo "[3/3] Building thomaz.nro ..."
make -C build_switch thomaz.nro -j"$(nproc)"
'

if [ -n "${DEVKITPRO:-}" ] && [ -d "${DEVKITPRO:-/nonexistent}" ]; then
    echo "[*] DEVKITPRO=$DEVKITPRO detected — building natively."
    eval "$STEPS"
else
    echo "[*] No local devkitPro — building inside devkitpro/devkita64 (Docker)."
    command -v docker >/dev/null 2>&1 || {
        echo "ERROR: Docker not found. Install Docker, or set DEVKITPRO to build natively." >&2
        exit 1
    }
    # safe.directory: the bind-mounted repo is owned by your user, not root.
    docker run --rm -e STEPS -v "$PWD":/app -w /app devkitpro/devkita64 \
        bash -lc 'git config --global --add safe.directory /app; eval "$STEPS"'
fi

echo
echo "Done. Output: build_switch/thomaz.nro"
echo "Copy it to your SD card at: /switch/thomaz.nro"
