#include "libultraship/bridge/consolevariablebridge.h"

// The active CVar store. libultraship routes this through
// Context::GetInstance()->GetConsoleVariables(); until Context owns its
// subsystems, libultragx keeps a single process-wide store here. The C bridge
// signatures the games call are unchanged.
static Ship::ConsoleVariable& Store() {
    static Ship::ConsoleVariable instance;
    return instance;
}

std::shared_ptr<Ship::CVar> CVarGet(const char* name) {
    return Store().Get(name);
}

extern "C" {

int32_t CVarGetInteger(const char* name, int32_t defaultValue) {
    return Store().GetInteger(name, defaultValue);
}
float CVarGetFloat(const char* name, float defaultValue) {
    return Store().GetFloat(name, defaultValue);
}
const char* CVarGetString(const char* name, const char* defaultValue) {
    return Store().GetString(name, defaultValue);
}
Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue) {
    return Store().GetColor(name, defaultValue);
}
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue) {
    return Store().GetColor24(name, defaultValue);
}

void CVarSetInteger(const char* name, int32_t value) { Store().SetInteger(name, value); }
void CVarSetFloat(const char* name, float value) { Store().SetFloat(name, value); }
void CVarSetString(const char* name, const char* value) { Store().SetString(name, value); }
void CVarSetColor(const char* name, Color_RGBA8 value) { Store().SetColor(name, value); }
void CVarSetColor24(const char* name, Color_RGB8 value) { Store().SetColor24(name, value); }

void CVarRegisterInteger(const char* name, int32_t defaultValue) { Store().RegisterInteger(name, defaultValue); }
void CVarRegisterFloat(const char* name, float defaultValue) { Store().RegisterFloat(name, defaultValue); }
void CVarRegisterString(const char* name, const char* defaultValue) { Store().RegisterString(name, defaultValue); }
void CVarRegisterColor(const char* name, Color_RGBA8 defaultValue) { Store().RegisterColor(name, defaultValue); }
void CVarRegisterColor24(const char* name, Color_RGB8 defaultValue) { Store().RegisterColor24(name, defaultValue); }

void CVarClear(const char* name) { Store().ClearVariable(name); }
bool CVarExists(const char* name) { return Store().Get(name) != nullptr; }
void CVarClearBlock(const char* name) { Store().ClearBlock(name); }
void CVarCopy(const char* from, const char* to) { Store().CopyVariable(from, to); }
void CVarLoad() { Store().Load(); }
void CVarSave() { Store().Save(); }

} // extern "C"
