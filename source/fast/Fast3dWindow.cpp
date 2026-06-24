// Lean GX Fast3dWindow: a thin wrapper around the Fast3D interpreter + the GX
// backends (the exact path apps/realdltest exercises by hand), packaged behind the
// Ship::Window surface the game holds. gbi-side TU (pulls interpreter.h); the GX
// backends are created across the link via the gbi-free factory declarations below.

#include "fast/Fast3dWindow.h"

namespace Fast {

// Defined in the GX translation units (gfx_gx_api.cpp / gfx_gx_window.cpp); declared
// here gbi-free so this gbi-side TU can create the concrete backends across the link.
GfxRenderingAPI* lugx_create_gx_rendering_api();
GfxWindowBackend* lugx_create_gx_window_backend();
// Registers the interpreter as the singleton the global gbi command handlers reach.
void GfxSetInstance(std::shared_ptr<Interpreter> gfx);

Fast3dWindow::Fast3dWindow() = default;

Fast3dWindow::Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> /*guiWindows*/) {
}

Fast3dWindow::~Fast3dWindow() = default;

void Fast3dWindow::Init() {
    if (mInterpreter != nullptr) {
        return; // already initialised
    }
    mRenderingApi = lugx_create_gx_rendering_api();
    mWindowManagerApi = lugx_create_gx_window_backend();
    mInterpreter = std::make_shared<Interpreter>();
    GfxSetInstance(mInterpreter);
    // Run() dereferences the debugger per command (a hard crash on real hardware if
    // null); give it a real one.
    mGfxDebugger = std::make_shared<GfxDebugger>();
    mInterpreter->SetGfxDebugger(mGfxDebugger);
    mInterpreter->Init(mWindowManagerApi, mRenderingApi, "libultragx", false, 640, 480, 0, 0);
}

void Fast3dWindow::Close() {
    if (mWindowManagerApi != nullptr) {
        mWindowManagerApi->Close();
    }
}

void Fast3dWindow::RunGuiOnly() {
    if (mInterpreter != nullptr) {
        mInterpreter->RunGuiOnly();
    }
}

void Fast3dWindow::StartFrame() {
    if (mInterpreter != nullptr) {
        mInterpreter->StartFrame();
    }
}

void Fast3dWindow::EndFrame() {
    if (mInterpreter != nullptr) {
        mInterpreter->EndFrame();
    }
}

bool Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements,
                                              const std::unordered_map<Gfx*, Gfx*>& dlReplacements) {
    if (mInterpreter == nullptr || commands == nullptr) {
        return false;
    }
    // Render straight to the screen for now (the interpreter framebuffer path needs
    // GX EFB-as-texture support our backend still stubs).
    mInterpreter->mRendersToFb = false;
    mInterpreter->StartFrame();
    mInterpreter->Run(commands, mtxReplacements, dlReplacements);
    mInterpreter->EndFrame();
    return true;
}

bool Fast3dWindow::IsFrameReady() {
    return true;
}

void Fast3dWindow::HandleEvents() {
    if (mWindowManagerApi != nullptr) {
        mWindowManagerApi->HandleEvents();
    }
}

bool Fast3dWindow::IsRunning() {
    return mWindowManagerApi != nullptr && mWindowManagerApi->IsRunning();
}

uint32_t Fast3dWindow::GetWidth() {
    uint32_t w = 0, h = 0;
    int32_t px = 0, py = 0;
    if (mWindowManagerApi != nullptr) {
        mWindowManagerApi->GetDimensions(&w, &h, &px, &py);
    }
    return w;
}

uint32_t Fast3dWindow::GetHeight() {
    uint32_t w = 0, h = 0;
    int32_t px = 0, py = 0;
    if (mWindowManagerApi != nullptr) {
        mWindowManagerApi->GetDimensions(&w, &h, &px, &py);
    }
    return h;
}

float Fast3dWindow::GetAspectRatio() {
    uint32_t h = GetHeight();
    return h == 0 ? (4.0f / 3.0f) : (float)GetWidth() / (float)h;
}

uint32_t Fast3dWindow::GetCurrentRefreshRate() {
    uint32_t rr = 60;
    if (mWindowManagerApi != nullptr) {
        mWindowManagerApi->GetActiveWindowRefreshRate(&rr);
    }
    return rr;
}

bool Fast3dWindow::CanDisableVerticalSync() {
    return false; // VI is locked to the display refresh on console
}

uintptr_t Fast3dWindow::GetGfxFrameBuffer() {
    return 0; // direct-to-screen; no game framebuffer handle yet
}

int32_t Fast3dWindow::GetTargetFps() {
    return mTargetFps;
}

void Fast3dWindow::SetTargetFps(int32_t fps) {
    mTargetFps = fps;
}

void Fast3dWindow::SetMaximumFrameLatency(int32_t /*latency*/) {
}

void Fast3dWindow::SetRendererUCode(UcodeHandlers ucode) {
    gfx_set_target_ucode(ucode);
}

std::weak_ptr<Interpreter> Fast3dWindow::GetInterpreterWeak() const {
    return mInterpreter;
}

std::shared_ptr<GfxDebugger> Fast3dWindow::GetGfxDebugger() const {
    return mGfxDebugger;
}

} // namespace Fast
