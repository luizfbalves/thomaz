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
        # CRITICAL (Pitfall 1, the real root cause): hactool's C TUs include
        # <mbedtls/cipher.h> with angle brackets. Without this -I, that resolves
        # to the devkitPro PORTLIB mbedtls headers (different config) while the
        # linked implementation is OUR mbedtls. The mbedtls_cipher_context_t /
        # cipher_base_t struct layouts then DISAGREE between caller (portlib
        # header) and callee (our compiled .c), so cipher_setkey reads the wrong
        # fields and AES-XTS decrypt setkey fails with -0x6100 (enc succeeds, dec
        # fails — the on-hardware symptom). This -I is searched before the
        # toolchain's -isystem portlib dir, so hactool now compiles against the
        # SAME headers and config as the mbedtls library it actually links.
        ${CMAKE_SOURCE_DIR}/lib/mbedtls/include
)

# Compile hactool against the SAME mbedtls user config the library was built
# with (CMAC on, self-test off, etc.). Must match lib/mbedtls/CMakeLists.txt
# exactly, or the config-gated struct fields drift again.
target_compile_definitions(hactool
    PRIVATE
        MBEDTLS_USER_CONFIG_FILE="${CMAKE_SOURCE_DIR}/lib/mbedtls/thomaz_cmac_config.h"
)

# CRITICAL: define __SWITCH__ for the hactool target. Borealis' toolchain.cmake
# only does `set(__SWITCH__ ON)` (a CMake var, NOT a -D), and because USE_GLFW
# defaults ON on Switch it `add_definitions(-D__GLFW__)` at directory scope —
# which hactool inherits. The app target gets -D__SWITCH__ transitively from the
# borealis library it links, but hactool links NO borealis, so without this it
# compiled with __GLFW__ and NO __SWITCH__. That excluded every `#ifdef __SWITCH__`
# block in hactool — most importantly nca_decrypt_key_area's SPL key-area
# derivation, falling back to the desktop path that reads an EMPTY key_area_keys
# table -> zero section keys -> "section N is corrupted!". Define it so the
# on-device SPL crypto path (and <switch.h>) compile in.
if (PLATFORM_SWITCH)
    target_compile_definitions(hactool PRIVATE __SWITCH__)
endif ()

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
