#pragma once

#include <string>
#include "ship/window/gui/GuiElement.h"

namespace Ship {

// A named GUI window. Upstream this wraps an ImGui Begin/End pair; on libultragx
// it is an inert shell (no ImGui) that keeps the name + visibility-CVar API so the
// games' window subclasses still link. Subclasses implement the (no-op) element
// hooks. The ImVec2 size / window-flag constructors are dropped since nothing
// renders.
class GuiWindow : public GuiElement {
  public:
    GuiWindow() = default;
    GuiWindow(const std::string& consoleVariable, bool isVisible, const std::string& name);
    GuiWindow(const std::string& consoleVariable, const std::string& name);

    void Draw() override;
    std::string GetName();

  protected:
    void SetVisibility(bool visible) override;
    void SyncVisibilityConsoleVariable();

  private:
    std::string mName;
    std::string mVisibilityConsoleVariable;
};

} // namespace Ship
