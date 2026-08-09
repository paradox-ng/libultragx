#pragma once

#include <string>

namespace Ship {

// On-screen notifications/overlay. Inert no-op on libultragx (no ImGui).
class GameOverlay {
  public:
    // Overlay text needs a font atlas upstream; nothing is drawn here.
    void LoadFont(const std::string& name, float size, const std::string& path) {
        (void)name;
        (void)size;
        (void)path;
    }
    void SetCurrentFont(const std::string&) {}
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
