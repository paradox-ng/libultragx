// Wii Remote reading, isolated so libogc's GX types never reach the controller layer.
// See lugx_wpad.h for why this separation is mandatory rather than tidy.

#include "lugx_wpad.h"

#include <string.h>

#ifdef HW_RVL

#include <math.h>
#include <wiiuse/wpad.h>

// The N64 stick reads to roughly this at full deflection, and the GameCube pad libogc
// reports lands in the same range, so both controller kinds feed the game the same scale.
#define LUGX_STICK_RANGE 80.0f

// wiiuse reports a stick as an angle in degrees with zero pointing up, plus a magnitude
// that is nominally 0..1 but can overshoot slightly at the corners, so it is clamped
// before use rather than trusted.
static void lugx_wpad_stick(const joystick_t* js, int8_t* out_x, int8_t* out_y) {
    float mag = js->mag;
    if (mag > 1.0f) {
        mag = 1.0f;
    } else if (mag < 0.0f) {
        mag = 0.0f;
    }
    const float rad = js->ang * (float)M_PI / 180.0f;
    *out_x = (int8_t)(sinf(rad) * mag * LUGX_STICK_RANGE);
    *out_y = (int8_t)(cosf(rad) * mag * LUGX_STICK_RANGE);
}

void lugx_wpad_init(void) {
    WPAD_Init();
    // Buttons and expansion data only: the pointer and motion data cost bandwidth and
    // nothing here uses them.
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS);
}

void lugx_wpad_scan(void) {
    WPAD_ScanPads();
}

int lugx_wpad_read(int chan, LugxWpadState* out) {
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    uint32_t type = 0;
    if (WPAD_Probe(chan, &type) != WPAD_ERR_NONE) {
        return 0;
    }

    const uint32_t held = WPAD_ButtonsHeld(chan);
    uint32_t b = 0;

    // The remote's own buttons. With a nunchuk attached the remote is held upright, which
    // is the orientation these d-pad constants already describe, so no rotation is needed.
    if (held & WPAD_BUTTON_A) {
        b |= LUGX_WPAD_A;
    }
    if (held & WPAD_BUTTON_B) {
        b |= LUGX_WPAD_B;
    }
    if (held & WPAD_BUTTON_1) {
        b |= LUGX_WPAD_X;
    }
    if (held & WPAD_BUTTON_2) {
        b |= LUGX_WPAD_Y;
    }
    if (held & WPAD_BUTTON_PLUS) {
        b |= LUGX_WPAD_START;
    }
    if (held & WPAD_BUTTON_MINUS) {
        b |= LUGX_WPAD_SELECT;
    }
    if (held & WPAD_BUTTON_HOME) {
        b |= LUGX_WPAD_HOME;
    }
    if (held & WPAD_BUTTON_UP) {
        b |= LUGX_WPAD_UP;
    }
    if (held & WPAD_BUTTON_DOWN) {
        b |= LUGX_WPAD_DOWN;
    }
    if (held & WPAD_BUTTON_LEFT) {
        b |= LUGX_WPAD_LEFT;
    }
    if (held & WPAD_BUTTON_RIGHT) {
        b |= LUGX_WPAD_RIGHT;
    }

    expansion_t exp;
    memset(&exp, 0, sizeof(exp));
    WPAD_Expansion(chan, &exp);

    if (type == WPAD_EXP_NUNCHUK) {
        out->ext = LUGX_WPAD_EXT_NUNCHUK;
        if (held & WPAD_NUNCHUK_BUTTON_C) {
            b |= LUGX_WPAD_C;
        }
        if (held & WPAD_NUNCHUK_BUTTON_Z) {
            b |= LUGX_WPAD_Z;
        }
        lugx_wpad_stick(&exp.nunchuk.js, &out->stick_x, &out->stick_y);
    } else if (type == WPAD_EXP_CLASSIC) {
        out->ext = LUGX_WPAD_EXT_CLASSIC;
        // The classic controller reports its own buttons in the same held word, and its
        // face buttons are distinct constants from the remote's, so they are read here
        // rather than folded in above.
        if (held & WPAD_CLASSIC_BUTTON_A) {
            b |= LUGX_WPAD_A;
        }
        if (held & WPAD_CLASSIC_BUTTON_B) {
            b |= LUGX_WPAD_B;
        }
        if (held & WPAD_CLASSIC_BUTTON_X) {
            b |= LUGX_WPAD_X;
        }
        if (held & WPAD_CLASSIC_BUTTON_Y) {
            b |= LUGX_WPAD_Y;
        }
        if (held & WPAD_CLASSIC_BUTTON_FULL_L) {
            b |= LUGX_WPAD_L;
        }
        if (held & WPAD_CLASSIC_BUTTON_FULL_R) {
            b |= LUGX_WPAD_R;
        }
        if (held & WPAD_CLASSIC_BUTTON_ZL) {
            b |= LUGX_WPAD_ZL;
        }
        if (held & WPAD_CLASSIC_BUTTON_ZR) {
            b |= LUGX_WPAD_ZR;
        }
        if (held & WPAD_CLASSIC_BUTTON_PLUS) {
            b |= LUGX_WPAD_START;
        }
        if (held & WPAD_CLASSIC_BUTTON_MINUS) {
            b |= LUGX_WPAD_SELECT;
        }
        if (held & WPAD_CLASSIC_BUTTON_HOME) {
            b |= LUGX_WPAD_HOME;
        }
        if (held & WPAD_CLASSIC_BUTTON_UP) {
            b |= LUGX_WPAD_UP;
        }
        if (held & WPAD_CLASSIC_BUTTON_DOWN) {
            b |= LUGX_WPAD_DOWN;
        }
        if (held & WPAD_CLASSIC_BUTTON_LEFT) {
            b |= LUGX_WPAD_LEFT;
        }
        if (held & WPAD_CLASSIC_BUTTON_RIGHT) {
            b |= LUGX_WPAD_RIGHT;
        }
        lugx_wpad_stick(&exp.classic.ljs, &out->stick_x, &out->stick_y);
        lugx_wpad_stick(&exp.classic.rjs, &out->stick2_x, &out->stick2_y);
    } else {
        out->ext = LUGX_WPAD_EXT_NONE;
    }

    out->held = b;
    return 1;
}

#else // GameCube: there is no remote, so the shim reports nothing and costs nothing.

void lugx_wpad_init(void) {
}

void lugx_wpad_scan(void) {
}

int lugx_wpad_read(int chan, LugxWpadState* out) {
    (void)chan;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return 0;
}

#endif // HW_RVL
