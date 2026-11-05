// SPDX-License-Identifier: GPL-3.0-or-later
//
// Universal OBD Gauge - firmware entry point.
// Phase 0: board bring-up only. No vehicle bus code is linked in this build.
#include "board/board.h"
#include "bringup_ui.h"
#include "core/util/log.h"

#include "esp_log.h"

namespace {
const char *TAG = "main";

void core_log_sink(core::log::Level level, const char *tag, const char *message)
{
    switch (level) {
        case core::log::Level::Error:
            ESP_LOGE(tag, "%s", message);
            break;
        case core::log::Level::Warn:
            ESP_LOGW(tag, "%s", message);
            break;
        case core::log::Level::Info:
            ESP_LOGI(tag, "%s", message);
            break;
        case core::log::Level::Debug:
            ESP_LOGD(tag, "%s", message);
            break;
    }
}
} // namespace

extern "C" void app_main(void)
{
    core::log::set_sink(core_log_sink);
    ESP_LOGI(TAG, "Universal OBD Gauge - bench bring-up");
    if (!board::init()) {
        ESP_LOGE(TAG, "board init failed");
        return;
    }

    // The SD card shares GPIO1/2 with the LCD init SPI: mount it after board::init().
    hal::IRemovableStorage &sd = board::sdcard();
    const bool sd_ok = sd.mount();
    ESP_LOGI(TAG, "SD card: %s", sd_ok ? "mounted" : "not present");

    if (!board::lvgl_start()) {
        ESP_LOGE(TAG, "LVGL start failed");
        return;
    }
    if (board::lvgl_lock(1000)) {
        bringup_ui_create(board::display().info(), sd_ok, sd.capacity_mib());
        board::lvgl_unlock();
    }
    board::display().set_backlight(80);
}
