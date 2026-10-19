#pragma once

#include <string>
#include <string_view>

// Modern C++23 colour-setting interface.
// Uses enum class, std::string_view, concepts and [[nodiscard]].

#define ColorDescrs(o) o(TEXT,"txt") o(OWNER,"owner") o(GROUP,"group") o(NRLINK,"nrlink") \
                       o(DATE,"date") o(NUM,"num") o(DESCR,"descr") o(SIZE,"size")
#define ColorModes(o)  o(TYPE,"type") o(MODE,"mode") o(INFO,"info")

#define EXPAND_ENUM(val,str) val,
enum class ColorDescr { ColorDescrs(EXPAND_ENUM) };
enum class ColorMode  { ColorModes(EXPAND_ENUM) };
#undef EXPAND_ENUM

extern bool Dumping;
extern bool WasNormalColor;

// Keep legacy using for compatibility with old code that uses unqualified `string`
using std::string;

int GetModeColor(ColorMode mode, signed char ch);
int GetDescrColor(ColorDescr descr, int index);
void PrintSettings();

[[nodiscard]] int NameColor(std::string_view name, int default_color);
