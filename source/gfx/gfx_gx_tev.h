#pragma once

#include <gccore.h>

// N64 color-combiner -> GX TEV mapping (the core of gfx_gx).
//
// The N64 combiner evaluates (A - B) * C + D for color and, separately, for alpha,
// each slot selecting one of a small set of sources. GX's TEV stage computes
// out = D + ((1-C)*A + C*B) (add/sub, bias, scale, clamp). The common N64 forms
// map onto one TEV stage:
//   * B == 0:   (A)*C + D     -> GX in (a=ZERO, b=A, c=C, d=D)  -> D + A*C
//   * D == B:   (A-B)*C + B   -> GX in (a=B,    b=A, c=C, d=ZERO) -> lerp(B,A,C)
// The fully general B!=0, D!=B case needs a second stage; that is a TODO. Every
// combiner SM64/Ghostship uses falls into the two cases above.

// Combiner input sources (the subset the games use).
enum LugxCcSrc {
    LUGX_CC_0,
    LUGX_CC_1,
    LUGX_CC_COMBINED,
    LUGX_CC_TEXEL0,
    LUGX_CC_TEXEL0_A,
    LUGX_CC_SHADE,
    LUGX_CC_SHADE_A,
    LUGX_CC_PRIM,
    LUGX_CC_PRIM_A,
    LUGX_CC_ENV,
    LUGX_CC_ENV_A,
};

// A 1-cycle N64 combiner: color = (cA-cB)*cC+cD, alpha = (aA-aB)*aC+aD.
// Field order matches the 8 arguments of the gbi G_CC_* macros.
struct LugxCombiner {
    LugxCcSrc cA, cB, cC, cD; // color
    LugxCcSrc aA, aB, aC, aD; // alpha
};

// Configure GX TEV stage 0 to evaluate the given combiner. Caller must have the
// primitive color in GX_TEVREG0 and the environment color in GX_TEVREG1.
void lugx_tev_from_combiner(const LugxCombiner* cc);

// Configure the GX TEV stage(s) from the interpreter's decoded combiner features.
// `inputs` are the resolved constant-input colours (CombinerUniforms::inputs):
// generic SHADER_INPUT_1..6 constants are loaded into GX TEV registers here. This
// is the path DrawTriangles uses; lugx_tev_from_combiner above is the older
// explicit-source prototype.
struct CCFeatures;
void lugx_tev_from_features(const CCFeatures* cc, const float inputs[6][4]);
