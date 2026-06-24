#include "libultraship/bridge/gfxdebuggerbridge.h"

extern "C" {

// Inert on console: nothing requests capture, so the game never enters the debug
// display-list path.
void GfxDebuggerRequestDebugging() {
}

bool GfxDebuggerIsDebugging() {
    return false;
}

bool GfxDebuggerIsDebuggingRequested() {
    return false;
}

void GfxDebuggerDebugDisplayList(void* /*cmds*/) {
}
}
