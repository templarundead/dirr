#ifndef dirr3_argh_hh
#define dirr3_argh_hh

#ifdef ARGH_VERSION_2
#include "argh2.hh"
#else

#include <memory>
#include <string>
#include <utility>
#include <vector>

/* This file is part of Bisqwit's dirr and remotegcc packages. */

inline constexpr int max_argc = 100'000;

class arghandler
{
private:
    class MethodPtr
    {
    public:
        virtual std::string CallBack(arghandler *base, const std::string &param) = 0;
        virtual ~MethodPtr() = default;
    };
    template<class T>
    class MethodPtrImplementation: public MethodPtr
    {
    public:
        typedef std::string(T::*ptr)(const std::string &);

        explicit MethodPtrImplementation(ptr mptr): methodPtr(mptr) {}

        std::string CallBack(arghandler *base, const std::string &param) override
        {
            return (static_cast<T*>(base)->*methodPtr)(param);
        }

     private:
        ptr methodPtr;
    };

    using argfun = std::unique_ptr<MethodPtr>;

    class option
    {
    public:
        const char *Short, *Long;
        std::string Descr;
        argfun handler;
        option(const char *s, const char *l, const std::string &d, argfun h)
              : Short(s), Long(l), Descr(d), handler(std::move(h)) { }

        option(option&&) = default;
        option(const option&) = delete;
        void operator=(const option&) = delete;
    };

    std::vector<option>      options{};
    std::vector<std::string> args{};

protected:
    std::string a0{};
    void subadd(const char *Short, const char *Long, const std::string &Descr, argfun handler);

public:
    arghandler(const char *defopts, int argc, const char *const *argv);
    virtual ~arghandler();

    template<typename T>
    inline void add(const char *Short, const char *Long, const std::string &Descr, std::string(T::*handler)(const std::string &))
    {
        auto callback = std::make_unique<MethodPtrImplementation<T>>(handler);
        subadd(Short, Long, Descr, std::move(callback));
    }

    virtual void parse();
    virtual void defarg(const std::string &s) = 0;
    void argerror(char c);
    void argerror(const std::string &s, bool param = true);
    void listoptions();
    virtual void suggesthelp();
};

#endif

#endif
