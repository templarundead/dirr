#include <cstdio>
#include <cstring>

#include <config.h>
#include "colouring.hh"
#include "setfun.hh"
#include "strfun.hh"
#include "getname.hh"
#include "printf.hh"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <unistd.h>

#ifndef MAJOR
 #define MAJOR(dev) ((dev) >> 8)
 #define MINOR(dev) ((dev) & 255)
#endif

std::string BlkStr, ChrStr;

static bool LinkFormatWithoutSize()
{
    return Links==1 || Links==4;
}
static bool LinkFormatStatsOfTarget()
{
    return Links==3 || Links==5;
}
bool SizeUsesLinkTarget()
{
    return LinkFormatStatsOfTarget();
}

std::string GetSize(const std::string &s, const StatType &Sta, std::size_t Space, int Seps,
                    LinkTargetStatCache* link_stat_cache)
{
    StatSummary current{Sta};
    const StatSummary *Stat = &current;
#ifdef S_ISLNK
    StatSummary target_summary{}; // Must outlive the GotSize retry path below.
#endif

    std::string result;
    ColorDescr descr = ColorDescr::DESCR;

#ifdef S_ISLNK
GotSize:
#endif
         if(S_ISDIR (Stat->mode)) result = "<DIR>";
    #ifdef S_ISFIFO
    else if(S_ISFIFO(Stat->mode)) result = "<PIPE>";
    #endif
    #ifdef S_ISSOCK
    else if(S_ISSOCK(Stat->mode)) result = "<SOCKET>";
    #endif
    else if(S_ISCHR (Stat->mode))
    {
        result = Printf(ChrStr, MAJOR(Stat->rdev), MINOR(Stat->rdev));
    }
    else if(S_ISBLK (Stat->mode))
    {
        result = Printf(BlkStr, MAJOR(Stat->rdev), MINOR(Stat->rdev));
    }
    #ifdef S_ISLNK
    else if(S_ISLNK (Stat->mode))
    {
        if(!LinkFormatWithoutSize())goto P1;
LinkProblem:
        result = "<LINK>";
    }
    #endif
    else
    {
        /* Tähän päädytään, jos kyseessä oli tavanomainen tiedosto */
        SizeType l;

#ifdef S_ISLNK
P1:
        /* Tämä iffi on tässä siksi uudestaan, kun tähän voidaan päätyä
         * sekä gotolla, että myös jos oli tavanomainen tiedosto.
         */
        if(S_ISLNK(Stat->mode))
        {
            if(LinkFormatStatsOfTarget())
            {
                // Reuse the link resolution the name field already did.
                LinkTargetStatCache local_cache;
                LinkTargetStatCache& cache = link_stat_cache ? *link_stat_cache : local_cache;
                (void)LinkTargetCached(s, cache);
                if (const StatSummary* target = LinkTargetStatCached(cache)) {
                    target_summary = *target;
                    Stat = &target_summary;
                    goto GotSize;
                }
                goto LinkProblem;
            }
            /* different colour */
            descr = (ColorDescr)-1;
        }
#endif
        l = Stat->size;

        const char* Suffix = "";

        if(Seps == -1)
        {
            Seps=0;
            static const char SIsuffixTable[] = "\0\0k\0M\0G\0T\0P\0E\0Z\0Y";
            Suffix = SIsuffixTable;
            while(l >= 1000)
            {
                l /= 1000;
                Suffix += 2;
            }
        }

        // The separator is a single command-line byte; -1 was normalized above.
        PrintNum(result, Seps == -1 ? '\0' : static_cast<char>(Seps), l);
        result += Suffix;

        if(descr != ColorDescr(-1)) descr = ColorDescr::SIZE;
    }

    if(descr != ColorDescr(-1))
        GetDescrColor(descr, 1);
    else
        GetModeColor(ColorMode::INFO, '@');

    if(result.size() < Space)
        result.insert(0, Space-result.size(), ' ');

    return result;
}
