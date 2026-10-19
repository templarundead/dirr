#pragma once

// POSIX metadata aliases and C++23 error-aware wrappers. Filesystem errors
// are returned as std::expected values instead of being read from global errno.

#include <config.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <cerrno>
#include <concepts>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

using StatType = struct stat;
using SizeType = long long;
inline constexpr std::string_view SizeFormat{"%lld"};

// Accept string-like values and filesystem paths without forcing callers to
// build a temporary C string themselves.
template<class T>
concept StatPath = std::convertible_to<T, std::string_view>
                || std::same_as<std::remove_cvref_t<T>, std::filesystem::path>;

[[nodiscard]] inline std::expected<StatType, std::error_code>
try_stat(std::string_view path) {
    const std::string native_path{path};
    StatType metadata{};
    if (::stat(native_path.c_str(), &metadata) == 0)
        return metadata;
    return std::unexpected(std::error_code(errno, std::generic_category()));
}

[[nodiscard]] inline std::expected<StatType, std::error_code>
try_lstat(std::string_view path) {
    const std::string native_path{path};
    StatType metadata{};
    if (::lstat(native_path.c_str(), &metadata) == 0)
        return metadata;
    return std::unexpected(std::error_code(errno, std::generic_category()));
}

template<StatPath Path>
[[nodiscard]] inline std::expected<StatType, std::error_code>
try_stat_path(const Path& path) {
    if constexpr (std::same_as<std::remove_cvref_t<Path>, std::filesystem::path>)
        return try_stat(std::string_view{path.string()});
    else
        return try_stat(std::string_view{path});
}

template<StatPath Path>
[[nodiscard]] inline std::expected<StatType, std::error_code>
try_lstat_path(const Path& path) {
    if constexpr (std::same_as<std::remove_cvref_t<Path>, std::filesystem::path>)
        return try_lstat(std::string_view{path.string()});
    else
        return try_lstat(std::string_view{path});
}
