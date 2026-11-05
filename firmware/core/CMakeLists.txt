# SPDX-License-Identifier: GPL-3.0-or-later
# Hardware-independent core. Builds as an ESP-IDF component or as a plain host library.
set(CORE_SRCS
    "src/transport/safety_gate.cpp"
    "src/transport/isotp.cpp"
    "src/obd/obd.cpp"
    "src/obd/vin.cpp"
    "src/signals/units.cpp"
    "src/util/log.cpp")

if(ESP_PLATFORM)
    idf_component_register(SRCS ${CORE_SRCS} INCLUDE_DIRS "include")
else()
    list(TRANSFORM CORE_SRCS PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
    add_library(gauge_core STATIC ${CORE_SRCS})
    target_include_directories(gauge_core PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
    target_compile_features(gauge_core PUBLIC cxx_std_17)
    if(NOT MSVC)
        target_compile_options(gauge_core PRIVATE -fno-exceptions -fno-rtti)
    endif()
endif()
