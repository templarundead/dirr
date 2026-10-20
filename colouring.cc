#include "colouring.hh"
#include <config.h>
#include "cons.hh"
#include "printf.hh"
#include "setfun.hh"

#include <array>
#include <string_view>
#ifdef HAVE_FORMAT
# include <format>
#endif

int GetNameAttr(const StatType& st, std::string_view fn) {
    if (int col = NameColor(fn, -1); col != -1) [[likely]]
        return col;

#ifdef S_ISLNK
    if (S_ISLNK(st.st_mode)) [[unlikely]]
        return GetModeColor(ColorMode::TYPE, -'l');
    else
#endif
    if (S_ISDIR(st.st_mode))  return GetModeColor(ColorMode::TYPE, -'d');
    else if (S_ISCHR(st.st_mode)) return GetModeColor(ColorMode::TYPE, -'c');
    else if (S_ISBLK(st.st_mode)) return GetModeColor(ColorMode::TYPE, -'b');
#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode)) return GetModeColor(ColorMode::TYPE, -'p');
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode)) return GetModeColor(ColorMode::TYPE, -'s');
#endif
#ifdef S_ISDOOR
    else if (S_ISDOOR(st.st_mode)) return GetModeColor(ColorMode::TYPE, -'D');
#endif
    else if (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
        return GetModeColor(ColorMode::TYPE, -'x');
    else if (S_ISREG(st.st_mode))
        return GetModeColor(ColorMode::TYPE, -'-');

    return GetModeColor(ColorMode::TYPE, -'?');
}

std::size_t PrintAttr(const StatType& st, char attrs) {
    std::size_t len = 0;

    // Lambda replaces the old PutSet macro — type-safe, no macro hygiene issues.
    auto put_set = [&](char c) {
        GetModeColor(ColorMode::MODE, c);
        Gputch(c);
        ++len;
    };

    switch (attrs) {
        case '0':
            // DOS AHSR — only relevant on DJGPP; no-op on POSIX.
            break;

        case '1': {
#ifndef DJGPP
# ifdef S_ISLNK
            if (S_ISLNK(st.st_mode)) put_set('l');
            else
# endif
            if (S_ISDIR(st.st_mode)) put_set('d');
            else if (S_ISCHR(st.st_mode)) put_set('c');
            else if (S_ISBLK(st.st_mode)) put_set('b');
# ifdef S_ISFIFO
            else if (S_ISFIFO(st.st_mode)) put_set('p');
# endif
# ifdef S_ISSOCK
            else if (S_ISSOCK(st.st_mode)) put_set('s');
# endif
# ifdef S_ISDOOR
            else if (S_ISDOOR(st.st_mode)) put_set('D');
# endif
            else if (S_ISREG(st.st_mode)) put_set('-');
            else put_set('?');

            // Modern: constexpr pattern table, lambda capturing stat.
#if defined(SUNOS) || defined(__sun) || defined(SOLARIS)
            constexpr std::array<char,16> patterns{
                '-','r','-','w', '-','x','S','s',
                '-','x','l','s', '-','x','T','t'
            };
#else
            constexpr std::array<char,16> patterns{
                '-','r','-','w', '-','x','S','s',
                '-','x','S','s', '-','x','T','t'
            };
#endif
            auto has = [&st](unsigned m) -> bool { return st.st_mode & m; };

            const int def_col = GetModeColor(ColorMode::MODE, -('-'));
            const int exe_col = GetModeColor(ColorMode::MODE, -(has(S_IXUSR) ? 'x' : '-'));

            SetAttr(def_col);
            Gputch(patterns[0 + has(S_IRUSR)]);
            Gputch(patterns[2 + has(S_IWUSR)]);
            SetAttr(exe_col);
            Gputch(patterns[4 + has(S_ISUID)*2 + has(S_IXUSR)]);
            SetAttr(def_col);
            Gputch(patterns[0 + has(S_IRGRP)]);
            Gputch(patterns[2 + has(S_IWGRP)]);
            SetAttr(exe_col);
            Gputch(patterns[8 + has(S_ISGID)*2 + has(S_IXGRP)]);
            SetAttr(def_col);
            Gputch(patterns[0 + has(S_IROTH)]);
            Gputch(patterns[2 + has(S_IWOTH)]);
            SetAttr(exe_col);
            Gputch(patterns[12 + has(S_ISVTX)*2 + has(S_IXOTH)]);
            len += 9;
#endif
            break;
        }
        default: { // '2'..'9' → octal tail
            GetModeColor(ColorMode::MODE, '#');
#ifdef HAVE_FORMAT
            std::string s = std::format("{:032o}", st.st_mode);
#else
            std::string s = Printf("%032o", st.st_mode);
#endif
            const std::size_t keep = static_cast<std::size_t>(attrs - '0');
            std::string_view tail = std::string_view(s).substr(s.size() - keep);
            len += Gwrite(tail);
            break;
        }
    }
    return len;
}

#ifdef DJGPP
std::size_t PrintAttr(const StatType& st, char attrs, unsigned dosattr) {
    if (attrs == '0') {
        std::size_t len = 0;
        auto put_set = [&](char c) { GetModeColor(ColorMode::MODE, c); Gputch(c); ++len; };
        if (dosattr & 0x20) put_set('A'); else put_set('-');
        if (dosattr & 0x02) put_set('H'); else put_set('-');
        if (dosattr & 0x04) put_set('S'); else put_set('-');
        if (dosattr & 0x01) put_set('R'); else put_set('-');
        return len;
    }
    return PrintAttr(st, attrs);
}
#endif
