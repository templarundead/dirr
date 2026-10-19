#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <limits.h>

#include "printf.hh"

#ifndef NAME_MAX
# define NAME_MAX 255
#endif
#ifndef PATH_MAX
# define PATH_MAX 1023
#endif

std::string_view NameOnly(std::string_view path) noexcept;
std::string_view DirOnly(std::string_view path) noexcept;
std::string LinkTarget(const std::string& link, bool fixit = false);
std::string GetError(int err);
std::string Relativize(std::string_view base, std::string_view name);
inline std::string RelativizePath(const std::filesystem::path& base,
                                  const std::filesystem::path& target) {
    return Relativize(base.string(), target.string());
}

/// Formatted number with optional thousands separator.
template<typename... Args>
void PrintNum(std::string& dest, char seps, std::string_view fmt, Args&&... args) {
    dest = Printf(fmt, std::forward<Args>(args)...);
    if (seps != '\0') {
        std::size_t dot = dest.find('.');
        if (dot == std::string::npos) dot = dest.size();
        for (std::size_t groups = (dot == 0 ? 0 : (dot - 1) / 3); groups > 0; --groups) {
            dot -= 3;
            dest.insert(dot, 1, seps);
        }
    }
}

template<std::size_t N, typename... Args>
inline void PrintNum(std::string& dest, char seps, const char (&fmt)[N], Args&&... args) {
    PrintNum(dest, seps, std::string_view(fmt, N - 1), std::forward<Args>(args)...);
}
