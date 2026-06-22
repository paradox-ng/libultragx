#include "ship/config/ConsoleVariable.h"

#include <cstdlib>
#include <cstring>

namespace Ship {

ConsoleVariable::ConsoleVariable() = default;
ConsoleVariable::~ConsoleVariable() = default;

// Frees a heap string if the CVar currently holds one, so a slot can be reused
// for a value of a different type without leaking.
static void freeIfString(CVar* c) {
    if (c->Type == ConsoleVariableType::String && c->String != nullptr) {
        free(c->String);
        c->String = nullptr;
    }
}

std::shared_ptr<CVar> ConsoleVariable::Get(const char* name) {
    auto it = mVariables.find(name);
    return it != mVariables.end() ? it->second : nullptr;
}

std::shared_ptr<CVar> ConsoleVariable::GetOrCreate(const char* name) {
    auto it = mVariables.find(name);
    if (it != mVariables.end()) {
        return it->second;
    }
    auto cvar = std::make_shared<CVar>();
    mVariables[name] = cvar;
    return cvar;
}

int32_t ConsoleVariable::GetInteger(const char* name, int32_t defaultValue) {
    auto c = Get(name);
    return (c && c->Type == ConsoleVariableType::Integer) ? c->Integer : defaultValue;
}

float ConsoleVariable::GetFloat(const char* name, float defaultValue) {
    auto c = Get(name);
    return (c && c->Type == ConsoleVariableType::Float) ? c->Float : defaultValue;
}

const char* ConsoleVariable::GetString(const char* name, const char* defaultValue) {
    auto c = Get(name);
    return (c && c->Type == ConsoleVariableType::String) ? c->String : defaultValue;
}

Color_RGBA8 ConsoleVariable::GetColor(const char* name, Color_RGBA8 defaultValue) {
    auto c = Get(name);
    return (c && c->Type == ConsoleVariableType::Color) ? c->Color : defaultValue;
}

Color_RGB8 ConsoleVariable::GetColor24(const char* name, Color_RGB8 defaultValue) {
    auto c = Get(name);
    return (c && c->Type == ConsoleVariableType::Color24) ? c->Color24 : defaultValue;
}

void ConsoleVariable::SetInteger(const char* name, int32_t value) {
    auto c = GetOrCreate(name);
    freeIfString(c.get());
    c->Type = ConsoleVariableType::Integer;
    c->Integer = value;
}

void ConsoleVariable::SetFloat(const char* name, float value) {
    auto c = GetOrCreate(name);
    freeIfString(c.get());
    c->Type = ConsoleVariableType::Float;
    c->Float = value;
}

void ConsoleVariable::SetString(const char* name, const char* value) {
    auto c = GetOrCreate(name);
    freeIfString(c.get());
    c->Type = ConsoleVariableType::String;
    c->String = value ? strdup(value) : nullptr;
}

void ConsoleVariable::SetColor(const char* name, Color_RGBA8 value) {
    auto c = GetOrCreate(name);
    freeIfString(c.get());
    c->Type = ConsoleVariableType::Color;
    c->Color = value;
}

void ConsoleVariable::SetColor24(const char* name, Color_RGB8 value) {
    auto c = GetOrCreate(name);
    freeIfString(c.get());
    c->Type = ConsoleVariableType::Color24;
    c->Color24 = value;
}

void ConsoleVariable::RegisterInteger(const char* name, int32_t defaultValue) {
    if (!Get(name)) SetInteger(name, defaultValue);
}
void ConsoleVariable::RegisterFloat(const char* name, float defaultValue) {
    if (!Get(name)) SetFloat(name, defaultValue);
}
void ConsoleVariable::RegisterString(const char* name, const char* defaultValue) {
    if (!Get(name)) SetString(name, defaultValue);
}
void ConsoleVariable::RegisterColor(const char* name, Color_RGBA8 defaultValue) {
    if (!Get(name)) SetColor(name, defaultValue);
}
void ConsoleVariable::RegisterColor24(const char* name, Color_RGB8 defaultValue) {
    if (!Get(name)) SetColor24(name, defaultValue);
}

void ConsoleVariable::ClearVariable(const char* name) {
    mVariables.erase(name);
}

void ConsoleVariable::ClearBlock(const char* name) {
    const size_t prefixLen = strlen(name);
    for (auto it = mVariables.begin(); it != mVariables.end();) {
        if (it->first.compare(0, prefixLen, name) == 0) {
            it = mVariables.erase(it);
        } else {
            ++it;
        }
    }
}

void ConsoleVariable::CopyVariable(const char* from, const char* to) {
    auto src = Get(from);
    if (!src) return;
    switch (src->Type) {
        case ConsoleVariableType::Integer: SetInteger(to, src->Integer); break;
        case ConsoleVariableType::Float:   SetFloat(to, src->Float); break;
        case ConsoleVariableType::String:  SetString(to, src->String); break;
        case ConsoleVariableType::Color:   SetColor(to, src->Color); break;
        case ConsoleVariableType::Color24: SetColor24(to, src->Color24); break;
    }
}

// TODO(M4): persist to/from a flat file under Context::GetAppDirectoryPath().
void ConsoleVariable::Save() {}
void ConsoleVariable::Load() {}

} // namespace Ship
