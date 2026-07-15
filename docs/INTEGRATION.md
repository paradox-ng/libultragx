# Game integration: how Ghostship drives libultragx

Ghostship (the SM64 PC port) runs on GameCube/Wii by linking against libultragx
instead of libultraship, with its source unchanged. This document describes how the
game drives the runtime. The integration is complete: SM64 is playable on the Wii
build.

## Boot

`Ghostship/src/port/Engine.cpp` `GameEngine()` calls, in order:

```
Ship::Context::CreateUninitializedInstance("Ghostship", "sm64", "ghostship.cfg.json")
  -> InitConfiguration() / InitConsoleVariables() / InitLogging()
  -> InitControlDeck(...)
  -> InitResourceManager({assets_path}, {}, 3)
  -> InitConsole()
  -> gsFast3dWindow = std::make_shared<Fast::Fast3dWindow>({})
  -> InitWindow(gsFast3dWindow)
  -> InitEventSystem()
  -> InitScriptLoader(...)            // TCC mod runtime - no-op on console
then GameEngine::LoadResourceFiles()  // AddArchive("sm64.o2r"), patches/mods
then the per-frame loop drives Fast3D.
```

libultragx provides `CreateUninitializedInstance` and every `Init*` the game calls,
matching the `Engine.cpp` signatures. Before this runs, the SD card is mounted and
the base directory (`sd:/Ghostship/`) is resolved from `argv[0]`; `config.ini` is
read here.

## The render bridge

The game submits N64 display lists per frame through:

```
GameEngine::ProcessGfxCommands(Gfx*)
  -> RunCommands(commands, frame-interpolation replacements)
     -> for each replacement: Fast3dWindow::DrawAndRunGraphicsCommands(commands, mtx, dl)
```

`Fast3dWindow` owns the GX rendering backend (`GfxRenderingAPIGX`) and the GX window
backend (`GfxWindowBackendGX`), and runs the interpreter's StartFrame / Run /
EndFrame around each set of commands. The frame-interpolation system supplies the
matrix/display-list replacements; with interpolation enabled it emits one
interpolated plus one keyframe replacement per 30 fps logic tick, so `RunCommands`
renders and presents two frames per tick (60 fps motion, 30 fps simulation).

`GBIMiddleware.cpp` is the `extern "C"` GBI shim layer (`gSPDisplayList`/`gSPVertex`/
...): it resolves resource paths via `ResourceManager::LoadResource` and touches
`Fast::DisplayList::Instructions[]`. The `<libultraship.h>` umbrella (with
`classes.h`) aggregates libultragx's `Ship::` surface so the game's includes are
unchanged; it is C-safe (its C++ class includes are `#ifdef __cplusplus`-guarded)
because the decomp `.c` files pull it in.

## What was integrated

Everything the game needs to boot and run is in place:

- **Context + Init surface.** `CreateUninitializedInstance` and the `Init*` calls,
  wired to the real ResourceManager, CVar store, ControlDeck, Window, and event
  system.
- **`Ship::Window` + `Fast::Fast3dWindow`.** A lean version that owns the GX backends
  and the interpreter loop, without the ImGui/GUI window list.
- **`GBIMiddleware` + `<libultraship.h>`.** The GBI shims and the umbrella header.
- **`ControlDeck` over PAD/WPAD.** The GameCube pad and Wii remote mapped to the N64
  buttons.
- **ImGui stripped.** `src/port/ui/` is out of the console build; fixed config comes
  from `config.ini` + CVar defaults, and resolution/aspect/fps are the backend's job.
- **Game source cross-compiled for PPC.** The decomp SM64 + goddard + audio + menu +
  port layer, built with a devkitPPC Makefile in the Ghostship fork (libultragx as a
  submodule). Assets are runtime `.o2r` resources (no generated asset headers).
  Endianness is load-bearing: multi-byte resource reads must use the stored byte order
  on big-endian PPC.
- **Audio, saves, boot.** The game's audio synthesis plays on the DSP (ASND); saves
  are JSON files under `sd:/Ghostship/saves/`.

## Notes

- The N64 vertices the interpreter consumes are already quantized (s16 position, s16
  texcoord, s8 normal); the interpreter currently expands them to float for the
  software transform. A hardware-T&L path would feed them to GX in their native form.
- The GameCube build compiles and links; native GameCube runtime bring-up (SD access
  on a console with no built-in SD slot) is the remaining platform item.
