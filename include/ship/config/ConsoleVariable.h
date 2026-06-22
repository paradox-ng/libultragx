#pragma once

#include "ship/utils/color.h"
#include <stdint.h>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <string>

namespace Ship {

// Discriminator tag for the active field of the CVar union.
enum class ConsoleVariableType { Integer, Float, String, Color, Color24 };

// A single console variable holding exactly one typed value.
typedef struct CVar {
    ConsoleVariableType Type;
    union {
        int32_t Integer;
        float Float;
        char* String = nullptr; // heap-allocated when Type == String
        Color_RGBA8 Color;
        Color_RGB8 Color24;
    };
    ~CVar() {
        if (Type == ConsoleVariableType::String && String != nullptr) {
            free(String);
        }
    }
} CVar;

// Lean reimplementation of libultraship's CVar store. Unlike upstream this
// carries no nlohmann/json dependency; Save/Load persist to a flat file under
// the per-game folder (persistence wiring is a TODO). Obtain the active instance
// from the CVar bridge (eventually Context::GetConsoleVariables()).
class ConsoleVariable {
  public:
    ConsoleVariable();
    ~ConsoleVariable();

    std::shared_ptr<CVar> Get(const char* name);

    int32_t GetInteger(const char* name, int32_t defaultValue);
    float GetFloat(const char* name, float defaultValue);
    const char* GetString(const char* name, const char* defaultValue);
    Color_RGBA8 GetColor(const char* name, Color_RGBA8 defaultValue);
    Color_RGB8 GetColor24(const char* name, Color_RGB8 defaultValue);

    void SetInteger(const char* name, int32_t value);
    void SetFloat(const char* name, float value);
    void SetString(const char* name, const char* value);
    void SetColor(const char* name, Color_RGBA8 value);
    void SetColor24(const char* name, Color_RGB8 value);

    void RegisterInteger(const char* name, int32_t defaultValue);
    void RegisterFloat(const char* name, float defaultValue);
    void RegisterString(const char* name, const char* defaultValue);
    void RegisterColor(const char* name, Color_RGBA8 defaultValue);
    void RegisterColor24(const char* name, Color_RGB8 defaultValue);

    void ClearVariable(const char* name);
    void ClearBlock(const char* name);
    void CopyVariable(const char* from, const char* to);

    void Save();
    void Load();

  private:
    std::shared_ptr<CVar> GetOrCreate(const char* name);

    std::unordered_map<std::string, std::shared_ptr<CVar>> mVariables;
};

} // namespace Ship
