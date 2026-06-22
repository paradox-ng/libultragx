#pragma once

#include <string>

namespace Ship {

// On-screen notifications/overlay. Inert no-op on libultragx (no ImGui).
class GameOverlay {
  public:
    GameOverlay() = default;
    virtual ~GameOverlay() = default;

    void Init() {}
    void Draw() {}
    void TextDrawNotification(float duration, bool centered, const char* fmt, ...) {
        (void)duration;
        (void)centered;
        (void)fmt;
    }
};

} // namespace Ship
