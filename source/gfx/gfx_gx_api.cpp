#include "fast/backends/gfx_gx.h"
#include "fast/backends/gfx_gx_camera.h"
#include "fast/cc_features.h"

#include "gfx/gfx_gx_tex.h"
#include "gfx/gfx_gx_state.h"
#include "platform/lugx_config.h"

#include <gccore.h>
#include <ogc/gu.h>
#include <ogc/lwp_watchdog.h> // gettime / ticks_to_microsecs (fps counter)
#include <malloc.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <unistd.h>

// The live EFB size, set by the window backend from the selected render mode (it
// varies by TV standard and halves under antialiasing, so it is never assumed).
extern "C" uint16_t g_lugx_efb_width;
extern "C" uint16_t g_lugx_efb_height;

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

// Matrix-load cache. The 3D software-clip path feeds every draw a constant identity
// pos-matrix and a constant pass-through projection, and the lit path a constant
// identity normal matrix. GX matrix memory persists until overwritten, so track which
// of those are already resident and skip the redundant per-draw reloads (many draws
// per frame). Any code that loads a different pos/projection (the 2D path,
// ClearFramebuffer, the fps overlay) must clear the flags via lugx_gx_invalidate_mtx_cache.
static bool sPassThroughLoaded = false; // identity pos-mtx + pass-through projection resident
static bool sNrmIdentityLoaded = false; // identity normal matrix resident
static void lugx_gx_invalidate_mtx_cache() {
    sPassThroughLoaded = false;
    sNrmIdentityLoaded = false;
}

// --- temporary per-frame CPU profiler (interpreter-overhead pass) --------------------
// g_lugx_prof_total_ticks = wall time of the whole-frame render (Interpreter::Run, set
// by Fast3dWindow); g_lugx_prof_draw_ticks = summed time inside DrawTriangles. The
// difference (total - draw) is the DL walk / command-dispatch overhead. Averages are
// shown under the fps counter in microseconds/frame. Remove once the split is measured.
extern "C" {
volatile int g_lugx_prof_enabled = 0;         // set from config each frame; gates all timers
volatile uint64_t g_lugx_prof_total_ticks = 0;
volatile uint64_t g_lugx_prof_draw_ticks = 0;
volatile uint64_t g_lugx_prof_vtx_ticks = 0;  // GfxSpVertex (vertex load/expand)
volatile uint64_t g_lugx_prof_tri_ticks = 0;  // GfxSpTri1 whole (includes its Flush->draw)
volatile uint64_t g_lugx_prof_comb_ticks = 0; // LookupOrCreateColorCombiner
volatile uint32_t g_lugx_prof_frames = 0;
}
namespace {
struct LugxProfScope {
    uint64_t start;
    volatile uint64_t* accum;
    explicit LugxProfScope(volatile uint64_t* a) : accum(g_lugx_prof_enabled ? a : nullptr) {
        if (accum) start = gettime();
    }
    ~LugxProfScope() {
        if (accum) *accum += gettime() - start;
    }
};
} // namespace

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

void GfxRenderingAPIGX::SetNextTexturePack(LugxTexPack pack) {
    mNextPack = pack;
}

void GfxRenderingAPIGX::UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) {
    // Consume the interpreter's format hint and return to the RGB5A3 default, so
    // standalone uploads (the CI palette, the fallback checker) never inherit a
    // stale intensity/IA pack from a preceding decode.
    const LugxTexPack pack = mNextPack;
    mNextPack = LugxTexPack::RGB5A3;

    if (mCurrentTile < 0 || mCurrentTile >= 2) {
        return;
    }
    uint32_t id = mTileTexture[mCurrentTile];
    if (id >= mTextures.size()) {
        return;
    }
    GxTexture& t = mTextures[id];

    // Store each texture in the tightest GX format that reproduces its N64 source
    // (chosen by the interpreter). RGBA16 is already optimal at RGB5A3; intensity
    // and intensity-alpha sources pack to I4/I8/IA4/IA8 for 2-4x less texture RAM
    // and GP bandwidth. GX derives the tiling from (width, height, fmt), so pass the
    // real dimensions to GX_InitTexObj; the packed buffer is sized for the format's
    // tile-aligned dimensions (block sizes vary: I4 8x8, I8/IA4 8x4, else 4x4).
    u8 gxFmt;
    uint32_t blockW, blockH;
    uint32_t bitsPerTexel;
    switch (pack) {
        case LugxTexPack::I4:  gxFmt = GX_TF_I4;  blockW = 8; blockH = 8; bitsPerTexel = 4;  break;
        case LugxTexPack::I8:  gxFmt = GX_TF_I8;  blockW = 8; blockH = 4; bitsPerTexel = 8;  break;
        case LugxTexPack::IA4: gxFmt = GX_TF_IA4; blockW = 8; blockH = 4; bitsPerTexel = 8;  break;
        case LugxTexPack::IA8: gxFmt = GX_TF_IA8; blockW = 4; blockH = 4; bitsPerTexel = 16; break;
        case LugxTexPack::RGB5A3:
        default:               gxFmt = GX_TF_RGB5A3; blockW = 4; blockH = 4; bitsPerTexel = 16; break;
    }
    const uint32_t padW = (width + (blockW - 1)) & ~(blockW - 1);
    const uint32_t padH = (height + (blockH - 1)) & ~(blockH - 1);
    const size_t sz = ((size_t)padW * padH * bitsPerTexel) / 8;
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
    switch (pack) {
        case LugxTexPack::I4:  lugx_tex_rgba32_to_gx_i4(rgba32Buf, (u8*)t.data, width, height);  break;
        case LugxTexPack::I8:  lugx_tex_rgba32_to_gx_i8(rgba32Buf, (u8*)t.data, width, height);  break;
        case LugxTexPack::IA4: lugx_tex_rgba32_to_gx_ia4(rgba32Buf, (u8*)t.data, width, height); break;
        case LugxTexPack::IA8: lugx_tex_rgba32_to_gx_ia8(rgba32Buf, (u8*)t.data, width, height); break;
        case LugxTexPack::RGB5A3:
        default:               lugx_tex_rgba32_to_gx_rgb5a3(rgba32Buf, (u8*)t.data, width, height); break;
    }
    DCFlushRange(t.data, sz);
    t.width = width;
    t.height = height;
    t.fmt = gxFmt;
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
// Shadows and other decals sit exactly on the surface they belong to, so they need to be
// nudged toward the camera to win the depth test against it. The nudge has to be a fixed
// distance in the world: depth is stored so that it bunches up with distance, so a fixed
// nudge in stored depth would be worth a few units near the camera but tens of units far
// away, which is enough for a shadow to cover an object standing in front of it.
//
// Because this backend transforms vertices itself and hands GX a depth that is the
// distance from the eye, the nudge can simply be subtracted from that distance. A few
// units is comfortably more than the rounding between a floor and the shadow lying on it,
// and far less than the gap to anything genuinely in front. It moves nothing on screen;
// it only decides which surface wins where they overlap.
static const float kDecalEyeBias = 5.0f;

static void lugx_issue_viewport() {
    // The decal bias is applied per vertex, in the distance the vertex actually sits at
    // (see kDecalEyeBias), so the depth range stays whole.
    GX_SetViewport((f32)sVpX, (f32)sVpY, (f32)sVpW, (f32)sVpH, 0.0f, 1.0f);
    sAppliedFarZ = 1.0f;
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
    // The rectangle arrives measured from the bottom of the framebuffer upward, and its y
    // names the LOWER edge: the interpreter builds it from the game's lower bound and
    // then flips the axis. GX measures from the top down, so its upper edge is the
    // framebuffer height less the far side of the rectangle. Reading y as the upper edge
    // instead collapses a full-screen rectangle to nothing and blanks the display. The
    // size comes from the window rather than being assumed: it differs per TV standard
    // and halves again under antialiasing.
    const int fbWidth = (int)g_lugx_efb_width;
    const int fbHeight = (int)g_lugx_efb_height;

    int left = x;
    int top = fbHeight - (y + height);
    int wd = width;
    int ht = height;

    // Clip to the framebuffer. A rectangle reaching past an edge is normal (the game
    // scissors in its own coordinates); one that misses entirely would be rejected by GX,
    // so collapse it to something empty but valid instead.
    if (left < 0) {
        wd += left;
        left = 0;
    }
    if (top < 0) {
        ht += top;
        top = 0;
    }
    if (left + wd > fbWidth) {
        wd = fbWidth - left;
    }
    if (top + ht > fbHeight) {
        ht = fbHeight - top;
    }
    if (wd <= 0 || ht <= 0) {
        // A rectangle that came out inside-out or empty is a quirk of the game's own
        // arithmetic rather than a request to draw nothing (the dialogue bounds invert
        // while the box is opening). Clipping to nothing there blanks whatever is being
        // drawn, so fall back to the whole framebuffer.
        GX_SetScissor(0, 0, (u32)fbWidth, (u32)fbHeight);
        return;
    }
    GX_SetScissor((u32)left, (u32)top, (u32)wd, (u32)ht);
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

// Identity fill for the constant GX position/normal matrices loaded below. Done inline
// rather than via libogc's guMtxIdentity so the game build never links libogc's gu.o -
// whose guLookAt/guOrtho/guPerspective would collide with the N64 gu functions a game
// defines in its own engine (e.g. Starship's src/engine/lookat.c, guPerspectiveF.c).
static inline void lugx_mtx_identity(Mtx m) {
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
}

volatile unsigned g_dt_total = 0;     // total triangle batches submitted (gdb-readable)
volatile unsigned g_dt_lastverts = 0; // verts of the most recent batch

// A clip-space vertex plus its interpolable attributes, used by the 3D path's
// near-plane clipping. `a` packs normal|colour (at shadeOff) then texcoord, matching
// emitAttribs's order; only the first nAttr floats are meaningful for a given draw.
struct LugxClipVert {
    float x, y, z, w;
    float a[9];
};

// Fog. GX has a dedicated fog unit, so this costs no TEV stage and no CPU work: the
// hardware blends each pixel toward the fog colour as a function of its depth.
//
// Picking the mode needs care. The N64 derives its fog factor from the vertex's depth
// AFTER the perspective divide, so the factor is linear in screen depth. GX's "PERSP"
// modes undo the projection to make fog linear in eye depth instead, which is a
// different curve. The ORTHO modes take the depth as-is, and since this backend already
// hands GX post-divide coordinates through a fixed pass-through projection, ORTHO is the
// mode that reproduces what the N64 did.
//
// The interpreter hands us the N64's own multiplier and offset, a ramp over the depth
// range expressed in 0..255. Solving that against GX's (depth - start) / (end - start)
// gives the start and end below.
// Near and far planes of the fixed pass-through projection this backend loads (see
// sPassPersp). The fog unit needs them to turn rasterised depth back into eye space.
static constexpr float kPassNearZ = 16.0f;
static constexpr float kPassFarZ = 24000.0f;

static u8 sFogType = GX_FOG_NONE;
static float sFogStart = 0.0f, sFogEnd = 0.0f;
static GXColor sFogColor = { 0, 0, 0, 0 };

// The N64 also carries a fog factor in each vertex's alpha, which the fog unit cannot
// express because it is per-vertex data rather than a function of distance. That case
// gets a texture environment stage instead: the stage mixes whatever the combiner
// produced toward the fog colour by the rasterised alpha, leaving alpha untouched. GX
// allows sixteen such stages and the combiner uses one, so this is affordable.
//
// A stage may only reference a single constant colour, so the fog colour goes in one and
// the factor comes from the vertex. The remaining case, a constant veil with no per-pixel
// variation, would need a second constant and is left alone for now.
static bool lugx_apply_tev_fog(const CCFeatures& cc, const CombinerUniforms& u) {
    if (!cc.opt_fog) {
        return false;
    }
    const GXColor fog = { float_to_u8(u.fog_color[0]), float_to_u8(u.fog_color[1]), float_to_u8(u.fog_color[2]),
                          255 };

    if (u.fog_params[3] == 2.0f) {
        // Factor carried per vertex. One stage suffices: the factor arrives as the
        // rasterised alpha, so the stage's single constant is free for the fog colour.
        GX_SetTevKColor(GX_KCOLOR0, fog);
        GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K0);
        GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        // out = (1 - factor) * previous + factor * fog colour
        GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_KONST, GX_CC_RASA, GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetNumTevStages(2);
        return true;
    }

    if (u.fog_params[3] == 1.0f) {
        // A constant veil over everything. This needs both the fog colour and the blend
        // amount, and a stage may only reference one constant colour, so it is split in
        // two: fade what the combiner produced, then add the fog colour already scaled by
        // the amount. Splitting keeps it exact, where reusing one of the combiner's own
        // registers would silently corrupt whatever it was holding.
        const float f = u.fog_params[2];
        const GXColor amount = { float_to_u8(f), float_to_u8(f), float_to_u8(f), 255 };
        const GXColor premultiplied = { float_to_u8(u.fog_color[0] * f), float_to_u8(u.fog_color[1] * f),
                                        float_to_u8(u.fog_color[2] * f), 255 };
        GX_SetTevKColor(GX_KCOLOR0, amount);
        GX_SetTevKColor(GX_KCOLOR1, premultiplied);

        // out = (1 - amount) * previous
        GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K0);
        GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_KONST, GX_CC_ZERO);
        GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

        // out = previous + fog colour * amount
        GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K1);
        GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
        GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetNumTevStages(3);
        return true;
    }
    return false;
}

static void lugx_apply_fog(const CCFeatures& cc, const CombinerUniforms& u, const float (*Mc)[4]) {
    u8 type = GX_FOG_NONE;
    float startZ = 0.0f, endZ = 1.0f;
    GXColor color = { 0, 0, 0, 255 };

    // fog_params[3] flags the other two ways a fog factor arrives, neither of which is a
    // function of distance; those go through texture environment stages instead.
    const bool depthFog = cc.opt_fog && u.fog_params[3] == 0.0f && u.fog_params[0] != 0.0f;
    if (depthFog && Mc != nullptr) {
        const float m = u.fog_params[0];
        const float o = u.fog_params[1];

        // The N64 ramps its factor over depth AFTER the perspective divide, as
        // (z/w) * mul + offset across 0..255. Its z and w both come from the game's own
        // matrix, in which they are related linearly: z = -alpha * w + beta. Recover that
        // pair from the matrix rather than assuming a projection, which is what made an
        // earlier attempt fog far too heavily: the ramp was converted using this
        // backend's own near and far planes, which have nothing to do with the game's.
        // The W column is Mc[i][3] and the Z column Mc[i][2]; for the three directional
        // rows the two are proportional, and the remaining row carries the offset.
        int pick = 0;
        float best = 0.0f;
        for (int i = 0; i < 3; i++) {
            const float mag = Mc[i][3] < 0.0f ? -Mc[i][3] : Mc[i][3];
            if (mag > best) {
                best = mag;
                pick = i;
            }
        }
        if (best > 1e-6f) {
            const float alpha = -Mc[pick][2] / Mc[pick][3];
            const float beta = Mc[3][2] + alpha * Mc[3][3];
            // Solve for the distances at which the factor reaches nothing and reaches
            // full. Both divisors and beta are commonly negative here (the depth axis
            // points away from the eye), so judge the result by the distances that come
            // out rather than by the sign of the terms going in.
            const float denomStart = alpha * m - o;
            const float denomEnd = denomStart + 255.0f;
            // Convert a distance into the depth this backend stores for it, which is what
            // the fog unit compares against once told to read depth as it stands.
            auto distanceToStoredDepth = [](float w) {
                const float n = kPassNearZ, f = kPassFarZ;
                if (w < n) {
                    w = n;
                }
                return 1.0f + (n / (f - n)) * (1.0f - f / w);
            };
            const float absStart = denomStart < 0.0f ? -denomStart : denomStart;
            const float absEnd = denomEnd < 0.0f ? -denomEnd : denomEnd;
            if (absStart > 1e-6f && absEnd > 1e-6f) {
                const float wStart = beta * m / denomStart;
                const float wEnd = beta * m / denomEnd;
                startZ = distanceToStoredDepth(wStart);
                endZ = distanceToStoredDepth(wEnd);
                if (wStart > 0.0f && wEnd > 0.0f && endZ > startZ + 1e-6f) {
                    // The factor is linear in stored depth, so the fog unit must read
                    // depth as it stands rather than converting it back to a distance:
                    // that conversion would bend the ramp into a different curve, which
                    // showed up as fog thickening too early.
                    type = GX_FOG_ORTHO_LIN;
                    color.r = float_to_u8(u.fog_color[0]);
                    color.g = float_to_u8(u.fog_color[1]);
                    color.b = float_to_u8(u.fog_color[2]);
                }
            }
        }
    }

    if (type == sFogType && startZ == sFogStart && endZ == sFogEnd && color.r == sFogColor.r &&
        color.g == sFogColor.g && color.b == sFogColor.b) {
        return; // already resident; the fog unit keeps its state between draws
    }
    sFogType = type;
    sFogStart = startZ;
    sFogEnd = endZ;
    sFogColor = color;
    // Start and end are given as stored depth, so the near and far handed over are the
    // span of stored depth itself rather than the projection's planes: read as it stands,
    // which is the relationship the N64's ramp is written against.
    GX_SetFog(type, startZ, endZ, 0.0f, 1.0f, color);
}

void GfxRenderingAPIGX::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    (void)buf_vbo_len;
    if (g_gx_skip_draw || mCurrentShader == nullptr || buf_vbo_num_tris == 0) {
        return;
    }
    LugxProfScope _ps(&g_lugx_prof_draw_ticks); // profiler: time the draw path
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
    // Fog reaches us either as a distance ramp, which the fog unit handles, or carried in
    // the vertex alpha, which needs its own stage after the combiner's.
    lugx_apply_tev_fog(cc, mCombinerUniforms);
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
        // Reflective surfaces ask for their texture coordinates to be generated from the
        // surface normal rather than read from the vertex, which is how the N64 makes
        // metal and chrome sweep as an object turns. It derives each coordinate by
        // projecting the normal onto one of two model-space vectors and then scaling and
        // offsetting the result, which is precisely a 2x4 matrix applied to the normal,
        // so GX generates it directly from the normal with no CPU work per vertex.
        if (cc.opt_texgen) {
            const LightingUniforms& lu = mLightingUniforms;
            Mtx tg;
            memset(tg, 0, sizeof(tg));
            // Normals reach us as the game stores them, spanning roughly plus or minus a
            // hundred and twenty seven rather than a unit length, so the projection onto
            // each lookat vector is brought back to that range before the tile scale is
            // applied. Leaving it out makes the generated coordinates far too large and
            // the reflection unrecognisable. The N64 also clamps the projection to the
            // unit range, which a matrix cannot express; the texture's wrap mode covers
            // the overshoot in practice.
            const float kNormalScale = 1.0f / 127.0f;
            for (int i = 0; i < 3; i++) {
                tg[0][i] = lu.lookat_x[i] * kNormalScale * lu.texgen[0][0];
                tg[1][i] = lu.lookat_y[i] * kNormalScale * lu.texgen[0][2];
            }
            tg[0][3] = lu.texgen[0][1];
            tg[1][3] = lu.texgen[0][3];
            GX_LoadTexMtxImm(tg, GX_TEXMTX0, GX_MTX2x4);
            GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX0);
        } else {
            GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        }
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
    // Depth fog needs this draw's matrix to know how its depth maps to distance.
    lugx_apply_fog(cc, mCombinerUniforms, Mc);
    const bool swclip = (Mc[0][3] < -0.001f || Mc[0][3] > 0.001f || Mc[1][3] < -0.001f || Mc[1][3] > 0.001f ||
                         Mc[2][3] < -0.001f || Mc[2][3] > 0.001f || Mc[3][3] < 0.999f || Mc[3][3] > 1.001f);
    if (swclip) {
        // Fixed pass-through perspective (built once): GX computes W = -z_in and maps a
        // w-buffered depth in [n,f] to NDC z [-1,0]. n/f are SM64's.
        static Mtx44 sPassPersp;
        static bool sPassBuilt = false;
        if (!sPassBuilt) {
            const float n = kPassNearZ, f = kPassFarZ;
            memset(sPassPersp, 0, sizeof(sPassPersp));
            sPassPersp[0][0] = 1.0f;
            sPassPersp[1][1] = 1.0f;
            sPassPersp[2][2] = -n / (f - n);
            sPassPersp[2][3] = -(n * f) / (f - n);
            sPassPersp[3][2] = -1.0f;
            sPassBuilt = true;
        }
        // Identity pos-matrix + pass-through projection are constant across every 3D
        // draw; only (re)load them when they are not already resident (see the cache).
        if (!sPassThroughLoaded) {
            Mtx ident;
            lugx_mtx_identity(ident);
            GX_LoadPosMtxImm(ident, GX_PNMTX0);
            GX_LoadProjectionMtx(sPassPersp, GX_PERSPECTIVE);
            sPassThroughLoaded = true;
        }
    } else {
        // A flat, affine transform. GX does not keep a projection as a whole matrix: an
        // orthographic one is six numbers, the two scales, the two translations and the
        // depth pair. Everything off the diagonal is discarded, and that is precisely
        // where a rotation lives, so passing the game's matrix as the projection quietly
        // drops any spin: the dialogue box grew and slid into place instead of turning.
        //
        // The position matrix is a full three-by-four and does carry rotation, so the
        // game's transform belongs there, leaving the projection to do nothing but hand
        // the result through and settle the depth convention.
        Mtx mv;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                // The interpreter composes its transform the other way round from GX
                // (a point multiplies the matrix rather than the matrix a point), so it
                // arrives transposed.
                mv[r][c] = mTransform.mtx_palette[slot0][c][r];
            }
        }
        GX_LoadPosMtxImm(mv, GX_PNMTX0);

        static Mtx44 sFlatProj;
        static bool sFlatProjBuilt = false;
        if (!sFlatProjBuilt) {
            memset(sFlatProj, 0, sizeof(sFlatProj));
            sFlatProj[0][0] = 1.0f;
            sFlatProj[1][1] = 1.0f;
            // Depth arrives spanning minus one to one and GX wants minus one to zero.
            sFlatProj[2][2] = 0.5f;
            sFlatProj[2][3] = -0.5f;
            sFlatProj[3][3] = 1.0f;
            sFlatProjBuilt = true;
        }
        GX_LoadProjectionMtx(sFlatProj, GX_ORTHOGRAPHIC);
        sPassThroughLoaded = false; // 2D loaded a different pos-mtx + projection
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
        // Constant, so load it only when not already resident (see the cache).
        if (!sNrmIdentityLoaded) {
            Mtx nrmMtx;
            lugx_mtx_identity(nrmMtx);
            GX_LoadNrmMtxImm(nrmMtx, GX_PNMTX0);
            sNrmIdentityLoaded = true;
        }

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
        // Returns true if the triangle should be dropped: either all 3 vertices are
        // outside the same frustum plane (clip reject), or it fails the backface cull.
        // Operates on the ALREADY-transformed clip verts (from toCV) so the per-vertex
        // MVP transform runs once per frame instead of twice - it was the dominant
        // per-frame CPU cost and the reject/cull was re-transforming every vertex. The
        // cull is the wii-port screen-space cross product with a w<0 sign flip; we can't
        // use GX hardware culling because we feed it clip coords (wrong winding).
        auto triDropCV = [&](const LugxClipVert& A, const LugxClipVert& B, const LugxClipVert& C) -> bool {
            auto rej = [](const LugxClipVert& v) -> unsigned {
                unsigned r = 0;
                if (v.x < -v.w) r |= 1u;
                if (v.x > v.w) r |= 2u;
                if (v.y < -v.w) r |= 4u;
                if (v.y > v.w) r |= 8u;
                if (v.z < -v.w) r |= 16u;
                if (v.z > v.w) r |= 32u;
                return r;
            };
            if (rej(A) & rej(B) & rej(C)) {
                return true;
            }
            if (mCullKeepSign != 0) {
                const float dx1 = A.x / A.w - B.x / B.w;
                const float dy1 = A.y / A.w - B.y / B.w;
                const float dx2 = C.x / C.w - B.x / B.w;
                const float dy2 = C.y / C.w - B.y / B.w;
                float cross = dx1 * dy2 - dy1 * dx2;
                if ((A.w < 0) ^ (B.w < 0) ^ (C.w < 0)) {
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
        // Per-vertex transform to a clip-space vertex + its interpolable attributes, and
        // the emit that feeds GX clip coords (the pass-through perspective does the divide)
        // with the w-clamp as a last resort for anything at/behind the eye that survives.
        const int nAttr = (submitNormal ? 3 : 0) + (submitColor ? 4 : 0) + (numTex > 0 ? 2 : 0);
        auto toCV = [&](const float* v) -> LugxClipVert {
            LugxClipVert o;
            int s = (int)v[4];
            if (s < 0 || s >= GFX_MTX_PALETTE_SIZE) {
                s = 0;
            }
            const float(*M)[4] = mTransform.mtx_palette[s];
            // Object-space w is always 1 (GfxSpVertex sets loaded_vertices w = 1 for
            // every vertex, and rects likewise), so the M[3][*] column adds directly -
            // four fewer multiplies per vertex than the general ow * M[3][*].
            const float ox = v[0], oy = v[1], oz = v[2];
            o.x = ox * M[0][0] + oy * M[1][0] + oz * M[2][0] + M[3][0];
            o.y = ox * M[0][1] + oy * M[1][1] + oz * M[2][1] + M[3][1];
            o.z = ox * M[0][2] + oy * M[1][2] + oz * M[2][2] + M[3][2];
            o.w = ox * M[0][3] + oy * M[1][3] + oz * M[2][3] + M[3][3];
            int k = 0;
            if (submitNormal) {
                o.a[k++] = v[shadeOff + 0];
                o.a[k++] = v[shadeOff + 1];
                o.a[k++] = v[shadeOff + 2];
            }
            if (submitColor) {
                o.a[k++] = v[shadeOff + 0];
                o.a[k++] = v[shadeOff + 1];
                o.a[k++] = v[shadeOff + 2];
                o.a[k++] = v[shadeOff + 3];
            }
            if (numTex > 0) {
                o.a[k++] = v[texOff + 0];
                o.a[k++] = v[texOff + 1];
            }
            return o;
        };
        auto lerpCV = [&](const LugxClipVert& A, const LugxClipVert& B, float t) -> LugxClipVert {
            LugxClipVert o;
            o.x = A.x + (B.x - A.x) * t;
            o.y = A.y + (B.y - A.y) * t;
            o.z = A.z + (B.z - A.z) * t;
            o.w = A.w + (B.w - A.w) * t;
            for (int i = 0; i < nAttr; i++) {
                o.a[i] = A.a[i] + (B.a[i] - A.a[i]) * t;
            }
            return o;
        };
        auto emitCV = [&](const LugxClipVert& c) {
            // Pull decals a fixed distance toward the eye so they beat the surface they
            // lie on, at any distance (see kDecalEyeBias). Never let that nudge push a
            // vertex through the near plane: clipping has already been done by this
            // point, so a vertex sent behind the eye here is clamped to almost nothing
            // and smears its triangle across the screen. Close to the eye the nudge is
            // not needed anyway, since depth is at its most precise there.
            float cw = c.w;
            if (sDecalOn && (cw - kDecalEyeBias) > kPassNearZ) {
                cw -= kDecalEyeBias;
            }
            cw = cw < 0.001f ? 0.001f : cw;
            GX_Position3f32(c.x, c.y, -cw);
            int k = 0;
            if (submitNormal) {
                GX_Normal3f32(c.a[k], c.a[k + 1], c.a[k + 2]);
                k += 3;
            }
            if (submitColor) {
                u8 al = useAlpha ? float_to_u8(c.a[k + 3]) : 255;
                GX_Color4u8(float_to_u8(c.a[k]), float_to_u8(c.a[k + 1]), float_to_u8(c.a[k + 2]), al);
                k += 4;
            }
            if (numTex > 0) {
                const float* uvt = mCombinerUniforms.uv_transform[0];
                GX_TexCoord2f32(c.a[k] * uvt[0] + uvt[1], c.a[k + 1] * uvt[2] + uvt[3]);
            }
        };
        // A triangle fully in front of the near plane (all w >= W_NEAR) passes through
        // unchanged; one that crosses it is near-plane clipped (Sutherland-Hodgman against
        // w = W_NEAR, <=4 output verts fanned into triangles) so the near part keeps a sane
        // w and the perspective-correct texture no longer stretches/warps (the floor warp).
        static std::vector<LugxClipVert> scratch;
        scratch.clear();
        const float W_NEAR = 1.0f;
        LugxClipVert poly[4];
        const size_t nTris = buf_vbo_num_tris;
        for (size_t t = 0; t < nTris; t++) {
            const float* a = &buf_vbo[(t * 3 + 0) * (size_t)stride];
            const float* b = &buf_vbo[(t * 3 + 1) * (size_t)stride];
            const float* c = &buf_vbo[(t * 3 + 2) * (size_t)stride];
            const LugxClipVert in[3] = { toCV(a), toCV(b), toCV(c) };
            if (triDropCV(in[0], in[1], in[2])) {
                continue; // outside one frustum plane or backface-culled
            }
            if (in[0].w >= W_NEAR && in[1].w >= W_NEAR && in[2].w >= W_NEAR) {
                scratch.push_back(in[0]);
                scratch.push_back(in[1]);
                scratch.push_back(in[2]);
                continue;
            }
            int np = 0;
            for (int i = 0; i < 3; i++) {
                const LugxClipVert& cur = in[i];
                const LugxClipVert& nxt = in[(i + 1) % 3];
                const bool curIn = cur.w >= W_NEAR;
                const bool nxtIn = nxt.w >= W_NEAR;
                if (curIn) {
                    poly[np++] = cur;
                }
                if (curIn != nxtIn) {
                    const float tt = (W_NEAR - cur.w) / (nxt.w - cur.w);
                    poly[np++] = lerpCV(cur, nxt, tt);
                }
            }
            for (int i = 1; i + 1 < np; i++) {
                scratch.push_back(poly[0]);
                scratch.push_back(poly[i]);
                scratch.push_back(poly[i + 1]);
            }
        }
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, (u16)scratch.size());
        for (const LugxClipVert& c : scratch) {
            emitCV(c);
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

// --- on-screen fps counter (7-segment, drawn in EndFrame) ----------------------
//
// Rendered in-game (not via a host overlay) so it is visible on real hardware.
// Two green 7-segment digits over a dark box in the top-left, gated on config.ini.

// Segment bitmasks for 0-9. Bits: a(top) b(top-right) c(bottom-right) d(bottom)
// e(bottom-left) f(top-left) g(middle).
static const unsigned char kFpsSeg[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

// Framebuffer size for the pixel->NDC mapping, set per frame in lugx_draw_fps_overlay.
static float s_fpsFw = 640.0f, s_fpsFh = 480.0f;

// Draw a filled rect given in pixel space (0..fw across, 0..fh down), converting to
// NDC (-1..1, y up) fed through the identity projection set up by the caller. The
// pixel->NDC path (not GX_ORTHOGRAPHIC) is what actually works here: a loaded ortho
// projection was being overridden by the game's leftover perspective, placing the
// overlay through the last 3D object's transform instead of the screen.
static void lugx_fps_rect(float px, float py, float pw, float ph, u8 r, u8 g, u8 b) {
    const float x0 = px / (s_fpsFw * 0.5f) - 1.0f;
    const float x1 = (px + pw) / (s_fpsFw * 0.5f) - 1.0f;
    const float y0 = 1.0f - py / (s_fpsFh * 0.5f);
    const float y1 = 1.0f - (py + ph) / (s_fpsFh * 0.5f);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x0, y0, 0.0f);
    GX_Color4u8(r, g, b, 255);
    GX_Position3f32(x1, y0, 0.0f);
    GX_Color4u8(r, g, b, 255);
    GX_Position3f32(x1, y1, 0.0f);
    GX_Color4u8(r, g, b, 255);
    GX_Position3f32(x0, y1, 0.0f);
    GX_Color4u8(r, g, b, 255);
    GX_End();
}

static void lugx_fps_digit(int d, float x, float y, float dw, float dh, float t) {
    if (d < 0 || d > 9) {
        return;
    }
    const unsigned m = kFpsSeg[d];
    const float half = dh * 0.5f;
    const u8 R = 0, G = 255, B = 80; // bright green
    if (m & 0x01) lugx_fps_rect(x + t, y, dw - 2 * t, t, R, G, B);                   // a
    if (m & 0x02) lugx_fps_rect(x + dw - t, y + t, t, half - t, R, G, B);            // b
    if (m & 0x04) lugx_fps_rect(x + dw - t, y + half, t, half - t, R, G, B);         // c
    if (m & 0x08) lugx_fps_rect(x + t, y + dh - t, dw - 2 * t, t, R, G, B);          // d
    if (m & 0x10) lugx_fps_rect(x, y + half, t, half - t, R, G, B);                  // e
    if (m & 0x20) lugx_fps_rect(x, y + t, t, half - t, R, G, B);                     // f
    if (m & 0x40) lugx_fps_rect(x + t, y + half - t * 0.5f, dw - 2 * t, t, R, G, B); // g
}

// Render `digits` decimal digits of `val`, right-aligned so the rightmost digit's right
// edge sits at xr, on row y. Used for the profiler microsecond readouts.
static void lugx_fps_number(int val, int digits, float xr, float y, float dw, float dh, float t, float gap) {
    if (val < 0) {
        val = 0;
    }
    for (int i = 0; i < digits; i++) {
        int d = val % 10;
        val /= 10;
        lugx_fps_digit(d, xr - dw - (float)i * (dw + gap), y, dw, dh, t);
    }
}

static void lugx_draw_fps_overlay(int fps, int totalUs, int drawUs, int vtxUs, int triUs, int combUs, float fw,
                                  float fh) {
    if (fps < 0) {
        fps = 0;
    }
    if (fps > 99) {
        fps = 99;
    }
    // 2D screen-space state for flat quads (0..640 x 0..480, y down). No texture,
    // depth or blend. This is the last draw before the EFB copy, so it needn't
    // restore the interpreter's state (StartFrame/DrawTriangles re-set it next frame).
    // Force a full-screen viewport + scissor: the game's last draw leaves a sub /
    // decal-biased viewport, which would otherwise place and scale the overlay wrong.
    // Viewport + scissor are set full-screen by the caller (the window backend,
    // which knows the real framebuffer size); we load the matching ortho and draw.
    s_fpsFw = fw;
    s_fpsFh = fh;
    // Identity projection so vertices are fed directly in NDC (clip space), bypassing
    // any leftover game projection/posmtx (lugx_fps_rect maps pixel coords into NDC).
    Mtx44 proj;
    memset(proj, 0, sizeof(proj));
    proj[0][0] = 1.0f;
    proj[1][1] = 1.0f;
    proj[2][2] = 1.0f;
    proj[3][3] = 1.0f;
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
    Mtx mv;
    lugx_mtx_identity(mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);
    lugx_gx_invalidate_mtx_cache(); // overlay loaded its own pos-mtx + projection
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetNumTevStages(1);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetCullMode(GX_CULL_NONE);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    // Top-right readout over a dark box (pixel coords in a 640x480-style space):
    //   row 0 = fps (2 digits); row 1 = whole-frame render us; row 2 = draw-path us.
    // total - draw = the DL walk / dispatch overhead. All right-aligned at xr.
    // Rows: 0 fps | 1 whole-frame us | 2 draw-path us | 3 GfxSpVertex us.
    // walk (DL dispatch) = total - draw; the vtx row is a sub-cost inside the walk.
    const bool showFps = g_lugx_config.fps_counter;
    const bool showProf = g_lugx_config.debug_profiler;
    const float dw = 15.0f, dh = 24.0f, t = 4.0f, gap = 6.0f;
    const float xr = fw - 14.0f;
    const float y0 = 12.0f, y1 = 44.0f, y2 = 76.0f, y3 = 108.0f, y4 = 140.0f, y5 = 172.0f;
    const float boxL = xr - (dw * 5.0f + gap * 4.0f) - 5.0f;
    const float yTop = showFps ? y0 : y1;   // first visible row
    const float yBot = showProf ? y5 : y0;  // last visible row
    lugx_fps_rect(boxL, yTop - 5, xr - boxL + 5, (yBot + dh + 5) - (yTop - 5), 0, 0, 0); // dark box
    if (showFps) {
        lugx_fps_number(fps, 2, xr, y0, dw, dh, t, gap);
    }
    if (showProf) {
        lugx_fps_number(totalUs, 5, xr, y1, dw, dh, t, gap); // whole-frame render
        lugx_fps_number(drawUs, 5, xr, y2, dw, dh, t, gap);  // DrawTriangles (transform+emit+state)
        lugx_fps_number(vtxUs, 5, xr, y3, dw, dh, t, gap);   // GfxSpVertex
        lugx_fps_number(triUs, 5, xr, y4, dw, dh, t, gap);   // GfxSpTri1 whole (includes its draw)
        lugx_fps_number(combUs, 5, xr, y5, dw, dh, t, gap);  // combiner lookup
    }
}

// Measure the present rate and draw the fps overlay. Called from the window
// backend right before the EFB->XFB copy, so it lands on the final image (after
// any render-to-framebuffer resolve the game did).
void lugx_fps_overlay(float fbWidth, float fbHeight) {
    g_lugx_prof_enabled = g_lugx_config.debug_profiler ? 1 : 0; // gate the hot-path timers
    if (!g_lugx_config.fps_counter && !g_lugx_config.debug_profiler) {
        return;
    }
    static uint64_t lastTick = 0;
    static int frames = 0;
    static int fpsValue = 0;
    static int totalUs = 0, drawUs = 0, vtxUs = 0, triUs = 0, combUs = 0;
    const uint64_t now = gettime();
    if (lastTick == 0) {
        lastTick = now;
    }
    frames++;
    const uint64_t elapsed = ticks_to_microsecs(now - lastTick);
    if (elapsed >= 500000) {
        // Round to nearest (e.g. 29.97 -> 30) rather than truncating (-> 29).
        fpsValue = (int)(((uint64_t)frames * 1000000ull + elapsed / 2) / elapsed);
        // Average the profiler accumulators over the frames since the last refresh.
        uint32_t pf = g_lugx_prof_frames;
        if (pf > 0) {
            totalUs = (int)(ticks_to_microsecs(g_lugx_prof_total_ticks) / pf);
            drawUs = (int)(ticks_to_microsecs(g_lugx_prof_draw_ticks) / pf);
            vtxUs = (int)(ticks_to_microsecs(g_lugx_prof_vtx_ticks) / pf);
            triUs = (int)(ticks_to_microsecs(g_lugx_prof_tri_ticks) / pf);
            combUs = (int)(ticks_to_microsecs(g_lugx_prof_comb_ticks) / pf);
        }
        g_lugx_prof_total_ticks = 0;
        g_lugx_prof_draw_ticks = 0;
        g_lugx_prof_vtx_ticks = 0;
        g_lugx_prof_tri_ticks = 0;
        g_lugx_prof_comb_ticks = 0;
        g_lugx_prof_frames = 0;
        frames = 0;
        lastTick = now;
    }
    lugx_draw_fps_overlay(fpsValue, totalUs, drawUs, vtxUs, triUs, combUs, fbWidth, fbHeight);
}

void GfxRenderingAPIGX::StartFrame() {}

void GfxRenderingAPIGX::EndFrame() {
    // No GP sync here. The window backend's SwapBuffersEnd issues the fps overlay and
    // GX_CopyDisp, then a single GX_DrawDone that waits for the whole frame (geometry +
    // overlay + copy). A GX_DrawDone here would just stall the CPU waiting for the GP to
    // drain the game geometry before we even queue the copy, then block again - the GP
    // processes the FIFO in order, so one sync after the copy is sufficient.
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
    lugx_mtx_identity(ident);
    GX_LoadPosMtxImm(ident, GX_PNMTX0);
    lugx_gx_invalidate_mtx_cache(); // clear loaded its own pos-mtx + projection

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
