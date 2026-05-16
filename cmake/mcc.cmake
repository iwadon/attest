set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)

find_program(MCC_EXECUTABLE mcc)
if(NOT MCC_EXECUTABLE)
    message(FATAL_ERROR
        "Could not locate the mcc compiler. "
        "Install it and ensure `mcc` is on PATH, or pass -DMCC_EXECUTABLE=<path>.")
endif()
find_program(MCC_AR_EXECUTABLE mcc-ar)
if(NOT MCC_AR_EXECUTABLE)
    message(FATAL_ERROR
        "Could not locate the mcc archiver. "
        "Install it and ensure `mcc-ar` is on PATH, or pass -DMCC_AR_EXECUTABLE=<path>.")
endif()

execute_process(
    COMMAND ${MCC_EXECUTABLE} --print-sysroot
    OUTPUT_VARIABLE _mcc_sysroot
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _mcc_sysroot_result)

if(NOT _mcc_sysroot_result EQUAL 0 OR NOT IS_DIRECTORY "${_mcc_sysroot}")
    message(FATAL_ERROR
        "Failed to obtain sysroot from `${MCC_EXECUTABLE} --print-sysroot` "
        "(exit code ${_mcc_sysroot_result}, output: '${_mcc_sysroot}').")
endif()

set(CMAKE_SYSROOT "${_mcc_sysroot}")

set(CMAKE_C_COMPILER   "${MCC_EXECUTABLE}")
# set(CMAKE_CXX_COMPILER "${MCC_EXECUTABLE}")
set(CMAKE_AR "${MCC_AR_EXECUTABLE}")

# Human68k hupair archives have no symbol table — ranlib is a no-op.
# Point CMAKE_RANLIB at /usr/bin/true to keep CMake's link rules happy
# without invoking the host ranlib (which rejects non-mach-o archives).
find_program(_mcc_true_exe true REQUIRED)
set(CMAKE_RANLIB "${_mcc_true_exe}")
set(CMAKE_C_COMPILER_RANLIB "${_mcc_true_exe}")

# Human68k uses .o object files (not .obj). Also keeps archive member names
# within mcc-ar's 23-char limit. CMAKE_C_OUTPUT_EXTENSION_REPLACE makes
# CMake replace the source suffix instead of appending — i.e. foo.c.o
# becomes foo.o. The CMAKE_USER_MAKE_RULES_OVERRIDE indirection is needed
# because CMakeCInformation.cmake unconditionally resets
# CMAKE_C_OUTPUT_EXTENSION to .obj on non-UNIX platforms (we're "Generic"),
# overriding anything we set directly in this toolchain file.
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/mcc_overrides.cmake")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Human68k executables use the .x extension
set(CMAKE_EXECUTABLE_SUFFIX ".x")

# Flag to enable Human68k-specific source files
set(ATTEST_TARGET_HUMAN68K ON)
