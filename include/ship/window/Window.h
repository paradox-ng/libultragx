#pragma once

#include <cstdint>
#include <memory>

// Lean stand-in for libultraship's ship/window/Window.h. Upstream this drags in
// ImGui, the controller mapping headers and a full abstract Window class; on
// GameCube/Wii the GX window backend owns VI/GX directly, so this exposes only the
// surface the game actually drives: the frame lifecycle, dimensions, and the few
// scancode/gui/refresh accessors. Desktop-only concepts (mouse, fullscreen, msaa)
// are deliberately absent; add them if a game's non-UI path turns out to need one.

namespace Ship {

struct WindowRect {
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
};

class Gui;

// Abstract window the game holds via Context::GetWindow(). Fast::Fast3dWindow is
// the concrete GX-backed implementation.
class Window {
  public:
    virtual ~Window() = default;

    virtual void Init() = 0;
    virtual void Close() = 0;
    virtual void RunGuiOnly() = 0;
    virtual void StartFrame() = 0;
    virtual void EndFrame() = 0;
    virtual bool IsFrameReady() = 0;
    virtual void HandleEvents() = 0;
    virtual bool IsRunning() = 0;

    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;
    virtual float GetAspectRatio() = 0;
    virtual uint32_t GetCurrentRefreshRate() = 0;
    virtual bool CanDisableVerticalSync() = 0;
    virtual uintptr_t GetGfxFrameBuffer() = 0;

    // Keyboard scancode latch (the game polls it for debug hotkeys); inert on console.
    int32_t GetLastScancode() {
        return mLastScancode;
    }
    void SetLastScancode(int32_t scancode) {
        mLastScancode = scancode;
    }
    // The dev-overlay GUI; null on console (the ImGui UI layer is stripped).
    std::shared_ptr<Gui> GetGui() {
        return mGui;
    }

  protected:
    int32_t mLastScancode = -1;
    std::shared_ptr<Gui> mGui;
};

} // namespace Ship
