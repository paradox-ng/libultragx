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
| `ControlDeck`, `ControllerStick`, `PhysicalDeviceType` | reimplemented | libogc PAD, and WPAD behind a C shim |
| audio output | reimplemented (the game's synthesis, played on the DSP) | libogc ASND |
| `bridge.h`, `consolevariablebridge.h` (CVar) | reimplemented lean | - |
| `libultra/types.h`, `libultra/controller.h` | reimplemented (OS stubs) | libogc threads/DMA |
| `GuiWindow`, `ConsoleWindow`, `*DebuggerWindow`, `Font` | no-op stubs (the ImGui "PC baggage") | - |
| `Window` / window-manager | VI/GX bring-up | libogc VI |

## A constraint that shapes the module boundaries

The N64 ABI and libogc define **the same type names with different meanings**, so
some of the separation here is mandatory rather than stylistic:

- `Vtx` is an N64 vertex in `gbi.h` and a GX vertex in `gccore.h`.
- `Mtx` is the N64 fixed-point matrix in the N64 ABI and a `f32[3][4]` in GX.

Two headers that each define one of them cannot meet in a single translation unit.
The architecture already separates them by role, and that separation has to be kept
deliberately: the GX backend is GX-only and never includes gbi, while the
interpreter and the resource factories are gbi-only and reach the backend through
the abstract `GfxRenderingAPI`. Where the two genuinely must meet, the crossing is
made explicit through a small C shim with neutral types. Reading the Wii Remote is
the clearest example: `wiiuse/wpad.h` carries the GX `Mtx`, while the `OSContPad`
the game consumes carries the N64 one, so the remote is read in its own C file that
reports plain integers, and that file is the only place the two worlds touch.

## Rendering

GameCube/Wii have no shaders, only GX's fixed-function **TEV** combiner. N64 Fast3D
combiner modes are a finite, enumerable set, so mapping them to TEV is a decode, not
a shader compiler. `gfx_gx` implements the `GfxRenderingAPI` interface (vertex
submission, texture upload, combiner -> TEV, render state):

- **Combiner.** The N64 color combiner `(A-B)*C+D` is decoded over its real
  `(A,B,C,D)` inputs into TEV stage setup (`gfx_gx_tev`).
- **Textures.** N64 texture data is packed into native GX formats -
  `GX_TF_RGB5A3` for the N64's dominant 16-bit RGBA (lossless) and `GX_TF_I*`/
  `GX_TF_IA*` for intensity/alpha (`gfx_gx_tex`) - keeping texture memory within the
  GameCube budget rather than expanding everything to 32-bit. Colour-index textures
  are expanded through their palette on the CPU, because GX's fixed-function TEV has
  no dependent palette fetch; the interpreter asks the backend whether it can do the
  lookup and falls back accordingly.
- **Transform.** Vertex transform and near-plane clipping run on the PowerPC CPU
  (following the sm64-port Wii reference): each vertex is multiplied by its
  matrix-palette slot into clip space and fed to GX with a fixed pass-through
  perspective, so GX performs only the perspective divide and rasterization.
  Lighting uses GX hardware (light objects + the diffuse channel). This software
  T&L sidesteps GX's separate affine-modelview / projection matrix model, which does
  not map cleanly onto the interpreter's combined per-object MVP; moving the
  transform onto GX hardware is a possible future rewrite (see the GX manual notes).
- **Fog.** The N64 supplies a fog factor three ways, and each lands somewhere
  different on GX. A ramp over depth drives GX's dedicated fog unit, which costs no
  TEV stage and no CPU work. The other two, a factor carried in each vertex's alpha
  and a constant veil, are not functions of distance and so cannot use that unit;
  they get TEV stages instead.
- **Reflection mapping.** Surfaces that ask for texture coordinates generated from
  the vertex normal are handled by GX's own texture coordinate generation: the N64
  projects the normal onto two lookat vectors and applies the tile transform, which
  is exactly a 2x4 matrix on the normal, so GX generates the coordinates from the
  normal it already has for lighting. No CPU work per vertex.

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

Target GameCube setup: PicoBoot + Swiss + SD2SP2 + microSD. Each game keeps its
assets and writable data in one folder:

```
sd:/
  Ghostship/              <- base dir (the "app directory")
    sm64.o2r              read-only assets (ResourceManager streams these)
    config.ini            settings (aspect ratio, fps counter, frame interpolation)
    saves/                save data (one JSON file per save slot)
```

libultragx mounts the SD card (GameCube: libfat `__io_gcsd2` for SD2SP2, with
SD Gecko slots A/B as fallback; Wii: the front SD slot). The base dir is then set one
of two ways:

- **Games** get `sd:/<name>/` from the name passed to
  `Context::CreateUninitializedInstance`, which is set when the SD card is mounted
  during boot. This is what actually applies in practice, and it means the `.dol` can
  live anywhere and be named anything.
- **Programs that call `InitPaths` explicitly** (the standalone test apps) derive it
  from `argv[0]`, the path a loader such as Swiss passes via the argv protocol:
  `dirname(argv0) + "/" + stem(argv0) + "/"`, falling back to `sd:/libultragx/` when
  no path is given. A game could opt into this by calling `InitPaths`, which would
  then override the name-based default.

This is how libultragx implements libultraship's path API:
`Context::GetAppDirectoryPath`, `GetPathRelativeToAppDirectory`, and
`LocateFileAcrossAppDirs` all resolve under this base dir. The archive is streamed
from SD on demand rather than read fully into RAM, which is what keeps the ~10 MB of
game assets off the GameCube's 24 MB budget.

## Current state

Three ports exercise the runtime at three different depths, which is the point:
each one finds a different class of gap.

- **Ghostship (Super Mario 64, F3D)** is playable on the Wii build. It boots,
  renders the whole game (3D world, actors, HUD, menus, dialogs, the interactive
  title-screen head), plays music and sound effects through the DSP, and reads and
  writes saves. It runs at its native 30 fps, with optional 60 fps frame
  interpolation.
- **Starship (Star Fox 64, F3DEX)** boots and runs at 30 fps with music, voices and
  sound effects. It found the colour-index texture path and a family of big-endian
  audio import bugs that SM64 never touched. Some main-menu geometry is still drawn
  wrong.
- **Shipwright (Ocarina of Time, F3DEX2)** compiles completely as native PowerPC and
  links. It does not run yet: the excluded ImGui interface leaves undefined symbols
  that the boot path reaches, and the binary is far too large for a GameCube and
  marginal on a Wii, so dead-code stripping is a prerequisite rather than a polish
  item.

The GameCube build compiles and links; native GameCube runtime bring-up (SD access
on a console with no built-in SD slot) is the main remaining platform item.

Ongoing work: performance headroom for the heaviest scenes and for the heavier
ports (reducing the DL-walk interpreter cost, and a possible hardware-T&L rewrite
around native quantized vertex arrays), and additional ports. Antialiasing has
landed as a `config.ini` option; how much it is worth is a question for real
hardware, since emulators do not reproduce the copy filter faithfully.

## Notes / constraints

1. **`.o2r` is a zip archive.** A lean streaming zip reader sits over libfat/SD;
   entries are read on demand so the archive is not held resident. Saves live in the
   per-game SD folder; a GameCube memory-card backend is a possible later option.
2. **C++ footprint on GameCube's 24 MB.** libultragx stays C++ (devkitPPC handles
   it) but lean; the main RAM lever was streaming the archive instead of holding it
   in memory. The Wii is validated first when RAM is tight.
3. **Endianness is load-bearing.** The archives store little-endian N64 data and the
   consoles are big-endian PowerPC. The reader handles byte order for typed reads,
   so the bugs cluster in the places that bypass it: raw block reads of multi-byte
   data, and hand-written byte swaps that are unconditional when they should follow
   the stored order.
4. **API version pinning.** libultragx matches Ghostship's pinned libultraship commit
   (`11ad2a55164cff42396a28ece2da478d8afc5e1a`). Other games pin different commits;
   reconcile per game.

## References

- `Kenix3/libultraship` (the API replaced; Ghostship pins `11ad2a55`)
- `sm64-port` Wii branch by [mkst](https://github.com/mkst/sm64-port) - the
  software-transform-to-GX reference (clip-space vertices + a pass-through
  perspective so GX performs only the divide)
- `prism-processor` (KiritoDv) - the combiner/shader translator, vendored
- `gfx_pc.c` Fast3D interpreter (reused, MIT)
- The Nintendo *Revolution Graphics Library (GX)* manual - the authoritative
  reference for the GX pipeline, matrix conventions, and texture formats
- The Nintendo *N64 Programming Manual* graphics chapters - the authoritative
  reference for what the game is asking the RSP and RDP to do
