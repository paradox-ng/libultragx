# libultragx architecture

## Goal

libultragx is a lean GameCube/Wii reimplementation of
[libultraship](https://github.com/Kenix3/libultraship) (LUS). It is **not** a
backend bolted onto libultraship and **not** a fork of it. It is a drop-in
**replacement**: a game that today links libultraship instead links libultragx,
keeps its source unchanged, and runs natively on GX via libogc.

Primary target: **GameCube** (Gekko 485 MHz, 24 MB RAM + 3 MB GPU RAM,
big-endian). Secondary target: **Wii** (Broadway 729 MHz, 88 MB). GX is identical
on both, so one rendering backend serves both; the GameCube RAM budget is the
real constraint and drives every "lean" decision here.

## Where libultragx sits

The N64 decomp games (Ghostship/SM64 first) are not rewritten. They call a
per-game C glue layer (`GameEngine_*`), which calls the framework. We replace the
framework, not the glue:

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
must present that same API.

## Module layout (mirrors libultraship)

libultraship at the pinned commit (`11ad2a55`, the version Ghostship uses) splits
into three layers. libultragx mirrors them so the include paths and namespaces
match:

| Layer | libultraship | libultragx plan |
|---|---|---|
| `fast/` | Fast3D: `gfx_pc` interpreter, `GfxRenderingAPI` (`include/fast/backends/gfx_rendering_api.h`), backends (`gfx_opengl`, ...) | **Reuse** `gfx_pc` + the API; **add `gfx_gx`** backend + a libogc window-manager backend |
| `ship/` | 102 files: `Context`, `ResourceManager` + `Archive` + `ResourceFactory*`, `Window`, `ControlDeck`, `BinaryReader/Writer`, audio, GUI | **Reimplement lean** on libogc/GX. The bulk of the work |
| `libultraship/` | umbrella `<libultraship.h>`, C `bridge`/CVar, libultra stubs | **Reimplement API-identical** so game includes are unchanged |

### Per-module decisions

| Piece (usage in Ghostship) | Plan | Backed by |
|---|---|---|
| `Ship::Context` (~200x) | reimplement: singleton hub, app dirs, accessors | libfat paths |
| Fast3D `gfx_pc` + `GfxRenderingAPI` | reuse as-is, write GX backend | - |
| `gfx_gx` (new) | **the hard piece**: N64 combiner -> TEV mapping | GX TEV |
| `ResourceManager`, `Archive`, `ResourceFactoryBinary/XML`, `IResource`, `ResourceInitData` | reimplement | libfat + zip reader |
| `BinaryReader/Writer`, `File`, `Endianness::Big`, `SeekOffsetType` | reimplement (endianness is load-bearing) | - |
| `ControlDeck`, `ControllerStick`, `PhysicalDeviceType` | reimplement | libogc PAD/WPAD |
| `Ship::AudioBackend` | reimplement | libogc ASND |
| `bridge.h`, `consolevariablebridge.h` (CVar) | reimplement lean | - |
| `libultra/types.h`, `libultra/controller.h` | reimplement (OS stubs) | libogc threads/DMA |
| `GuiWindow`, `ConsoleWindow`, `ShaderSettingsWindow`, `*DebuggerWindow`, `Font` | **stub as no-ops** (the ImGui "PC baggage" we drop) | - |
| `Window` / window-manager | reimplement = our VI/GX bring-up | libogc VI |

## Rendering: the core problem

GameCube/Wii have no shaders, only GX's fixed-function **TEV** (8 stages). N64
Fast3D combiner modes are a finite, enumerable set, so mapping them to TEV is a
lookup-table problem, not a shader compiler. The reference is the Wii U port's
`gx2_shader_gen.c` (in `HarbourMasters/libultraship-wiiu` /
`GaryOderNichts/Shipwright`), which generates GX2 shaders from the same combiners;
we translate that logic to **TEV stage setup** instead.

`gfx_gx` implements the `GfxRenderingAPI` interface (vertex submission, texture
upload, combiner -> TEV, render state). Transform and lighting run on the
PowerPC CPU since GX has no vertex shaders.

## Build

devkitPPC + libogc, built in Docker (`devkitpro/devkitppc`), tested on Dolphin
(host) and real hardware. GameCube is the default target:

```sh
./build.sh                 # -> libultragx-gamecube.dol  (HW_DOL, PAD input)
./build.sh PLATFORM=wii    # -> libultragx-wii.dol       (HW_RVL, PAD + Wii remote)
./run.sh                   # boot the GameCube .dol in Dolphin
```

Per-platform object dirs (`build_gamecube/`, `build_wii/`) so both coexist. The
GX/VI code is identical across targets; only Wii-remote input is `HW_RVL`-gated.

## Milestones to "Mario renders"

- **M0 - done.** GX bring-up: spinning triangle, pipeline proven on GameCube.
- **M1 - linkable skeleton.** `Context` + `Window` + stubbed GUI + libultra stubs
  + CVar bridge. Goal: Ghostship compiles and links against libultragx and
  reaches its main loop (black screen). Link errors reveal the true API surface.
- **M2 - geometry.** `gfx_gx` behind Fast3D `gfx_pc`; the game's display lists
  render as untextured geometry.
- **M3 - assets + combiners.** `ResourceManager` loads `.otr`/`.o2r` from SD +
  the TEV combiner mapping. A **recognizable, textured Mario** appears here.
- **M4 - playable.** ControlDeck (PAD/WPAD), audio (ASND), save.

## Risks / open questions

1. **`.otr`/`.o2r` is a zip archive.** Need a lean zip reader over libfat/SD.
   GameCube SD access needs an adapter (SD Gecko / SD2SP2); memory card is the
   native save path.
2. **C++ footprint on GameCube's 24 MB.** libultragx stays C++ (devkitPPC handles
   it; the Wii U port is C++) but lean; watch the budget at M3. Wii validated first
   when RAM is tight.
3. **API version pinning.** We match Ghostship's pinned libultraship commit
   (`11ad2a55164cff42396a28ece2da478d8afc5e1a`). Other games may pin different
   commits; reconcile per game.

## References

- `Kenix3/libultraship` (the API we replace; pinned `11ad2a55`)
- `HarbourMasters/libultraship-wiiu`, `GaryOderNichts/Shipwright` -
  `gfx_gx2.cpp` + `gx2_shader_gen.c`, the combiner->hardware reference
- `gfx_pc.c` Fast3D interpreter (reused, MIT)
