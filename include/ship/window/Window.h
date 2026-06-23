#pragma once

#include <cstdint>

// Lean stand-in for libultraship's ship/window/Window.h. Upstream this drags in
// ImGui, the controller mapping headers and a full abstract Window class; on
// GameCube/Wii the only piece the Fast3D window-manager API needs is WindowRect,
// so that is all we expose here. (Our GX window backend owns VI/GX directly.)

namespace Ship {

struct WindowRect {
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
};

} // namespace Ship
