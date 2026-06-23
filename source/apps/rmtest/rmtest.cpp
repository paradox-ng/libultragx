// libultragx ResourceManager test (GX/render side).
//
// The load path runs in rmtest_run.cpp (a SEPARATE TU: it uses gbi.h, which
// collides with libogc gx.h). This file renders the result as a clear color:
//   GREEN  = loaded a DisplayList by name through the ResourceManager + cache ok
//   RED    = archive open / SD mount failed
//   ORANGE = LoadResource miss / wrong type
//   BLUE   = decode or cache sanity failed
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

// Defined in rmtest_run.cpp: 0 pass, 1 archive/mount, 2 load miss, 3 decode/cache.
int lugx_rmtest_run(void);

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

    int result = lugx_rmtest_run();
    GXColor colors[4] = {
        { 0x20, 0xE0, 0x20, 0xff }, // 0 pass         -> green
        { 0xE0, 0x20, 0x20, 0xff }, // 1 archive/mount -> red
        { 0xF0, 0x80, 0x00, 0xff }, // 2 load miss    -> orange
        { 0x20, 0x40, 0xF0, 0xff }, // 3 decode/cache -> blue
    };
    GXColor bg = colors[result % 4];

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
