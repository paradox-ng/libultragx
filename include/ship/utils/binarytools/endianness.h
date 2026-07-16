#pragma once

#ifdef __cplusplus
namespace Ship {
// Byte order for multi-byte reads/writes. Native resolves at compile time to the
// target's order; on GameCube/Wii (big-endian PowerPC) that is Big.
enum class Endianness {
    Little = 0,
    Big = 1,
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || defined(__BIG_ENDIAN__)
    Native = Big,
#else
    Native = Little,
#endif
};
} // namespace Ship
#endif

// Define IS_BIGENDIAN when the compiler reports a big-endian target (our case).
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || defined(__BIG_ENDIAN__)
#ifndef IS_BIGENDIAN
#define IS_BIGENDIAN
#endif
#endif

#define BSWAP16 __builtin_bswap16
#define BSWAP32 __builtin_bswap32
#define BSWAP64 __builtin_bswap64

// Swap helpers named by the stored byte order. On a big-endian target, reading
// big-endian data is a no-op and little-endian data is byte-swapped.
#ifdef IS_BIGENDIAN
#define BE16SWAP(x) (x)
#define BE32SWAP(x) (x)
#define BE64SWAP(x) (x)
#define LE16SWAP(x) BSWAP16(x)
#define LE32SWAP(x) BSWAP32(x)
#define LE64SWAP(x) BSWAP64(x)
#else
#define BE16SWAP(x) BSWAP16(x)
#define BE32SWAP(x) BSWAP32(x)
#define BE64SWAP(x) BSWAP64(x)
#define LE16SWAP(x) (x)
#define LE32SWAP(x) (x)
#define LE64SWAP(x) (x)
#endif

// Constant-expression swaps. The __builtin_bswap* forms above are not usable in every
// static-initializer context (some games byte-swap constants at file scope), so these
// use pure arithmetic and are valid constant expressions. Named by stored byte order
// like the runtime helpers: on a big-endian target BE*_CONST is a no-op.
#define BSWAP16_CONST(x) ((((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00))
#define BSWAP32_CONST(x)                                                                                                \
    ((((x) >> 24) & 0x000000FF) | (((x) >> 8) & 0x0000FF00) | (((x) << 8) & 0x00FF0000) | (((x) << 24) & 0xFF000000))
#define BSWAP64_CONST(x)                                                                                                \
    ((((x) >> 56) & 0x00000000000000FFULL) | (((x) >> 40) & 0x000000000000FF00ULL) |                                   \
     (((x) >> 24) & 0x0000000000FF0000ULL) | (((x) >> 8) & 0x00000000FF000000ULL) |                                    \
     (((x) << 8) & 0x000000FF00000000ULL) | (((x) << 24) & 0x0000FF0000000000ULL) |                                    \
     (((x) << 40) & 0x00FF000000000000ULL) | (((x) << 56) & 0xFF00000000000000ULL))

#ifdef IS_BIGENDIAN
#define BE16SWAP_CONST(x) (x)
#define BE32SWAP_CONST(x) (x)
#define BE64SWAP_CONST(x) (x)
#define LE16SWAP_CONST(x) BSWAP16_CONST(x)
#define LE32SWAP_CONST(x) BSWAP32_CONST(x)
#define LE64SWAP_CONST(x) BSWAP64_CONST(x)
#else
#define BE16SWAP_CONST(x) BSWAP16_CONST(x)
#define BE32SWAP_CONST(x) BSWAP32_CONST(x)
#define BE64SWAP_CONST(x) BSWAP64_CONST(x)
#define LE16SWAP_CONST(x) (x)
#define LE32SWAP_CONST(x) (x)
#define LE64SWAP_CONST(x) (x)
#endif
