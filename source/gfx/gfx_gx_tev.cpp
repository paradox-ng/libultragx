#include "gfx/gfx_gx_tev.h"

// N64 combiner source -> GX TEV color input.
static u8 gx_cc(LugxCcSrc s) {
    switch (s) {
        case LUGX_CC_0:        return GX_CC_ZERO;
        case LUGX_CC_1:        return GX_CC_ONE;
        case LUGX_CC_COMBINED: return GX_CC_CPREV;
        case LUGX_CC_TEXEL0:   return GX_CC_TEXC;
        case LUGX_CC_TEXEL0_A: return GX_CC_TEXA;
        case LUGX_CC_SHADE:    return GX_CC_RASC;
        case LUGX_CC_SHADE_A:  return GX_CC_RASA;
        case LUGX_CC_PRIM:     return GX_CC_C0; // primitive color in TEV register 0
        case LUGX_CC_PRIM_A:   return GX_CC_A0;
        case LUGX_CC_ENV:      return GX_CC_C1; // environment color in TEV register 1
        case LUGX_CC_ENV_A:    return GX_CC_A1;
    }
    return GX_CC_ZERO;
}

// N64 combiner source -> GX TEV alpha input. In an alpha combiner every source is
// already a scalar, so the .._A variants and their colors map to the same input.
static u8 gx_ca(LugxCcSrc s) {
    switch (s) {
        case LUGX_CC_0:        return GX_CA_ZERO;
        case LUGX_CC_1:        return GX_CA_KONST; // konst alpha is set to 1.0 below
        case LUGX_CC_COMBINED: return GX_CA_APREV;
        case LUGX_CC_TEXEL0:
        case LUGX_CC_TEXEL0_A: return GX_CA_TEXA;
        case LUGX_CC_SHADE:
        case LUGX_CC_SHADE_A:  return GX_CA_RASA;
        case LUGX_CC_PRIM:
        case LUGX_CC_PRIM_A:   return GX_CA_A0;
        case LUGX_CC_ENV:
        case LUGX_CC_ENV_A:    return GX_CA_A1;
    }
    return GX_CA_ZERO;
}

static bool uses_texel(LugxCcSrc s) {
    return s == LUGX_CC_TEXEL0 || s == LUGX_CC_TEXEL0_A;
}

void lugx_tev_from_combiner(const LugxCombiner* cc) {
    GX_SetNumTevStages(1);
    GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1); // so GX_CA_KONST == 1.0

    bool tex = uses_texel(cc->cA) || uses_texel(cc->cB) || uses_texel(cc->cC) || uses_texel(cc->cD) ||
               uses_texel(cc->aA) || uses_texel(cc->aB) || uses_texel(cc->aC) || uses_texel(cc->aD);
    GX_SetTevOrder(GX_TEVSTAGE0,
                   tex ? GX_TEXCOORD0 : GX_TEXCOORDNULL,
                   tex ? GX_TEXMAP0 : GX_TEXMAP_NULL,
                   GX_COLOR0A0);

    // color = (A - B) * C + D
    if (cc->cB == LUGX_CC_0) {
        // A*C + D  ->  out = d + (1-c)*a + c*b  with a=ZERO, b=A, c=C, d=D
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, gx_cc(cc->cA), gx_cc(cc->cC), gx_cc(cc->cD));
    } else if (cc->cB == cc->cD) {
        // (A-B)*C + B = lerp(B,A,C)  ->  a=B, b=A, c=C, d=ZERO
        GX_SetTevColorIn(GX_TEVSTAGE0, gx_cc(cc->cB), gx_cc(cc->cA), gx_cc(cc->cC), GX_CC_ZERO);
    } else {
        // TODO: B!=0 and D!=B needs a second TEV stage; approximate as A*C+D.
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, gx_cc(cc->cA), gx_cc(cc->cC), gx_cc(cc->cD));
    }
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // alpha = (A - B) * C + D
    if (cc->aB == LUGX_CC_0) {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, gx_ca(cc->aA), gx_ca(cc->aC), gx_ca(cc->aD));
    } else if (cc->aB == cc->aD) {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, gx_ca(cc->aB), gx_ca(cc->aA), gx_ca(cc->aC), GX_CA_ZERO);
    } else {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, gx_ca(cc->aA), gx_ca(cc->aC), gx_ca(cc->aD));
    }
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}
