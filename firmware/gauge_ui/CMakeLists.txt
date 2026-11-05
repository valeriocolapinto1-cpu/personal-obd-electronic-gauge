# SPDX-License-Identifier: GPL-3.0-or-later
# LVGL screens. Display-agnostic: sizes and shape come from hal::DisplayInfo.
idf_component_register(SRCS "gauge_ui.cpp" "analog_view.cpp" "digital_view.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES core gauge_hal lvgl)
