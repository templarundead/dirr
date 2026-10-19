#include "strfun.hh"
#include <config.h>

#include <array>
#include <system_error>
#include <unistd.h>

[[nodiscard]] std::string_view NameOnly(std::string_view path) noexcept {
    if (auto pos = path.rfind('/'); pos != std::string_view::npos) {
        // Regular file: everything after last '/'
        if (pos + 1 < path.size()) {
            path.remove_prefix(pos + 1);
            return path;
        }
        // Trailing '/' — strip it and re-search
        // e.g. "/usr/bin/" → "bin"
        if (path.size() == 1) return {}; // root "/"
        auto second = path.substr(0, path.size() - 1).rfind('/');
        if (second == std::string_view::npos) {
            path.remove_suffix(1);
            return path;
        }
        path.remove_prefix(second + 1);
        path.remove_suffix(1);
        return path;
    }
    return path;
}

[[nodiscard]] std::string_view DirOnly(std::string_view path) noexcept {
    if (auto pos = path.rfind('/'); pos != std::string_view::npos) {
        path.remove_suffix(path.size() - (pos + 1));
        return path;
    }
    return {};
}

[[nodiscard]] std::string LinkTarget(const std::string& link, bool fixit) {
    // Use a bounded buffer; construct the result with an explicit byte count.
    std::array<char, PATH_MAX + 1> buf{};
    const auto len = ::readlink(link.c_str(), buf.data(), static_cast<std::size_t>(PATH_MAX));
    const std::size_t length = (len < 0) ? 0 : static_cast<std::size_t>(len);

    if (!fixit || (length > 0 && buf[0] == '/')) {
        return std::string(buf.data(), length);
    }

    // Relative link — prepend directory component.
    std::string_view dir = DirOnly(link);
    std::string result;
    result.reserve(dir.size() + length);
    result.append(dir);
    result.append(buf.data(), length);
    return result;
}

std::string GetError(int err) {
    return std::error_code(err, std::generic_category()).message();
}

[[nodiscard]] std::string Relativize(std::string_view base, std::string_view name) {
    // Modernised with explicit loop + string_view::compare
    // (kept string_view for zero-copy; std::filesystem::relative is similar
    // but handles symlinks differently — we preserve original semantics.)

    while (true) {
        const auto slash_base = base.find('/');
        const auto slash_name = name.find('/');
        if (slash_base == std::string_view::npos || slash_name == std::string_view::npos)
            break;
        if (base.compare(0, slash_base, name, 0, slash_base) != 0)
            break;
        base.remove_prefix(slash_base + 1);
        name.remove_prefix(slash_name + 1);
    }

    // Strip leading "./" sequences from name
    while (name.size() >= 2 && name[0] == '.' && name[1] == '/')
        name.remove_prefix(2);

    if (name.empty() || name[0] == '/')
        return std::string(name);

    std::size_t dotdots = 0;
    while (true) {
        while (base.size() >= 2 && base[0] == '.' && base[1] == '/')
            base.remove_prefix(2);
        const auto slash = base.find('/');
        if (slash == std::string_view::npos) break;
        base.remove_prefix(slash + 1);
        ++dotdots;
    }

    // Build "../" * dotdots + name efficiently
    std::string out;
    out.reserve(dotdots * 3 + name.size());
    for (std::size_t i = 0; i < dotdots; ++i) out += "../";
    out += name;
    return out;
}
