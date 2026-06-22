#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 8-bit RGB without alpha. Each component is [0, 255].
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color_RGB8;

// 8-bit RGBA. Alpha 0 = transparent, 255 = opaque.
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Color_RGBA8;

#ifdef __cplusplus
};
#endif
