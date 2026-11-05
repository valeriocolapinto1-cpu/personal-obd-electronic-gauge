// SPDX-License-Identifier: GPL-3.0-or-later
//
// LVGL v9 glue: direct rendering into the two PSRAM frame buffers of the RGB panel,
// buffer swap on VSYNC (no tearing), touch input device, handler task on core 1.
#include <algorithm>

#include "board/board.h"

#include "esp_attr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

namespace board {

esp_lcd_panel_handle_t internal_panel(); // board.cpp

namespace {

const char *TAG = "board_lvgl";

SemaphoreHandle_t s_lvgl_mutex = nullptr;
SemaphoreHandle_t s_vsync_sem = nullptr;

bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_vsync_sem, &woken);
    return woken == pdTRUE;
}

uint32_t tick_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void flush_cb(lv_display_t *disp, const lv_area_t *, uint8_t *px_map)
{
    if (lv_display_flush_is_last(disp)) {
        const int32_t w = lv_display_get_horizontal_resolution(disp);
        const int32_t h = lv_display_get_vertical_resolution(disp);
        xSemaphoreTake(s_vsync_sem, 0); // drop a stale VSYNC
        // px_map is one of the panel frame buffers: the driver only switches buffer.
        esp_lcd_panel_draw_bitmap(internal_panel(), 0, 0, w, h, px_map);
        xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(100));
    }
    lv_display_flush_ready(disp);
}

void touch_cb(lv_indev_t *, lv_indev_data_t *data)
{
    hal::TouchPoint p{};
    if (touch().read(p) && p.pressed) {
        data->point.x = p.x;
        data->point.y = p.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvgl_task(void *)
{
    for (;;) {
        uint32_t wait_ms = 10;
        if (lvgl_lock(100)) {
            wait_ms = lv_timer_handler();
            lvgl_unlock();
        }
        wait_ms = std::clamp<uint32_t>(wait_ms, 2, 30);
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

} // namespace

bool lvgl_lock(uint32_t timeout_ms)
{
    return xSemaphoreTakeRecursive(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void lvgl_unlock()
{
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}

bool lvgl_start()
{
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    s_vsync_sem = xSemaphoreCreateBinary();
    if (s_lvgl_mutex == nullptr || s_vsync_sem == nullptr) {
        return false;
    }

    esp_lcd_panel_handle_t panel = internal_panel();
    esp_lcd_rgb_panel_event_callbacks_t cbs{};
    cbs.on_vsync = on_vsync;
    if (esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, nullptr) != ESP_OK) {
        return false;
    }

    void *fb0 = nullptr;
    void *fb1 = nullptr;
    if (esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1) != ESP_OK) {
        return false;
    }

    const hal::DisplayInfo info = display().info();
    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_display_create(info.width, info.height);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, fb0, fb1, static_cast<uint32_t>(info.width) * info.height * 2,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_cb);
    lv_indev_set_display(indev, disp);

    const BaseType_t ok = xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 4, nullptr, 1);
    ESP_LOGI(TAG, "LVGL started (%ux%u)", info.width, info.height);
    return ok == pdPASS;
}

} // namespace board
