# libultragx

A lean GameCube/Wii native runtime for N64 PC decomp ports - the GX-backed
counterpart to [libultraship](https://github.com/Kenix3/libultraship). Where
libultraship targets PC, libultragx targets **GameCube and Wii** via
devkitPPC + libogc, so Fast3D-based N64 decomp ports can run natively on the
real hardware.

## Status

Bring-up, GameCube-first.

- **Fast3D on GX.** The libultraship Fast3D interpreter is ported onto the GX
  fixed-function pipeline: TEV stages implement the color combiner, with hardware
  transform, lighting, and texturing in place of shaders. N64 F3D display lists
  decoded from an `.o2r` archive render on real hardware.
- **Resource system.** `.o2r` archive mounting, CRC64 path hashing, `ResourceManager`,
  and a `ResourceLoader` keyed on (format, type, version). Binary factories cover
  the Fast3D types (display list, vertex, matrix, texture, light) and Blob/JSON.
  Class and method names track libultraship, so a port's factory registrations
  build against libultragx unchanged.
- **Framework.** `Ship::Context`, the `Ship::Window` abstraction with a Fast3D
  window (`Fast::Fast3dWindow`), a `ControlDeck` reading the GameCube pad through
  libogc, an event system, and the C bridges (resource, cvar, window, events).
- **Platform.** SD card mounting (SD2SP2 / SD Gecko / Wii SD) and per-game path
  resolution from `argv[0]`.

Current focus: building a port (Super Mario 64, via the Ghostship fork) against
libultragx in place of libultraship and booting it under Dolphin.

Several standalone test apps are included, selected with `APP=` (see Building).

See `docs/ARCHITECTURE.md` for the design (libultragx as a drop-in libultraship
replacement) and the roadmap.

## Building

The toolchain runs in Docker so nothing is installed on the host. GameCube is the
default target:

```sh
./build.sh                       # GameCube, default app -> libultragx-gamecube.dol
./build.sh PLATFORM=wii          # Wii                    -> libultragx-wii.dol
./build.sh APP=realdltest        # pick a test app (renders an N64 display list)
./build.sh clean                 # remove build artifacts
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
├── include/               # public API headers (libultraship-compatible)
│   ├── libultraship/      # umbrella, bridges, color, log, N64 ABI (libultra)
│   ├── ship/              # Ship:: framework (context, resource, window, events, ...)
│   └── fast/              # Fast3D interpreter + GX window/resource headers
├── source/
│   ├── apps/              # standalone test apps (smoke, realdltest, fwtest, ...)
│   ├── platform/          # libogc platform layer (sd, paths)
│   ├── gfx/               # GX rendering backend (TEV combiner, T&L, textures)
│   ├── fast/              # Fast3D interpreter, Fast3dWindow, resource factories
│   ├── ship/              # Context, resource (archive/loader/factories), events, controller
│   ├── window/gui/        # minimal GUI surface
│   ├── config/            # CVar store
│   ├── bridge/            # C bridge implementations
│   └── log/               # log sink
├── extern/                # vendored prism (shader translator) + nlohmann json
├── docs/ARCHITECTURE.md   # design + roadmap
├── Makefile               # devkitPPC build, GameCube default (PLATFORM=wii for Wii)
├── Dockerfile             # pinned build image (build.sh uses upstream by default)
├── build.sh               # containerized `make` wrapper (APP=, PLATFORM=)
└── run.sh                 # launches Dolphin on the built .dol
```
