#pragma once

#include <string>

#include "stat.h"

#ifdef S_ISLNK
extern int Links;
#endif

int GetName(std::string filename,
              const StatType& stat,
              int space,
              bool fill,
              bool name_only,
              const char* hardlink_target);
