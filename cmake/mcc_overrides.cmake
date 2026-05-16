# Loaded early via CMAKE_USER_MAKE_RULES_OVERRIDE from mcc.cmake, before
# CMakeCInformation.cmake resets CMAKE_C_OUTPUT_EXTENSION to .obj on the
# "Generic" platform. See mcc.cmake for the full rationale.
set(CMAKE_C_OUTPUT_EXTENSION ".o")
