#pragma once
// Inert stand-in for libultraship's JSON Config. libultragx has no equivalent: it
// reads its own config.ini (see platform/lugx_config.h) and keeps CVars separately.
// SoH's config-version updaters derive from ConfigVersionUpdater and take Config*,
// so both names must exist for its headers to parse; the migration bodies themselves
// (ConfigUpdaters.cpp) are excluded from the console build.
#include <cstdint>
#include <string>
#include <memory>

namespace Ship {
class ConfigVersionUpdater;

class Config {
  public:
    bool GetBool(const std::string&, bool defaultValue = false) { return defaultValue; }
    int32_t GetInt(const std::string&, int32_t defaultValue = 0) { return defaultValue; }
    float GetFloat(const std::string&, float defaultValue = 0.0f) { return defaultValue; }
    std::string GetString(const std::string&, const std::string& defaultValue = "") { return defaultValue; }
    void SetBool(const std::string&, bool) {}
    void SetInt(const std::string&, int32_t) {}
    void SetFloat(const std::string&, float) {}
    void SetString(const std::string&, const std::string&) {}
    void Erase(const std::string&) {}
    void EraseBlock(const std::string&) {}
    void Save() {}
    void RegisterVersionUpdater(std::shared_ptr<ConfigVersionUpdater>) {}
    void RunVersionUpdates() {}
};

class ConfigVersionUpdater {
  protected:
    uint32_t mVersion = 0;

  public:
    explicit ConfigVersionUpdater(uint32_t toVersion) : mVersion(toVersion) {}
    virtual ~ConfigVersionUpdater() = default;
    virtual void Update(Config* conf) = 0;
    uint32_t GetVersion() const { return mVersion; }
};
} // namespace Ship
