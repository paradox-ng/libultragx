#pragma once

#include <gccore.h>

// Render-state pieces of gfx_gx: the N64 render-mode bits (z-buffer, alpha
// compare, blending) mapped onto GX. The N64 blender does standard src-alpha
// transparency, and alpha compare discards pixels below a threshold (the common
// G_AC_THRESHOLD path used for cutout textures).

// Z-buffer: enable depth test and/or depth write.
void lugx_set_depth(bool test, bool write);

// Alpha compare: when on, a pixel is kept only if its alpha > threshold (and the
// z compare moves after the alpha test so discarded pixels do not write depth).
void lugx_set_alpha_test(bool on, u8 threshold);

// Blending: standard source-alpha transparency (src*a + dst*(1-a)) when on.
void lugx_set_blend(bool on);
