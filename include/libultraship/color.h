#pragma once

#include <stdint.h>
#include <ship/utils/color.h>

#ifdef __cplusplus
extern "C" {
#endif

// Packed 32-bit RGBA with both component and packed-integer access. Component
// order follows endianness; GameCube/Wii are big-endian, so IS_BIGENDIAN is
// defined by the build and the components are laid out r, g, b, a.
typedef union {
    struct {
#ifdef IS_BIGENDIAN
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
#else
        uint8_t a;
        uint8_t b;
        uint8_t g;
        uint8_t r;
#endif
    };
    uint32_t rgba;
} Color_RGBA8_u32;

// Single-precision RGBA, each component in [0.0, 1.0].
typedef struct {
    float r;
    float g;
    float b;
    float a;
} Color_RGBAf;

// 16-bit RGBA 5-5-5-1.
typedef union {
    struct {
        uint16_t r : 5;
        uint16_t g : 5;
        uint16_t b : 5;
        uint16_t a : 1;
    };
    uint16_t rgba;
} Color_RGBA16;

#ifdef __cplusplus
};
#endif
