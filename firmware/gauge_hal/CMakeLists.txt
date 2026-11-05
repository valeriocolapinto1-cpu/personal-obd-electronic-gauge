# SPDX-License-Identifier: GPL-3.0-or-later
# Hardware abstraction interfaces (header only, no ESP-IDF headers).
if(ESP_PLATFORM)
    idf_component_register(INCLUDE_DIRS "include")
else()
    add_library(gauge_hal INTERFACE)
    target_include_directories(gauge_hal INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")
endif()
