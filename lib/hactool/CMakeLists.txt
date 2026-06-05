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
