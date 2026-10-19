#pragma once
#include <expected>
#include <optional>
#include <string>

// Modern C++23 user/group lookup.
// Returns std::string (empty = not found) for compatibility,
// plus new helpers returning std::optional / std::expected.

std::string Getpwuid(int uid);
std::string Getgrgid(int gid);

[[nodiscard]] std::optional<std::string> try_getpwuid(int uid);
[[nodiscard]] std::optional<std::string> try_getgrgid(int gid);

std::expected<std::string, std::string> expect_getpwuid(int uid);
std::expected<std::string, std::string> expect_getgrgid(int gid);
