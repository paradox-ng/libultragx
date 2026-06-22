// libultragx gfx demo: prove one N64 combiner mode maps onto GX TEV.
//
// Renders a spinning triangle textured with a procedural checkerboard, with a
// single TEV stage configured as MODULATE (texel * rasterized vertex color).
// That stage is exactly the N64 G_CC_MODULATERGBA combiner (texel * shade), so
// this is the first concrete proof that N64 Fast3D combiners can be expressed as
// GX fixed-function TEV stages. Everything visible here is direct GX (no Fast3D
// yet) to keep the proof isolated. Press START (GC) / HOME (Wii) to exit.

#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

#define DEFAULT_FIFO_SIZE (256 * 1024)
#define TEX_W 32
#define TEX_H 32

static void *frameBuffer[2] = { NULL, NULL };
static GXRModeObj *rmode = NULL;
static u16 *texData = NULL; // GX_TF_RGB565, tiled, 32-byte aligned

// Build a checkerboard in linear RGB565, then swizzle it into GX_TF_RGB565 tile
// order (4x4 texels per tile, row-major within and across tiles). This swizzle
// is the same job gfx_gx's UploadTexture will do for N64 textures.
static void make_checker_texture(void) {
    static u16 linear[TEX_W * TEX_H];
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            bool on = (((x >> 2) + (y >> 2)) & 1) != 0;
            linear[y * TEX_W + x] = on ? 0xFFFF /* white */ : 0x001F /* blue */;
        }
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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Mtx view, model, modelview;
    Mtx44 perspective;
    u32 fb = 0;
    f32 rot = 0.0f;
    GXColor background = { 0x10, 0x10, 0x10, 0xff };
    guVector cam = { 0, 0, 0 }, up = { 0, 1, 0 }, look = { 0, 0, -1 }, yaxis = { 0, 1, 0 };

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

    // --- texture upload ---
    make_checker_texture();
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texData, TEX_W, TEX_H, GX_TF_RGB565, GX_REPEAT, GX_REPEAT, GX_FALSE);
    GX_InitTexObjFilterMode(&texObj, GX_NEAR, GX_NEAR); // point sampling, like the N64 default
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    // --- vertex format: position + color + texcoord, all supplied inline ---
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

    // ONE TEV stage = MODULATE: output = texel * rasterized vertex color.
    // This IS the N64 G_CC_MODULATERGBA combiner (texel * shade).
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

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

        guMtxRotAxisDeg(model, &yaxis, rot);
        guMtxTransApply(model, model, 0.0f, 0.0f, -3.0f);
        guMtxConcat(view, model, modelview);
        GX_LoadPosMtxImm(modelview, GX_PNMTX0);

        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
            GX_Position3f32( 0.0f,  1.0f, 0.0f); GX_Color4u8(0xff, 0xff, 0xff, 0xff); GX_TexCoord2f32(0.5f, 0.0f);
            GX_Position3f32(-1.0f, -1.0f, 0.0f); GX_Color4u8(0xff, 0x60, 0x60, 0xff); GX_TexCoord2f32(0.0f, 1.0f);
            GX_Position3f32( 1.0f, -1.0f, 0.0f); GX_Color4u8(0x60, 0x60, 0xff, 0xff); GX_TexCoord2f32(1.0f, 1.0f);
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
