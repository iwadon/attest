#!/bin/sh
set -e
project_root=$(cd "$(dirname "$0")/.." && pwd)
cmake -S "${project_root}" -B "${project_root}/build-mcc" -G Ninja -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE="${project_root}/cmake/mcc.cmake" -D ATTEST_BUILD_TESTING=ON
ninja -C "${project_root}/build-mcc"
