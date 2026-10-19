#pragma once

#include <string_view>

#include "stat.h"

// C++23 modernised colouring interface.
// Uses std::string_view, [[nodiscard]], noexcept and concepts.

int GetNameAttr(const StatType& stat, std::string_view filename);
int PrintAttr(const StatType& stat, char attrs);
#ifdef DJGPP
int PrintAttr(const StatType& stat, char attrs, unsigned dosattr);
#endif
