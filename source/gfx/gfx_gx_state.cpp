#include "gfx/gfx_gx_state.h"

void lugx_set_depth(bool test, bool write) {
    GX_SetZMode(test ? GX_TRUE : GX_FALSE, GX_LEQUAL, write ? GX_TRUE : GX_FALSE);
}

void lugx_set_alpha_test(bool on, u8 threshold) {
    if (on) {
        // Keep pixels with alpha > threshold. Move the z compare after the alpha
        // test (GX_SetZCompLoc false) so discarded pixels do not write depth.
        GX_SetAlphaCompare(GX_GREATER, threshold, GX_AOP_OR, GX_GREATER, threshold);
        GX_SetZCompLoc(GX_FALSE);
    } else {
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetZCompLoc(GX_TRUE);
    }
}

void lugx_set_blend(bool on) {
    if (on) {
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    } else {
        GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    }
}
