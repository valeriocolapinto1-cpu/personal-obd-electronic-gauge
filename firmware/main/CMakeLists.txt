# SPDX-License-Identifier: GPL-3.0-or-later
idf_component_register(SRCS "main.cpp" "bringup_ui.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES board hal lvgl)
