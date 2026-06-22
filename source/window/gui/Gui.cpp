#include "ship/window/gui/Gui.h"

namespace Ship {

Gui::Gui(std::vector<std::shared_ptr<GuiWindow>> guiWindows) {
    for (auto& w : guiWindows) {
        AddGuiWindow(w);
    }
}

Gui::~Gui() = default;

void Gui::Init() {}
void Gui::StartDraw() {}
void Gui::EndDraw() {}
void Gui::SaveConsoleVariablesNextFrame() {}

bool Gui::SupportsViewports() {
    return false;
}

uint32_t Gui::GetMainGameWindowID() {
    return 0;
}

void Gui::AddGuiWindow(std::shared_ptr<GuiWindow> guiWindow) {
    if (guiWindow) {
        mGuiWindows[guiWindow->GetName()] = guiWindow;
    }
}

std::shared_ptr<GuiWindow> Gui::GetGuiWindow(const std::string& name) {
    auto it = mGuiWindows.find(name);
    return it != mGuiWindows.end() ? it->second : nullptr;
}

void Gui::RemoveGuiWindow(std::shared_ptr<GuiWindow> guiWindow) {
    if (guiWindow) {
        mGuiWindows.erase(guiWindow->GetName());
    }
}

void Gui::RemoveGuiWindow(const std::string& name) {
    mGuiWindows.erase(name);
}

void Gui::RemoveAllGuiWindows() {
    mGuiWindows.clear();
}

std::shared_ptr<GameOverlay> Gui::GetGameOverlay() {
    return mGameOverlay;
}

void Gui::SetMenuBar(std::shared_ptr<GuiMenuBar> menuBar) {
    mMenuBar = menuBar;
}

std::shared_ptr<GuiMenuBar> Gui::GetMenuBar() {
    return mMenuBar;
}

void Gui::SetMenu(std::shared_ptr<GuiWindow> menu) {
    mMenu = menu;
}

std::shared_ptr<GuiWindow> Gui::GetMenu() {
    return mMenu;
}

bool Gui::GetMenuOrMenubarVisible() {
    return false;
}

bool Gui::IsMouseOverAnyGuiItem() {
    return false;
}

bool Gui::IsMouseOverActivePopup() {
    return false;
}

bool Gui::GamepadNavigationEnabled() {
    return false;
}

void Gui::BlockGamepadNavigation() {}
void Gui::UnblockGamepadNavigation() {}

} // namespace Ship
