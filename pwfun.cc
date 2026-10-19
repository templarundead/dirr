#include "pwfun.hh"
#include <config.h>

#if defined(HAVE_GETPWENT_PWD_H) || defined(HAVE_GETPWUID_PWD_H)
# include <pwd.h>
#endif
#if defined(HAVE_GETGRENT_GRP_H) || defined(HAVE_GETGRGID_GRP_H)
# include <grp.h>
#endif

#include <mutex>
#include <string>
#include <unordered_map>

namespace {
// The traditional NSS lookup functions may return pointers to shared static
// storage. Serialize all calls (including passwd and group calls) and copy the
// name before releasing the lock.
std::mutex& nss_mutex() {
    static std::mutex mutex;
    return mutex;
}

template<class Id, class Lookup>
std::string cached_lookup(Id id, Lookup&& lookup) {
    static std::mutex cache_mutex;
    static std::unordered_map<Id, std::string> cache;

    std::scoped_lock lock(cache_mutex);
    if (const auto it = cache.find(id); it != cache.end())
        return it->second;

    std::string value = lookup(id);
    cache.emplace(id, value);
    return value;
}

#if PRELOAD_UIDGID && defined(HAVE_GETPWENT_PWD_H) && defined(HAVE_GETPWENT)
const std::unordered_map<int, std::string>& passwd_table() {
    static const auto table = [] {
        std::unordered_map<int, std::string> entries;
        std::scoped_lock lock(nss_mutex());
        ::setpwent();
        while (const passwd* entry = ::getpwent())
            entries.emplace(static_cast<int>(entry->pw_uid), entry->pw_name);
        ::endpwent();
        return entries;
    }();
    return table;
}
#endif

#if PRELOAD_UIDGID && defined(HAVE_GETGRENT_GRP_H) && defined(HAVE_GETGRENT)
const std::unordered_map<int, std::string>& group_table() {
    static const auto table = [] {
        std::unordered_map<int, std::string> entries;
        std::scoped_lock lock(nss_mutex());
        ::setgrent();
        while (const group* entry = ::getgrent())
            entries.emplace(static_cast<int>(entry->gr_gid), entry->gr_name);
        ::endgrent();
        return entries;
    }();
    return table;
}
#endif
} // namespace

std::string Getpwuid(int uid) {
#if defined(HAVE_GETPWUID) && defined(HAVE_GETPWENT_PWD_H)
# if PRELOAD_UIDGID && defined(HAVE_GETPWENT)
    const auto& table = passwd_table();
    if (const auto it = table.find(uid); it != table.end())
        return it->second;
    return {};
# else
    return cached_lookup(uid, [](int id) {
        std::scoped_lock lock(nss_mutex());
        if (const passwd* entry = ::getpwuid(static_cast<uid_t>(id)))
            return std::string(entry->pw_name);
        return std::string{};
    });
# endif
#else
    (void)uid;
    return {};
#endif
}

std::string Getgrgid(int gid) {
#if defined(HAVE_GETGRGID) && defined(HAVE_GETGRENT_GRP_H)
# if PRELOAD_UIDGID && defined(HAVE_GETGRENT)
    const auto& table = group_table();
    if (const auto it = table.find(gid); it != table.end())
        return it->second;
    return {};
# else
    return cached_lookup(gid, [](int id) {
        std::scoped_lock lock(nss_mutex());
        if (const group* entry = ::getgrgid(static_cast<gid_t>(id)))
            return std::string(entry->gr_name);
        return std::string{};
    });
# endif
#else
    (void)gid;
    return {};
#endif
}

std::optional<std::string> try_getpwuid(int uid) {
    if (auto name = Getpwuid(uid); !name.empty())
        return name;
    return std::nullopt;
}

std::optional<std::string> try_getgrgid(int gid) {
    if (auto name = Getgrgid(gid); !name.empty())
        return name;
    return std::nullopt;
}

std::expected<std::string, std::string> expect_getpwuid(int uid) {
    if (auto name = Getpwuid(uid); !name.empty())
        return name;
    return std::unexpected("unknown uid " + std::to_string(uid));
}

std::expected<std::string, std::string> expect_getgrgid(int gid) {
    if (auto name = Getgrgid(gid); !name.empty())
        return name;
    return std::unexpected("unknown gid " + std::to_string(gid));
}
