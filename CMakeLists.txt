cmake_minimum_required(VERSION 3.20)
project(dirr
    VERSION 3.39.1
    DESCRIPTION "Colourful ls replacement — dirr"
    HOMEPAGE_URL "https://iki.fi/bisqwit/source/dirr.html"
    LANGUAGES CXX
)

# ─────────────────────────────────────────────────────────────────────
#  C++23 — strict, no extensions
# ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─────────────────────────────────────────────────────────────────────
#  Options
# ─────────────────────────────────────────────────────────────────────
option(DIRR_STATIC "Build statically linked binary (Linux)" OFF)
option(DIRR_ENABLE_LTO "Enable link-time optimisation" ON)
option(DIRR_ENABLE_SANITIZER "Enable address/undefined sanitizers" OFF)
option(DIRR_PRELOAD_UIDGID "Preload passwd/group at startup" OFF)
option(DIRR_ALWAYS_SHOW_DOTFILES "Show dot-files by default" OFF)
option(BUILD_TESTING "Build tests" ON)

if(DIRR_PRELOAD_UIDGID)
    set(DIRR_PRELOAD_UIDGID_VALUE 1)
else()
    set(DIRR_PRELOAD_UIDGID_VALUE 0)
endif()
if(DIRR_ALWAYS_SHOW_DOTFILES)
    set(DIRR_ALWAYS_SHOW_DOTFILES_VALUE 1)
else()
    set(DIRR_ALWAYS_SHOW_DOTFILES_VALUE 0)
endif()

# ─────────────────────────────────────────────────────────────────────
#  Feature detection — replaces old ./configure shell script
# ─────────────────────────────────────────────────────────────────────
include(CheckIncludeFileCXX)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)
include(GNUInstallDirs)

# Essential POSIX headers
check_include_file_cxx("sys/stat.h"      HAVE_SYS_STAT_H)
check_include_file_cxx("sys/types.h"     HAVE_SYS_TYPES_H)
check_include_file_cxx("sys/ioctl.h"     HAVE_IOCTL_SYS_IOCTL_H)
check_include_file_cxx("sys/file.h"     HAVE_FLOCK_SYS_FILE_H)
check_include_file_cxx("sys/vfs.h"       HAVE_STATFS_SYS_VFS_H)
check_include_file_cxx("sys/statfs.h"   HAVE_STATFS_SYS_STATFS_H)
check_include_file_cxx("sys/mount.h"    HAVE_STATFS_SYS_MOUNT_H)
check_include_file_cxx("sys/statvfs.h"  HAVE_STATFS_SYS_STATVFS_H)
check_include_file_cxx("termios.h"      HAVE_TCGETATTR_TERMIOS_H)
check_include_file_cxx("termio.h"       HAVE_TERMIO_H)
check_include_file_cxx("dirent.h"       HAVE_READDIR_DIRENT_H)
check_include_file_cxx("grp.h"          HAVE_GETGRENT_GRP_H)
check_include_file_cxx("pwd.h"          HAVE_GETPWENT_PWD_H)
if(NOT HAVE_SYS_STAT_H OR NOT HAVE_SYS_TYPES_H OR NOT HAVE_READDIR_DIRENT_H)
    message(FATAL_ERROR "dirr requires POSIX sys/stat.h, sys/types.h and dirent.h")
endif()
# These functions share the same headers as the *ent variants.
set(HAVE_GETGRGID_GRP_H ${HAVE_GETGRENT_GRP_H})
set(HAVE_GETPWUID_PWD_H ${HAVE_GETPWENT_PWD_H})
check_include_file_cxx("ext/stdio_filebuf.h" HAVE_STDIO_FILEBUF)

# Functions: use CheckCXXSymbolExists with the required declaration header.
check_cxx_symbol_exists(statfs "sys/vfs.h;sys/statfs.h;sys/mount.h;sys/statvfs.h" HAVE_STATFS)
check_cxx_symbol_exists(ioctl  "sys/ioctl.h;unistd.h" HAVE_IOCTL)
check_cxx_symbol_exists(tcgetattr "termios.h" HAVE_TCGETATTR)
check_cxx_symbol_exists(readdir "dirent.h" HAVE_READDIR)
check_cxx_symbol_exists(getgrent "grp.h" HAVE_GETGRENT)
check_cxx_symbol_exists(getpwent "pwd.h" HAVE_GETPWENT)
check_cxx_symbol_exists(getgrgid "grp.h" HAVE_GETGRGID)
check_cxx_symbol_exists(getpwuid "pwd.h" HAVE_GETPWUID)
check_cxx_symbol_exists(flock "sys/file.h" HAVE_FLOCK)

# C++23 / C++20 feature probes
check_cxx_source_compiles("
    #include <concepts>
    template<typename T> concept C = true;
    int main(){}
" HAVE_CONCEPTS)
if(NOT HAVE_CONCEPTS)
    message(FATAL_ERROR "dirr requires C++20 concepts (included in C++23)")
endif()

check_cxx_source_compiles("
    #include <charconv>
    int main(){ char buf[16]; auto r = std::to_chars(buf, buf+16, 42); return 0; }
" HAVE_CHARCONV)

check_cxx_source_compiles("
    #include <string_view>
    int main(){ std::string_view sv=\"hi\"; return 0; }
" HAVE_STRINGVIEW)

check_cxx_source_compiles("
    #include <type_traits>
    static_assert(std::is_trivially_copyable_v<int>);
    int main(){}
" HAVE_IS_TRIVIALLY_COPYABLE)

check_cxx_source_compiles("
    #include <type_traits>
    using T = std::remove_cvref_t<int&>;
    int main(){}
" HAVE_REMOVE_CVREF)

check_cxx_source_compiles("
    int main(){ char8_t c = u8'x'; return 0; }
" HAVE_CHAR8_T)

# std::expected is used in the C++23 POSIX wrappers; fail early if the library lacks it.
check_cxx_source_compiles("
    #include <expected>
    #include <string>
    int main() {
        std::expected<int, std::string> value = 42;
        return value.value() == 42 ? 0 : 1;
    }
" HAVE_EXPECTED)
if(NOT HAVE_EXPECTED)
    message(FATAL_ERROR "dirr requires a C++23 standard library with std::expected")
endif()
# Optional newer library facilities. Probe the APIs we actually use rather than
# only checking that the headers exist (some libstdc++ releases ship partial headers).
check_cxx_source_compiles("
    #include <format>
    #include <string>
    #include <string_view>
    #include <utility>
    template<typename... Args>
    std::string typed_format(std::format_string<Args...> fmt, Args&&... args) {
        return std::format(fmt, std::forward<Args>(args)...);
    }
    int main() {
        const std::string owned{\"owned\"};
        const auto text = typed_format(\"{:04} {} {}\", 42, std::string_view{\"probe\"}, owned);
        const auto octal = std::format(\"{:032o}\", 7u);
        const auto fixed = std::format(\"{:.{}f}\", 1.25, 1u);
        return text.empty() || octal.empty() || fixed.empty();
    }
" HAVE_FORMAT)
check_cxx_source_compiles("
    #include <cstdio>
    #include <print>
    #include <string_view>
    int main() {
        std::print(stdout, \"{}\", std::string_view{\"probe\"});
    }
" HAVE_STD_PRINT)
set(HAVE_PRINT ${HAVE_STD_PRINT})
check_include_file_cxx("filesystem" HAVE_FILESYSTEM)
if(NOT HAVE_FILESYSTEM)
    message(FATAL_ERROR "dirr requires <filesystem>")
endif()

# ─────────────────────────────────────────────────────────────────────
#  Generate config.h from template
#  Source files include <config.h> (angle brackets) so this generated
#  platform-specific header takes precedence over the Makefile fallback.
# ─────────────────────────────────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

# ─────────────────────────────────────────────────────────────────────
#  Sources — explicit list, no globs
# ─────────────────────────────────────────────────────────────────────
set(DIRR_SOURCES
    main.cc
    argh.cc
    cons.cc
    setfun.cc
    strfun.cc
    colouring.cc
    getname.cc
    getsize.cc
    totals.cc
    pwfun.cc
    dfa_match.cc
    printf.cc
)

add_executable(dirr ${DIRR_SOURCES})

target_include_directories(dirr BEFORE PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"  # generated config.h first
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Use modern compile options
target_compile_options(dirr PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra -Wundef -Wcast-qual -Wpointer-arith
        -Wconversion -Wwrite-strings -Wsign-compare -Wredundant-decls
        -Winit-self -Wparentheses -Wcast-align -Wformat
        -Woverloaded-virtual -Wshadow -Wimplicit-fallthrough
        -Werror
        -funsigned-char
        $<$<BOOL:${DIRR_ENABLE_SANITIZER}>:-fsanitize=address,undefined>
    >
)

# Optimisation / LTO
if(DIRR_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_ok OUTPUT ipo_err)
    if(ipo_ok)
        set_property(TARGET dirr PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(DIRR_ENABLE_SANITIZER)
    target_link_options(dirr PRIVATE -fsanitize=address,undefined)
endif()

# Static build
if(DIRR_STATIC)
    target_link_options(dirr PRIVATE -static -Wl,--gc-sections)
else()
    target_link_options(dirr PRIVATE -Wl,--gc-sections)
endif()

# Required libs
find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(dirr PRIVATE Threads::Threads)
endif()

# Link filesystem if needed (GCC <9 needs -lstdc++fs, but we are GCC12 so not)
if(HAVE_FILESYSTEM)
    # No extra link needed on modern libstdc++
endif()

# Version as compile definition — replaces Makefile CPPFLAGS -DVERSION=...
target_compile_definitions(dirr PRIVATE VERSION="${PROJECT_VERSION}")

# ─────────────────────────────────────────────────────────────────────
#  Install
# ─────────────────────────────────────────────────────────────────────
install(TARGETS dirr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ─────────────────────────────────────────────────────────────────────
#  Testing — CTest
# ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    enable_testing()

    add_executable(dirr-dfa-cache-smoke
        tests/dfa-cache-smoke.cc
        dfa_match.cc
    )
    target_include_directories(dirr-dfa-cache-smoke BEFORE PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(Threads_FOUND)
        target_link_libraries(dirr-dfa-cache-smoke PRIVATE Threads::Threads)
    endif()
    target_compile_options(dirr-dfa-cache-smoke PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall -Wextra -Wundef -Wconversion -Wsign-compare -Wformat -Werror
        >
    )
    if(DIRR_ENABLE_SANITIZER)
        target_compile_options(dirr-dfa-cache-smoke PRIVATE -fsanitize=address,undefined)
        target_link_options(dirr-dfa-cache-smoke PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME dirr-dfa-cache-smoke COMMAND dirr-dfa-cache-smoke)

    add_executable(dirr-formatting-smoke
        tests/formatting-smoke.cc
        printf.cc
    )
    target_include_directories(dirr-formatting-smoke BEFORE PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(dirr-formatting-smoke PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall -Wextra -Wundef -Wconversion -Wsign-compare -Wformat -Werror
        >
    )
    if(DIRR_ENABLE_SANITIZER)
        target_compile_options(dirr-formatting-smoke PRIVATE -fsanitize=address,undefined)
        target_link_options(dirr-formatting-smoke PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME dirr-formatting-smoke COMMAND dirr-formatting-smoke)

    add_test(NAME dirr-help
        COMMAND dirr --help
    )
    set_tests_properties(dirr-help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

    add_test(NAME dirr-version
        COMMAND dirr --version
    )
    set_tests_properties(dirr-version PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

    add_test(NAME dirr-list-tmp
        COMMAND dirr -C -c ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test(NAME dirr-list-dot
        COMMAND dirr -A ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_test(NAME dirr-cli-smoke
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-smoke.sh $<TARGET_FILE:dirr>
    )

    # Smoke test for all predefined aliases
    foreach(alias IN ITEMS 0 1 2 3 4 5 6)
        add_test(NAME dirr-alias-a${alias}
            COMMAND dirr -a${alias} ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endforeach()
endif()

# ─────────────────────────────────────────────────────────────────────
#  Summary
# ─────────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "dirr ${PROJECT_VERSION} — C++23 configuration")
message(STATUS "  C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Static        : ${DIRR_STATIC}")
message(STATUS "  LTO           : ${DIRR_ENABLE_LTO}")
message(STATUS "  Sanitizer     : ${DIRR_ENABLE_SANITIZER}")
message(STATUS "  HAVE_CONCEPTS : ${HAVE_CONCEPTS}")
message(STATUS "  HAVE_EXPECTED : ${HAVE_EXPECTED}")
message(STATUS "  HAVE_FORMAT   : ${HAVE_FORMAT}")
message(STATUS "  HAVE_PRINT    : ${HAVE_PRINT}")
message(STATUS "  HAVE_FILESYSTEM: ${HAVE_FILESYSTEM}")
message(STATUS "")

# ─────────────────────────────────────────────────────────────────────
#  CPack (optional archives)
# ─────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "dirr")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_SOURCE_GENERATOR "TGZ;TXZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;~$;.o$;.a$")
include(CPack)
