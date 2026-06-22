#include "ship/window/gui/GuiWindow.h"

namespace Ship {

GuiWindow::GuiWindow(const std::string& consoleVariable, bool isVisible, const std::string& name)
    : GuiElement(isVisible), mName(name), mVisibilityConsoleVariable(consoleVariable) {}

GuiWindow::GuiWindow(const std::string& consoleVariable, const std::string& name)
    : GuiElement(false), mName(name), mVisibilityConsoleVariable(consoleVariable) {}

void GuiWindow::Draw() {
    // Nothing renders on libultragx; the element hooks stay no-ops.
}

std::string GuiWindow::GetName() {
    return mName;
}

void GuiWindow::SetVisibility(bool visible) {
    GuiElement::SetVisibility(visible);
}

void GuiWindow::SyncVisibilityConsoleVariable() {
    // No-op: visibility is meaningless without a GUI to show/hide.
}

} // namespace Ship
