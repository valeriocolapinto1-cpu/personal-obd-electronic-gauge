# SPDX-License-Identifier: GPL-3.0-or-later
#
# Host build: hardware-independent core, simulator and tests.
# The firmware itself is built with ESP-IDF from firmware/ (see README).
cmake_minimum_required(VERSION 3.16)
project(obd_gauge_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GAUGE_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wno-sign-conversion)
    if(GAUGE_SANITIZE)
        # vptr checks need RTTI, which core/ is built without.
        add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# GoogleTest: system package first (CI, Linux), otherwise download (Windows).
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)
enable_testing()

add_subdirectory(firmware/core)
add_subdirectory(firmware/gauge_hal)
add_subdirectory(sim)
add_subdirectory(tests)
