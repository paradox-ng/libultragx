#pragma once

// Lean fmt stand-in. libultragx ships no fmt library; the Fast3D code uses
// fmt::format for log/error strings, which we never render. format() returns the
// format string and ignores the arguments - enough to compile and link.

#include <string>

// The Fast3D interpreter includes only <spdlog/fmt/fmt.h> but also uses the
// SPDLOG_* logging macros; pull in our spdlog shim here so they are defined.
#include <spdlog/spdlog.h>

namespace fmt {

template <typename... Args>
inline std::string format(const char* f, Args&&...) {
    return f != nullptr ? std::string(f) : std::string();
}

template <typename... Args>
inline std::string format(const std::string& f, Args&&...) {
    return f;
}

} // namespace fmt

// Newer spdlog exposes its formatting library as spdlog::fmt_lib so callers do not
// depend on whether it bundles fmt or uses std::format. Shipwright moved to this
// spelling; alias it onto our shim.
namespace spdlog {
namespace fmt_lib = ::fmt;
} // namespace spdlog
