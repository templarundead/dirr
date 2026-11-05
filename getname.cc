#include <config.h>

#include "getname.hh"
#include "strfun.hh"
#include "setfun.hh"
#include "colouring.hh"
#include "cons.hh"

#include <utility>

static const char SLinkArrow[] = " -> ";
static const char HLinkArrow[] = " = ";

#ifdef S_ISLNK
int Links;
#endif

LinkTargetInfo LinkTargetCached(std::string_view link, LinkTargetStatCache& cache)
{
    // A different source path (a link target that is itself a link, or a
    // hardlink target) invalidates the entry.
    if(!cache.target_initialized || cache.source != link)
    {
        std::string unfixed_target;
        cache.fixed_target = LinkTarget(std::string(link), true, &unfixed_target);
        cache.link_target  = std::move(unfixed_target);
        cache.source       = std::string(link);
        cache.target_initialized = true;
        cache.stat_cached  = false;
        cache.lstat_cached = false;
    }
    return LinkTargetInfo{ cache.link_target, cache.fixed_target };
}

const StatType* LinkTargetStatCached(LinkTargetStatCache& cache)
{
    if(!cache.stat_cached)
    {
        // Failures are not memoised; a later call simply retries, as before.
        if(auto target = try_stat(cache.fixed_target); target)
        {
            cache.target_stat = *target;
            cache.target_mode = target->st_mode;
            cache.stat_cached = true;
        }
    }
    return cache.stat_cached ? &cache.target_stat : nullptr;
}

const StatType* LinkTargetLstatCached(LinkTargetStatCache& cache)
{
    if(!cache.lstat_cached)
    {
        if(auto link = try_lstat(cache.fixed_target); link)
        {
            cache.link_stat = *link;
            cache.lstat_cached = true;
        }
    }
    return cache.lstat_cached ? &cache.link_stat : nullptr;
}

std::size_t GetName(std::string_view fn,
                    const StatType &sta, std::size_t Space,
                    bool Fill, bool nameonly,
                    const char *hardlinkfn,
                    LinkTargetStatCache* link_stat_cache)
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
#ifdef S_ISLNK
    // After a redo the name is no longer the item path, so the per-item cache
    // must not be overwritten with the target's entry.
    bool redone = false;
#endif

    // The name is printed as a view; only a renamed link target or a
    // relativised hardlink needs storage of its own.
    std::string fn_owned;
    std::string fn_print_owned;
    std::string_view fn_print = nameonly ? NameOnly(fn) : fn;
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

            LinkTargetStatCache redo_cache;
            LinkTargetStatCache& cache = (!redone && link_stat_cache) ? *link_stat_cache
                                                                     : redo_cache;
            const LinkTargetInfo target_info = LinkTargetCached(fn, cache);

            // Probe the followed target first, then fall back to its link metadata.
            mode_t target_mode{};
            bool target_found = false;
            if (const StatType* target = LinkTargetStatCached(cache)) {
                target_mode = target->st_mode;
                target_found = true;
            }

            if (!target_found) {
                if (const StatType* link = LinkTargetLstatCached(cache); !link) {
                    wasinvalid = true;
                    if (remaining_space > 0) GetModeColor(ColorMode::TYPE, '?');
                } else {
                    LinkStat = *link;
                    if (remaining_space > 0) GetModeColor(ColorMode::TYPE, 'l');
                }
                maysublink = false;
            } else {
                LinkStat.st_mode = target_mode;
                if (remaining_space > 0) {
                    if (const StatType* link = LinkTargetLstatCached(cache);
                       link && S_ISLNK(link->st_mode))
                        GetModeColor(ColorMode::TYPE, 'l');
                    else
                        SetAttr(GetNameAttr(LinkStat, target_info.fixed));
                }
            }

            // Unfixed link text, read with the same readlink call.
            fn_owned.assign(target_info.unfixed);
            fn       = fn_owned;
            fn_print = fn_owned;
            Stat = &LinkStat;
            redone = true;
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

        fn_print_owned = Relativize(fn, hardlinkfn);
        fn_print = fn_print_owned;

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
