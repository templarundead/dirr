#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "argh.hh"

#ifdef ColourPrints
# include "cons.hh"
#else
# ifdef HAVE_PRINT
#  include <print>
# endif
#endif

/* This file is part of Bisqwit's dirr and remotegcc packages. */

namespace
{
void WriteText(std::FILE *stream, std::string_view text)
{
#ifdef ColourPrints
    // The legacy colour-aware formatter writes to stdout.
    (void)stream;
    (void)GwriteFormatted(text);
#elif defined(HAVE_PRINT)
    std::print(stream, "{}", text);
#else
    (void)std::fwrite(text.data(), 1, text.size(), stream);
#endif
}

void SetTextAttr(int attr)
{
#ifdef ColourPrints
    SetAttr(attr);
#else
    (void)attr;
#endif
}

void ResetTextAttr()
{
#ifdef ColourPrints
    SetAttr(DEFAULTATTR);
#endif
}
}

arghandler::arghandler(const char *defopts, int argc, const char *const *argv)
{
    const char *q;

    q = std::strrchr(argv[0], '/');
    a0 = q ? q+1 : argv[0];

    if(defopts)
    {
        q = defopts;
        while(*q)
        {
            const char *p = std::strchr(q, ' ');
            if(!p)break;
            args.emplace_back(q, p-q);
            q = p+1;
        }
        args.push_back(q);
    }
    bool terminated = false;

    /* If you have 5 parameters and want to skip 2 parameters
     * at the beginning, specify argc as: (2-argc)*max_argc-2
     */
    if(argc < 0)
    {
        argc = -argc;
        argv += (argc % max_argc);
        argc /= max_argc;
    }
    while(--argc)
    {
        q = *++argv;
        if(!std::strcmp(q, "--"))
            terminated = true;
        else if(!terminated)
        {
            if(!std::strncmp(q, "--", 2))
            {
                const char *p = std::strchr(q, '=');
                if(p)
                {
                    std::string tmp(q, p-q);
                    args.push_back(std::move(tmp));
                    args.push_back(p+1);
                    continue;
                }
            }
        }
        args.push_back(q);
    }
}

arghandler::~arghandler() = default;

void arghandler::subadd(const char *Short, const char *Long, const std::string &Descr, argfun handler)
{
    options.emplace_back(Short ? Short+1 : "",
                         Long ? Long+2 : "",
                         Descr, std::move(handler));
}

void arghandler::parse()
{
    bool terminated = false;
    for(std::size_t a=0, b=args.size(); a<b; ++a)
    {
        std::string s = args[a];
        if(s.size() > 1 && !terminated && s[0] == '-')
        {
            if(s == "--")
                terminated = true;
            else if(s[1] == '-')
            {
                s.erase(0, 2);

                bool ok = false;
                for(auto& o: options)
                    if(o.Long && s == o.Long)
                    {
                        std::size_t c = a+1;
                        if(c == b) --c; // Don't access out of bounds
                        s = args[c];
                        s = o.handler->CallBack(this, s);
                        if(s.empty())
                        {
                            // It consumed the long parameter's option
                            a = c;
                        }
                        else if(s != args[c])
                        {
                            // It partially consumed the option. This is an error
                            argerror(args[a]);
                            return;
                        }
                        else
                        {
                            // It didn't use the long parameter's option.
                        }
                        ok = true;
                        break;
                    }
                if(!ok)
                {
                    argerror(s, false);
                    return;
                }
            }
            else
            {
                s.erase(0, 1);
                while(!s.empty())
                {
                    bool ok = false;
                    for(auto& o: options)
                        if(s.substr(0, std::strlen(o.Short)) == o.Short)
                        {
                            s = o.handler->CallBack(this, s.substr(std::strlen(o.Short)));
                            ok = true;
                            break;
                        }
                    if(!ok)
                    {
                        argerror(s[0]);
                        return;
                    }
                }
            }
        }
        else
            defarg(s);
    }
}

void arghandler::argerror(char c)
{
    std::string message = a0 + ": illegal option -- ";
    message.push_back(c);
    message.push_back('\n');
    WriteText(stderr, message);
    suggesthelp();
    exit(1);
}

void arghandler::argerror(const std::string &s, bool param)
{
    std::string message = a0 + ": ";
    message += param ? "invalid parameter `" : "unrecognized option `--";
    message += s;
    message += "'\n";
    WriteText(stderr, message);
    if(!param) suggesthelp();
    exit(1);
}

void arghandler::suggesthelp()
{
    WriteText(stderr, "Try `" + a0 + " --help' for more information.\n");
}

void arghandler::listoptions()
{
    std::size_t longestshort=0, longestlong=0;
    for(auto& o: options)
    {
        std::size_t slen = std::strlen(o.Short);
        if(slen > longestshort) longestshort = slen;
        std::size_t llen = std::strlen(o.Long);
        if(llen > longestlong) longestlong = llen;
    }
    const std::size_t space = longestshort + longestlong + 1;
    for(auto& o: options)
    {
        const char *s = o.Short;
        const char *l = o.Long;

        SetTextAttr(*s ? 3 : 0);
        WriteText(stdout, "  -");
        ResetTextAttr();
        WriteText(stdout, s);

        SetTextAttr(*l ? 3 : 0);
        WriteText(stdout, ", --");
        ResetTextAttr();
        WriteText(stdout, l);

        const std::size_t names_length = std::strlen(s) + std::strlen(l);
        WriteText(stdout, std::string(space - names_length, ' '));

        ResetTextAttr();

        const char *q = o.Descr.c_str();
        std::string description;
        bool needspace = false;
        bool needeol = true;
        for(; *q; ++q)
        {
            if(needspace)
            {
                description.push_back('\n');
                description.append(space + 7, ' ');
                needspace = false;
            }
            needeol = true;
            if(*q != '\n')
            {
                description.push_back(*q);
                continue;
            }
            needspace = true;
            needeol = false;
        }
        if(needspace || needeol) description.push_back('\n');
        WriteText(stdout, description);
    }
}
