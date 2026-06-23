#!/usr/bin/env bash
# Launch the built .dol in Dolphin (host flatpak). Build first with ./build.sh.
#   ./run.sh                 # boots libultragx-gamecube.dol (default target)
#   ./run.sh libultragx-wii.dol   # boot the Wii build instead
#   ./run.sh some_other.dol  # boot a different image
set -euo pipefail
cd "$(dirname "$0")"

DOL="${1:-libultragx-gamecube.dol}"
if [ ! -f "$DOL" ]; then
  echo "No '$DOL' found - run ./build.sh first." >&2
  exit 1
fi

# -b batch mode (quit when emulation stops), -e boot the given executable.
#  -C Dolphin.Display.Fullscreen=False : run in a window, not fullscreen.
#  -C Dolphin.Movie.DumpFrames + GFX.Settings.DumpFrames : record an AVI to
#     ~/.var/app/org.DolphinEmu.dolphin-emu/data/dolphin-emu/Dump/Frames/.
#     BOTH flags are required - the GFX one alone silently records nothing.
#     Set LUGX_NO_DUMP=1 to skip recording.
DUMP_FLAGS=()
if [ "${LUGX_NO_DUMP:-0}" != "1" ]; then
  DUMP_FLAGS=(-C "Dolphin.Movie.DumpFrames=True" -C "GFX.Settings.DumpFrames=True")
fi
exec flatpak run org.DolphinEmu.dolphin-emu -b -e "$PWD/$DOL" \
  -C "Dolphin.Display.Fullscreen=False" "${DUMP_FLAGS[@]}"
