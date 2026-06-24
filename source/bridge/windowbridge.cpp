#include "libultraship/bridge/windowbridge.h"

#include "ship/Context.h"
#include "ship/window/Window.h"

extern "C" {

bool WindowIsRunning() {
    auto window = Ship::Context::GetInstance()->GetWindow();
    return window != nullptr && window->IsRunning();
}

uint32_t WindowGetWidth() {
    auto window = Ship::Context::GetInstance()->GetWindow();
    return window != nullptr ? window->GetWidth() : 0;
}

uint32_t WindowGetHeight() {
    auto window = Ship::Context::GetInstance()->GetWindow();
    return window != nullptr ? window->GetHeight() : 0;
}

float WindowGetAspectRatio() {
    auto window = Ship::Context::GetInstance()->GetWindow();
    return window != nullptr ? window->GetAspectRatio() : 0.0f;
}
}
