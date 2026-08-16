// Wii Remote input behind a plain-C shim.
//
// <wiiuse/wpad.h> pulls in the GX `Mtx` (f32[3][4]), which collides with the N64 ABI `Mtx`
// that the OSContPad header brings along, so the two can never meet in one translation
// unit. This header describes the remote in neutral types only, so the controller layer
// reads it without ever seeing a libogc header. The implementation is the only place the
// two worlds touch, and it includes nothing from the N64 side.
//
// Buttons are named for the physical control rather than any N64 meaning, so the mapping
// policy stays in the controller layer where it can differ per game.

#ifndef LUGX_WPAD_H
#define LUGX_WPAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LUGX_WPAD_EXT_NONE = 0,    // remote alone: no analog stick, so unusable for this game
    LUGX_WPAD_EXT_NUNCHUK = 1, // remote + nunchuk: stick and Z arrive from the nunchuk
    LUGX_WPAD_EXT_CLASSIC = 2, // classic controller, including the Pro variants
};

#define LUGX_WPAD_A (1u << 0)
#define LUGX_WPAD_B (1u << 1)
#define LUGX_WPAD_X (1u << 2)  // remote 1, classic X
#define LUGX_WPAD_Y (1u << 3)  // remote 2, classic Y
#define LUGX_WPAD_L (1u << 4)  // classic only
#define LUGX_WPAD_R (1u << 5)  // classic only
#define LUGX_WPAD_ZL (1u << 6) // classic only
#define LUGX_WPAD_ZR (1u << 7) // classic only
#define LUGX_WPAD_START (1u << 8)
#define LUGX_WPAD_SELECT (1u << 9)
#define LUGX_WPAD_HOME (1u << 10)
#define LUGX_WPAD_UP (1u << 11)
#define LUGX_WPAD_DOWN (1u << 12)
#define LUGX_WPAD_LEFT (1u << 13)
#define LUGX_WPAD_RIGHT (1u << 14)
#define LUGX_WPAD_C (1u << 15) // nunchuk only
#define LUGX_WPAD_Z (1u << 16) // nunchuk only

typedef struct {
    uint32_t held;   // LUGX_WPAD_* bits currently held
    int32_t ext;     // LUGX_WPAD_EXT_*
    int8_t stick_x;  // primary analog stick, scaled to the N64's range
    int8_t stick_y;
    int8_t stick2_x; // classic right stick; zero on a nunchuk
    int8_t stick2_y;
} LugxWpadState;

// All four are safe to call on GameCube, where they do nothing and report no controller.
void lugx_wpad_init(void);
void lugx_wpad_scan(void);
// Fills `out` and returns non-zero when a remote is connected on that channel.
int lugx_wpad_read(int chan, LugxWpadState* out);

#ifdef __cplusplus
}
#endif

#endif // LUGX_WPAD_H
