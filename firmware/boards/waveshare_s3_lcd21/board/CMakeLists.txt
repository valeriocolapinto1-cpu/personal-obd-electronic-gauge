# SPDX-License-Identifier: GPL-3.0-or-later
idf_component_register(SRCS "board.cpp" "board_lvgl.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES hal lvgl
                       PRIV_REQUIRES esp_driver_i2c esp_driver_spi esp_driver_gpio esp_driver_ledc
                                     esp_driver_sdmmc esp_lcd fatfs sdmmc esp_timer)
