#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C bridge to the Fast3D graphics debugger. The debugger is a desktop ImGui tool;
// on console it is inert, so capture is never requested and IsDebugging is always
// false (the game's per-frame "should I hand the display list to the debugger?"
// checks compile and short-circuit).
void GfxDebuggerRequestDebugging();
bool GfxDebuggerIsDebugging();
bool GfxDebuggerIsDebuggingRequested();
void GfxDebuggerDebugDisplayList(void* cmds);

#ifdef __cplusplus
}
#endif
