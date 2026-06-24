#pragma once

#include <string>

#include "ship/window/gui/Gui.h"

// ImVec4 is an ImGui type; libultragx strips ImGui, so only the name is needed for
// these (no-op) signatures. A port that includes this builds against its own ImGui
// stub for the full type.
struct ImVec4;

namespace Fast {
class Texture;

// The Fast3D GUI overlay (ImGui-based HUD/debug tooling) is not built on console.
// This type exists so a port's GUI calls compile and link: GetGui() returns the
// base Ship::Gui, ports static_pointer_cast it to Fast3dGui, and the texture-load
// helpers below do nothing.
class Fast3dGui : public Ship::Gui {
  public:
    void LoadGuiTexture(const std::string& /*name*/, const std::string& /*path*/, const ImVec4& /*tint*/) {}
    void LoadGuiTexture(const std::string& /*name*/, const Texture& /*tex*/, const ImVec4& /*tint*/) {}
};

} // namespace Fast
