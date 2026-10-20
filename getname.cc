#include <config.h>

#include "getname.hh"
#include "strfun.hh"
#include "setfun.hh"
#include "colouring.hh"
#include "cons.hh"

static const char SLinkArrow[] = " -> ";
static const char HLinkArrow[] = " = ";

#ifdef S_ISLNK
int Links;
#endif

std::size_t GetName(std::string fn /* modified, so operate on a copy */,
                    const StatType &sta, std::size_t Space,
                    bool Fill, bool nameonly,
                    const char *hardlinkfn)
{
    const StatType *Stat = &sta;
    // The formatter re-enters Redo with these records. Keep them alive for the
    // full call instead of pointing at temporaries inside the link branches.
    StatType LinkStat{};
    StatType HardlinkStat{};

    std::size_t Len = 0;
    std::size_t remaining_space = Space;
    bool over_budget = false;
    bool estimating = (Space == 0);
    bool maysublink = true;
    bool wasinvalid = false;

    std::string fn_print = nameonly ? std::string(NameOnly(fn)) : fn;
#ifdef S_ISLNK
Redo:
#endif
    // We land here to print a name.
    // The color has already been prepared where
    // either this function is called, or Redo is jumped to.

    auto PrintIfRoom = [&](std::string_view what)
    {
        std::size_t width = WidthInColumns(what);
        Len += width;

        if(width > remaining_space && nameonly) width = remaining_space;

        const std::size_t printed = WidthPrint(width, what, Fill);
        if(printed > remaining_space)
        {
            remaining_space = 0;
            over_budget = true;
        }
        else
        {
            remaining_space -= printed;
        }
    };
    auto PutSet = [&](char ch)
    {
        if(estimating)
            ++Len;
        else if(remaining_space > 0 || over_budget)
        {
            ++Len;
            if(remaining_space > 0) --remaining_space;
            else over_budget = true;
            GetModeColor(ColorMode::INFO, ch);
            Gputch(ch);
        }
    };

    PrintIfRoom(fn_print);

    if(wasinvalid)
    {
        PutSet('?');
    }
    else
    {
        #ifdef S_ISSOCK
        if(S_ISSOCK(Stat->st_mode)) PutSet('=');
        #endif
        #ifdef S_ISFIFO
        if(S_ISFIFO(Stat->st_mode)) PutSet('|');
        #endif
    }
    #ifdef S_ISLNK
    if(!wasinvalid && S_ISLNK(Stat->st_mode))
    {
        if(Links >= 2 && maysublink)
        {
            if(remaining_space > 0) GetModeColor(ColorMode::INFO, '@');
            PrintIfRoom(SLinkArrow);

            std::string Buf = LinkTarget(fn, true);

            // Probe the followed target first, then fall back to its link metadata.
            if (auto target = try_stat(Buf); !target) {
                if (auto link = try_lstat(Buf); !link) {
                    wasinvalid = true;
                    if (remaining_space > 0) GetModeColor(ColorMode::TYPE, '?');
                } else {
                    LinkStat = *link;
                    if (remaining_space > 0) GetModeColor(ColorMode::TYPE, 'l');
                }
                maysublink = false;
            } else {
                LinkStat = *target;
                if (remaining_space > 0) {
                    if (auto link = try_lstat(Buf); link && S_ISLNK(link->st_mode))
                        GetModeColor(ColorMode::TYPE, 'l');
                    else
                        SetAttr(GetNameAttr(LinkStat, Buf));
                }
            }

            fn       = LinkTarget(fn, false); // Unfixed link.
            fn_print = fn;
            Stat = &LinkStat;
            goto Redo;
        }
        PutSet('@');
    }
    else if(!wasinvalid)
    #endif
    {
        if(S_ISDIR(Stat->st_mode))     PutSet('/');
        else if(Stat->st_mode & 00111) PutSet('*'); // Executable by someone
    }

    if(hardlinkfn && (remaining_space > 0 || over_budget))
    {
        GetModeColor(ColorMode::INFO, '&');
        PrintIfRoom(HLinkArrow);

        fn_print = Relativize(fn, hardlinkfn);

        if (auto hardlink_stat = try_stat(hardlinkfn); hardlink_stat)
            HardlinkStat = *hardlink_stat;
        SetAttr(GetNameAttr(HardlinkStat, NameOnly(hardlinkfn)));
        hardlinkfn = NULL;
        Stat = &HardlinkStat;

        maysublink = false;
        wasinvalid = false;

        goto Redo;
    }

    if(Fill && remaining_space > 0)
        Gwrite("", remaining_space);
    return Len;
}
