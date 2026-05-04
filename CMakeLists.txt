cmake_minimum_required(VERSION 3.16)
project(attest VERSION 0.1.0 LANGUAGES C)

# PROJECT_IS_TOP_LEVEL は CMake 3.21+ なので互換変数を用意する
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        set(PROJECT_IS_TOP_LEVEL TRUE)
    else()
        set(PROJECT_IS_TOP_LEVEL FALSE)
    endif()
endif()

# ライブラリ本体は C99 で動作可能。_Generic 等の C11 機能は
# include/attest/attest.h 内で __STDC_VERSION__ ガード付きで提供される。
# selftest 側はターゲットごとに C_STANDARD を個別指定する。
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Thread library detection (for parallel execution support)
find_package(Threads)

set(ATTEST_SOURCES
    src/attest.c
    src/attest_cli.c
    src/attest_assert.c
    src/attest_capture.c
    src/attest_fixture.c
    src/attest_parallel.c
    src/internal/attest_registry.c
)

# Platform-specific timeout implementation
if(WIN32)
    list(APPEND ATTEST_SOURCES src/internal/attest_timeout_win32.c)
else()
    list(APPEND ATTEST_SOURCES src/internal/attest_timeout_posix.c)
endif()

add_library(attest STATIC ${ATTEST_SOURCES})
add_library(attest::attest ALIAS attest)

target_include_directories(attest
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link pthread library if available (for parallel execution)
if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(attest PRIVATE Threads::Threads)
endif()

# Platform-specific compile definitions
if(WIN32)
    # Windows: Suppress MSVC security warnings (also affects consumers such
    # as the test binaries, where clang-cl warns about deprecated CRT calls
    # like getenv that MSVC's own headers tag with __declspec(deprecated)).
    target_compile_definitions(attest PUBLIC
        _CRT_SECURE_NO_WARNINGS
    )
    # MSVC: Treat sources as UTF-8 to avoid C4819 on Japanese (CP932)
    # Windows systems without forcing the execution charset for consumers.
    if(MSVC)
        target_compile_options(attest PUBLIC /source-charset:utf-8)
    endif()
else()
    # POSIX: Define _POSIX_C_SOURCE for signal and timer features
    target_compile_definitions(attest PRIVATE
        _POSIX_C_SOURCE=200809L
    )
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

  # selftest_main.c は _Generic を使うため C11 を要求する
  set_target_properties(attest_selftest PROPERTIES
      C_STANDARD 11
      C_STANDARD_REQUIRED ON
  )

  # Link math library on Unix-like systems and Human68k (for nextafter in ULP tests)
  if(UNIX OR ATTEST_TARGET_HUMAN68K)
      target_link_libraries(attest_selftest PRIVATE m)
  endif()

  # Platform-specific compile definitions for tests
  if(NOT WIN32)
      target_compile_definitions(attest_selftest PRIVATE _POSIX_C_SOURCE=200809L)
  endif()

  add_test(NAME attest_selftest COMMAND attest_selftest)

  # C99スモークテスト: 明示型マクロのみで主要機能を検証する
  add_executable(attest_selftest_c99
      tests/selftest_c99.c
  )
  target_link_libraries(attest_selftest_c99 PRIVATE attest)
  set_target_properties(attest_selftest_c99 PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
  )
  if(UNIX OR ATTEST_TARGET_HUMAN68K)
      target_link_libraries(attest_selftest_c99 PRIVATE m)
  endif()
  if(NOT WIN32)
      target_compile_definitions(attest_selftest_c99 PRIVATE _POSIX_C_SOURCE=200809L)
  endif()
  add_test(NAME attest_selftest_c99 COMMAND attest_selftest_c99)

  # Timeout test executable (for manual testing)
  add_executable(attest_timeout_test
      tests/timeout_test.c
  )
  target_link_libraries(attest_timeout_test PRIVATE attest)

  # Platform-specific compile definitions for timeout test
  if(NOT WIN32)
      target_compile_definitions(attest_timeout_test PRIVATE _POSIX_C_SOURCE=200809L)
  endif()

  # Thread detection test executable (for verifying Phase 1 implementation)
  add_executable(attest_thread_detection_test
      tests/test_thread_detection.c
  )
  target_include_directories(attest_thread_detection_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  # Platform-specific compile definitions for thread detection test
  if(NOT WIN32)
      target_compile_definitions(attest_thread_detection_test PRIVATE _POSIX_C_SOURCE=200809L)
  endif()
endif()

# ---------------------------------------------------------------------------
# Install / export rules (find_package(attest) support)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ATTEST_INSTALL "Generate install rules for attest" ${PROJECT_IS_TOP_LEVEL})

if(ATTEST_INSTALL)
    install(TARGETS attest
        EXPORT attestTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/attest
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT attestTargets
        FILE attestTargets.cmake
        NAMESPACE attest::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attest
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/attestConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/attestConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attest
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/attestConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/attestConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/attestConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/attest
    )
endif()
