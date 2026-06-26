#include "gfx/gfx_gx_tex.h"

// RGBA8888 -> GX_TF_RGB5A3 (16-bit). Halves texture RAM and GP texture bandwidth
// versus RGBA8. Lossless for the N64's dominant RGBA16 (5551) textures, which are
// already 5-bit colour; lossy only for true RGBA32 sources (5-bit colour, 3-bit
// alpha). Per texel: if alpha is essentially opaque, store RGB555 (top bit 1);
// otherwise store 3-bit alpha + RGB444 (top bit 0).
static inline u16 lugx_rgba8_to_rgb5a3(u8 r, u8 g, u8 b, u8 a) {
    if (a >= 0xE0) {
        return (u16)(0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    }
    return (u16)(((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
}

void lugx_tex_rgba32_to_gx_rgb5a3(const u8* src, u8* dst, u32 w, u32 h) {
    // GX_TF_RGB5A3 tiles are 4x4, 16 bits/texel, row-major within the tile, stored
    // big-endian (high byte first). Iterate over padded (tile-aligned) dimensions and
    // clamp source reads to the edge so non-multiple-of-4 textures (e.g. a 256x1
    // palette) neither over-read the source nor under-fill the padded tile.
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 4) {
        for (u32 tx = 0; tx < w; tx += 4) {
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 4; x++) {
                    u32 sy = (ty + y < h) ? (ty + y) : (h - 1);
                    u32 sx = (tx + x < w) ? (tx + x) : (w - 1);
                    const u8* p = src + ((sy * w) + sx) * 4;
                    u16 v = lugx_rgba8_to_rgb5a3(p[0], p[1], p[2], p[3]);
                    dst[o++] = (u8)(v >> 8);
                    dst[o++] = (u8)(v & 0xFF);
                }
            }
        }
    }
}

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
