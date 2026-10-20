#pragma once

#include <cstddef>
#include <string>

#include "stat.h"

#ifdef S_ISLNK
extern int Links;
#endif

std::size_t GetName(std::string filename,
                    const StatType& stat,
                    int space,
                    bool fill,
                    bool name_only,
                    const char* hardlink_target);
