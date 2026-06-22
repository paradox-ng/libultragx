# libultragx

A lean GameCube/Wii native runtime for N64 PC decomp ports - the GX-backed
counterpart to [libultraship](https://github.com/Kenix3/libultraship). Where
libultraship targets PC, libultragx targets **GameCube and Wii** via
devkitPPC + libogc, so Fast3D-based N64 decomp ports can run natively on the
real hardware.

## Status

**M0 - pipeline bring-up (done).** A spinning vertex-colored triangle through GX
proved the devkitPPC -> libogc -> Docker -> Dolphin loop on GameCube.

**M1 - platform foundation (in progress).** SD card mount (SD2SP2 / SD Gecko /
Wii SD) and per-game path resolution from `argv[0]`. The current `.dol` is a
console smoke test that reports the SD device, the resolved per-game base dir,
and its contents - run it on hardware to validate the SD layout.

See `docs/ARCHITECTURE.md` for the full design (libultragx as a drop-in
libultraship replacement) and the milestone roadmap.

## Building

The toolchain runs in Docker so nothing is installed on the host. GameCube is the
default target:

```sh
./build.sh                 # GameCube -> libultragx-gamecube.dol
./build.sh PLATFORM=wii    # Wii      -> libultragx-wii.dol
./build.sh clean           # remove build artifacts
```

First run pulls the `devkitpro/devkitppc` image (~1-2 GB).

## Running

Dolphin runs natively on the host (flatpak) and loads the `.dol`:

```sh
./run.sh                       # boots libultragx-gamecube.dol
./run.sh libultragx-wii.dol    # boot the Wii build instead
```

Press **START** (GC controller) or **HOME** (Wii remote) to exit.

## Layout

```
libultragx/
├── source/
│   ├── main.c             # current app (M1 smoke test)
│   └── platform/          # libogc platform layer (sd, paths, ...)
├── docs/ARCHITECTURE.md   # design + milestone roadmap
├── Makefile               # devkitPPC build, GameCube default (PLATFORM=wii for Wii)
├── Dockerfile             # pinned build image (build.sh uses upstream by default)
├── build.sh               # containerized `make` wrapper
└── run.sh                 # launches Dolphin on the built .dol
```
