#pragma once

#include <string>

#include <imgui.h>

#include "ship/window/gui/Gui.h"

// ImGui is stripped on console; a port supplies a type-only <imgui.h> stub on its
// include path (so ImVec4 is a complete type for callers that construct it). These
// GUI helpers are no-ops.

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
