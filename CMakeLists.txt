cmake_minimum_required(VERSION 3.16)
project(attest C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(attest STATIC
    src/attest.c
    src/attest_cli.c
    src/attest_assert.c
    src/attest_capture.c
    src/internal/attest_registry.c
)

target_include_directories(attest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(attest_runner
    src/attest_main.c
)

target_link_libraries(attest_runner PRIVATE attest)

enable_testing()

add_executable(attest_selftest
    tests/selftest_main.c
)

target_link_libraries(attest_selftest PRIVATE attest)

add_test(NAME attest_selftest COMMAND attest_selftest)
