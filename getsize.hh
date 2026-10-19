#pragma once

#include <string>

#include "stat.h"

extern std::string BlkStr, ChrStr;

/// Human / numeric size formatting.
/// @param space  max columns (0 = estimate — no output)
/// @param seps   thousands separator (0 = none, -1 = compact SI)
std::string GetSize(const std::string& path,
                      const StatType& st,
                      int space,
                      int seps);
