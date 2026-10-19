#!/usr/bin/env sh
# Build and run the C++23 test suite with the selected compiler.
# Example: CXX=clang++ BUILD_DIR=/tmp/dirr-clang ./compiler-tests.sh
set -eu

: "${CXX:=c++}"
: "${BUILD_DIR:=${TMPDIR:-/tmp}/dirr-cxx23-build}"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DDIRR_ENABLE_LTO=OFF
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
