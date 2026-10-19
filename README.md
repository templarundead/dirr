# Dirr — colourful `ls` replacement

`dirr` lists files and directories on Unix-like systems. It is a colourful alternative to `/bin/ls`, with its own options and output formats; it is not intended to be fully compatible with `ls`.

## Requirements

- A C++23 compiler and standard library, including `std::expected` (for example, GCC 12+ with libstdc++ or a recent Clang/libc++ combination).
- CMake 3.20+ for the recommended build, or GNU make for the legacy build.

## Build with CMake (recommended)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Useful CMake options:

- `-DDIRR_STATIC=ON` — request a statically linked binary.
- `-DDIRR_ENABLE_LTO=OFF` — disable link-time optimisation.
- `-DDIRR_ENABLE_SANITIZER=ON` — build with AddressSanitizer and UBSan.
- `-DDIRR_PRELOAD_UIDGID=ON` — preload passwd/group names at startup.
- `-DDIRR_ALWAYS_SHOW_DOTFILES=ON` — show dot-files by default.

The executable is built as `build/dirr`; `cmake --install build` installs it using the configured `CMAKE_INSTALL_PREFIX`.

## Legacy GNU make build

The historical `configure` and make setup remain available:

```sh
./configure
make -j
```

The legacy configure script now requires C++23 as well. CMake is preferred because it keeps generated files in the build directory and provides the smoke-test suite.

## Usage

```sh
dirr --help
dirr --version
dirr -A -C .
```

Options can also be supplied through the `DIRR` environment variable. `DIRR_COLORS` customizes colour settings.

## Copying

Dirr was written by Joel Yliluoma (Bisqwit) and is distributed under the terms of the GNU General Public License; see [COPYING](COPYING). Portions were contributed by Warp.
