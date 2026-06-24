#pragma once

#include <functional>
#include <memory>
#include <string>

namespace Ship {
class Archive;

// Runtime mod scripting is a desktop feature: upstream compiles C/C++ mod sources
// with an embedded TCC at runtime and dlopens them. Neither runtime compilation nor
// dynamic loading exists on GameCube/Wii, so this is a no-op that lets a port's
// mod-loading calls compile and link (they simply load nothing).
class ScriptLoader {
  public:
    void SetCacheDir(const std::string& /*dir*/) {}
    void LoadAll() {}
    void UnloadAll() {}
    void CompileAll(const std::function<void(const std::shared_ptr<Archive>&)>& /*pre*/ = {},
                    const std::function<void()>& /*post*/ = {}) {}
};

} // namespace Ship
