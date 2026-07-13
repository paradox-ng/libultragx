#pragma once

#include <fast/backends/gfx_rendering_api.h>

#include <unordered_map>
#include <vector>

#include "gfx/gfx_gx_tev.h"

namespace Fast {

// GX (GameCube/Wii) implementation of the Fast3D rendering backend. It wires the
// libultragx gfx_gx primitives (combiner -> TEV, RGBA32 -> GX texture, render
// state) under the Fast3D interpreter. Rendering goes direct to the screen
// (EFB -> XFB); the ImGui framebuffer-texture path is unused on console, so those
// methods are inert.
//
// Status: the state/texture/lifecycle surface is implemented over the verified
// primitives; the vertex-buffer parsing in DrawTriangles, the shader-id ->
// combiner decode, and render-to-texture framebuffers are marked TODO and are the
// remaining integration work.
class GfxRenderingAPIGX final : public GfxRenderingAPI {
  public:
    GfxRenderingAPIGX() = default;
    ~GfxRenderingAPIGX() override = default;

    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;

    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    void ClearShaderCache() override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint64_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint64_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;

    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetNextTexturePack(LugxTexPack pack) override;
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;

    void SetDepthTestAndMask(bool depth_test, bool z_upd) override;
    void SetCullMode(int8_t keepSign) override;
    void SetZmodeDecal(bool decal) override;
    void SetStrictDecal(bool on) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void SetCurrentPrimDepth(float depth) override;
    void SetSrgbMode() override;

    // Stores the resolved combiner constant colours for the next DrawTriangles.
    void SetCombinerUniforms(const CombinerUniforms& uniforms) override;
    // Stores the matrix palette (combined MVP per slot) for the next DrawTriangles.
    void SetTransformUniforms(const TransformUniforms& uniforms) override;
    // Stores the per-draw lights/ambient for GX hardware lighting (lit batches).
    void SetLightingUniforms(const LightingUniforms& uniforms) override;

    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override;

    // Bring-up self-test (piece 4): render one Gouraud-shaded triangle through the
    // real DrawTriangles + TEV path with the identity/ortho transform. No
    // interpreter involved; validates the combiner decode and GX vertex submission.
    void DrawBringupTriangle();

    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;

    // Framebuffers: console renders direct to screen; render-to-texture is TODO.
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    ImTextureID GetTextureById(int id) override;

  private:
    struct GxTexture {
        void* data = nullptr; // tiled GX data in main RAM (format per fmt)
        u32 dataBytes = 0;    // allocated size of data, so a same-size re-upload reuses it
        u32 width = 0;
        u32 height = 0;
        u8 fmt = GX_TF_RGBA8; // GX texel format the data is tiled in
        bool linearFilter = false;
        u8 wrapS = 0; // GX_REPEAT / GX_CLAMP / GX_MIRROR
        u8 wrapT = 0;
    };

    std::vector<GxTexture> mTextures;     // indexed by texture id
    uint32_t mTileTexture[2] = { 0, 0 };  // texture id bound to each tile
    int mCurrentTile = 0;
    LugxTexPack mNextPack = LugxTexPack::RGB5A3; // GX format for the next UploadTexture
    FilteringMode mFilterMode = FILTER_THREE_POINT;
    ShaderProgram* mCurrentShader = nullptr;
    std::unordered_map<uint64_t, ShaderProgram*> mShaderCache; // keyed by shaderId0 (id1 folded in)
    int8_t mCullKeepSign = 0;             // 0 none, +1 keep cross>0 (G_CULL_FRONT), -1 keep cross<0 (G_CULL_BACK)
    CombinerUniforms mCombinerUniforms{}; // latest resolved constants (prim/env/...)
    TransformUniforms mTransform{};       // latest matrix palette (combined MVP per slot)
    LightingUniforms mLighting{};         // latest lights/ambient for HW lighting
};

// Factory used by the window backend to create the GX rendering backend.
GfxRenderingAPI* lugx_create_gx_rendering_api();

} // namespace Fast
