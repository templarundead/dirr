#pragma once

#include <cstddef>
#include <string>

#include "getname.hh"
#include "stat.h"

extern std::string BlkStr, ChrStr;

/// True when the current link mode prints statistics of the followed target,
/// i.e. when resolving a link target is part of formatting the size column.
bool SizeUsesLinkTarget();

/// Human / numeric size formatting.
/// @param space  max columns (0 = estimate — no output)
/// @param seps   thousands separator (0 = none, -1 = compact SI)
std::string GetSize(const std::string& path,
                    const StatType& st,
                    std::size_t space,
                    int seps,
                    LinkTargetStatCache* link_stat_cache = nullptr);
