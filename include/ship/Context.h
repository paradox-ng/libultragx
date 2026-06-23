#pragma once

#include <memory>
#include <string>

namespace Ship {

class ResourceManager;
class ConsoleVariable;

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

  private:
    std::shared_ptr<ResourceManager> mResourceManager;
    std::shared_ptr<ConsoleVariable> mConsoleVariables;
};

} // namespace Ship
