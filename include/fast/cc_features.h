#pragma once

#include <cstdint>

// Shared, gbi-free description of a decoded N64 color combiner, extracted from
// the Fast3D interpreter so the GX rendering backend can consume it.
//
// The backend is a libogc gx.h translation unit and cannot include
// interpreter.h: interpreter.h pulls fast/types.h, whose N64 `Mtx` collides with
// GX's `Mtx`. This header is plain POD plus one function declaration, so both the
// interpreter (which defines gfx_cc_get_features) and the GX backend include it,
// and the backend calls gfx_cc_get_features across the link with POD arguments.

// Combiner input slots: the value stored in each CCFeatures::c[cycle][rgb/a][k].
// SHADER_INPUT_1..6 are the generic constant inputs (resolved to actual colours
// in CombinerUniforms::inputs[0..5]); SHADER_INPUT_7 is the per-vertex shade.
enum {
    SHADER_0,
    SHADER_INPUT_1,
    SHADER_INPUT_2,
    SHADER_INPUT_3,
    SHADER_INPUT_4,
    SHADER_INPUT_5,
    SHADER_INPUT_6,
    SHADER_INPUT_7,
    SHADER_TEXEL0,
    SHADER_TEXEL0A,
    SHADER_TEXEL1,
    SHADER_TEXEL1A,
    SHADER_1,
    SHADER_COMBINED,
    SHADER_NOISE,
    SHADER_LOD_FRAC
};

struct CCFeatures {
    int c[2][2][4];
    bool opt_alpha;
    bool opt_fog;
    bool opt_texture_edge;
    bool opt_noise;
    bool opt_2cyc;
    bool opt_alpha_threshold;
    bool opt_invisible;
    bool opt_grayscale;
    bool opt_prim_depth;
    bool opt_tex_lod;   // LOD_FRACTION computed from per-pixel UV derivatives
    bool opt_mip_lod;   // TEXEL0 carries a real mip pyramid; TEXEL1 = next mip level
    bool uses_lod_frac; // any combiner slot references SHADER_LOD_FRAC
    bool opt_shade;     // combiner reads the per-vertex shade color (SHADER_INPUT_7)
    bool opt_lighting;  // shade computed in the vertex shader from normals + lights
    bool opt_point_lighting;
    bool opt_texgen;
    bool opt_texgen_linear;
    bool usedTextures[2];
    bool used_palette[2]; // texel is a CI index texture; palette lookup in the shader
    bool used_masks[2];
    bool used_blend[2];
    bool clamp[2][2];
    int numInputs;
    bool do_single[2][2];
    bool do_multiply[2][2];
    bool do_mix[2][2];
    bool color_alpha_same[2];
    int16_t shader_id;
};

void gfx_cc_get_features(uint64_t shader_id0, uint64_t shader_id1, struct CCFeatures* cc_features);
