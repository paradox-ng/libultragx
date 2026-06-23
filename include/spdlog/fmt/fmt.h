#pragma once

// Lean fmt stand-in. libultragx ships no fmt library; the Fast3D code uses
// fmt::format for log/error strings, which we never render. format() returns the
// format string and ignores the arguments - enough to compile and link.

#include <string>

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
