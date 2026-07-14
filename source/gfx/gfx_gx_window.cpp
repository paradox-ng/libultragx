#include "fast/backends/gfx_gx_window.h"

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h> // gettime / ticks_to_microsecs
#include <malloc.h>
#include <string.h>
#include <cstdio>
#include <unistd.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

// TEMP boot bring-up trace hooks (defined in the game's Game.cpp).
extern "C" void bootlog(const char*);
extern "C" void bootflush(void);

namespace Fast {

#define LUGX_FIFO_SIZE (256 * 1024)

// VI field counter, bumped by the post-retrace interrupt. libogc has no
// GetRetraceCount, so track it here to pace presents to a target framerate
// (present every Nth field) in SwapBuffersEnd. The VI ISR passes the running
// count; storing it is safe alongside VIDEO_WaitVSync (which uses its own signal).
static volatile uint32_t s_viRetraceCount = 0;
static void lugx_on_retrace(u32 cnt) {
    s_viRetraceCount = cnt;
}

void GfxWindowBackendGX::Init(const char* /*gameName*/, const char* /*apiName*/, bool /*startFullScreen*/,
                              uint32_t width, uint32_t height, int32_t /*posX*/, int32_t /*posY*/) {
    if (mInitialized) {
        return;
    }

    VIDEO_Init();
    VIDEO_SetPostRetraceCallback(lugx_on_retrace);
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif

    GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
    mRmode = rmode;
    mWidth = width != 0 ? width : (uint32_t)rmode->fbWidth;
    mHeight = height != 0 ? height : (uint32_t)rmode->efbHeight;

    mFrameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    mFrameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(mFrameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    mFifo = memalign(32, LUGX_FIFO_SIZE);
    memset(mFifo, 0, LUGX_FIFO_SIZE);
    GX_Init(mFifo, LUGX_FIFO_SIZE);

    // EFB clear colour. The N64 framebuffer is cleared to black where the game
    // draws no fill or geometry (e.g. the surround on the title screen), so clear
    // to black rather than a placeholder; gameplay frames overdraw it entirely.
    GXColor clearColor = { 0x00, 0x00, 0x00, 0xFF };
    GX_SetCopyClear(clearColor, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight)));
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    mStartTicks = gettime();
    mInitialized = true;
}

void GfxWindowBackendGX::Close() {
}

void GfxWindowBackendGX::SetKeyboardCallbacks(bool (*)(int), bool (*)(int), void (*)()) {
}
void GfxWindowBackendGX::SetMouseCallbacks(bool (*)(int), bool (*)(int)) {
}
void GfxWindowBackendGX::SetFullscreenChangedCallback(void (*)(bool)) {
}
void GfxWindowBackendGX::SetFullscreen(bool) {
}
void GfxWindowBackendGX::GetActiveWindowRefreshRate(uint32_t* refreshRate) {
    if (refreshRate) {
        *refreshRate = 60;
    }
}
void GfxWindowBackendGX::SetCursorVisibility(bool) {
}
void GfxWindowBackendGX::SetMousePos(int32_t, int32_t) {
}
void GfxWindowBackendGX::GetMousePos(int32_t* x, int32_t* y) {
    if (x) *x = 0;
    if (y) *y = 0;
}
void GfxWindowBackendGX::GetMouseDelta(int32_t* x, int32_t* y) {
    if (x) *x = 0;
    if (y) *y = 0;
}
void GfxWindowBackendGX::GetMouseWheel(float* x, float* y) {
    if (x) *x = 0;
    if (y) *y = 0;
}
bool GfxWindowBackendGX::GetMouseState(uint32_t) {
    return false;
}
void GfxWindowBackendGX::SetMouseCapture(bool) {
}
bool GfxWindowBackendGX::IsMouseCaptured() {
    return false;
}

void GfxWindowBackendGX::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    if (width) *width = mWidth;
    if (height) *height = mHeight;
    if (posX) *posX = 0;
    if (posY) *posY = 0;
}
void GfxWindowBackendGX::SetDimensions(uint32_t, uint32_t, int32_t, int32_t) {
}
Ship::WindowRect GfxWindowBackendGX::GetPrimaryMonitorRect() {
    return { 0, 0, (int32_t)mWidth, (int32_t)mHeight };
}

void GfxWindowBackendGX::HandleEvents() {
    PAD_ScanPads();
#ifdef HW_RVL
    WPAD_ScanPads();
#endif
}

bool GfxWindowBackendGX::IsFrameReady() {
    return true;
}

void GfxWindowBackendGX::SwapBuffersBegin() {
}

void GfxWindowBackendGX::SwapBuffersEnd() {
    if (!mInitialized) {
        return;
    }
    GX_CopyDisp(mFrameBuffer[mFbIndex], GX_TRUE);
    GX_DrawDone();
    VIDEO_SetNextFramebuffer(mFrameBuffer[mFbIndex]);
    VIDEO_Flush();
    // Pace the present to the target framerate instead of presenting every field.
    // SM64 is a 30fps game, so mTargetFps is 30 (from GetInterpolationFPS) and we
    // present on every other video field - a steady 30 rather than the 30<->60
    // double-buffer oscillation (a frame that just misses the 60Hz deadline would
    // otherwise slip to the next field and read as judder). Gate on the retrace
    // counter so a frame that already overran its field budget presents at once
    // instead of being padded further. With interpolation on (mTargetFps=60) this
    // is one field per present = 60.
    uint32_t fields = (mTargetFps > 0 && mTargetFps < 60u) ? (60u / mTargetFps) : 1u;
    while (s_viRetraceCount < mLastPresentRetrace + fields) {
        VIDEO_WaitVSync();
    }
    mLastPresentRetrace = s_viRetraceCount;
    mFbIndex ^= 1;
}

double GfxWindowBackendGX::GetTime() {
    if (!mInitialized) {
        return 0.0;
    }
    return (double)ticks_to_microsecs(gettime() - mStartTicks) / 1000000.0;
}

int GfxWindowBackendGX::GetTargetFps() {
    return (int)mTargetFps;
}
void GfxWindowBackendGX::SetTargetFps(int fps) {
    mTargetFps = (uint32_t)fps;
}
void GfxWindowBackendGX::SetMaxFrameLatency(int) {
}
const char* GfxWindowBackendGX::GetKeyName(int) {
    return "";
}
bool GfxWindowBackendGX::CanDisableVsync() {
    return false; // VI is vsync-locked on console
}
bool GfxWindowBackendGX::IsRunning() {
    return mIsRunning;
}
void GfxWindowBackendGX::Destroy() {
}
bool GfxWindowBackendGX::IsFullscreen() {
    return true; // always fullscreen on console
}

GfxWindowBackend* lugx_create_gx_window_backend() {
    return new GfxWindowBackendGX();
}

} // namespace Fast
