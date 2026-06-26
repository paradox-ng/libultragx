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

// Count of texture uploads that failed because the heap was exhausted. Non-zero
// means we are running out of texture RAM (see UploadTexture).
int g_gx_tex_oom = 0;

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
    // Store as GX_TF_RGB5A3 (16-bit): half the RAM and GP texture bandwidth of
    // RGBA8, and lossless for the N64's dominant RGBA16 textures. Size for the
    // tile-aligned (multiple-of-4) dimensions so the tiled write stays in bounds.
    const uint32_t padW = (width + 3u) & ~3u;
    const uint32_t padH = (height + 3u) & ~3u;
    size_t sz = (size_t)padW * padH * 2;
    // Reuse the existing buffer when it is already the right size. The SoH dialog
    // font invalidates the texture cache per glyph, so the same glyphs re-upload
    // every frame; free()+memalign() each time churns and fragments the heap (a big
    // chunk of the per-glyph cost and the source of the OOM that corrupts other
    // textures). Only (re)allocate when the size actually changes. Allocate the new
    // buffer before freeing the old, and on failure keep the previous data and count
    // it rather than binding null (which GX would sample as garbage).
    if (t.data == nullptr || t.dataBytes != sz) {
        void* newData = memalign(32, sz);
        if (newData == nullptr) {
            g_gx_tex_oom++;
            return;
        }
        if (t.data != nullptr) {
            free(t.data);
        }
        t.data = newData;
        t.dataBytes = (u32)sz;
    }
    lugx_tex_rgba32_to_gx_rgb5a3(rgba32Buf, (u8*)t.data, width, height);
    DCFlushRange(t.data, sz);
    t.width = width;
    t.height = height;
    t.fmt = GX_TF_RGB5A3;
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
        mTextures[texId].dataBytes = 0;
    }
}

void GfxRenderingAPIGX::SetTextureFilter(FilteringMode mode) {
    mFilterMode = mode;
}

FilteringMode GfxRenderingAPIGX::GetTextureFilter() {
    return mFilterMode;
}

// --- render state --------------------------------------------------------------

// Last render state the interpreter set (it only re-issues these on CHANGE, so a
// mid-frame ClearFramebuffer that clobbers GX depth/blend must restore them, else the
// interpreter won't and the next draws inherit the clear's state).
static bool sLastDepthTest = false;
static bool sLastDepthMask = false;
static bool sLastBlend = false;

// Coplanar decal handling. SM64 draws shadows and surface decals with G_ZMODE_DEC: they
// sit exactly on the floor they belong to, so a plain LEQUAL depth test z-fights them
// against that floor (and the flicker shifts with the camera angle). Rather than alter
// the depth compare, bias the decal's depth slightly toward the camera through the
// viewport far-z (the wii port's approach), so a decal reliably passes LEQUAL against its
// surface. The depth test/mask themselves stay owned by SetDepthTestAndMask.
static int sVpX = 0, sVpY = 0, sVpW = 0, sVpH = 0;
static float sAppliedFarZ = 1.0f;
static bool sDecalOn = false;
static const float kGxDecalBias = 0.0001f;

static void lugx_issue_viewport() {
    const float farZ = sDecalOn ? (1.0f - kGxDecalBias) : 1.0f;
    GX_SetViewport((f32)sVpX, (f32)sVpY, (f32)sVpW, (f32)sVpH, 0.0f, farZ);
    sAppliedFarZ = farZ;
}

void GfxRenderingAPIGX::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    sLastDepthTest = depth_test;
    sLastDepthMask = z_upd;
    lugx_set_depth(depth_test, z_upd);
}

void GfxRenderingAPIGX::SetZmodeDecal(bool decal) {
    if (decal != sDecalOn) {
        sDecalOn = decal;
        lugx_issue_viewport();
    }
}

void GfxRenderingAPIGX::SetStrictDecal(bool on) {
    (void)on;
}

void GfxRenderingAPIGX::SetViewport(int x, int y, int width, int height) {
    sVpX = x;
    sVpY = y;
    sVpW = width;
    sVpH = height;
    lugx_issue_viewport();
}

void GfxRenderingAPIGX::SetScissor(int x, int y, int width, int height) {
    GX_SetScissor((u32)x, (u32)y, (u32)width, (u32)height);
}

void GfxRenderingAPIGX::SetUseAlpha(bool useAlpha) {
    sLastBlend = useAlpha;
    lugx_set_blend(useAlpha);
}

void GfxRenderingAPIGX::SetCullMode(int8_t keepSign) {
    // GX hardware backface culling can't be used here: the 3D path feeds GX clip-space
    // coords (the slot MVP is applied on the CPU, GX only does the perspective divide),
    // so the rasterizer's winding test sees the wrong handedness, and vertices behind
    // the eye (w<0) need a sign flip GX has no way to express. The interpreter already
    // flushes before changing cull mode, so we just record the keep sign and apply the
    // cross-product test per triangle in DrawTriangles (matching the wii port).
    mCullKeepSign = keepSign;
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
    static int dtc = 0;
    bool dz = false; // frame-1 draw confirmed to complete; trace moved downstream
    char dzb[48];
#define DZ(tag) do { if (dz) { snprintf(dzb, sizeof(dzb), "dt %s", tag); bootlog(dzb); bootflush(); } } while (0)
    const CCFeatures& cc = mCurrentShader->features;

    // Drive the TEV stage(s) from the decoded combiner + resolved constant colours.
    lugx_tev_from_features(&cc, mCombinerUniforms.inputs);
    // Alpha test (cutout). The N64 discards near-transparent texels via the alpha
    // compare for text glyphs and texture-edge cutouts; the interpreter only encodes
    // this for the GL shader's discard, so GX must apply it here or the transparent
    // texels render opaque (a solid rectangle around text, broken cutouts).
    lugx_set_alpha_test(cc.opt_alpha_threshold || cc.opt_texture_edge, 128);
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
            GX_InitTexObj(&obj, t.data, (u16)t.width, (u16)t.height, t.fmt, t.wrapS, t.wrapT, GX_FALSE);
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
    // Detect 2D vs 3D by transforming v0's w through its slot matrix: 2D rects come
    // pre-transformed to clip space with an identity palette matrix (w stays 1); 3D
    // geometry has w = camera depth != 1. 3D uses the Wii-port software-clip technique
    // (transform each vertex in the loop, feed GX clip coords + a pass-through
    // perspective) which handles the camera-baked + per-object MVPs GX_LoadProjectionMtx
    // can't. 2D keeps the proven single-matrix-as-projection path.
    // 3D vs 2D from the MATRIX's W column (col 3), not a single vertex's w (fragile):
    // 2D rects / orthographic come pre-transformed with MP_col3 == [0,0,0,1]; a
    // perspective MVP has MP_col3 = -modelview_col2 (a non-trivial direction - the
    // perspective W coupling, which is in [0][3]/[1][3] for a horizontal camera, not
    // just [2][3]). Non-trivial W column -> 3D -> software-clip.
    const float(*Mc)[4] = mTransform.mtx_palette[slot0];
    const bool swclip = (Mc[0][3] < -0.001f || Mc[0][3] > 0.001f || Mc[1][3] < -0.001f || Mc[1][3] > 0.001f ||
                         Mc[2][3] < -0.001f || Mc[2][3] > 0.001f || Mc[3][3] < 0.999f || Mc[3][3] > 1.001f);
    if (swclip) {
        Mtx ident;
        guMtxIdentity(ident);
        GX_LoadPosMtxImm(ident, GX_PNMTX0);
        // Fixed pass-through perspective (built once): GX computes W = -z_in and maps a
        // w-buffered depth in [n,f] to NDC z [-1,0]. n/f are SM64's.
        static Mtx44 sPassPersp;
        static bool sPassBuilt = false;
        if (!sPassBuilt) {
            const float n = 16.0f, f = 24000.0f;
            memset(sPassPersp, 0, sizeof(sPassPersp));
            sPassPersp[0][0] = 1.0f;
            sPassPersp[1][1] = 1.0f;
            sPassPersp[2][2] = -n / (f - n);
            sPassPersp[2][3] = -(n * f) / (f - n);
            sPassPersp[3][2] = -1.0f;
            sPassBuilt = true;
        }
        GX_LoadProjectionMtx(sPassPersp, GX_PERSPECTIVE);
    } else {
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
        // Clip-z remap: GL-convention NDC z [-1,1] -> GX z [-1,0].
        for (int c = 0; c < 4; c++) {
            proj[2][c] = 0.5f * proj[2][c] - 0.5f * proj[3][c];
        }
        // This path is taken only when the slot matrix has no perspective term (W column
        // [0,0,0,1]), i.e. an orthographic/affine 2D transform, so load it as ORTHOGRAPHIC.
        // GX_PERSPECTIVE would read the z-coupling column (proj[i][2]) and force w = -z,
        // discarding the translation column (proj[i][3]); 2D quads positioned purely by a
        // translate matrix (menu glyphs, the title background tiles, the file-select cursor)
        // would then collapse to the object origin and vanish. ORTHOGRAPHIC reads proj[i][3]
        // (the translation) and uses w = 1.
        GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    }
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
    // Emit one vertex's NON-position attributes (normal/colour/texcoord), shared by
    // both the 2D and the software-clipped 3D paths.
    auto emitAttribs = [&](const float* v) {
        if (submitNormal) {
            GX_Normal3f32(v[shadeOff + 0], v[shadeOff + 1], v[shadeOff + 2]);
        }
        if (submitColor) {
            u8 a = useAlpha ? float_to_u8(v[shadeOff + 3]) : 255;
            GX_Color4u8(float_to_u8(v[shadeOff + 0]), float_to_u8(v[shadeOff + 1]),
                        float_to_u8(v[shadeOff + 2]), a);
        }
        if (numTex > 0) {
            // Per-draw UV transform (raw RSP texcoord -> normalized GX texcoord):
            // uv = raw * scale + offset (CombinerUniforms.uv_transform).
            const float* uvt = mCombinerUniforms.uv_transform[0];
            GX_TexCoord2f32(v[texOff + 0] * uvt[0] + uvt[1], v[texOff + 1] * uvt[2] + uvt[3]);
        }
    };

    if (swclip) {
        // 3D path. Transform each vertex by its slot's MVP to clip space and feed GX
        // clip coords (the pass-through perspective does only the divide). Per the wii
        // port's gfx_sp_tri1: compute each vertex's clip_rej (which frustum plane it is
        // outside of, from the REAL pre-clamp w) and DROP any triangle whose 3 vertices
        // are all outside the SAME plane. Without this, off-screen / behind-camera
        // triangles get w-clamped and fling across the screen (the "vertex explosion").
        // Two passes: GX_Begin needs the exact kept-vertex count up front.
        auto clipOf = [&](const float* v, float& cx, float& cy, float& cw) -> unsigned {
            int s = (int)v[4];
            if (s < 0 || s >= GFX_MTX_PALETTE_SIZE) {
                s = 0;
            }
            const float(*M)[4] = mTransform.mtx_palette[s];
            const float ox = v[0], oy = v[1], oz = v[2], ow = v[3];
            cx = ox * M[0][0] + oy * M[1][0] + oz * M[2][0] + ow * M[3][0];
            cy = ox * M[0][1] + oy * M[1][1] + oz * M[2][1] + ow * M[3][1];
            const float cz = ox * M[0][2] + oy * M[1][2] + oz * M[2][2] + ow * M[3][2];
            cw = ox * M[0][3] + oy * M[1][3] + oz * M[2][3] + ow * M[3][3];
            unsigned r = 0;
            if (cx < -cw) r |= 1u;
            if (cx > cw) r |= 2u;
            if (cy < -cw) r |= 4u;
            if (cy > cw) r |= 8u;
            if (cz < -cw) r |= 16u;
            if (cz > cw) r |= 32u;
            return r;
        };
        // Returns true if the triangle should be dropped: either all 3 vertices are
        // outside the same frustum plane (clip reject), or it fails the backface cull.
        // The cull is the wii-port screen-space cross product with a w<0 sign flip; we
        // can't use GX hardware culling because we feed it clip coords (wrong winding).
        auto triDrop = [&](const float* a, const float* b, const float* c) -> bool {
            float ax, ay, aw, bx, by, bw, ccx, ccy, ccw;
            const unsigned ra = clipOf(a, ax, ay, aw);
            const unsigned rb = clipOf(b, bx, by, bw);
            const unsigned rc = clipOf(c, ccx, ccy, ccw);
            if (ra & rb & rc) {
                return true;
            }
            if (mCullKeepSign != 0) {
                const float dx1 = ax / aw - bx / bw;
                const float dy1 = ay / aw - by / bw;
                const float dx2 = ccx / ccw - bx / bw;
                const float dy2 = ccy / ccw - by / bw;
                float cross = dx1 * dy2 - dy1 * dx2;
                if ((aw < 0) ^ (bw < 0) ^ (ccw < 0)) {
                    cross = -cross; // one vertex behind the eye flips the screen winding
                }
                if (mCullKeepSign > 0) {
                    if (cross <= 0.0f) {
                        return true;
                    }
                } else {
                    if (cross >= 0.0f) {
                        return true;
                    }
                }
            }
            return false;
        };
        const size_t nTris = buf_vbo_num_tris;
        size_t kept = 0;
        for (size_t t = 0; t < nTris; t++) {
            const float* a = &buf_vbo[(t * 3 + 0) * (size_t)stride];
            const float* b = &buf_vbo[(t * 3 + 1) * (size_t)stride];
            const float* c = &buf_vbo[(t * 3 + 2) * (size_t)stride];
            if (!triDrop(a, b, c)) {
                kept++;
            }
        }
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)(kept * 3));
        for (size_t t = 0; t < nTris; t++) {
            const float* tri[3] = { &buf_vbo[(t * 3 + 0) * (size_t)stride],
                                    &buf_vbo[(t * 3 + 1) * (size_t)stride],
                                    &buf_vbo[(t * 3 + 2) * (size_t)stride] };
            if (triDrop(tri[0], tri[1], tri[2])) {
                continue; // outside one frustum plane or backface-culled
            }
            for (int k = 0; k < 3; k++) {
                const float* v = tri[k];
                int s = (int)v[4];
                if (s < 0 || s >= GFX_MTX_PALETTE_SIZE) {
                    s = 0;
                }
                const float(*M)[4] = mTransform.mtx_palette[s];
                const float ox = v[0], oy = v[1], oz = v[2], ow = v[3];
                const float cx = ox * M[0][0] + oy * M[1][0] + oz * M[2][0] + ow * M[3][0];
                const float cy = ox * M[0][1] + oy * M[1][1] + oz * M[2][1] + ow * M[3][1];
                float cw = ox * M[0][3] + oy * M[1][3] + oz * M[2][3] + ow * M[3][3];
                if (cw < 0.001f) {
                    cw = 0.001f;
                }
                GX_Position3f32(cx, cy, -cw);
                emitAttribs(v);
            }
        }
        GX_End();
    } else {
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)verts);
        for (size_t i = 0; i < verts; i++) {
            const float* v = &buf_vbo[i * (size_t)stride];
            GX_Position3f32(v[0], v[1], v[2]);
            emitAttribs(v);
        }
        GX_End();
    }
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
    (void)color; // colour is cleared by GX_CopyDisp's EFB clear each frame; a full-
                 // screen colour fill here wipes the HUD/level. We only need DEPTH.
    if (!depth) {
        return;
    }
    // GX has no direct mid-frame EFB clear (GX_CopyDisp clears, but it also copies the
    // EFB out to the XFB - we can't present mid-frame). The game clears the depth buffer
    // BETWEEN passes (e.g. a depth-only clear before the 3D world; interpreter.cpp
    // ClearFramebuffer(false,true)); ignoring it left stale near-depth that made the 3D
    // fail LEQUAL and vanish. So reset Z by drawing a full-screen quad at the FAR plane
    // with colour writes OFF (depth only) so later geometry passes LEQUAL.
    GX_SetColorUpdate(GX_FALSE);
    GX_SetAlphaUpdate(GX_FALSE);
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

    // Orthographic, identity: clip.z = z-1, clip.w = 1, so z=1 -> NDC z 0 = GX far.
    static Mtx44 sClearOrtho;
    static bool sClearOrthoBuilt = false;
    if (!sClearOrthoBuilt) {
        memset(sClearOrtho, 0, sizeof(sClearOrtho));
        sClearOrtho[0][0] = 1.0f;
        sClearOrtho[1][1] = 1.0f;
        sClearOrtho[2][2] = 1.0f;
        sClearOrtho[2][3] = -1.0f;
        sClearOrtho[3][3] = 1.0f;
        sClearOrthoBuilt = true;
    }
    GX_LoadProjectionMtx(sClearOrtho, GX_ORTHOGRAPHIC);
    Mtx ident;
    guMtxIdentity(ident);
    GX_LoadPosMtxImm(ident, GX_PNMTX0);

    // Flat-colour TEV (the clear colour rides COLOR0 as the material).
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GXColor clr = { 0, 0, 0, 255 }; // TODO: the game's actual clear colour
    GX_SetChanMatColor(GX_COLOR0A0, clr);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    const float Z = 1.0f; // -> GX far depth
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6);
    GX_Position3f32(-1.0f, -1.0f, Z);
    GX_Position3f32(1.0f, -1.0f, Z);
    GX_Position3f32(-1.0f, 1.0f, Z);
    GX_Position3f32(-1.0f, 1.0f, Z);
    GX_Position3f32(1.0f, -1.0f, Z);
    GX_Position3f32(1.0f, 1.0f, Z);
    GX_End();

    // Restore the interpreter's render state (it won't re-issue unchanged state).
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetZMode(sLastDepthTest ? GX_TRUE : GX_FALSE, GX_LEQUAL, sLastDepthMask ? GX_TRUE : GX_FALSE);
    lugx_set_blend(sLastBlend);
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
