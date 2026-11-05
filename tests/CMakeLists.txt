# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(unit_tests
    unit/test_safety_gate.cpp
    unit/test_isotp.cpp
    unit/test_obd.cpp
    unit/test_units_vin.cpp)
target_link_libraries(unit_tests PRIVATE gauge_core GTest::gtest_main)
gtest_discover_tests(unit_tests PROPERTIES LABELS unit)

add_executable(sim_tests
    sim/test_sim_bus.cpp)
target_link_libraries(sim_tests PRIVATE gauge_sim GTest::gtest_main)
gtest_discover_tests(sim_tests PROPERTIES LABELS sim)
