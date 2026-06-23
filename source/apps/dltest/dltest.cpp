// libultragx DisplayList decode test (GX/render side).
//
// The decode itself lives in dltest_decode.cpp, in a SEPARATE translation unit:
// the N64 gbi.h defines `Vtx` (vertex) and libogc's gx.h also defines `Vtx`, so a
// TU cannot include both. That mirrors the real architecture - gfx_gx uses GX with
// no gbi, the interpreter uses gbi via the abstract GfxRenderingAPI with no GX.
//
// This file (GX only, no gbi) renders the result as a clear color:
//   GREEN  = type 'ODLT', decoded, terminated     RED = open/read fail
//   ORANGE = wrong type or no clean termination
// Press START (GC) / HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "platform/sd.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)

// Defined in dltest_decode.cpp (the gbi-using TU): 0 pass, 1 open/read, 2 decode.
int lugx_dltest_run(void);

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    VIDEO_Init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif
    rmode = VIDEO_GetPreferredMode(NULL);
    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    int result = lugx_dltest_run();
    GXColor colors[3] = {
        { 0x20, 0xE0, 0x20, 0xff }, // 0 pass   -> green
        { 0xE0, 0x20, 0x20, 0xff }, // 1 open   -> red
        { 0xF0, 0x80, 0x00, 0xff }, // 2 decode -> orange
    };
    GXColor bg = colors[result % 3];

    void *gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);
    GX_SetCopyClear(bg, GX_MAX_Z24);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight)));
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    u32 fb = 0;
    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) break;
#ifdef HW_RVL
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
#endif
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        GX_DrawDone();
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb ^= 1;
    }

    lugx_sd_unmount();
    return 0;
}
