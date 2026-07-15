# libultragx architecture

## Goal

libultragx is a lean GameCube/Wii reimplementation of
[libultraship](https://github.com/Kenix3/libultraship) (LUS). It is **not** a
backend bolted onto libultraship and **not** a fork of it. It is a drop-in
**replacement**: a game that today links libultraship instead links libultragx,
keeps its source unchanged, and runs natively on GX via libogc.

Primary target: **GameCube** (Gekko 485 MHz, 24 MB RAM + 3 MB GPU RAM,
big-endian). Secondary target: **Wii** (Broadway 729 MHz, 88 MB), which is what
currently runs and is tested under Dolphin. GX is identical on both, so one
rendering backend serves both; the GameCube RAM budget is the real constraint and
drives every "lean" decision here.

## Where libultragx sits

The N64 decomp games (Ghostship/SM64 first) are not rewritten. They call a
per-game C glue layer (`GameEngine_*`), which calls the framework. libultragx
replaces the framework, not the glue:

```
SM64 decomp C   (unchanged)
      |
GameEngine_* glue   (per-game, C; unchanged: GameEngine_LoadSequence, _LoadDialog, _OTRSigCheck, ...)
      |
  <libultraship.h> API   <-- libultragx exposes this same surface
      |
[ libultraship -> SDL/OpenGL ]      replaced by      [ libultragx -> libogc/GX ]
```

The games `#include <libultraship.h>` and call into the `Ship::` namespace
(`Ship::Context::GetInstance` is the hub, used ~200x in Ghostship). libultragx
presents that same API.

## Module layout (mirrors libultraship)

libultragx mirrors libultraship's three layers so the include paths and namespaces
match. All three are implemented:

| Layer | libultraship | libultragx |
|---|---|---|
| `fast/` | Fast3D: the `gfx_pc` interpreter, `GfxRenderingAPI`, backends (`gfx_opengl`, ...) | Reuses the Fast3D interpreter and the API; adds the `gfx_gx` GX backend and a libogc window backend |
| `ship/` | `Context`, `ResourceManager` + `Archive` + `ResourceFactory*`, `Window`, `ControlDeck`, `BinaryReader/Writer`, audio, GUI | Reimplemented lean on libogc/GX; the ImGui/GUI windows are no-op stubs |
| `libultraship/` | umbrella `<libultraship.h>`, C `bridge`/CVar, libultra stubs | Reimplemented API-identical so game includes are unchanged |

### Per-module mapping

| Piece (usage in Ghostship) | libultragx | Backed by |
|---|---|---|
| `Ship::Context` (~200x) | singleton hub, app dirs, accessors | libfat paths |
| Fast3D `gfx_pc` + `GfxRenderingAPI` | reused; drives the GX backend | - |
| `gfx_gx` | N64 combiner -> TEV, software T&L, native textures | GX TEV |
| `ResourceManager`, `Archive`, `ResourceFactoryBinary/XML`, `IResource` | reimplemented | libfat + streaming zip reader |
| `BinaryReader/Writer`, `File`, `Endianness`, `SeekOffsetType` | reimplemented (endianness is load-bearing on big-endian PPC) | - |
| `ControlDeck`, `ControllerStick`, `PhysicalDeviceType` | reimplemented | libogc PAD/WPAD |
| audio output | reimplemented (the game's synthesis, played on the DSP) | libogc ASND |
| `bridge.h`, `consolevariablebridge.h` (CVar) | reimplemented lean | - |
| `libultra/types.h`, `libultra/controller.h` | reimplemented (OS stubs) | libogc threads/DMA |
| `GuiWindow`, `ConsoleWindow`, `*DebuggerWindow`, `Font` | no-op stubs (the ImGui "PC baggage") | - |
| `Window` / window-manager | VI/GX bring-up | libogc VI |

## Rendering

GameCube/Wii have no shaders, only GX's fixed-function **TEV** combiner. N64 Fast3D
combiner modes are a finite, enumerable set, so mapping them to TEV is a decode, not
a shader compiler. `gfx_gx` implements the `GfxRenderingAPI` interface (vertex
submission, texture upload, combiner -> TEV, render state):

- **Combiner.** The N64 color combiner `(A-B)*C+D` is decoded over its real
  `(A,B,C,D)` inputs into TEV stage setup (`gfx_gx_tev`), covering SM64's combiner
  set.
- **Textures.** N64 texture data is packed into native GX formats -
  `GX_TF_RGB5A3` for the N64's dominant 16-bit RGBA (lossless) and `GX_TF_I*`/
  `GX_TF_IA*` for intensity/alpha (`gfx_gx_tex`) - keeping texture memory within the
  GameCube budget rather than expanding everything to 32-bit.
- **Transform.** Vertex transform and near-plane clipping run on the PowerPC CPU
  (following the sm64-port Wii reference): each vertex is multiplied by its
  matrix-palette slot into clip space and fed to GX with a fixed pass-through
  perspective, so GX performs only the perspective divide and rasterization.
  Lighting uses GX hardware (light objects + the diffuse channel). This software
  T&L sidesteps GX's separate affine-modelview / projection matrix model, which does
  not map cleanly onto the interpreter's combined per-object MVP; moving the
  transform onto GX hardware is a possible future rewrite (see the GX manual notes).

To keep the per-frame CPU cost down, the interpreter caches the per-triangle
render-state decode: it is re-derived only when a non-drawing DL command may have
changed the state, and reused across the run of same-state triangles that follows.

## Build

devkitPPC + libogc, built in Docker (`devkitpro/devkitppc`), tested on Dolphin
(host) and real hardware. GameCube is the default target:

```sh
./build.sh                 # -> libultragx-gamecube.dol  (HW_DOL, PAD input)
./build.sh PLATFORM=wii    # -> libultragx-wii.dol       (HW_RVL, PAD + Wii remote)
./run.sh                   # boot the .dol in Dolphin
```

Per-platform object dirs so both coexist. The GX/VI code is identical across
targets; only Wii-remote input is `HW_RVL`-gated.

## On-SD layout and path resolution

Target GameCube setup: PicoBoot + Swiss + SD2SP2 + microSD. Each game is a `.dol`
next to a same-named folder holding its assets and writable data:

```
sd:/<anywhere>/
  Ghostship.dol
  Ghostship/              <- base dir (the "app directory"); name = .dol stem
    sm64.o2r              read-only assets (ResourceManager streams these)
    config.ini            settings (aspect ratio, fps counter, frame interpolation)
    saves/                save data (one JSON file per save slot)
```

libultragx mounts the SD card (GameCube: libfat `__io_gcsd2` for SD2SP2, with
SD Gecko slots A/B as fallback; Wii: the front SD slot) and derives the base dir
from `argv[0]` (the `.dol` path Swiss passes via the argv protocol):
`dirname(argv0) + "/" + stem(argv0) + "/"`. So naming the folder after the `.dol`
and dropping the pair anywhere on the card works with zero config. When `argv[0]`
is absent, the base dir defaults to the per-game folder (e.g. `sd:/Ghostship/`).

This is how libultragx implements libultraship's path API:
`Context::GetAppDirectoryPath`, `GetPathRelativeToAppDirectory`, and
`LocateFileAcrossAppDirs` all resolve under this base dir. The archive is streamed
from SD on demand rather than read fully into RAM, which is what keeps the ~10 MB of
game assets off the GameCube's 24 MB budget.

## Current state

Super Mario 64, through the Ghostship fork, is playable on the Wii build: it boots,
renders the whole game (3D world, actors, HUD, menus, dialogs, the interactive
title-screen head), plays music and sound effects through the DSP, and reads and
writes saves. It runs at its native 30 fps, with optional 60 fps frame
interpolation.

The GameCube build compiles and links; native GameCube runtime bring-up (SD access
on a console with no built-in SD slot) is the main remaining platform item.

Ongoing work: performance headroom for the heaviest scenes and for the heavier
ports (reducing the DL-walk interpreter cost, and a possible hardware-T&L rewrite
around native quantized vertex arrays), antialiasing, and additional ports.

## Notes / constraints

1. **`.o2r` is a zip archive.** A lean streaming zip reader sits over libfat/SD;
   entries are read on demand so the archive is not held resident. Saves live in the
   per-game SD folder; a GameCube memory-card backend is a possible later option.
2. **C++ footprint on GameCube's 24 MB.** libultragx stays C++ (devkitPPC handles
   it) but lean; the main RAM lever was streaming the archive instead of holding it
   in memory. The Wii is validated first when RAM is tight.
3. **API version pinning.** libultragx matches Ghostship's pinned libultraship commit
   (`11ad2a55164cff42396a28ece2da478d8afc5e1a`). Other games may pin different
   commits; reconcile per game.

## References

- `Kenix3/libultraship` (the API replaced; Ghostship pins `11ad2a55`)
- `sm64-port` Wii branch by [mkst](https://github.com/mkst/sm64-port) - the
  software-transform-to-GX reference (clip-space vertices + a pass-through
  perspective so GX performs only the divide)
- `prism-processor` (KiritoDv) - the combiner/shader translator, vendored
- `gfx_pc.c` Fast3D interpreter (reused, MIT)
- The Nintendo *Revolution Graphics Library (GX)* manual - the authoritative
  reference for the GX pipeline, matrix conventions, and texture formats
