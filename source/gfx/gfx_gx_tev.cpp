#include "gfx/gfx_gx_tev.h"

void lugx_tev_setup(LugxCombiner mode, GXColor envColor) {
    GX_SetNumTevStages(1);

    switch (mode) {
        case LUGX_CC_SHADE:
            // out = rasterized vertex color; texture not sampled.
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
            GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
            break;

        case LUGX_CC_TEXTURE:
            // out = texel; shade ignored.
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
            GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
            break;

        case LUGX_CC_MODULATE:
            // out = texel * shade  (the N64 G_CC_MODULATERGBA combiner).
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
            GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
            break;

        case LUGX_CC_MODULATE_ENV:
            // out = texel * env. The env color goes in TEV register 0; the stage
            // computes c*b = C0 * TEXC  (a = ZERO, b = TEXC, c = C0, d = ZERO).
            GX_SetTevColor(GX_TEVREG0, envColor);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
            GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
            GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
            GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);
            GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
            break;
    }
}
