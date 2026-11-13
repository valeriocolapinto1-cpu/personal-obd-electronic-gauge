// SPDX-License-Identifier: GPL-3.0-or-later
//
// Universal OBD Gauge - firmware entry point.
// Read-only: every vehicle bus request passes the SafetyGate (core/transport/safety_gate.h).
#include "board/board.h"
#include "bus_task.h"
#include "core/util/log.h"
#include "gauge_ui/gauge_ui.h"

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

/// Connects the UI to the bus task's signal store.
class BusDataSource final : public ui::IDataSource {
public:
    uint32_t signals_generation() const override { return app::signals_generation(); }
    std::vector<core::signals::SignalDef> signals() const override { return app::active_signals(); }
    core::signals::Sample read(size_t slot) const override { return app::signal_store().read(slot); }
    std::string car_name() const override { return app::bus_status().profile; }
    std::string status() const override { return app::bus_status().phase; }
    void set_focus(size_t slot) override { app::set_focus_slot(slot); }
};

BusDataSource s_data;

} // namespace

extern "C" void app_main(void)
{
    core::log::set_sink(core_log_sink);
    ESP_LOGI(TAG, "Universal OBD Gauge");
    if (!board::init()) {
        ESP_LOGE(TAG, "board init failed");
        return;
    }

    // The SD card shares GPIO1/2 with the LCD init SPI: mount it after board::init().
    const bool sd_ok = board::sdcard().mount();
    ESP_LOGI(TAG, "SD card: %s", sd_ok ? "mounted" : "not present");

    // Vehicle bus first, so the profile name may already be known at the end of the boot sweep.
    app::bus_task_start();

    if (!board::lvgl_start()) {
        ESP_LOGE(TAG, "LVGL start failed");
        return;
    }
    if (board::lvgl_lock(1000)) {
        ui::start(board::display().info(), s_data, ui::UiSettings{});
        board::lvgl_unlock();
    }
    board::display().set_backlight(80);
}
