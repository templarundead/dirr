#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "stat.h"

#ifdef S_ISLNK
extern int Links;
#endif

// The parts of struct stat that the name and size columns look at. One of
// these is kept per listed entry, so it holds no more than the formatters need.
struct StatSummary
{
    mode_t mode{};
    SizeType size{};
    dev_t rdev{};

    StatSummary() = default;
    explicit StatSummary(const StatType& st) noexcept
        : mode(st.st_mode), size(st.st_size), rdev(st.st_rdev) {}
};

// Per-item memo of the symlink target resolution. Both the estimating pass and
// the rendering pass ask for the same link target of the same item, and the
// name and size fields resolve it again. Keeping the readlink() text and the
// stat()/lstat() results here turns those repeated syscalls into one lookup.
struct LinkTargetStatCache
{
    std::string link_target;         // raw readlink() text (not fixed up)
    std::string fixed_target;        // path used to stat() the followed file
    StatSummary target{};            // the followed target, valid when stat_cached
    mode_t link_mode{};              // lstat() mode of it, valid when lstat_cached
    bool target_initialized{};
    bool stat_cached{};
    bool lstat_cached{};
};

// View into a cache entry. The views stay valid until the entry is replaced.
struct LinkTargetInfo
{
    std::string_view unfixed;        // readlink() text as stored in the link
    std::string_view fixed;          // target path to pass to stat()
};

// Fills `cache` for `link` (one readlink() at most) and returns its views.
LinkTargetInfo LinkTargetCached(std::string_view link, LinkTargetStatCache& cache);

// stat()/lstat() of the followed target, memoised in `cache`.
// The getters return nullptr/false when the call fails, like try_stat().
const StatSummary* LinkTargetStatCached(LinkTargetStatCache& cache);
bool LinkTargetLstatCached(LinkTargetStatCache& cache, mode_t& link_mode);

std::size_t GetName(std::string_view filename,
                    const StatType& stat,
                    std::size_t space,
                    bool fill,
                    bool name_only,
                    const char* hardlink_target,
                    LinkTargetStatCache* link_stat_cache = nullptr);
