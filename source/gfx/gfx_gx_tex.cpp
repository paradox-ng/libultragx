#include "gfx/gfx_gx_tex.h"

void lugx_tex_rgba32_to_gx_rgba8(const u8* src, u8* dst, u32 w, u32 h) {
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 4) {
        for (u32 tx = 0; tx < w; tx += 4) {
            // AR plane: alpha, red for the 16 texels of this tile.
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 4; x++) {
                    const u8* p = src + (((ty + y) * w) + (tx + x)) * 4;
                    dst[o++] = p[3]; // A
                    dst[o++] = p[0]; // R
                }
            }
            // GB plane: green, blue for the same 16 texels.
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 4; x++) {
                    const u8* p = src + (((ty + y) * w) + (tx + x)) * 4;
                    dst[o++] = p[1]; // G
                    dst[o++] = p[2]; // B
                }
            }
        }
    }
}
