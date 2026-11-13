# SPDX-License-Identifier: GPL-3.0-or-later
# Hardware-independent core. Builds as an ESP-IDF component or as a plain host library.
set(CORE_SRCS
    "src/transport/safety_gate.cpp"
    "src/transport/isotp.cpp"
    "src/transport/can_detect.cpp"
    "src/obd/obd.cpp"
    "src/obd/vin.cpp"
    "src/obd/obd_session.cpp"
    "src/signals/units.cpp"
    "src/signals/signal.cpp"
    "src/signals/signal_store.cpp"
    "src/profiles/profile.cpp"
    "src/util/json.cpp"
    "src/scheduler/poll_scheduler.cpp"
    "src/display/gauge_math.cpp"
    "src/display/screens.cpp"
    "src/display/theme.cpp"
    "src/display/boot_sequence.cpp"
    "src/input/input.cpp"
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
