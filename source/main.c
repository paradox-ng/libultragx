// libultragx - step 1: boot to GX and draw a spinning triangle.
//
// This is the canonical libogc GX bring-up: VI + GX init, a double-buffered
// external framebuffer (XFB), and a single TEV stage that passes per-vertex
// colors straight through. It exists only to prove the
// devkitPPC + libogc + Docker + Dolphin pipeline end-to-end before any Fast3D
// work lands in gfx_gx.c. Press START (GC controller) or HOME (Wii) to exit.
//
// Builds for both targets from one source: GameCube is the default (HW_DOL),
// Wii is the secondary build (HW_RVL). The GX/VI code is identical on both; only
// the Wii-remote input is Wii-only, guarded by HW_RVL (defined by wii_rules).

#include <gccore.h>
#include <ogcsys.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#define DEFAULT_FIFO_SIZE (256 * 1024)

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Mtx view;            // camera (world -> view)
    Mtx model, modelview;
    Mtx44 perspective;
    u32 fb = 0;
    f32 rot = 0.0f;

    GXColor background = { 0x00, 0x00, 0x00, 0xff };
    guVector cam   = { 0.0f, 0.0f, 0.0f };
    guVector up    = { 0.0f, 1.0f, 0.0f };
    guVector look  = { 0.0f, 0.0f, -1.0f };
    guVector yaxis = { 0.0f, 1.0f, 0.0f };

    // --- video init -------------------------------------------------------
    VIDEO_Init();
    PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif

    rmode = VIDEO_GetPreferredMode(NULL);

    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[fb]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    // --- GX (Flipper/Hollywood) init -------------------------------------
    void *gpfifo = memalign(32, DEFAULT_FIFO_SIZE);
    memset(gpfifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gpfifo, DEFAULT_FIFO_SIZE);

    GX_SetCopyClear(background, GX_MAX_Z24);

    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

    GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    GX_SetCullMode(GX_CULL_NONE);
    GX_CopyDisp(frameBuffer[fb], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    // --- vertex format: position + per-vertex color, both supplied inline --
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    // --- one TEV stage that just passes the rasterized vertex color through -
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);

    // --- camera / projection ---------------------------------------------
    guLookAt(view, &cam, &up, &look);
    guPerspective(perspective, 45.0f,
                  (f32)rmode->viWidth / (f32)rmode->viHeight, 0.1f, 300.0f);
    GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) break;
#ifdef HW_RVL
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
#endif

        GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);

        // spin about Y, pushed 5 units into the screen
        guMtxRotAxisDeg(model, &yaxis, rot);
        guMtxTransApply(model, model, 0.0f, 0.0f, -5.0f);
        guMtxConcat(view, model, modelview);
        GX_LoadPosMtxImm(modelview, GX_PNMTX0);

        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
            GX_Position3f32( 0.0f,  1.0f, 0.0f); GX_Color4u8(0xff, 0x00, 0x00, 0xff);
            GX_Position3f32(-1.0f, -1.0f, 0.0f); GX_Color4u8(0x00, 0xff, 0x00, 0xff);
            GX_Position3f32( 1.0f, -1.0f, 0.0f); GX_Color4u8(0x00, 0x00, 0xff, 0xff);
        GX_End();

        GX_DrawDone();

        fb ^= 1;
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);

        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();

        rot += 1.0f;
    }

    return 0;
}
