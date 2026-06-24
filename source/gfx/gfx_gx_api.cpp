#include "fast/backends/gfx_gx.h"
#include "fast/backends/gfx_gx_camera.h"
#include "fast/cc_features.h"

#include "gfx/gfx_gx_tex.h"
#include "gfx/gfx_gx_state.h"

#include <gccore.h>
#include <ogc/gu.h>
#include <malloc.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <unistd.h>

namespace Fast {

// The GX modelview (affine camera/view) DrawTriangles loads into GX_PNMTX0.
// Identity by default; a caller (e.g. the standalone DL test, which has no game
// to set up the camera) provides the view here so the camera-back translation is
// applied as the modelview rather than folded into the projection. See
// gfx_gx_camera.h for why that matters.
static float sGxViewMtx[4][4] = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

void lugx_gx_set_view_matrix(const float m[4][4]) {
    memcpy(sGxViewMtx, m, sizeof(sGxViewMtx));
}

// What a "shader" is for the GX backend: the N64 combiner decoded from the 64-bit
// shader ids (which encode the RDP combine state), kept as the full CCFeatures so
// DrawTriangles knows the vertex layout and how to drive the TEV stages.
struct ShaderProgram {
    uint64_t id0 = 0;
    uint64_t id1 = 0;
    CCFeatures features{};
    LugxCombiner combiner;
    uint8_t numInputs = 1;
    bool usedTextures[2] = { true, false };
};

const char* GfxRenderingAPIGX::GetName() {
    return "GX";
}

int GfxRenderingAPIGX::GetMaxTextureSize() {
    return 1024; // GX maximum texture dimension
}

GfxClipParameters GfxRenderingAPIGX::GetClipParameters() {
    // GX viewport maps NDC z to [0,1]. invertY is a placeholder until real
    // geometry confirms the framebuffer-y orientation the interpreter expects.
    return { true, false };
}

// --- shaders -------------------------------------------------------------------

// TODO: decode shaderId0/shaderId1 (LUS's packed RDP combine state) into the real
// LugxCombiner. For now every shader is a MODULATE (texel * shade) combiner so the
// backend is exercisable end to end before the id decoder lands.
static LugxCombiner default_combiner() {
    return LugxCombiner{
        LUGX_CC_TEXEL0, LUGX_CC_0, LUGX_CC_SHADE, LUGX_CC_0,
        LUGX_CC_TEXEL0_A, LUGX_CC_0, LUGX_CC_SHADE_A, LUGX_CC_0,
    };
}

ShaderProgram* GfxRenderingAPIGX::CreateAndLoadNewShader(uint64_t shaderId0, uint64_t shaderId1) {
    ShaderProgram* p = new ShaderProgram();
    p->id0 = shaderId0;
    p->id1 = shaderId1;
    // Decode the real combiner features from the packed RDP combine ids (the same
    // decoder the interpreter uses, called across the link - see cc_features.h).
    gfx_cc_get_features(shaderId0, shaderId1, &p->features);
    p->numInputs = (uint8_t)p->features.numInputs;
    p->usedTextures[0] = p->features.usedTextures[0];
    p->usedTextures[1] = p->features.usedTextures[1];
    // TODO(piece 2): build the TEV combiner from p->features.c[][][] instead of the
    // MODULATE placeholder, loading CombinerUniforms::inputs into GX const regs.
    p->combiner = default_combiner();
    mShaderCache[shaderId0] = p;
    mCurrentShader = p;
    lugx_tev_from_combiner(&p->combiner);
    return p;
}

ShaderProgram* GfxRenderingAPIGX::LookupShader(uint64_t shaderId0, uint64_t shaderId1) {
    auto it = mShaderCache.find(shaderId0);
    return (it != mShaderCache.end() && it->second->id1 == shaderId1) ? it->second : nullptr;
}

void GfxRenderingAPIGX::LoadShader(ShaderProgram* newPrg) {
    mCurrentShader = newPrg;
    if (newPrg != nullptr) {
        lugx_tev_from_combiner(&newPrg->combiner);
    }
}

void GfxRenderingAPIGX::UnloadShader(ShaderProgram* oldPrg) {
    if (mCurrentShader == oldPrg) {
        mCurrentShader = nullptr;
    }
}

void GfxRenderingAPIGX::ClearShaderCache() {
    for (auto& kv : mShaderCache) {
        delete kv.second;
    }
    mShaderCache.clear();
    mCurrentShader = nullptr;
}

void GfxRenderingAPIGX::ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    if (prg == nullptr) {
        return;
    }
    *numInputs = prg->numInputs;
    usedTextures[0] = prg->usedTextures[0];
    usedTextures[1] = prg->usedTextures[1];
}

// --- textures ------------------------------------------------------------------

uint32_t GfxRenderingAPIGX::NewTexture() {
    mTextures.emplace_back();
    return (uint32_t)(mTextures.size() - 1);
}

void GfxRenderingAPIGX::SelectTexture(int tile, uint32_t textureId) {
    mCurrentTile = tile;
    if (tile >= 0 && tile < 2) {
        mTileTexture[tile] = textureId;
    }
}

void GfxRenderingAPIGX::UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) {
    if (mCurrentTile < 0 || mCurrentTile >= 2) {
        return;
    }
    uint32_t id = mTileTexture[mCurrentTile];
    if (id >= mTextures.size()) {
        return;
    }
    GxTexture& t = mTextures[id];
    // TODO: handle width/height not divisible by 4 (pad to the GX 4x4 tile size).
    size_t sz = (size_t)width * height * 4;
    if (t.data != nullptr) {
        free(t.data);
    }
    t.data = memalign(32, sz);
    lugx_tex_rgba32_to_gx_rgba8(rgba32Buf, (u8*)t.data, width, height);
    DCFlushRange(t.data, sz);
    t.width = width;
    t.height = height;
}

void GfxRenderingAPIGX::SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (sampler < 0 || sampler >= 2) {
        return;
    }
    uint32_t id = mTileTexture[sampler];
    if (id >= mTextures.size()) {
        return;
    }
    GxTexture& t = mTextures[id];
    t.linearFilter = linear_filter;
    // N64 G_TX_ clamp/mirror bits -> GX wrap modes (G_TX_MIRROR=1, G_TX_CLAMP=2).
    auto wrap = [](uint32_t m) -> u8 {
        if (m & 0x2) return GX_CLAMP;
        if (m & 0x1) return GX_MIRROR;
        return GX_REPEAT;
    };
    t.wrapS = wrap(cms);
    t.wrapT = wrap(cmt);
}

void GfxRenderingAPIGX::DeleteTexture(uint32_t texId) {
    if (texId < mTextures.size() && mTextures[texId].data != nullptr) {
        free(mTextures[texId].data);
        mTextures[texId].data = nullptr;
    }
}

void GfxRenderingAPIGX::SetTextureFilter(FilteringMode mode) {
    mFilterMode = mode;
}

FilteringMode GfxRenderingAPIGX::GetTextureFilter() {
    return mFilterMode;
}

// --- render state --------------------------------------------------------------

void GfxRenderingAPIGX::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    lugx_set_depth(depth_test, z_upd);
}

void GfxRenderingAPIGX::SetZmodeDecal(bool decal) {
    // TODO: proper coplanar decal offset. Approximate with EQUAL compare + no write.
    GX_SetZMode(GX_TRUE, decal ? GX_LEQUAL : GX_LEQUAL, decal ? GX_FALSE : GX_TRUE);
}

void GfxRenderingAPIGX::SetStrictDecal(bool on) {
    (void)on;
}

void GfxRenderingAPIGX::SetViewport(int x, int y, int width, int height) {
    // TODO: confirm y orientation (GX framebuffer origin is top-left).
    GX_SetViewport((f32)x, (f32)y, (f32)width, (f32)height, 0.0f, 1.0f);
}

void GfxRenderingAPIGX::SetScissor(int x, int y, int width, int height) {
    GX_SetScissor((u32)x, (u32)y, (u32)width, (u32)height);
}

void GfxRenderingAPIGX::SetUseAlpha(bool useAlpha) {
    lugx_set_blend(useAlpha);
}

void GfxRenderingAPIGX::SetCurrentPrimDepth(float depth) {
    (void)depth; // TODO: primitive depth for decal/z-source modes
}

void GfxRenderingAPIGX::SetSrgbMode() {
    // GX has no sRGB framebuffer mode; nothing to do.
}

void GfxRenderingAPIGX::SetCombinerUniforms(const CombinerUniforms& uniforms) {
    mCombinerUniforms = uniforms;
}

void GfxRenderingAPIGX::SetTransformUniforms(const TransformUniforms& uniforms) {
    mTransform = uniforms;
}

void GfxRenderingAPIGX::SetLightingUniforms(const LightingUniforms& uniforms) {
    mLighting = uniforms;
}

// --- draw ----------------------------------------------------------------------

static inline u8 float_to_u8(float f) {
    int v = (int)(f * 255.0f + 0.5f);
    return (u8)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// BISECT: when set, skip all GX drawing (no FIFO writes) to test whether the
// frame-1 render hang lives in the triangle/GP submission path. Defined in Game.cpp.
extern "C" int g_gx_skip_draw;
extern "C" int g_gx_skip_geom; // BISECT: skip only GX_Begin/vertices/GX_End (keep state-setting)
extern "C" int g_gx_stop_at;   // BISECT: stop after stage N (1=TEV 2=vtxdesc 3=mtx 4=chan 5=full)
extern "C" void bootlog(const char*);
extern "C" void bootflush(void);

volatile unsigned g_dt_total = 0;     // total triangle batches submitted (gdb-readable)
volatile unsigned g_dt_lastverts = 0; // verts of the most recent batch

void GfxRenderingAPIGX::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    (void)buf_vbo_len;
    if (g_gx_skip_draw || mCurrentShader == nullptr || buf_vbo_num_tris == 0) {
        return;
    }
    g_dt_total++;
    g_dt_lastverts = (unsigned)(buf_vbo_num_tris * 3);
    // BISECT: log every DrawTriangles call's params; the last "dt#N" before the SD
    // budget/hang is the batch that stalls the GP. Sub-step trace off (dz=false).
    static int dtc = 0;
    bool dz = false; // frame-1 draw confirmed to complete; trace moved downstream
    char dzb[48];
#define DZ(tag) do { if (dz) { snprintf(dzb, sizeof(dzb), "dt %s", tag); bootlog(dzb); bootflush(); } } while (0)
    const CCFeatures& cc = mCurrentShader->features;

    // Drive the TEV stage(s) from the decoded combiner + resolved constant colours.
    lugx_tev_from_features(&cc, mCombinerUniforms.inputs);
    DZ("post-tev");
    if (g_gx_stop_at == 1 || g_gx_stop_at == 10) { dtc++; return; }

    // Per-vertex float layout (mirrors Interpreter::GfxSpTri1):
    //   x,y,z,w, mtx_slot, [u,v per used tile], shade(3 rgb | 3 normal)[+1 a]
    // Derive the stride from the actual buffer length (the interpreter's exact
    // packing) rather than predicting it - opt_alpha etc. can shift it.
    const int numTex = (cc.usedTextures[0] ? 1 : 0) + (cc.usedTextures[1] ? 1 : 0);
    const bool lighting = cc.opt_lighting;
    if (false) { // draw counter off; presents counted in SwapBuffersEnd
        snprintf(dzb, sizeof(dzb), "dt#%d v=%u", dtc, (unsigned)(buf_vbo_num_tris * 3));
        bootlog(dzb);
        bootflush();
    }
    const int stride = (int)(buf_vbo_len / (buf_vbo_num_tris * 3));
    const int texOff = 5;
    const int shadeOff = 5 + 2 * numTex;
    const int shadeFloats = stride - shadeOff;
    const bool hasShade = shadeFloats >= 3;
    const bool useAlpha = shadeFloats >= 4;
    // When the combiner uses shade and lighting is on, the vbo's shade slot holds the
    // object-space NORMAL (per-vertex) and GX computes the lit colour in hardware;
    // otherwise the shade slot is a vertex colour we submit directly.
    const bool submitNormal = hasShade && lighting;
    const bool submitColor = hasShade && !lighting;

    // Vertex descriptor / format. Submission order is fixed: POS, NRM, CLR0, TEX0.
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    if (submitNormal) {
        GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    }
    if (submitColor) {
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    }
    if (numTex > 0) {
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumTexGens(1);
        GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        // Bind the texture currently selected on tile 0 to GX_TEXMAP0.
        uint32_t tid = mTileTexture[0];
        if (tid < mTextures.size() && mTextures[tid].data != nullptr) {
            GxTexture& t = mTextures[tid];
            GXTexObj obj;
            GX_InitTexObj(&obj, t.data, (u16)t.width, (u16)t.height, GX_TF_RGBA8, t.wrapS, t.wrapT, GX_FALSE);
            u8 filt = t.linearFilter ? GX_LINEAR : GX_NEAR;
            GX_InitTexObjFilterMode(&obj, filt, filt);
            GX_LoadTexObj(&obj, GX_TEXMAP0);
        }
    } else {
        GX_SetNumTexGens(0);
    }
    DZ("post-tex");
    if (g_gx_stop_at == 2) { dtc++; return; }

    // GX T&L matrix path (piece 5): the vbo carries object-space positions and a
    // matrix-palette slot. The interpreter's mtx_palette[slot] is the combined MVP
    // (perspective baked in). GX has no per-vertex projection, so for now we load
    // the slot's MVP as the single GX projection (perspective-correct for batches
    // that use one matrix) with identity model-view; the per-vertex PNMTXIDX +
    // model-view/projection split is the follow-up for multi-matrix batches.
    // TODO: the interpreter's palette convention is GX's transpose; confirm the
    // exact transpose against real interpreter geometry (loaded direct for now).
    int slot0 = (int)buf_vbo[4]; // mtx_slot of the first vertex
    if (slot0 < 0 || slot0 >= GFX_MTX_PALETTE_SIZE) {
        slot0 = 0;
    }
    Mtx mv;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            mv[r][c] = sGxViewMtx[r][c];
        }
    }
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    Mtx44 proj;
    // The interpreter captures MV*P per slot (N64 transform clip = obj_row * M),
    // so GX needs the transpose.
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            proj[r][c] = mTransform.mtx_palette[slot0][c][r];
        }
    }
    // Clip-z remap: the interpreter/games supply GL-convention matrices (NDC z in
    // [-1,1]); GX's NDC z is [-1,0]. Map z' = 0.5*z - 0.5*w (this turns a GL
    // perspective into exactly libogc guPerspective's GX matrix).
    for (int c = 0; c < 4; c++) {
        proj[2][c] = 0.5f * proj[2][c] - 0.5f * proj[3][c];
    }
    GX_LoadProjectionMtx(proj, GX_PERSPECTIVE);
    DZ("post-mtx");
    if (g_gx_stop_at == 3) { dtc++; return; }

    // Lighting channel. N64 lit shade = matWhite * (ambient + sum_i lightCol_i *
    // max(0, N . dirToLight_i)). GX gives exactly that with GX_DF_CLAMP + GX_AF_NONE,
    // material/ambient from the registers. The interpreter already transformed each
    // light direction into the normal's space (CalculateNormalDir via the modelview).
    if (submitNormal) {
        // Normal matrix = inverse-transpose of the modelview 3x3; our modelview is a
        // pure translation (the view), so its 3x3 is identity -> normal matrix identity.
        Mtx nrmMtx;
        guMtxIdentity(nrmMtx);
        GX_LoadNrmMtxImm(nrmMtx, GX_PNMTX0);

        int nl = mLighting.num_lights;
        if (nl > 8) {
            nl = 8; // GX has 8 hardware lights
        }
        u32 lightmask = 0;
        for (int i = 0; i < nl; i++) {
            GXLightObj lobj;
            GXColor lc = { float_to_u8(mLighting.lights[i][0][0]), float_to_u8(mLighting.lights[i][0][1]),
                           float_to_u8(mLighting.lights[i][0][2]), 255 };
            GX_InitLightColor(&lobj, lc);
            // Place the light far along the direction-to-light so GX's light vector
            // L = normalize(pos - vtx) ~= that direction; diffuse = clamp(N . L).
            const float kFar = 1.0e6f;
            GX_InitLightPos(&lobj, mLighting.lights[i][1][0] * kFar, mLighting.lights[i][1][1] * kFar,
                            mLighting.lights[i][1][2] * kFar);
            GX_LoadLightObj(&lobj, GX_LIGHT0 << i);
            lightmask |= (u32)(GX_LIGHT0 << i);
        }
        GXColor amb = { float_to_u8(mLighting.ambient[0]), float_to_u8(mLighting.ambient[1]),
                        float_to_u8(mLighting.ambient[2]), 255 };
        GXColor matWhite = { 255, 255, 255, 255 };
        GX_SetNumChans(1);
        GX_SetChanAmbColor(GX_COLOR0A0, amb);
        GX_SetChanMatColor(GX_COLOR0A0, matWhite);
        GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, lightmask, GX_DF_CLAMP, GX_AF_NONE);
    } else {
        // Pass the vertex colour (or the constant material) straight to the rasteriser.
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    }

    DZ("post-light");
    if (g_gx_stop_at == 4) { dtc++; return; }
    const size_t verts = buf_vbo_num_tris * 3;
    if (dz) {
        // Scan ALL vertex coords for NaN/Inf/huge (GP rasteriser stalls on those).
        int bad = 0;
        for (size_t i = 0; i < verts; i++) {
            const float* vv = &buf_vbo[i * (size_t)stride];
            for (int k = 0; k < 3; k++) {
                unsigned b = *(unsigned*)&vv[k];
                unsigned e = (b >> 23) & 0xFFu;
                if (e == 0xFFu) { bad++; } // NaN/Inf
            }
        }
        snprintf(dzb, sizeof(dzb), "dt verts=%u stride=%d sN=%d sC=%d bad=%d", (unsigned)verts, stride,
                 (int)submitNormal, (int)submitColor, bad);
        bootlog(dzb);
        bootflush();
    }
    if (g_gx_skip_geom) { dtc++; return; } // BISECT: state set, but no geometry submitted
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)verts);
    if (dz) { bootlog("dt after-Begin"); bootflush(); }
    for (size_t i = 0; i < verts; i++) {
        const float* v = &buf_vbo[i * (size_t)stride];
        GX_Position3f32(v[0], v[1], v[2]);
        if (submitNormal) {
            GX_Normal3f32(v[shadeOff + 0], v[shadeOff + 1], v[shadeOff + 2]);
        }
        if (submitColor) {
            u8 a = useAlpha ? float_to_u8(v[shadeOff + 3]) : 255;
            GX_Color4u8(float_to_u8(v[shadeOff + 0]), float_to_u8(v[shadeOff + 1]),
                        float_to_u8(v[shadeOff + 2]), a);
        }
        if (numTex > 0) {
            // Apply the per-draw UV transform (raw RSP texcoord -> normalized GX
            // texcoord): uv = raw * scale + offset (CombinerUniforms.uv_transform).
            const float* uvt = mCombinerUniforms.uv_transform[0];
            float us = v[texOff + 0] * uvt[0] + uvt[1];
            float vt = v[texOff + 1] * uvt[2] + uvt[3];
            GX_TexCoord2f32(us, vt);
        }
    }
    if (dz) { bootlog("dt after-verts"); bootflush(); }
    GX_End();
    DZ("post-end");
    dtc++;
#undef DZ
}

void GfxRenderingAPIGX::DrawBringupTriangle() {
    // Texture bring-up: a DECAL combiner (output = TEXEL0) over a 16x16 RGBA32
    // checkerboard, to validate the GX texture upload + bind + texcoord path.
    static uint8_t checker[16 * 16 * 4];
    static uint32_t texId = 0xFFFFFFFF;
    if (texId == 0xFFFFFFFF) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                bool dark = (((x / 4) + (y / 4)) & 1) != 0;
                uint8_t* p = &checker[(y * 16 + x) * 4];
                p[0] = 255;             // R
                p[1] = dark ? 0 : 255;  // G  -> red / white
                p[2] = dark ? 0 : 255;  // B
                p[3] = 255;             // A
            }
        }
        texId = NewTexture();
        SelectTexture(0, texId);
        UploadTexture(checker, 16, 16);
        SetSamplerParameters(0, false, 2 /*clamp*/, 2 /*clamp*/);
    }
    SelectTexture(0, texId);

    // output = TEXEL0 ((A-B)*C+D with A=B=C=0, D=TEXEL0).
    CCFeatures cc{};
    cc.usedTextures[0] = true;
    cc.c[0][0][0] = SHADER_0; cc.c[0][0][1] = SHADER_0; cc.c[0][0][2] = SHADER_0; cc.c[0][0][3] = SHADER_TEXEL0;
    cc.c[0][1][0] = SHADER_0; cc.c[0][1][1] = SHADER_0; cc.c[0][1][2] = SHADER_0; cc.c[0][1][3] = SHADER_1;
    static ShaderProgram testShader;
    testShader.features = cc;
    mCurrentShader = &testShader;

    // Identity UV transform: the vbo texcoords below are already normalized [0,1].
    CombinerUniforms cu{};
    cu.uv_transform[0][0] = 1.0f; // scaleS
    cu.uv_transform[0][2] = 1.0f; // scaleT
    SetCombinerUniforms(cu);

    // GL-convention perspective (the backend remaps z to GX), stored transposed.
    const float fovy = 60.0f * 3.14159265f / 180.0f;
    const float cot = 1.0f / tanf(fovy * 0.5f);
    const float asp = 4.0f / 3.0f, n = 0.1f, f = 50.0f;
    float glp[4][4] = {};
    glp[0][0] = cot / asp;
    glp[1][1] = cot;
    glp[2][2] = -(f + n) / (f - n);
    glp[2][3] = -2.0f * f * n / (f - n);
    glp[3][2] = -1.0f;
    TransformUniforms t{};
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            t.mtx_palette[0][r][c] = glp[c][r]; // transposed; DrawTriangles transposes back
        }
    }
    t.y_scale[0] = 1.0f;
    SetTransformUniforms(t);

    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

    // vbo stride 7: [x,y,z,w, mtx_slot, u, v]; texcoords span the texture.
    static float tri[3 * 7] = {
         0.0f,  1.0f, -2.5f, 1.0f, 0.0f,  0.5f, 0.0f, // top
        -1.0f, -1.0f, -2.5f, 1.0f, 0.0f,  0.0f, 1.0f, // bottom-left
         1.0f, -1.0f, -2.5f, 1.0f, 0.0f,  1.0f, 1.0f, // bottom-right
    };
    DrawTriangles(tri, 3 * 7, 1);
}

// --- frame lifecycle -----------------------------------------------------------

void GfxRenderingAPIGX::Init() {
    // GX itself is initialized by the window backend; the interpreter sets render
    // state per draw, but only on CHANGE - so the GX defaults must match the
    // interpreter's initial RenderingState (cull_keep_sign 0, depth off, no blend),
    // otherwise GX_Init's own defaults (cull back, z-test on) silently drop the
    // first frames' geometry.
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
}

void GfxRenderingAPIGX::OnResize() {}

void GfxRenderingAPIGX::StartFrame() {}

void GfxRenderingAPIGX::EndFrame() {
    GX_DrawDone();
}

void GfxRenderingAPIGX::FinishRender() {}

// --- framebuffers (console renders direct to screen; render-to-texture is TODO) -

int GfxRenderingAPIGX::CreateFramebuffer() {
    return 0; // framebuffer 0 == the screen
}

void GfxRenderingAPIGX::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                                    bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                                    bool can_extract_depth) {
    (void)fb_id; (void)width; (void)height; (void)msaa_level;
    (void)opengl_invertY; (void)render_target; (void)has_depth_buffer; (void)can_extract_depth;
    // TODO: allocate an EFB-copy target texture for render-to-texture framebuffers.
}

void GfxRenderingAPIGX::StartDrawToFramebuffer(int fbId, float noiseScale) {
    (void)fbId; (void)noiseScale;
}

void GfxRenderingAPIGX::CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1,
                                        int dstX0, int dstY0, int dstX1, int dstY1) {
    (void)fbDstId; (void)fbSrcId;
    (void)srcX0; (void)srcY0; (void)srcX1; (void)srcY1;
    (void)dstX0; (void)dstY0; (void)dstX1; (void)dstY1;
    // TODO: GX_CopyTex for render-to-texture copies.
}

void GfxRenderingAPIGX::ClearFramebuffer(bool color, bool depth) {
    (void)color; (void)depth;
    // The EFB is cleared by GX_CopyDisp at frame end (GX_SetCopyClear).
}

void GfxRenderingAPIGX::ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) {
    (void)fbId; (void)width; (void)height; (void)rgba16Buf;
}

void GfxRenderingAPIGX::ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) {
    (void)fbIdTarger; (void)fbIdSrc;
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIGX::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    (void)fb_id; (void)coordinates;
    return {};
}

void* GfxRenderingAPIGX::GetFramebufferTextureId(int fbId) {
    (void)fbId;
    return nullptr;
}

void GfxRenderingAPIGX::SelectTextureFb(int fbId) {
    (void)fbId;
}

ImTextureID GfxRenderingAPIGX::GetTextureById(int id) {
    (void)id;
    return nullptr; // no ImGui on console
}

// Factory used by the window backend. Instantiating GfxRenderingAPIGX here also
// makes the compiler confirm every pure virtual of GfxRenderingAPI is satisfied.
GfxRenderingAPI* lugx_create_gx_rendering_api() {
    return new GfxRenderingAPIGX();
}

} // namespace Fast
