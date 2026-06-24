#pragma once

#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C window queries the game's C/C++ code calls. Backed by the active
// Ship::Window via Context::GetInstance()->GetWindow().
bool WindowIsRunning();
uint32_t WindowGetWidth();
uint32_t WindowGetHeight();
float WindowGetAspectRatio();

#ifdef __cplusplus
}
#endif
