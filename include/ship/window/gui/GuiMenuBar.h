#pragma once

#include <string>
#include "ship/window/gui/GuiElement.h"

namespace Ship {

// Top-of-screen menu bar. Inert no-op on libultragx (no ImGui).
class GuiMenuBar : public GuiElement {
  public:
    GuiMenuBar() = default;
    GuiMenuBar(const std::string& consoleVariable, bool isVisible) : GuiElement(isVisible) {
        (void)consoleVariable;
    }

    void Draw() override {}
    void DrawElement() override {}

  protected:
    void InitElement() override {}
    void UpdateElement() override {}
};

} // namespace Ship
