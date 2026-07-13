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

// RGBA32 -> GX intensity / intensity-alpha formats. The interpreter decodes N64 I
// and IA textures into RGBA32 with R=G=B=intensity (and A=intensity for the I
// formats), so intensity is read from R and alpha from A. These reproduce those
// sources at a fraction of RGB5A3's size and are the reason to keep the N64 format
// info alive to the backend. dst must be 32-byte aligned; the caller sizes it for
// the format's tile-aligned dimensions (see the block sizes below) and
// DCFlushRange()s it. GX derives the tiling from (width, height, format), so the
// packed block order here matches it: blocks row-major, texels row-major in-block.
void lugx_tex_rgba32_to_gx_ia8(const u8* src, u8* dst, u32 w, u32 h); // 16bpp, 4x4 tiles
void lugx_tex_rgba32_to_gx_ia4(const u8* src, u8* dst, u32 w, u32 h); // 8bpp,  8x4 tiles
void lugx_tex_rgba32_to_gx_i8(const u8* src, u8* dst, u32 w, u32 h);  // 8bpp,  8x4 tiles
void lugx_tex_rgba32_to_gx_i4(const u8* src, u8* dst, u32 w, u32 h);  // 4bpp,  8x8 tiles
