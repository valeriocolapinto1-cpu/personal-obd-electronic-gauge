# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)

# Board selection. Each board lives in boards/<name>/ and is an ESP-IDF component.
set(GAUGE_BOARD "waveshare_s3_lcd21" CACHE STRING "Target board (directory name under boards/)")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/hal"
    "${CMAKE_CURRENT_LIST_DIR}/boards/${GAUGE_BOARD}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(obd_gauge)
