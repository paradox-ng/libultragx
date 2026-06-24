#include "gfx/gfx_gx_tev.h"

#include "fast/cc_features.h"

extern "C" int g_gx_stop_at; // BISECT: 10 = minimal hardcoded TEV (skip decoded config)

// ---------------------------------------------------------------------------
// CCFeatures -> GX TEV (the modern path).
//
// The interpreter describes the combiner as CCFeatures::c[cycle][rgb|a][A,B,C,D]
// where each value is a SHADER_* slot. SHADER_INPUT_1..6 are generic constant
// inputs whose resolved colours arrive in CombinerUniforms::inputs[0..5]; we
// allocate a GX TEV register (GX_TEVREG0/1/2) to each constant used and load it.
// SHADER_INPUT_7 is the per-vertex shade. The (A-B)*C+D -> d+(1-c)a+c*b mapping is
// the same as lugx_tev_from_combiner.
// ---------------------------------------------------------------------------

namespace {

// Assigns SHADER_INPUT_1..6 generic constants to GX TEV registers 0/1/2, reusing
// one register per distinct constant across all 8 slots of the cycle.
struct ConstRegs {
    int inputForReg[3] = { -1, -1, -1 }; // reg index -> SHADER_INPUT_n (1..6)
    int RegFor(int n) {
        for (int i = 0; i < 3; i++) {
            if (inputForReg[i] == n) return i;
        }
        for (int i = 0; i < 3; i++) {
            if (inputForReg[i] == -1) { inputForReg[i] = n; return i; }
        }
        return 0; // out of registers (>3 distinct constants in one cycle): clamp to reg0
    }
};

bool IsConst(int s) {
    return s >= SHADER_INPUT_1 && s <= SHADER_INPUT_6;
}

u8 ColorIn(int s, ConstRegs& regs) {
    switch (s) {
        case SHADER_0:        return GX_CC_ZERO;
        case SHADER_1:        return GX_CC_ONE;
        case SHADER_COMBINED: return GX_CC_CPREV;
        case SHADER_TEXEL0:   return GX_CC_TEXC;
        case SHADER_TEXEL0A:  return GX_CC_TEXA;
        case SHADER_TEXEL1:   return GX_CC_TEXC; // TODO: second texmap
        case SHADER_TEXEL1A:  return GX_CC_TEXA;
        case SHADER_INPUT_7:  return GX_CC_RASC; // shade
        default: break;
    }
    if (IsConst(s)) {
        switch (regs.RegFor(s)) {
            case 0: return GX_CC_C0;
            case 1: return GX_CC_C1;
            case 2: return GX_CC_C2;
        }
    }
    return GX_CC_ZERO;
}

u8 AlphaIn(int s, ConstRegs& regs) {
    switch (s) {
        case SHADER_0:        return GX_CA_ZERO;
        case SHADER_1:        return GX_CA_KONST; // konst alpha set to 1.0
        case SHADER_COMBINED: return GX_CA_APREV;
        case SHADER_TEXEL0:
        case SHADER_TEXEL0A:  return GX_CA_TEXA;
        case SHADER_TEXEL1:
        case SHADER_TEXEL1A:  return GX_CA_TEXA;
        case SHADER_INPUT_7:  return GX_CA_RASA; // shade alpha
        default: break;
    }
    if (IsConst(s)) {
        switch (regs.RegFor(s)) {
            case 0: return GX_CA_A0;
            case 1: return GX_CA_A1;
            case 2: return GX_CA_A2;
        }
    }
    return GX_CA_ZERO;
}

u8 F2U8(float f) {
    int v = (int)(f * 255.0f + 0.5f);
    return (u8)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

} // namespace

void lugx_tev_from_features(const CCFeatures* cc, const float inputs[6][4]) {
    GX_SetNumTevStages(1);
    GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1); // GX_CA_KONST == 1.0

    // One channel: the rasterized colour is the per-vertex shade (no HW lighting yet).
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);

    if (g_gx_stop_at == 10) { // BISECT: minimal valid TEV, skip decoded combiner config
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        return;
    }

    const int* col = cc->c[0][0]; // color A,B,C,D
    const int* alp = cc->c[0][1]; // alpha A,B,C,D
    ConstRegs regs;

    const bool tex = cc->usedTextures[0] || cc->usedTextures[1];
    GX_SetTevOrder(GX_TEVSTAGE0,
                   tex ? GX_TEXCOORD0 : GX_TEXCOORDNULL,
                   tex ? GX_TEXMAP0 : GX_TEXMAP_NULL,
                   GX_COLOR0A0);

    // Map the four colour slots (allocates const registers), then pick the GX form.
    const u8 cA = ColorIn(col[0], regs), cB = ColorIn(col[1], regs);
    const u8 cC = ColorIn(col[2], regs), cD = ColorIn(col[3], regs);
    if (col[1] == SHADER_0) {
        // A*C + D  ->  a=ZERO, b=A, c=C, d=D
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, cA, cC, cD);
    } else if (col[1] == col[3]) {
        // (A-B)*C + B = lerp(B,A,C)  ->  a=B, b=A, c=C, d=ZERO
        GX_SetTevColorIn(GX_TEVSTAGE0, cB, cA, cC, GX_CC_ZERO);
    } else {
        // TODO: B!=0 && D!=B needs a second stage; approximate as A*C+D.
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, cA, cC, cD);
    }
    GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    const u8 aA = AlphaIn(alp[0], regs), aB = AlphaIn(alp[1], regs);
    const u8 aC = AlphaIn(alp[2], regs), aD = AlphaIn(alp[3], regs);
    if (alp[1] == SHADER_0) {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, aA, aC, aD);
    } else if (alp[1] == alp[3]) {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, aB, aA, aC, GX_CA_ZERO);
    } else {
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, aA, aC, aD);
    }
    GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    // Load the resolved constant colours into the registers we allocated.
    for (int i = 0; i < 3; i++) {
        int n = regs.inputForReg[i];
        if (n < SHADER_INPUT_1) {
            continue;
        }
        const float* in = inputs[n - SHADER_INPUT_1]; // SHADER_INPUT_1 -> inputs[0]
        GXColor c = { F2U8(in[0]), F2U8(in[1]), F2U8(in[2]), F2U8(in[3]) };
        GX_SetTevColor((u8)(GX_TEVREG0 + i), c);
    }
}

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
