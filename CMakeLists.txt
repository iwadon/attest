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
    src/attest_fixture.c
    src/internal/attest_registry.c
)

target_include_directories(attest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# POSIXシグナル機能のために_POSIX_C_SOURCEを定義
target_compile_definitions(attest PRIVATE _POSIX_C_SOURCE=200809L)

# ARM64 GCC workaround for setjmp/longjmp stack alignment issues
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # GCC 14.x on ARM64 has known issues with sigjmp_buf/setjmp/longjmp
        # These flags help mitigate the problem but may not fully resolve it
        message(WARNING "GCC ${CMAKE_C_COMPILER_VERSION} on ARM64 detected. "
                "Known issues with setjmp/longjmp. Applying workaround flags. "
                "If tests crash, please use Clang: cmake -DCMAKE_C_COMPILER=clang")
        target_compile_options(attest PRIVATE
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -O1  # Lower optimization level for GCC 14.x ARM64
        )
    endif()
endif()

add_executable(attest_runner
    src/attest_main.c
)

target_link_libraries(attest_runner PRIVATE attest)

# テストのデフォルト: トップレベルプロジェクトならON、サブプロジェクトならOFF
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  option(ATTEST_BUILD_TESTING "Build attest self-tests" ON)
else()
  option(ATTEST_BUILD_TESTING "Build attest self-tests" OFF)
endif()

if(ATTEST_BUILD_TESTING)
  enable_testing()

  add_executable(attest_selftest
      tests/selftest_main.c
  )

  target_link_libraries(attest_selftest PRIVATE attest)
  target_include_directories(attest_selftest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(attest_selftest PRIVATE _POSIX_C_SOURCE=200809L)

  add_test(NAME attest_selftest COMMAND attest_selftest)
endif()
