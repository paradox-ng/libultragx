// GameCube/Wii ControlDeck: read the PAD (and Wii remote on RVL) each frame and map
// it onto the N64 OSContPad the game consumes. libogc TU (no gbi/GX types here).

#include "libultraship/controller/controldeck/ControlDeck.h"

#include <cstring>
// NOTE: do NOT include <gccore.h>/<ogc/gu.h>/<wiiuse/wpad.h> here - their GX `Mtx`
// (f32[3][4]) collides with the N64 ABI `Mtx` pulled in via the OSContPad header.
// <ogc/pad.h> alone provides the GameCube PAD API without the GX matrix types.
// (GC pads read on Wii too; the Wii Remote needs WPAD, which has the same collision
// - that is a follow-up, e.g. isolate WPAD in its own TU behind a C shim.)
#include <ogc/pad.h>

namespace Ship {

void ControlDeck::Init(uint8_t* controllerBits) {
    mControllerBits = controllerBits;
    PAD_Init();
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

void ControlDeck::WriteToPad(void* pad) {
    PAD_ScanPads();
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

    out[0].button = b;
    out[0].stick_x = PAD_StickX(0);
    out[0].stick_y = PAD_StickY(0);

    if (out != mPads) {
        memcpy(mPads, out, sizeof(OSContPad) * MAXCONTROLLERS);
    }
}

} // namespace LUS
