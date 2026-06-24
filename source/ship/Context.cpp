#include "ship/Context.h"

#include "platform/paths.h"
#include "ship/resource/ResourceManager.h"
#include "ship/resource/archive/ArchiveManager.h"
#include "ship/resource/archive/O2rArchive.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/window/Window.h"
#include "ship/events/EventSystem.h"

#include <sys/stat.h>
#include <utility>

namespace Ship {

std::shared_ptr<Context> Context::GetInstance() {
    static std::shared_ptr<Context> sInstance = std::make_shared<Context>();
    return sInstance;
}

std::shared_ptr<ResourceManager> Context::GetResourceManager() {
    if (mResourceManager == nullptr) {
        mResourceManager = std::make_shared<ResourceManager>();
    }
    return mResourceManager;
}

std::shared_ptr<ConsoleVariable> Context::GetConsoleVariables() {
    if (mConsoleVariables == nullptr) {
        mConsoleVariables = std::make_shared<ConsoleVariable>();
    }
    return mConsoleVariables;
}

// Resolved once at startup from argv[0]; e.g. "sd:/Ghostship/". Always carries a
// trailing slash (lugx_resolve_base_dir guarantees it).
static std::string sBaseDir;
static std::string sShortName;

void Context::InitPaths(const std::string& argv0, const std::string& shortName) {
    sShortName = shortName;
    char buf[256];
    lugx_resolve_base_dir(argv0.empty() ? nullptr : argv0.c_str(), buf, (int)sizeof(buf));
    sBaseDir = buf;
}

std::string Context::GetAppBundlePath() {
    return sBaseDir;
}

std::string Context::GetAppDirectoryPath(const std::string& /*appName*/) {
    return sBaseDir;
}

std::string Context::GetPathRelativeToAppDirectory(const std::string& path, const std::string& /*appName*/) {
    return sBaseDir + path;
}

std::string Context::GetPathRelativeToAppBundle(const std::string& path) {
    return sBaseDir + path;
}

std::string Context::LocateFileAcrossAppDirs(const std::string& path, const std::string& /*appName*/) {
    std::string full = sBaseDir + path;
    struct stat st;
    if (stat(full.c_str(), &st) == 0) {
        return full;
    }
    return "";
}

std::string Context::GetShortName() {
    return sShortName;
}

// --- Boot lifecycle (Ghostship integration, increment 1) -----------------------

std::shared_ptr<Context> Context::CreateUninitializedInstance(const std::string& name, const std::string& shortName,
                                                              const std::string& configName) {
    auto ctx = GetInstance();
    ctx->mName = name;
    ctx->mConfigName = configName;
    sShortName = shortName; // the per-game SD base dir itself is resolved by InitPaths(argv0)
    return ctx;
}

bool Context::InitConfiguration() {
    return true; // config is CVar-backed; nothing to load from disk on console
}

bool Context::InitConsoleVariables() {
    GetConsoleVariables(); // lazily create the CVar store
    return true;
}

bool Context::InitLogging(int /*debugBuildLogLevel*/, int /*releaseBuildLogLevel*/) {
    return true; // luslog is always-on; no spdlog sink wiring needed here
}

bool Context::InitResourceManager(const std::vector<std::string>& archivePaths,
                                  const std::unordered_set<uint32_t>& /*validHashes*/,
                                  uint32_t /*reservedThreadCount*/) {
    auto rm = GetResourceManager();
    for (const auto& path : archivePaths) {
        if (path.empty()) {
            continue;
        }
        auto archive = std::make_shared<O2rArchive>();
        if (archive->Open(path)) {
            rm->GetArchiveManager()->AddArchive(archive);
        }
    }
    return true;
}

bool Context::InitControlDeck(std::shared_ptr<ControlDeck> controlDeck) {
    mControlDeck = std::move(controlDeck);
    return true;
}

bool Context::InitConsole() {
    return true;
}

bool Context::InitWindow(std::shared_ptr<Window> window) {
    mWindow = std::move(window);
    if (mWindow != nullptr) {
        mWindow->Init(); // bring up VI/GX + the interpreter (Fast3dWindow::Init)
    }
    return true;
}

bool Context::InitEventSystem() {
    GetEventSystem(); // lazily create the dispatcher
    return true;
}

std::shared_ptr<EventSystem> Context::GetEventSystem() {
    if (mEventSystem == nullptr) {
        mEventSystem = std::make_shared<EventSystem>();
    }
    return mEventSystem;
}

bool Context::InitScriptLoader(std::unordered_map<std::string, std::string> /*compileDefines*/, int /*codeVersion*/,
                               std::string /*compileFlags*/, std::vector<std::string> /*includePaths*/,
                               std::vector<std::string> /*libraryPaths*/, std::vector<std::string> /*libraries*/) {
    return true; // no runtime (TCC) script compilation on console
}

std::shared_ptr<Window> Context::GetWindow() const {
    return mWindow;
}

std::shared_ptr<ControlDeck> Context::GetControlDeck() const {
    return mControlDeck;
}

std::shared_ptr<Console> Context::GetConsole() const {
    return mConsole;
}

std::string Context::GetName() const {
    return mName;
}

} // namespace Ship
