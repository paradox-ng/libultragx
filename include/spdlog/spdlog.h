#pragma once

// Lean no-op stand-in for spdlog on GameCube/Wii.
//
// libultragx ships no real logging library (spdlog drags in fmt, iostreams and
// threads - the PC baggage we avoid on a 24MB GameCube). Code we reuse from the
// Fast3D module and prism logs via spdlog; these macros and types satisfy that
// code and compile to nothing. For real output, use libultragx's own luslog.

#include <memory>
#include <string>

namespace spdlog {

namespace level {
enum level_enum { trace, debug, info, warn, err, critical, off, n_levels };
}

class logger {
  public:
    template <typename... Args> void trace(Args&&...) {}
    template <typename... Args> void debug(Args&&...) {}
    template <typename... Args> void info(Args&&...) {}
    template <typename... Args> void warn(Args&&...) {}
    template <typename... Args> void error(Args&&...) {}
    template <typename... Args> void critical(Args&&...) {}
    void set_level(level::level_enum) {}
    void flush() {}
};

template <typename... Args> inline void trace(Args&&...) {}
template <typename... Args> inline void debug(Args&&...) {}
template <typename... Args> inline void info(Args&&...) {}
template <typename... Args> inline void warn(Args&&...) {}
template <typename... Args> inline void error(Args&&...) {}
template <typename... Args> inline void critical(Args&&...) {}

inline std::shared_ptr<logger> default_logger() { return nullptr; }
inline void set_level(level::level_enum) {}

} // namespace spdlog

#define SPDLOG_TRACE(...)    ((void)0)
#define SPDLOG_DEBUG(...)    ((void)0)
#define SPDLOG_INFO(...)     ((void)0)
#define SPDLOG_WARN(...)     ((void)0)
#define SPDLOG_ERROR(...)    ((void)0)
#define SPDLOG_CRITICAL(...) ((void)0)
