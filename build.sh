#!/usr/bin/env bash
# Build libultragx inside the devkitPPC container so the host stays clean.
# Default target is GameCube -> libultragx-gamecube.dol. Args pass through to make:
#   ./build.sh                 # GameCube build (default)
#   ./build.sh PLATFORM=wii    # Wii build -> libultragx-wii.dol
#   ./build.sh clean
set -euo pipefail
cd "$(dirname "$0")"

IMAGE="${LIBULTRAGX_IMAGE:-devkitpro/devkitppc:latest}"

exec docker run --rm \
  -v "$PWD":/project -w /project \
  -u "$(id -u):$(id -g)" \
  "$IMAGE" make "$@"
