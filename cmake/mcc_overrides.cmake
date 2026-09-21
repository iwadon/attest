# Loaded early via CMAKE_USER_MAKE_RULES_OVERRIDE from mcc.cmake, before
# CMakeCInformation.cmake resets CMAKE_C_OUTPUT_EXTENSION to .obj on the
# "Generic" platform. See mcc.cmake for the full rationale.
set(CMAKE_C_OUTPUT_EXTENSION ".o")
set(CMAKE_EXECUTABLE_SUFFIX ".x")

# --- C_STANDARD support -----------------------------------------------------
#
# CMake only sets CMAKE_C<N>_STANDARD_COMPILE_OPTION / CMAKE_C_STANDARD_DEFAULT
# for compiler IDs it recognizes (Modules/Compiler/<ID>-C.cmake); mcc's ID is
# empty, so it's never probed and C_STANDARD is silently ignored otherwise.
# We set them here instead — read by plain name regardless of who set them.
#
# This must live here, not in mcc.cmake: check_c_source_compiles()/
# try_compile() run a nested configure that reuses this file (via the cached
# CMAKE_USER_MAKE_RULES_OVERRIDE) but never re-runs the toolchain file, so a
# variable computed only in mcc.cmake would be invisible there. CMAKE_C_COMPILER
# is reliably set by this point in both cases, so the `-std=` probe runs here.
execute_process(
    COMMAND "${CMAKE_C_COMPILER}" --help
    OUTPUT_VARIABLE _mcc_help_output
    ERROR_QUIET)

if(_mcc_help_output MATCHES "-std=")
    # Current mcc: -std=c99|c11|c17|c23, default c23, no c89/c90 mode.
    set(CMAKE_C_STANDARD_DEFAULT "23")
    foreach(_mcc_std 99 11 17 23)
        set(CMAKE_C${_mcc_std}_STANDARD_COMPILE_OPTION "-std=c${_mcc_std}")
        set(CMAKE_C${_mcc_std}_EXTENSION_COMPILE_OPTION "-std=c${_mcc_std}")
    endforeach()
    unset(_mcc_std)
else()
    # Older mcc: no `-std=` flag at all, always compiles one fixed dialect
    # that matches C99.
    set(CMAKE_C_STANDARD_DEFAULT "99")
    set(CMAKE_C99_STANDARD_COMPILE_OPTION "")
    set(CMAKE_C99_EXTENSION_COMPILE_OPTION "")
endif()
unset(_mcc_help_output)
