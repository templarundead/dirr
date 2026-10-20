#pragma once

#include <concepts>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <limits.h>
#include <type_traits>

#include <config.h>

#ifdef HAVE_FORMAT
# include <format>
#endif

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

/// Insert an optional thousands separator into a decimal representation.
inline void AddThousandsSeparators(std::string& dest, char seps) {
    if (seps == '\0') return;
    std::size_t dot = dest.find('.');
    if (dot == std::string::npos) dot = dest.size();
    for (std::size_t groups = (dot == 0 ? 0 : (dot - 1) / 3); groups > 0; --groups) {
        dot -= 3;
        dest.insert(dot, 1, seps);
    }
}

/// Format integral counts and sizes using C++23's type-safe formatter when available.
template<std::integral T>
void PrintNum(std::string& dest, char seps, T value) {
#ifdef HAVE_FORMAT
    dest = std::format("{:d}", value);
#else
    if constexpr (std::is_signed_v<T>) {
        // Size/count values in dirr are no wider than long long.
        dest = Printf("%lld", static_cast<long long>(value));
    } else {
        // The explicit cast selects the matching legacy printf argument type.
        dest = Printf("%llu", static_cast<unsigned long long>(value));
    }
#endif
    AddThousandsSeparators(dest, seps);
}

/// Format floating totals with fixed precision (currently 0 or 1 decimals).
template<unsigned Precision, std::floating_point T>
void PrintNumFixed(std::string& dest, char seps, T value) {
#ifdef HAVE_FORMAT
    dest = std::format("{:.{}f}", value, Precision);
#else
    static_assert(Precision <= 1, "fixed total precision is intentionally limited to 0 or 1");
    std::ostringstream stream;
    // Precision is compile-time constrained to 0 or 1, so this conversion is safe.
    stream << std::fixed << std::setprecision(static_cast<int>(Precision)) << value;
    dest = std::move(stream).str();
#endif
    AddThousandsSeparators(dest, seps);
}

/// Legacy adapter retained for user-configurable printf-style device formats.
template<typename... Args>
void PrintNum(std::string& dest, char seps, std::string_view fmt, Args&&... args) {
    dest = Printf(fmt, std::forward<Args>(args)...);
    AddThousandsSeparators(dest, seps);
}

template<std::size_t N, typename... Args>
inline void PrintNum(std::string& dest, char seps, const char (&fmt)[N], Args&&... args) {
    PrintNum(dest, seps, std::string_view(fmt, N - 1), std::forward<Args>(args)...);
}
