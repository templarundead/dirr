#pragma once

#include <cstddef>
#include <string>

#include "stat.h"

#ifdef S_ISLNK
extern int Links;
#endif

struct LinkTargetStatCache
{
    std::string link_target;
    mode_t target_mode{};
    bool target_initialized{};
    bool stat_cached{};
};

std::size_t GetName(std::string filename,
                    const StatType& stat,
                    std::size_t space,
                    bool fill,
                    bool name_only,
                    const char* hardlink_target,
                    LinkTargetStatCache* link_stat_cache = nullptr);
