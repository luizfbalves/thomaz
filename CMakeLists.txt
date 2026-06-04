# thomaz — Nintendo Switch homebrew cheat-manager hub.
# Built on the maintained Borealis fork (xfangfang/borealis), CMake-based,
# targeting Nintendo Switch (devkitPro) and desktop PC (GLFW) from one tree.
# Modeled on the upstream demo's CMakeLists.

cmake_minimum_required(VERSION 3.10)

set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/lib/borealis/library)

# Borealis build options
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

# Toolchain auto-detection (sets PLATFORM_SWITCH when devkitPro is present).
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# Project info
project(thomaz)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "3")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")
set(PACKAGE_NAME "tech.orul.thomaz")
set(PROJECT_AUTHOR "luizfbalves")
set(PROJECT_TITLEID "0x010000000000b001")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

# Build the Borealis library (out-of-source: give it a binary dir name).
add_subdirectory(${BOREALIS_LIBRARY} borealis)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# HTTP (libcurl) for downloading cheats from switch-cheats-db.
if (PLATFORM_SWITCH)
    # switch-curl + switch-mbedtls + switch-zlib ship in the devkitpro/devkita64
    # image's switch-portlibs meta-package (no dkp-pacman step needed). Headers
    # and libs live under $DEVKITPRO/portlibs/switch. Link order matters for
    # the static GNU linker: curl -> mbedtls -> z, then archive before all of its
    # decompression filter deps (bz2, lzma, zstd, lz4) and zlib last. switch-libarchive
    # is built with zstd+lz4 support, so those portlibs MUST be on the link line or
    # the zip/7zip/zstd/lz4 filters fail with undefined ZSTD_*/LZ4_* references.
    list(APPEND APP_PLATFORM_INCLUDE $ENV{DEVKITPRO}/portlibs/switch/include)
    link_directories($ENV{DEVKITPRO}/portlibs/switch/lib)
    list(APPEND APP_PLATFORM_LIB curl mbedtls mbedx509 mbedcrypto
                                 archive bz2 lzma zstd lz4 z)
elseif (PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    find_package(LibArchive REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl LibArchive::LibArchive)
endif ()

# Our sources: app entry + pure-logic core. core/*.hpp resolved via -Isource,
# nlohmann/json via -Ilib/json.
list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/json)
file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
endif ()

# Build target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# Package per platform
if (PLATFORM_DESKTOP)
    add_custom_target(${PROJECT_NAME}.data
        COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources)
    if (NOT USE_LIBROMFS)
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
    endif ()
elseif (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources)
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
