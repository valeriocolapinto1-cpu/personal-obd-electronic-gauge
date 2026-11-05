# SPDX-License-Identifier: GPL-3.0-or-later
add_library(gauge_sim STATIC
    src/virtual_can_bus.cpp
    src/fake_obd_ecu.cpp
    src/foreign_dongle.cpp)
target_include_directories(gauge_sim PUBLIC include)
target_link_libraries(gauge_sim PUBLIC gauge_core)
