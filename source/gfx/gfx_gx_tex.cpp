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

// Clamp a source (x,y) to the edge and return the RGBA32 texel. Padding texels
// (beyond w/h, up to the tile boundary) replicate the edge so filtering and GX's
// tile-aligned reads see sane data rather than garbage.
static inline const u8* lugx_clamp_texel(const u8* src, u32 x, u32 y, u32 w, u32 h) {
    if (x >= w) {
        x = w - 1;
    }
    if (y >= h) {
        y = h - 1;
    }
    return src + ((size_t)y * w + x) * 4;
}

void lugx_tex_rgba32_to_gx_ia8(const u8* src, u8* dst, u32 w, u32 h) {
    // GX_TF_IA8: 4x4 tiles, 16 bits/texel, big-endian [alpha:8][intensity:8].
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 4) {
        for (u32 tx = 0; tx < w; tx += 4) {
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 4; x++) {
                    const u8* p = lugx_clamp_texel(src, tx + x, ty + y, w, h);
                    dst[o++] = p[3]; // A (high byte)
                    dst[o++] = p[0]; // I (low byte) = R
                }
            }
        }
    }
}

void lugx_tex_rgba32_to_gx_ia4(const u8* src, u8* dst, u32 w, u32 h) {
    // GX_TF_IA4: 8x4 tiles, 8 bits/texel, [alpha:4][intensity:4].
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 4) {
        for (u32 tx = 0; tx < w; tx += 8) {
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 8; x++) {
                    const u8* p = lugx_clamp_texel(src, tx + x, ty + y, w, h);
                    u8 i4 = (u8)(p[0] >> 4);
                    u8 a4 = (u8)(p[3] >> 4);
                    dst[o++] = (u8)((a4 << 4) | i4);
                }
            }
        }
    }
}

void lugx_tex_rgba32_to_gx_i8(const u8* src, u8* dst, u32 w, u32 h) {
    // GX_TF_I8: 8x4 tiles, 8 bits/texel intensity (GX samples it as I,I,I,I).
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 4) {
        for (u32 tx = 0; tx < w; tx += 8) {
            for (u32 y = 0; y < 4; y++) {
                for (u32 x = 0; x < 8; x++) {
                    const u8* p = lugx_clamp_texel(src, tx + x, ty + y, w, h);
                    dst[o++] = p[0]; // I = R
                }
            }
        }
    }
}

void lugx_tex_rgba32_to_gx_i4(const u8* src, u8* dst, u32 w, u32 h) {
    // GX_TF_I4: 8x8 tiles, 4 bits/texel; two texels per byte, even x in the high nibble.
    u32 o = 0;
    for (u32 ty = 0; ty < h; ty += 8) {
        for (u32 tx = 0; tx < w; tx += 8) {
            for (u32 y = 0; y < 8; y++) {
                for (u32 x = 0; x < 8; x += 2) {
                    const u8* p0 = lugx_clamp_texel(src, tx + x, ty + y, w, h);
                    const u8* p1 = lugx_clamp_texel(src, tx + x + 1, ty + y, w, h);
                    dst[o++] = (u8)(((p0[0] >> 4) << 4) | (p1[0] >> 4));
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
