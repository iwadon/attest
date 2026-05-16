set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)

set(triple m68k-xelf)

# Resolve elf2x68k installation root with the following precedence:
#   1. -DELF2X68K_ROOT=... on the command line
#   2. ELF2X68K_ROOT environment variable
#   3. `brew --prefix elf2x68k` (Homebrew users)
#   4. Parent of the directory containing m68k-xelf-gcc on PATH
if(NOT ELF2X68K_ROOT AND DEFINED ENV{ELF2X68K_ROOT})
    set(ELF2X68K_ROOT "$ENV{ELF2X68K_ROOT}")
endif()

if(NOT ELF2X68K_ROOT)
    find_program(_brew_executable brew)
    if(_brew_executable)
        execute_process(
            COMMAND ${_brew_executable} --prefix elf2x68k
            OUTPUT_VARIABLE _brew_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _brew_result)
        if(_brew_result EQUAL 0 AND IS_DIRECTORY "${_brew_prefix}")
            set(ELF2X68K_ROOT "${_brew_prefix}")
        endif()
    endif()
endif()

if(NOT ELF2X68K_ROOT)
    find_program(_m68k_xelf_gcc ${triple}-gcc)
    if(_m68k_xelf_gcc)
        get_filename_component(_xelf_bin "${_m68k_xelf_gcc}" DIRECTORY)
        get_filename_component(ELF2X68K_ROOT "${_xelf_bin}" DIRECTORY)
    endif()
endif()

if(NOT ELF2X68K_ROOT)
    message(FATAL_ERROR
        "Could not locate the elf2x68k toolchain. "
        "Install it (e.g. `brew install yunkya2/tap/elf2x68k`) and ensure "
        "${triple}-gcc is on PATH, or pass -DELF2X68K_ROOT=<install-prefix>.")
endif()

set(CMAKE_SYSROOT "${ELF2X68K_ROOT}")

set(CMAKE_C_COMPILER   "${ELF2X68K_ROOT}/bin/${triple}-gcc")
set(CMAKE_CXX_COMPILER "${ELF2X68K_ROOT}/bin/${triple}-g++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Human68k executables use the .x extension
set(CMAKE_EXECUTABLE_SUFFIX ".x")

# Flag to enable Human68k-specific source files
set(ATTEST_TARGET_HUMAN68K ON)
