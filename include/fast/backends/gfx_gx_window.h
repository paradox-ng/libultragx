#pragma once

#include <fast/backends/gfx_window_manager_api.h>

namespace Fast {

// libogc/GX implementation of the Fast3D window backend. Owns VI + the GX FIFO
// and presents each frame (EFB -> XFB -> scanout). On console most of the
// desktop window/mouse/fullscreen surface is inert; the live methods are Init,
// the per-frame SwapBuffers pair, HandleEvents (pad-driven quit), GetTime and
// the dimension getters. libogc types are kept out of this header (opaque void*)
// so it stays light; the .cpp is the GX translation unit.
class GfxWindowBackendGX final : public GfxWindowBackend {
  public:
    GfxWindowBackendGX() = default;
    ~GfxWindowBackendGX() override = default;

    void Init(const char* gameName, const char* apiName, bool startFullScreen, uint32_t width, uint32_t height,
              int32_t posX, int32_t posY) override;
    void Close() override;
    void SetKeyboardCallbacks(bool (*onKeyDown)(int), bool (*onKeyUp)(int), void (*onAllKeysUp)()) override;
    void SetMouseCallbacks(bool (*onMouseButtonDown)(int), bool (*onMouseButtonUp)(int)) override;
    void SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool)) override;
    void SetFullscreen(bool fullscreen) override;
    void GetActiveWindowRefreshRate(uint32_t* refreshRate) override;
    void SetCursorVisibility(bool visible) override;
    void SetMousePos(int32_t posX, int32_t posY) override;
    void GetMousePos(int32_t* x, int32_t* y) override;
    void GetMouseDelta(int32_t* x, int32_t* y) override;
    void GetMouseWheel(float* x, float* y) override;
    bool GetMouseState(uint32_t btn) override;
    void SetMouseCapture(bool capture) override;
    bool IsMouseCaptured() override;
    void GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) override;
    void SetDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) override;
    Ship::WindowRect GetPrimaryMonitorRect() override;
    void HandleEvents() override;
    bool IsFrameReady() override;
    void SwapBuffersBegin() override;
    void SwapBuffersEnd() override;
    double GetTime() override;
    int GetTargetFps() override;
    void SetTargetFps(int fps) override;
    void SetMaxFrameLatency(int latency) override;
    const char* GetKeyName(int scancode) override;
    bool CanDisableVsync() override;
    bool IsRunning() override;
    void Destroy() override;
    bool IsFullscreen() override;

  private:
    void* mRmode = nullptr;          // GXRModeObj*
    void* mFrameBuffer[2] = { nullptr, nullptr };
    void* mFifo = nullptr;
    uint32_t mFbIndex = 0;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint64_t mStartTicks = 0;
    bool mInitialized = false;
};

// Factory: create the GX window backend (returned as the abstract base so the
// gbi-side interpreter can drive it without pulling in GX headers).
GfxWindowBackend* lugx_create_gx_window_backend();

} // namespace Fast
