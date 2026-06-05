# lib/hactool/CMakeLists.txt
#
# Builds the exelix11 hactool fork as a static C library (`hactool`).
# This target is ONLY added when PLATFORM_SWITCH (see root CMakeLists.txt
# if-guard). The hactool fork adds in-memory buffer + filter callbacks
# required by the keyless NCA RomFS extraction path (EXTRACT-04).
#
# Upstream: https://github.com/exelix11/SwitchThemeInjector
# Pinned commit: 2618b0c31e007d019757dc4095eca08b4a89e3f5
# Imported path: SwitchThemesNX/Libs/hactool
# License: GPL v2

cmake_minimum_required(VERSION 3.10)

set(HACTOOL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB HACTOOL_SRC
    ${HACTOOL_DIR}/source/*.c
)

add_library(hactool STATIC ${HACTOOL_SRC})

target_include_directories(hactool
    PUBLIC
        ${HACTOOL_DIR}/include
    PRIVATE
        ${HACTOOL_DIR}/source
)

# The hactool fork targets devkitA64 (armv8-a+crc+crypto). The upstream
# Makefile uses -Wall -Werror; toolchain drift under the current devkitA64
# may surface new warnings. Per Assumption A2 (01-RESEARCH.md), relax
# -Werror for this vendored target only — the app's own warning flags are
# not affected.
target_compile_options(hactool PRIVATE -Wno-error)

# Library-safety: force-include hactool_recover.h into every hactool TU so its
# process-killing exit() calls become a recoverable longjmp back to the C++
# wrapper, instead of terminating the whole app on a parse/key error (the
# vendored fork is full of exit(EXIT_FAILURE) + FATAL_ERROR). hactool_recover.c
# (picked up by the GLOB above) provides the jmp_buf + recovery function.
target_compile_options(hactool PRIVATE -include ${HACTOOL_DIR}/source/hactool_recover.h)

# hactool is a C library; no C++ standard required here.
# The C++ facade (firmware_extract_switch.cpp) links against this target.
