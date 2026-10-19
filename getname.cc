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

int GetName(std::string fn /* modified, so operate on a copy */,
            const StatType &sta, int Space,
            bool Fill, bool nameonly,
            const char *hardlinkfn)
{
    const StatType *Stat = &sta;
    // The formatter re-enters Redo with these records. Keep them alive for the
    // full call instead of pointing at temporaries inside the link branches.
    StatType LinkStat{};
    StatType HardlinkStat{};

    unsigned Len = 0;
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
        int i = WidthInColumns(what);
        Len += i;

        if(i > Space && nameonly) i = Space;

        int j = WidthPrint(i, what, Fill);
        Space -= j;
    };
    auto PutSet = [&](char ch)
    {
        if(estimating)
            ++Len;
        else if(Space)
        {
            ++Len;
            --Space;
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
            if(Space > 0) GetModeColor(ColorMode::INFO, '@');
            PrintIfRoom(SLinkArrow);

            std::string Buf = LinkTarget(fn, true);

            // Probe the followed target first, then fall back to its link metadata.
            if (auto target = try_stat(Buf); !target) {
                if (auto link = try_lstat(Buf); !link) {
                    wasinvalid = true;
                    if (Space > 0) GetModeColor(ColorMode::TYPE, '?');
                } else {
                    LinkStat = *link;
                    if (Space > 0) GetModeColor(ColorMode::TYPE, 'l');
                }
                maysublink = false;
            } else {
                LinkStat = *target;
                if (Space > 0) {
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

    if(hardlinkfn && Space)
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

    if(Fill && Space > 0)
        Gprintf("%*s", Space, "");
    return Len;
}
