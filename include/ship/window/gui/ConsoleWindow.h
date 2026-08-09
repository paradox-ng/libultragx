#pragma once

#include "ship/window/gui/GuiWindow.h"

namespace Ship {

// Developer console window. Inert no-op on libultragx (no ImGui); the command
// system itself lives in ship/debug/Console.h.
class ConsoleWindow : public GuiWindow {
  public:
    // Upstream clears the dev-console key bindings; there is no console UI here.
    void ClearBindings() {}
    using GuiWindow::GuiWindow;
    ~ConsoleWindow() override = default;

    void Draw() override {}
    void DrawElement() override {}

  protected:
    void InitElement() override {}
    void UpdateElement() override {}
};

} // namespace Ship
