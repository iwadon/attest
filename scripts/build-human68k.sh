#!/bin/sh
set -e
project_root=$(cd "$(dirname "$0")/.." && pwd)
cmake -S "${project_root}" -B "${project_root}/build-human68k" -G Ninja -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE="${project_root}/cmake/human68k.cmake" -D ATTEST_BUILD_TESTING=ON
ninja -C "${project_root}/build-human68k"
