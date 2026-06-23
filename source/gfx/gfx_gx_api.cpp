#include "fast/backends/gfx_gx.h"
#include "fast/cc_features.h"

#include "gfx/gfx_gx_tex.h"
#include "gfx/gfx_gx_state.h"

#include <gccore.h>
#include <malloc.h>
#include <cstring>

namespace Fast {

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

// --- draw ----------------------------------------------------------------------

void GfxRenderingAPIGX::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    (void)buf_vbo;
    (void)buf_vbo_len;
    (void)buf_vbo_num_tris;
    // TODO: the interpreter packs an interleaved float vertex buffer whose stride
    // depends on the loaded shader (position[4], then optional rgba, one or two
    // texcoord sets, fog, grayscale). Read the stride from mCurrentShader, bind
    // mTileTexture[] textures, set the GX vertex descriptor/format to match, and
    // submit GX_Begin(GX_TRIANGLES, ...) ... GX_End for buf_vbo_num_tris triangles.
}

// --- frame lifecycle -----------------------------------------------------------

void GfxRenderingAPIGX::Init() {
    // GX itself is initialized by the window backend; the interpreter sets render
    // state per draw. TODO: establish default vertex attribute formats here.
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
