// libultragx gfx demo: the N64-combiner -> GX TEV mapping, several modes at once.
//
// Draws a 2x2 grid of quads, each sharing the same checkerboard texture and the
// same per-vertex color gradient, but rendered through a different N64 color
// combiner expressed as GX TEV (see source/gfx/gfx_gx_tev.cpp):
//
//   top-left  = G_CC_SHADE        (D = SHADE:  vertex color, no texture)
//   top-right = G_CC_DECALRGBA    (D = TEXEL0: texture replaces shade)
//   bot-left  = G_CC_MODULATERGBA (TEXEL0 * SHADE)
//   bot-right = G_CC_PRIMITIVE    (D = PRIM:   solid primitive color)
//
// Each is the real N64 combiner definition fed through the general decoder in
// source/gfx/gfx_gx_tev.cpp. Direct GX, no Fast3D yet. Press START / HOME to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#include "gfx/gfx_gx_tev.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define TEX_W 32
#define TEX_H 32

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;
static u16 *texData = NULL; // GX_TF_RGB565, tiled, 32-byte aligned

static void make_checker_texture(void) {
    static u16 linear[TEX_W * TEX_H];
    for (int y = 0; y < TEX_H; y++)
        for (int x = 0; x < TEX_W; x++) {
            bool on = (((x >> 2) + (y >> 2)) & 1) != 0;
            linear[y * TEX_W + x] = on ? 0xFFFF /* white */ : 0x001F /* blue */;
        }
    texData = (u16 *)memalign(32, TEX_W * TEX_H * sizeof(u16));
    int k = 0;
    for (int ty = 0; ty < TEX_H; ty += 4)
        for (int tx = 0; tx < TEX_W; tx += 4)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++)
                    texData[k++] = linear[(ty + y) * TEX_W + (tx + x)];
    DCFlushRange(texData, TEX_W * TEX_H * sizeof(u16));
}

static void draw_quad(float cx, float cy, float s) {
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(cx - s, cy + s, 0); GX_Color4u8(0xff, 0xff, 0xff, 0xff); GX_TexCoord2f32(0, 0);
        GX_Position3f32(cx + s, cy + s, 0); GX_Color4u8(0xff, 0x40, 0x40, 0xff); GX_TexCoord2f32(1, 0);
        GX_Position3f32(cx + s, cy - s, 0); GX_Color4u8(0x40, 0x40, 0xff, 0xff); GX_TexCoord2f32(1, 1);
        GX_Position3f32(cx - s, cy - s, 0); GX_Color4u8(0x40, 0xff, 0x40, 0xff); GX_TexCoord2f32(0, 1);
    GX_End();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Mtx view, modelview;
    Mtx44 perspective;
    u32 fb = 0;
    GXColor background = { 0x10, 0x10, 0x10, 0xff };
    GXColor primColor = { 0xff, 0x80, 0x00, 0xff }; // orange (-> TEV reg 0 = PRIM)
    GXColor envColor  = { 0x20, 0xff, 0x20, 0xff };  // green  (-> TEV reg 1 = ENV)

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

    // texture
    make_checker_texture();
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texData, TEX_W, TEX_H, GX_TF_RGB565, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjFilterMode(&texObj, GX_NEAR, GX_NEAR);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    // vertex format: pos + color + texcoord, supplied inline
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

    // Combiner register colors: primitive in TEV reg 0, environment in reg 1.
    GX_SetTevColor(GX_TEVREG0, primColor);
    GX_SetTevColor(GX_TEVREG1, envColor);

    guVector cam = { 0, 0, 0 }, up = { 0, 1, 0 }, look = { 0, 0, -1 };
    guLookAt(view, &cam, &up, &look);
    guPerspective(perspective, 45.0f, (f32)rmode->viWidth / (f32)rmode->viHeight, 0.1f, 300.0f);
    GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

    // Real SM64/Ghostship combiners, transcribed from the gbi.h G_CC_* defs.
    static const LugxCombiner CC_SHADE = {
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_SHADE,
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_SHADE_A };
    static const LugxCombiner CC_DECALRGBA = {
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_TEXEL0,
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_TEXEL0_A };
    static const LugxCombiner CC_MODULATERGBA = {
        LUGX_CC_TEXEL0, LUGX_CC_0, LUGX_CC_SHADE, LUGX_CC_0,
        LUGX_CC_TEXEL0_A, LUGX_CC_0, LUGX_CC_SHADE_A, LUGX_CC_0 };
    static const LugxCombiner CC_PRIMITIVE = {
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_PRIM,
        LUGX_CC_0, LUGX_CC_0, LUGX_CC_0, LUGX_CC_PRIM_A };

    const float C = 1.05f, S = 0.95f; // grid cell center offset, quad half-size
    struct { float cx, cy; const LugxCombiner* cc; } quads[4] = {
        { -C,  C, &CC_SHADE },        // top-left:     G_CC_SHADE
        {  C,  C, &CC_DECALRGBA },     // top-right:    G_CC_DECALRGBA
        { -C, -C, &CC_MODULATERGBA },  // bottom-left:  G_CC_MODULATERGBA
        {  C, -C, &CC_PRIMITIVE },     // bottom-right: G_CC_PRIMITIVE
    };

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

        for (int i = 0; i < 4; i++) {
            lugx_tev_from_combiner(quads[i].cc);
            draw_quad(quads[i].cx, quads[i].cy, S);
        }

        GX_DrawDone();
        fb ^= 1;
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

    return 0;
}
