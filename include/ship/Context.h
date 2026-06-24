#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A game's Engine.h declares ImFont* font members but does not include <imgui.h>
// (upstream pulled it transitively through the GUI). libultragx strips ImGui, so
// forward-declare the opaque type here - Context.h is on every game TU's include
// path - to keep those pointer members valid.
struct ImFont;

namespace Ship {

class ResourceManager;
class ConsoleVariable;
class Window;
class ControlDeck;
class Console;
class EventSystem;

// Lean GameCube/Wii reimplementation of libultraship's Ship::Context.
//
// First increment: the static application-path API the games use most
// (GetPathRelativeToAppDirectory ~15x, LocateFileAcrossAppDirs, GetAppDirectoryPath,
// GetAppBundlePath in Ghostship), mapped onto the on-SD per-game folder described
// in docs/ARCHITECTURE.md. Unlike upstream libultraship this carries no spdlog or
// Audio dependency. The subsystem accessors (ResourceManager/Window/ControlDeck/...)
// and the full CreateInstance/Init lifecycle arrive as those subsystems land.
class Context {
  public:
    // Singleton hub (matches libultraship usage: Context::GetInstance()->GetX()).
    // The Fast3D interpreter reaches the ResourceManager and CVars through here.
    static std::shared_ptr<Context> GetInstance();

    std::shared_ptr<ResourceManager> GetResourceManager();
    std::shared_ptr<ConsoleVariable> GetConsoleVariables();

    // libultragx extension: establish the per-game base directory from the
    // launched .dol path (argv[0]). Call once at startup before the path helpers.
    // e.g. "sd:/Ghostship.dol" -> base directory "sd:/Ghostship/".
    static void InitPaths(const std::string& argv0, const std::string& shortName);

    // libultraship-compatible static path API. On GameCube/Wii the "app
    // directory" and "app bundle" are both the per-game SD folder.
    static std::string GetAppBundlePath();
    static std::string GetAppDirectoryPath(const std::string& appName = "");
    static std::string GetPathRelativeToAppDirectory(const std::string& path, const std::string& appName = "");
    static std::string GetPathRelativeToAppBundle(const std::string& path);
    static std::string LocateFileAcrossAppDirs(const std::string& path, const std::string& appName = "");

    static std::string GetShortName();

    // --- Boot lifecycle (Ghostship integration, increment 1) ------------------
    // The game's GameEngine() calls CreateUninitializedInstance() then drives the
    // Init* subsystems one by one (it does NOT use the combined Init()/CreateInstance
    // path). Signatures match the calls in Ghostship src/port/Engine.cpp so the game
    // links unchanged. ResourceManager/CVars wire to the real lean subsystems; the
    // rest (ControlDeck/Window/Console/EventSystem/ScriptLoader) store-and-succeed
    // for now - their real backends (Fast3dWindow over GX, PAD/WPAD ControlDeck, ...)
    // land in later increments.
    static std::shared_ptr<Context> CreateUninitializedInstance(const std::string& name, const std::string& shortName,
                                                                const std::string& configName);

    bool InitConfiguration();
    bool InitConsoleVariables();
    // spdlog::level::level_enum passes through as int (the game computes it from spdlog).
    bool InitLogging(int debugBuildLogLevel = 0, int releaseBuildLogLevel = 0);
    bool InitResourceManager(const std::vector<std::string>& archivePaths = {},
                             const std::unordered_set<uint32_t>& validHashes = {}, uint32_t reservedThreadCount = 1);
    bool InitControlDeck(std::shared_ptr<ControlDeck> controlDeck = nullptr);
    bool InitConsole();
    bool InitWindow(std::shared_ptr<Window> window = nullptr);
    bool InitEventSystem();
    bool InitScriptLoader(std::unordered_map<std::string, std::string> compileDefines = {}, int codeVersion = 1,
                          std::string compileFlags = "", std::vector<std::string> includePaths = {},
                          std::vector<std::string> libraryPaths = {}, std::vector<std::string> libraries = {});

    std::shared_ptr<Window> GetWindow() const;
    std::shared_ptr<ControlDeck> GetControlDeck() const;
    std::shared_ptr<Console> GetConsole() const;
    std::shared_ptr<EventSystem> GetEventSystem();
    std::string GetName() const;

  private:
    std::shared_ptr<ResourceManager> mResourceManager;
    std::shared_ptr<ConsoleVariable> mConsoleVariables;
    std::shared_ptr<Window> mWindow;
    std::shared_ptr<ControlDeck> mControlDeck;
    std::shared_ptr<Console> mConsole;
    std::shared_ptr<EventSystem> mEventSystem;
    std::string mName;
    std::string mConfigName;
};

} // namespace Ship
