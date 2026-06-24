# Ghostship integration roadmap

Goal: boot the Ghostship (SM64 PC port) game on GameCube/Wii by linking it against
libultragx instead of libultraship, source unchanged. The render keystone is proven
(real Fast3D content renders lit on GX - see `apps/realdltest`); this phase wires the
*game* to drive that path through its own display-list stream.

## The boot the game performs

`Ghostship/src/port/Engine.cpp` `GameEngine()` calls, in order:

```
Ship::Context::CreateUninitializedInstance("Ghostship", "sm64", "ghostship.cfg.json")
  -> InitConfiguration() / InitConsoleVariables() / InitLogging()
  -> InitControlDeck(std::make_shared<LUS::ControlDeck>())
  -> InitResourceManager({assets_path}, {}, 3)
  -> InitConsole()
  -> gsFast3dWindow = std::make_shared<Fast::Fast3dWindow>({})
  -> InitWindow(gsFast3dWindow)
  -> InitEventSystem()
  -> InitScriptLoader(...)            // TCC mod runtime - no-op on console
then GameEngine::LoadResourceFiles()  // AddArchive("sm64.o2r"), patches/mods
then the per-frame loop drives Fast3D through GBIMiddleware.
```

## Increments

1. **Context boot surface** - DONE (this commit). `CreateUninitializedInstance` +
   all `Init*` the game calls, matching `Engine.cpp` signatures. ResourceManager +
   CVars wire to the real lean subsystems; ControlDeck/Window/Console/EventSystem/
   ScriptLoader store-and-succeed (real backends below). Compiles standalone.

2. **`Ship::Window` base + `Fast::Fast3dWindow`** (the core render integration).
   Fast3dWindow wraps a `GfxRenderingAPI` + `GfxWindowBackend` (we have the GX ones)
   and runs StartFrame/Run/EndFrame. Upstream is 460 lines tangled with ImGui/Gui
   windows - build a LEAN version: no GuiWindow list, no ImGui; just own the GX
   backends + the interpreter loop + expose the surface `Engine.cpp`/`GBIMiddleware`
   call (GetWidth/Height, the gfx-api accessor, MainLoop/draw entry). realdltest's
   driver is the proof-of-concept to generalize.

3. **`GBIMiddleware` + the `<libultraship.h>` umbrella** - PARTLY DONE. Confirmed the
   render bridge: `GameEngine::ProcessGfxCommands(Gfx*)` -> `RunCommands` ->
   `wnd->DrawAndRunGraphicsCommands(commands, r.mtx, r.dl)`, which is exactly the
   Fast3dWindow method we implemented (mtx/dl come from the frame-interpolation
   system). `GBIMiddleware.cpp` itself is 108 lines of `extern "C"` GBI shims
   (`gSPDisplayList`/`gSPVertex`/...) that resolve resource paths via
   `ResourceManager::LoadResource` + touch `Fast::DisplayList::Instructions[]` - all
   present. Created the `<libultraship.h>` umbrella (libultraship.h + classes.h) that
   aggregates libultragx's existing Ship:: surface; it compiles (apps/fwtest now pulls
   Context/ResourceManager/archives through it). STILL NEEDED for the game's full
   include surface: the gbi macro shims (`__gSPDisplayList` etc., `GfxPatch`,
   `ResourceGetDataByName`), and the subsystems the umbrella's classes.h still TODOs:
   **ControlDeck/Controller, Console, Config, CrashHandler, Audio** (steps 4 + below).

4. **`LUS::ControlDeck`** over PAD/WPAD - map GC pad + Wii remote to the N64 buttons
   the game reads. Minimal: one controller, the standard mapping.

5. **Strip ImGui**: remove `src/port/ui/` (~16 files) from the build and bake fixed
   config as CVar defaults (all `gEnhancements.*` off = vanilla; 480p 4:3; 30fps).
   Resolution/fps are OUR backend's job. See the ImGui decision in memory.

6. **Cross-compile the game source for PPC** - the big one. The decomp SM64 +
   goddard + audio + menu + port layer, built with the libultragx Makefile (add the
   game source dirs to SOURCES) or as a lib the devkitPPC game build links. Chase the
   first wave of compile errors: desktop assumptions (SDL/x86), endianness, the asset
   segment system. First milestone: **compiles**; then **links**; then **reaches the
   main loop** (M1); then first game frame on screen (M2/M3).

7. **Audio (ASND), save (libfat), boot polish.**

## Backend follow-ups the real game will need (beyond realdltest's single model)

- Per-vertex `GX_VA_PNMTXIDX` + split each combined-MVP palette slot into affine
  modelview + perspective projection (realdltest hard-loads one slot with the view in
  the GX modelview - that is the single-matrix proof; G_MTX-driven actors need the
  general split).
- 2-cycle TEV, TEXEL1 / second texmap, CI (palette) textures.
- Real render modes from `othermode` (z-compare/update, blend, cull).
