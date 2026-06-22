#pragma once

#include <cstddef>

namespace Ship {

typedef void (*CrashHandlerCallback)(char* buffer, size_t* size);

// No-op crash handler on GameCube/Wii: there is no POSIX backtrace or SDL to hook,
// so this exists purely for API compatibility. Registered callbacks are stored but
// never invoked.
class CrashHandler {
  public:
    CrashHandler() = default;
    ~CrashHandler() = default;

    void RegisterCallback(CrashHandlerCallback callback) { mCallback = callback; }

  private:
    CrashHandlerCallback mCallback = nullptr;
};

} // namespace Ship
