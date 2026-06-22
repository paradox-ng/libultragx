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
