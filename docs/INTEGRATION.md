# Porting a game to libultragx

A libultraship-based port runs on GameCube/Wii by linking against libultragx
instead of libultraship, with its game source unchanged. What does change is the
build and the port layer: the desktop-only parts of `src/port/` have to be gated
out, and the console's constraints have to be respected in a handful of places that
are the same for every game.

This document uses Ghostship (Super Mario 64) as the worked example, since it is the
port that is finished, and notes where Starship (Star Fox 64) and Shipwright
(Ocarina of Time) needed something different.

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

Newer ports use a combined `Context::Init()` with a `GetConfig()` accessor rather
than the piecemeal `Init*` calls. Starship was adapted to the piecemeal form on the
game side; absorbing the newer shape into libultragx so no adaptation is needed is a
pending improvement.

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

## Build scaffolding

Every port is CMake-only upstream, so each needs its own devkitPPC makefile. The
recipe is the same each time:

- **`Makefile.gx`** at the port root, auto-discovering the game sources and mirroring
  the exclusions the upstream CMake already makes, plus the desktop-only ones: the
  asset extractor, the ImGui interface, and the mod runtime.
- **`gx_stubs/`**, placed first on the include path so it shadows headers devkitPro
  does not have. The ImGui stub carries value types only (`ImVec2`, `ImVec4`, the flag
  typedefs) and no functions, which is enough for interface headers to compile while
  the interface sources stay out of the build.
- **`gx_compat/`**, forwarding the include paths a port uses (`<Fast3D/...>`,
  `<libultraship/src/...>`) to libultragx's leaner `fast/` and `ship/`.
- **`cmake/gx-cvars.mk`**, which is easy to miss and expensive to miss. The desktop
  CMake injects the `CVAR_PREFIX_*` defines as compiler flags; without them a single
  header produces thousands of errors and hides the real state of the build.
- **libultragx as a submodule**, remembering that it has its own submodules. A clone
  without `git submodule update --init --recursive` is missing the vendored prism and
  json and will not compile.

## Things every port hits

- **Endianness.** The archives hold little-endian N64 data and the console is
  big-endian PowerPC. Typed reads through the reader are handled for you; the bugs
  live in what bypasses it. Hunt every raw block read of multi-byte data and every
  hand-written byte swap, because an unconditional swap is correct only on a
  little-endian host. This class of bug dominated the Starship audio bring-up and
  produced silence, noise, and collapsed note envelopes rather than obvious errors.
- **Audio must be synchronous.** The desktop backend generates audio on a worker
  thread with a condition-variable handshake, which deadlocks on libogc's cooperative
  scheduler. On console the audio is generated inline in the frame. Generate exactly
  the number of updates per frame the game expects, because the sequence player and
  every note envelope advance once per update, so a varying count makes music and
  notes run at a varying rate.
- **Frame pacing.** A port may carry its own frame-interpolation setting independent
  of libultragx's, and if it writes the window's target frame rate every frame it will
  override anything set once at startup. Check the port's own path before concluding
  interpolation is off.
- **Link collisions with libogc.** Leaving any `gu` symbol for libogc to resolve pulls
  in its `gu.o` wholesale, which also defines `guLookAt` and `guPerspective` and then
  collides with the game's own. Keep the game's versions. Some ports also ship each
  `gu` function twice under two file names, so include one of each pair or the build
  collides with itself. A port's own N64 OS stub layer may duplicate libultragx's, in
  which case exclude the port's.
- **Compiler strictness.** Recent GCC promotes incompatible-pointer and int-conversion
  warnings to errors by default. Ports that reference assets as string paths assigned
  to typed pointers will produce these in the thousands, and `-w` does not demote a
  default-error: the specific `-Wno-` flags have to be passed explicitly.
- **Games with their own event system.** Newer ports reuse libultraship's generic
  event type names for their own system. Build with `LUGX_GAME_OWNS_EVENTS` so the
  umbrella stops exporting libultragx's, and the names no longer clash.

## Per-port state

- **Ghostship (SM64, F3D).** Complete. Boots, renders, plays audio, saves.
- **Starship (SF64, F3DEX).** Boots and runs at 30 fps with audio. Its colour-index
  textures and its big-endian audio import exposed gaps SM64 never reached. Some
  main-menu geometry is still drawn wrong.
- **Shipwright (OoT, F3DEX2).** Compiles fully as native PowerPC and links, which was
  the hard part. It does not run yet: the excluded interface leaves undefined symbols
  the boot path reaches, and the binary is far too large for a GameCube and marginal
  on a Wii, so dead-code stripping comes before anything else.

## Notes

- The N64 vertices the interpreter consumes are already quantized (s16 position, s16
  texcoord, s8 normal); the interpreter currently expands them to float for the
  software transform. A hardware-T&L path would feed them to GX in their native form.
- The GameCube build compiles and links; native GameCube runtime bring-up (SD access
  on a console with no built-in SD slot) is the remaining platform item.
