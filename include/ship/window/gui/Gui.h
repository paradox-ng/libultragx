#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ship/window/gui/GuiWindow.h"
#include "ship/window/gui/GameOverlay.h"
#include "ship/window/gui/GuiMenuBar.h"

namespace Ship {
class Window;

// Lean no-op GUI manager. Registered windows/menus are stored for API
// compatibility but nothing is ever rendered (libultragx ships no ImGui), so all
// draw entry points and queries are inert.
class Gui {
  public:
    Gui() = default;
    Gui(std::vector<std::shared_ptr<GuiWindow>> guiWindows);
    virtual ~Gui();

    void Init();
    void StartDraw();
    void EndDraw();
    void SaveConsoleVariablesNextFrame();
    virtual bool SupportsViewports();
    uint32_t GetMainGameWindowID();

    void AddGuiWindow(std::shared_ptr<GuiWindow> guiWindow);
    std::shared_ptr<GuiWindow> GetGuiWindow(const std::string& name);
    void RemoveGuiWindow(std::shared_ptr<GuiWindow> guiWindow);
    void RemoveGuiWindow(const std::string& name);
    void RemoveAllGuiWindows();

    std::shared_ptr<GameOverlay> GetGameOverlay();
    void SetMenuBar(std::shared_ptr<GuiMenuBar> menuBar);
    std::shared_ptr<GuiMenuBar> GetMenuBar();
    void SetMenu(std::shared_ptr<GuiWindow> menu);
    std::shared_ptr<GuiWindow> GetMenu();
    bool GetMenuOrMenubarVisible();
    bool IsMouseOverAnyGuiItem();
    bool IsMouseOverActivePopup();
    bool GamepadNavigationEnabled();
    void BlockGamepadNavigation();
    void UnblockGamepadNavigation();

  private:
    std::unordered_map<std::string, std::shared_ptr<GuiWindow>> mGuiWindows;
    std::shared_ptr<GameOverlay> mGameOverlay;
    std::shared_ptr<GuiMenuBar> mMenuBar;
    std::shared_ptr<GuiWindow> mMenu;
};

} // namespace Ship
