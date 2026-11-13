# SPDX-License-Identifier: GPL-3.0-or-later
# Simulator: host library for tests, and an ESP-IDF component for demo builds.
set(SIM_SRCS
    src/virtual_can_bus.cpp
    src/fake_obd_ecu.cpp
    src/foreign_dongle.cpp
    src/demo_car.cpp)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${SIM_SRCS} INCLUDE_DIRS "include" REQUIRES core)
else()
    add_library(gauge_sim STATIC ${SIM_SRCS})
    target_include_directories(gauge_sim PUBLIC include)
    target_link_libraries(gauge_sim PUBLIC gauge_core)
endif()
