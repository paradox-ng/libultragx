#include "ship/Context.h"

#include "platform/paths.h"
#include "ship/resource/ResourceManager.h"
#include "ship/config/ConsoleVariable.h"

#include <sys/stat.h>

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

} // namespace Ship
