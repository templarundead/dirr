#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "stat.h"

/// Indices for SumCnt / SumSizes with an explicit underlying type.
enum Sums : std::size_t {
    SumDir = 1,
    SumFifo,
    SumSock,
    SumFile,
    SumLink,
    SumChrDev,
    SumBlkDev,
    SumMax = 10
};

extern std::array<SizeType, SumMax> SumCnt;
extern std::array<SizeType, SumMax> SumSizes;

extern bool Totals;
extern int  TotalSep;
extern int  Compact;
extern std::string LastDir;

void PrintSums();
