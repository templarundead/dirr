#include "stat.h"

#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

int main()
{
    const std::string root{"/"};
    const char* root_c_str = root.c_str();
    const std::string nonterminated_storage{"/x"};
    const std::string_view root_view{nonterminated_storage.data(), 1};
    const std::filesystem::path root_path{root};

    if (!try_stat(root_c_str)) return 1;
    if (!try_stat(root)) return 2;
    if (!try_stat(root_view)) return 3;
    if (!try_stat_path(root)) return 4;
    if (!try_stat_path(root_c_str)) return 5;
    if (!try_stat_path(root_view)) return 6;
    if (!try_stat_path(root_path)) return 7;

    if (!try_lstat(root_c_str)) return 8;
    if (!try_lstat(root)) return 9;
    if (!try_lstat(root_view)) return 10;
    if (!try_lstat_path(root)) return 11;
    if (!try_lstat_path(root_c_str)) return 12;
    if (!try_lstat_path(root_view)) return 13;
    if (!try_lstat_path(root_path)) return 14;

    const std::string missing{
        "/__dirr_stat_path_smoke_missing_f5afd54774d7c11168b0b8cac61df3742dd667a5__"};
    const std::error_code no_such_file{ENOENT, std::generic_category()};
    const auto missing_stat = try_stat(missing);
    if (missing_stat || missing_stat.error() != no_such_file) return 15;
    const auto missing_lstat = try_lstat(missing.c_str());
    if (missing_lstat || missing_lstat.error() != no_such_file) return 16;
    const auto missing_view = try_stat_path(std::string_view{missing});
    if (missing_view || missing_view.error() != no_such_file) return 17;

    return 0;
}
