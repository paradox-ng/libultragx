#!/usr/bin/env bash
# Build libultragx inside the devkitPPC container so the host stays clean.
# Output: libultragx.dol / libultragx.elf in this directory. Load the .dol in
# Dolphin (see run.sh). Any args are passed through to make, e.g. ./build.sh clean
set -euo pipefail
cd "$(dirname "$0")"

IMAGE="${LIBULTRAGX_IMAGE:-devkitpro/devkitppc:latest}"

exec docker run --rm \
  -v "$PWD":/project -w /project \
  -u "$(id -u):$(id -g)" \
  "$IMAGE" make "$@"
