#pragma once

#include <gccore.h>

// First slice of the N64 color-combiner -> GX TEV mapping (the core of gfx_gx).
//
// The N64 RDP color combiner computes (A - B) * C + D from a small set of inputs
// (TEXEL0/1, SHADE, PRIM, ENV, ...). GX's TEV stages are a different fixed-function
// form (out = d + lerp(a, b, c), with bias/scale/clamp), so each N64 combiner mode
// becomes a specific TEV stage configuration. These are the most common modes,
// expressed directly; the full table (decoding Fast3D's combiner ids) builds on
// this. Call before GX_Begin.
enum LugxCombiner {
    LUGX_CC_SHADE,        // out = shade            (vertex color only; no texture)
    LUGX_CC_TEXTURE,      // out = texel            (texture replaces shade)
    LUGX_CC_MODULATE,     // out = texel * shade    (N64 G_CC_MODULATERGBA)
    LUGX_CC_MODULATE_ENV, // out = texel * env      (env-color tint; uses envColor)
};

// Configure GX TEV stage 0 for the given combiner. envColor is used by *_ENV modes.
void lugx_tev_setup(LugxCombiner mode, GXColor envColor);
