#pragma once

#include "ship/window/gui/GuiWindow.h"

namespace Ship {

// Performance stats overlay window. Inert no-op on libultragx (no ImGui).
class StatsWindow : public GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    ~StatsWindow() override = default;

    void DrawElement() override {}

  protected:
    void InitElement() override {}
    void UpdateElement() override {}
};

} // namespace Ship
