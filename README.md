# libultragx

libultragx (LUGX) is a library that provides reimplementations of libultra (the
N64 SDK) and the [libultraship](https://github.com/Kenix3/libultraship) runtime
that run natively on the Nintendo GameCube and Wii.

Where libultraship targets modern PCs, libultragx targets the GameCube and Wii
hardware directly through devkitPPC and libogc. It decodes the same Fast3D N64
display lists and drives them through the GX fixed-function pipeline (TEV color
combiner, hardware transform, lighting, and texturing) instead of a shader-based
PC graphics API. It loads assets from the same `.otr` and `.o2r` archive files,
keeping game data separate from the executable so a port ships its assets
unchanged.

The goal is to be a drop-in replacement for libultraship: a libultraship-based
N64 decomp port should build against libultragx in place of libultraship and run
on real hardware. Class names, method names, and the public API track
libultraship so that a port's resource factory registrations and framework calls
compile without modification.

## Status

Bring-up, GameCube first. The GameCube (24 MB of RAM) is the constraint that
drives the design; the Wii build is the secondary target and is what is currently
tested under Dolphin.

- **Fast3D on GX.** The libultraship Fast3D interpreter is driven by an original
  GX backend: TEV stages implement the N64 color combiner, with hardware
  transform, lighting, and texturing in place of shaders. N64 F3D display lists
  decoded from an `.o2r` archive render on hardware.
- **Resource system.** `.o2r` archive mounting, CRC64 path hashing,
  `ResourceManager`, and a `ResourceLoader` keyed on (format, type, version).
  Binary factories cover the Fast3D types (display list, vertex, matrix, texture,
  light) plus Blob and JSON. Class and method names track libultraship, so a
  port's factory registrations build against libultragx unchanged.
- **Framework.** `Ship::Context`, the `Ship::Window` abstraction with a Fast3D
  window (`Fast::Fast3dWindow`), a `ControlDeck` reading the GameCube pad through
  libogc, an event system, and the C bridges (resource, cvar, window, events).
- **Platform.** SD card mounting (SD2SP2 / SD Gecko / Wii SD) and per-game path
  resolution from `argv[0]`.

Current focus is bringing up Super Mario 64 (via the Ghostship fork) against
libultragx in place of libultraship. See `docs/ARCHITECTURE.md` for the design and
roadmap.

## Building

The toolchain runs in Docker, so nothing is installed on the host. The first run
pulls the `devkitpro/devkitppc` image (1-2 GB). GameCube is the default target.

```sh
./build.sh                       # GameCube, default app -> libultragx-gamecube.dol
./build.sh PLATFORM=wii          # Wii                    -> libultragx-wii.dol
./build.sh APP=realdltest        # pick a test app (renders an N64 display list)
./build.sh clean                 # remove build artifacts
```

Several standalone test apps are included and selected with `APP=` (for example
`smoke`, `realdltest`, `archtest`, `gfxdemo`).

A fresh clone needs its submodules before building, since the prism shader
translator is vendored as a submodule:

```sh
git submodule update --init --recursive
```

## Running

Dolphin runs natively on the host and loads the built `.dol`:

```sh
./run.sh                       # boots libultragx-gamecube.dol
./run.sh libultragx-wii.dol    # boot the Wii build instead
```

Press START (GameCube controller) or HOME (Wii remote) to exit. Dolphin is not
bit-accurate to the Flipper and Hollywood GPUs, so real hardware remains the final
check.

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

## Contributing

Contributions are welcome through pull requests and issues. Because the public API
is meant to stay compatible with libultraship, a change that alters a shared class,
method, or ABI signature should explain how a port that builds against
libultraship continues to build against libultragx.

## Versioning

libultragx follows semantic versioning. The public API covered by versioning is
the surface a port compiles and links against: the C-linkage bridge functions, the
`Ship::` and `Fast::` classes and their methods, the resource factory and type
interfaces, and the adopted N64 ABI headers. The GX rendering backend and the
platform layer are implementation details and may change between minor versions.

## Acknowledgements

libultragx reimplements the libultraship runtime; it would not exist without the
projects below.

- [libultraship](https://github.com/Kenix3/libultraship) (MIT), by Kenix3 and the
  Harbour Masters contributors. libultragx tracks its API and reuses two pieces
  directly: the Fast3D display-list interpreter (the proven heart of every
  libultraship port, driven here through a new GX backend) and the interface-only
  N64 ABI headers, which must stay byte-identical across all decomp ports. The
  exact files and their retained copyrights are listed in `NOTICE.md`.
- Fast3D, the N64 F3D/F3DEX display-list interpreter that libultraship is built
  around.
- libultra and the N64 decompilation lineage, which define the OS and graphics
  binary interface that every N64 PC port compiles against.
- [devkitPro](https://devkitpro.org/): devkitPPC and libogc provide the PowerPC
  toolchain and the GameCube/Wii runtime (GX, VI, SI/PAD, SD, threads) that
  libultragx builds on.
- [sm64](https://github.com/n64decomp/sm64), the Super Mario 64 decompilation,
  used as a reference while bringing up the first port.
- [sm64-port](https://github.com/sm64-port/sm64-port) and its Wii branch by
  [mkst](https://github.com/mkst/sm64-port), the reference for translating Fast3D
  to GX: software transform and clipping with a pass-through perspective so GX
  performs only the divide.
- [prism-processor](https://github.com/KiritoDv/prism-processor) by KiritoDv, the
  combiner and shader translator, vendored as a submodule
  ([our fork](https://github.com/paradox-ng/prism-processor) carries a devkitPPC
  build patch).
- [nlohmann/json](https://github.com/nlohmann/json), used for JSON resources.
- [Dolphin](https://dolphin-emu.org/), used to test the Wii build during bring-up.
- The libultraship-based N64 decomp ports that libultragx aims to run, by Harbour
  Masters and others: [Ghostship](https://github.com/HarbourMasters/Ghostship)
  (SM64), [Starship](https://github.com/HarbourMasters/Starship) (Star Fox 64),
  [SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) (Mario Kart 64),
  [BattleShip](https://github.com/JRickey/BattleShip) (Super Smash Bros.),
  [Shipwright](https://github.com/HarbourMasters/Shipwright) (Ocarina of Time), and
  [2ship2harkinian](https://github.com/HarbourMasters/2ship2harkinian) (Majora's
  Mask).

## License

The framework code in libultragx is original work. The third-party code adopted
from libultraship (the Fast3D interpreter and the N64 ABI headers) is MIT licensed
and retains its original copyright notices; the specific files are enumerated in
`NOTICE.md`. Vendored dependencies are governed by their own licenses.
