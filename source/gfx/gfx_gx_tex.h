#pragma once

#include <gccore.h>

// Texture conversion for gfx_gx.
//
// The Fast3D interpreter decodes every N64 texture format (RGBA16/32, CI4/8, IA,
// I, ...) into a flat 32-bit RGBA buffer before handing it to the rendering
// backend, so the backend only has to convert linear RGBA32 into the format GX
// samples. GX's general 32-bit format is GX_TF_RGBA8: 4x4 texel tiles where each
// tile is a 32-byte AR plane (a,r per texel) followed by a 32-byte GB plane.
//
// dst must be 32-byte aligned and w*h*4 bytes; w and h must be multiples of 4.
// The caller is responsible for DCFlushRange(dst, ...) before GX reads it.
void lugx_tex_rgba32_to_gx_rgba8(const u8* src, u8* dst, u32 w, u32 h);

// RGBA32 -> GX_TF_RGB5A3 (16-bit): half the RAM and GP bandwidth of RGBA8,
// lossless for RGBA16 sources. dst must be 32-byte aligned and w*h*2 bytes.
void lugx_tex_rgba32_to_gx_rgb5a3(const u8* src, u8* dst, u32 w, u32 h);
