#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "ship/window/Window.h"
#include "fast/interpreter.h"
#include "fast/debug/GfxDebugger.h"
#include "fast/backends/gfx_rendering_api.h"
#include "fast/backends/gfx_window_manager_api.h"
#include "fast/ucodehandlers.h"

namespace Ship {
class GuiWindow;
}

namespace Fast {

// Render backend identifiers (matches upstream Fast3D). libultragx only has the GX
// backend, but a game's window-settings menu references these names, so keep the
// enum for type/compile compatibility.
enum WindowBackend {
    FAST3D_DXGI_DX11 = 1,
    FAST3D_SDL_OPENGL = 2,
    FAST3D_SDL_METAL = 3,
    FAST3D_SDL_VULKAN = 4,
};

// Lean GX-backed Fast3D window. Wraps our GfxRenderingAPIGX + GfxWindowBackendGX and
// the Fast3D interpreter (all already proven by apps/realdltest), exposing the
// surface the game drives via Context::GetWindow() / its gsFast3dWindow handle.
// Upstream Fast3dWindow also carries the ImGui Gui, the mouse manager and SDL/DX
// backend selection - all dropped here (no ImGui, GX owns VI/GX directly).
class Fast3dWindow : public Ship::Window {
  public:
    Fast3dWindow();
    // The game constructs `Fast3dWindow({})` with its list of dev-overlay GuiWindows;
    // we ignore it (the ImGui UI layer is stripped on console).
    explicit Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows);
    ~Fast3dWindow() override;

    void Init() override;
    void Close() override;
    void RunGuiOnly() override;
    void StartFrame() override;
    void EndFrame() override;
    bool IsFrameReady() override;
    void HandleEvents() override;
    bool IsRunning() override;
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    float GetAspectRatio() override;
    uint32_t GetCurrentRefreshRate() override;
    bool CanDisableVerticalSync() override;
    uintptr_t GetGfxFrameBuffer() override;

    // Fast3D render surface the game drives each frame.
    int32_t GetTargetFps();
    void SetTargetFps(int32_t fps);

    // Depth readback at a screen point. Ocarina of Time uses it for its z-checks; the
    // GX backend does not resolve the EFB depth buffer back to main memory yet, so
    // this reports 0 (nothing occluded) rather than a wrong value.
    void GetPixelDepthPrepare(float x, float y);
    uint16_t GetPixelDepth(float x, float y);
    void SetMaximumFrameLatency(int32_t latency);
    void SetRendererUCode(UcodeHandlers ucode);
    // sRGB / gamma-boost output. Not yet wired to GX gamma (GX_SetDispCopyGamma); a
    // no-op keeps ports that toggle it (e.g. Starship's gEnableGammaBoost) compiling.
    void EnableSRGBMode() {}
    bool DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements,
                                    const std::unordered_map<Gfx*, Gfx*>& dlReplacements = {});

    std::weak_ptr<Interpreter> GetInterpreterWeak() const;
    std::shared_ptr<GfxDebugger> GetGfxDebugger() const;

  private:
    GfxRenderingAPI* mRenderingApi = nullptr;
    GfxWindowBackend* mWindowManagerApi = nullptr;
    std::shared_ptr<Interpreter> mInterpreter;
    std::shared_ptr<GfxDebugger> mGfxDebugger;
    int32_t mTargetFps = 30;
};

} // namespace Fast
