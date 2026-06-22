// libultragx gfx demo: N64 render state (alpha test vs alpha blend) on GX.
//
// A dark gradient backdrop fills the view; two textured quads are drawn in front
// of it through the MODULATE combiner, using a texture whose "off" checker squares
// are nearly transparent (alpha 32) and "on" squares opaque (alpha 255):
//
//   left  = ALPHA TEST  (alpha <= 128 discarded) -> hard holes, backdrop shows through
//   right = ALPHA BLEND (src-alpha transparency)  -> soft, backdrop shows faintly
//
// Exercises gfx_gx_tev (combiner), gfx_gx_tex (RGBA32->GX_TF_RGBA8), and
// gfx_gx_state (depth / alpha compare / blend). Direct GX, no Fast3D yet.
// Press START (GC) / HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "gfx/gfx_gx_tev.h"
#include "gfx/gfx_gx_tex.h"
#include "gfx/gfx_gx_state.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define TEX_W 32
#define TEX_H 32

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;
static u8 *texData = NULL; // GX_TF_RGBA8, tiled, 32-byte aligned

// Full-color texture with a checker-driven alpha: "on" squares opaque, "off"
// squares nearly transparent, so alpha test/blend have something to act on.
static void make_texture(void) {
    static u8 linear[TEX_W * TEX_H * 4];
    for (int y = 0; y < TEX_H; y++)
        for (int x = 0; x < TEX_W; x++) {
            u8 *p = &linear[(y * TEX_W + x) * 4];
            bool on = (((x >> 2) + (y >> 2)) & 1) != 0;
            p[0] = (u8)(x * 255 / (TEX_W - 1)); // R ramp
            p[1] = (u8)(y * 255 / (TEX_H - 1)); // G ramp
            p[2] = 0xC0;                        // B
            p[3] = on ? 0xFF : 0x20;            // A: opaque vs nearly transparent
        }
    texData = (u8 *)memalign(32, TEX_W * TEX_H * 4);
    lugx_tex_rgba32_to_gx_rgba8(linear, texData, TEX_W, TEX_H);
    DCFlushRange(texData, TEX_W * TEX_H * 4);
}

// A quad at (cx, cy) and depth z, half-size s, with corner colors + 0..1 texcoords.
static void draw_quad(float cx, float cy, float z, float s,
                      u8 r0, u8 g0, u8 b0, u8 r1, u8 g1, u8 b1) {
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(cx - s, cy + s, z); GX_Color4u8(r0, g0, b0, 0xff); GX_TexCoord2f32(0, 0);
        GX_Position3f32(cx + s, cy + s, z); GX_Color4u8(r1, g1, b1, 0xff); GX_TexCoord2f32(1, 0);
        GX_Position3f32(cx + s, cy - s, z); GX_Color4u8(r0, g0, b0, 0xff); GX_TexCoord2f32(1, 1);
        GX_Position3f32(cx - s, cy - s, z); GX_Color4u8(r1, g1, b1, 0xff); GX_TexCoord2f32(0, 1);
    GX_End();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Mtx view, modelview;
    Mtx44 perspective;
    u32 fb = 0;
    GXColor background = { 0x08, 0x08, 0x08, 0xff };

    static const LugxCombiner CC_SHADE = {
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_SHADE,
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_SHADE_A };
    static const LugxCombiner CC_MODULATERGBA = {
        LUGX_CC_TEXEL0, LUGX_CC_0, LUGX_CC_SHADE, LUGX_CC_0,
        LUGX_CC_TEXEL0_A, LUGX_CC_0, LUGX_CC_SHADE_A, LUGX_CC_0 };

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

    make_texture();
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texData, TEX_W, TEX_H, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjFilterMode(&texObj, GX_NEAR, GX_NEAR);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32,   0);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    guVector cam = { 0, 0, 0 }, up = { 0, 1, 0 }, look = { 0, 0, -1 };
    guLookAt(view, &cam, &up, &look);
    guPerspective(perspective, 45.0f, (f32)rmode->viWidth / (f32)rmode->viHeight, 0.1f, 300.0f);
    GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) break;
#ifdef HW_RVL
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) break;
#endif

        GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);

        guMtxIdentity(modelview);
        guMtxTransApply(modelview, modelview, 0.0f, 0.0f, -5.5f);
        guMtxConcat(view, modelview, modelview);
        GX_LoadPosMtxImm(modelview, GX_PNMTX0);

        // 1) Opaque backdrop (dark gradient), behind everything (local z = -1).
        lugx_tev_from_combiner(&CC_SHADE);
        lugx_set_depth(true, true);
        lugx_set_alpha_test(false, 0);
        lugx_set_blend(false);
        draw_quad(0.0f, 0.0f, -1.0f, 2.4f, 0x40, 0x10, 0x10, 0x10, 0x10, 0x40);

        // 2) Left: alpha-tested textured quad (hard cutout holes).
        lugx_tev_from_combiner(&CC_MODULATERGBA);
        lugx_set_depth(true, true);
        lugx_set_alpha_test(true, 128);
        lugx_set_blend(false);
        draw_quad(-1.05f, 0.0f, 0.0f, 0.9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

        // 3) Right: alpha-blended textured quad (soft transparency).
        lugx_tev_from_combiner(&CC_MODULATERGBA);
        lugx_set_depth(true, false);
        lugx_set_alpha_test(false, 0);
        lugx_set_blend(true);
        draw_quad(1.05f, 0.0f, 0.0f, 0.9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

        GX_DrawDone();
        fb ^= 1;
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

    return 0;
}
