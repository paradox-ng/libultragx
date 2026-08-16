// GameCube/Wii ControlDeck: read the PAD (and Wii remote on RVL) each frame and map
// it onto the N64 OSContPad the game consumes. libogc TU (no gbi/GX types here).

#include "libultraship/controller/controldeck/ControlDeck.h"

#include <cstring>
// NOTE: do NOT include <gccore.h>/<ogc/gu.h>/<wiiuse/wpad.h> here - their GX `Mtx`
// (f32[3][4]) collides with the N64 ABI `Mtx` pulled in via the OSContPad header.
// <ogc/pad.h> alone provides the GameCube PAD API without the GX matrix types.
// The Wii Remote has the same collision, so it is read through a plain-C shim that keeps
// every libogc type on its own side of the link (see platform/lugx_wpad.h).
#include <ogc/pad.h>

#include "platform/lugx_wpad.h"

namespace Ship {

void ControlDeck::Init(uint8_t* controllerBits) {
    mControllerBits = controllerBits;
    PAD_Init();
    lugx_wpad_init(); // no-op on GameCube
    if (mControllerBits != nullptr) {
        *mControllerBits = 1; // one controller on port 0 for now
    }
}

uint8_t* ControlDeck::GetControllerBits() {
    return mControllerBits;
}

void ControlDeck::BlockGameInput(int32_t blockId) {
    mGameInputBlockers[blockId] = true;
}
void ControlDeck::UnblockGameInput(int32_t blockId) {
    mGameInputBlockers.erase(blockId);
}
bool ControlDeck::AllGameInputBlocked() {
    return !mGameInputBlockers.empty();
}
bool ControlDeck::GamepadGameInputBlocked() {
    return AllGameInputBlocked();
}
bool ControlDeck::KeyboardGameInputBlocked() {
    return AllGameInputBlocked();
}
bool ControlDeck::MouseGameInputBlocked() {
    return AllGameInputBlocked();
}

} // namespace Ship

namespace LUS {

ControlDeck::ControlDeck() {
    mPads = new OSContPad[MAXCONTROLLERS];
    memset(mPads, 0, sizeof(OSContPad) * MAXCONTROLLERS);
}

ControlDeck::ControlDeck(std::vector<uint16_t> validButtons) : ControlDeck() {
    (void)validButtons;
}

ControlDeck::~ControlDeck() {
    delete[] mPads;
}

OSContPad* ControlDeck::GetPads() {
    return mPads;
}

// Map a Wii Remote onto the N64 pad. The two supported shapes are treated separately
// because they have genuinely different control counts, not because of preference:
//
// Nunchuk: the stick and Z come from the nunchuk, which is the only reason the remote is
// usable here at all. The d-pad drives the C buttons rather than the N64 d-pad, matching
// what the GameCube C-stick does above, because on this hardware C is the camera and the
// N64 d-pad goes almost unused.
//
// Classic: close enough to an N64 pad to map straight across, so the d-pad stays the
// d-pad and the right stick takes the C buttons. Either shoulder trigger gives N64 Z,
// since which one feels right differs between the Classic and the Pro shell.
//
// A bare remote has no analog stick and only about four reachable buttons, so it is left
// unmapped rather than offered as a broken option.
static uint16_t lugx_wpad_to_n64(const LugxWpadState& w) {
    uint16_t b = 0;
    if (w.held & LUGX_WPAD_A) {
        b |= CONT_A;
    }
    if (w.held & LUGX_WPAD_B) {
        b |= CONT_B;
    }
    if (w.held & LUGX_WPAD_START) {
        b |= CONT_START;
    }

    if (w.ext == LUGX_WPAD_EXT_NUNCHUK) {
        if (w.held & LUGX_WPAD_Z) {
            b |= CONT_G; // N64 Z
        }
        if (w.held & LUGX_WPAD_C) {
            b |= CONT_R;
        }
        if (w.held & LUGX_WPAD_X) {
            b |= CONT_L;
        }
        if (w.held & LUGX_WPAD_UP) {
            b |= CONT_E; // C-up
        }
        if (w.held & LUGX_WPAD_DOWN) {
            b |= CONT_D; // C-down
        }
        if (w.held & LUGX_WPAD_LEFT) {
            b |= CONT_C; // C-left
        }
        if (w.held & LUGX_WPAD_RIGHT) {
            b |= CONT_F; // C-right
        }
    } else if (w.ext == LUGX_WPAD_EXT_CLASSIC) {
        if (w.held & (LUGX_WPAD_ZL | LUGX_WPAD_ZR)) {
            b |= CONT_G; // N64 Z
        }
        if (w.held & LUGX_WPAD_L) {
            b |= CONT_L;
        }
        if (w.held & LUGX_WPAD_R) {
            b |= CONT_R;
        }
        if (w.held & LUGX_WPAD_UP) {
            b |= CONT_UP;
        }
        if (w.held & LUGX_WPAD_DOWN) {
            b |= CONT_DOWN;
        }
        if (w.held & LUGX_WPAD_LEFT) {
            b |= CONT_LEFT;
        }
        if (w.held & LUGX_WPAD_RIGHT) {
            b |= CONT_RIGHT;
        }
        if (w.stick2_y > 40) {
            b |= CONT_E;
        }
        if (w.stick2_y < -40) {
            b |= CONT_D;
        }
        if (w.stick2_x < -40) {
            b |= CONT_C;
        }
        if (w.stick2_x > 40) {
            b |= CONT_F;
        }
    }
    return b;
}

void ControlDeck::WriteToPad(void* pad) {
    PAD_ScanPads();
    lugx_wpad_scan();
    OSContPad* out = pad != nullptr ? (OSContPad*)pad : mPads;
    for (int i = 0; i < MAXCONTROLLERS; i++) {
        out[i].button = 0;
        out[i].stick_x = 0;
        out[i].stick_y = 0;
        out[i].err_no = 0;
    }
    if (AllGameInputBlocked()) {
        if (out != mPads) {
            memcpy(mPads, out, sizeof(OSContPad) * MAXCONTROLLERS);
        }
        return;
    }

    // Port 0 = the GameCube pad. (Wii remote + extra ports are a follow-up.)
    const uint32_t held = PAD_ButtonsHeld(0);
    uint16_t b = 0;
    if (held & PAD_BUTTON_A) {
        b |= CONT_A;
    }
    if (held & PAD_BUTTON_B) {
        b |= CONT_B;
    }
    if (held & PAD_TRIGGER_Z) {
        b |= CONT_G; // N64 Z
    }
    if (held & PAD_BUTTON_START) {
        b |= CONT_START;
    }
    if (held & PAD_BUTTON_UP) {
        b |= CONT_UP;
    }
    if (held & PAD_BUTTON_DOWN) {
        b |= CONT_DOWN;
    }
    if (held & PAD_BUTTON_LEFT) {
        b |= CONT_LEFT;
    }
    if (held & PAD_BUTTON_RIGHT) {
        b |= CONT_RIGHT;
    }
    if (held & PAD_TRIGGER_L) {
        b |= CONT_L;
    }
    if (held & PAD_TRIGGER_R) {
        b |= CONT_R;
    }
    // N64 C-buttons from the GameCube C-stick.
    const int cx = PAD_SubStickX(0);
    const int cy = PAD_SubStickY(0);
    if (cy > 40) {
        b |= CONT_E; // C-up
    }
    if (cy < -40) {
        b |= CONT_D; // C-down
    }
    if (cx < -40) {
        b |= CONT_C; // C-left
    }
    if (cx > 40) {
        b |= CONT_F; // C-right
    }

    int8_t sx = PAD_StickX(0);
    int8_t sy = PAD_StickY(0);

    // Merge the remote in rather than choosing between the two, so a player can pick up
    // either controller at any moment without the game needing to be told which. The
    // buttons simply combine; for the stick, whichever is actually deflected wins, since
    // a disconnected or centred stick reads as zero and must not cancel the other one.
    LugxWpadState w;
    if (lugx_wpad_read(0, &w) != 0) {
        b |= lugx_wpad_to_n64(w);
        const int gcMag = (sx < 0 ? -sx : sx) + (sy < 0 ? -sy : sy);
        const int wpMag = (w.stick_x < 0 ? -w.stick_x : w.stick_x) + (w.stick_y < 0 ? -w.stick_y : w.stick_y);
        if (wpMag > gcMag) {
            sx = w.stick_x;
            sy = w.stick_y;
        }
    }

    out[0].button = b;
    out[0].stick_x = sx;
    out[0].stick_y = sy;

    if (out != mPads) {
        memcpy(mPads, out, sizeof(OSContPad) * MAXCONTROLLERS);
    }
}

} // namespace LUS
