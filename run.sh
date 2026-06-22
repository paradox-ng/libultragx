#!/usr/bin/env bash
# Launch the built .dol in Dolphin (host flatpak). Build first with ./build.sh.
#   ./run.sh                 # boots libultragx.dol
#   ./run.sh some_other.dol  # boots a different image
set -euo pipefail
cd "$(dirname "$0")"

DOL="${1:-libultragx.dol}"
if [ ! -f "$DOL" ]; then
  echo "No '$DOL' found - run ./build.sh first." >&2
  exit 1
fi

# -b batch mode (quit when emulation stops), -e boot the given executable.
exec flatpak run org.DolphinEmu.dolphin-emu -b -e "$PWD/$DOL"
