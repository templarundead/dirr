#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "stat.h"

#ifdef S_ISLNK
extern int Links;
#endif

// Per-item memo of the symlink target resolution. Both the estimating pass and
// the rendering pass ask for the same link target of the same item, and the
// name and size fields resolve it again. Keeping readlink()/stat()/lstat()
// results here turns those repeated syscalls into one lookup per item.
struct LinkTargetStatCache
{
    std::string source;              // link path this entry describes
    std::string link_target;         // raw readlink() text (not fixed up)
    std::string fixed_target;        // path used to stat() the followed file
    StatType target_stat{};          // stat() of fixed_target
    StatType link_stat{};            // lstat() of fixed_target
    mode_t target_mode{};            // == target_stat.st_mode when stat_cached
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
LinkTargetInfo LinkTargetCached(const std::string& link, LinkTargetStatCache& cache);

// stat()/lstat() of the followed target, memoised in `cache`.
// Returns nullptr when the call fails, exactly like try_stat()/try_lstat().
const StatType* LinkTargetStatCached(LinkTargetStatCache& cache);
const StatType* LinkTargetLstatCached(LinkTargetStatCache& cache);

std::size_t GetName(std::string filename,
                    const StatType& stat,
                    std::size_t space,
                    bool fill,
                    bool name_only,
                    const char* hardlink_target,
                    LinkTargetStatCache* link_stat_cache = nullptr);
