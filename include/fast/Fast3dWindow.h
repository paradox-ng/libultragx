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
    void SetMaximumFrameLatency(int32_t latency);
    void SetRendererUCode(UcodeHandlers ucode);
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
