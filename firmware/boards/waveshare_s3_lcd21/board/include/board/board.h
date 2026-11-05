// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board support for the Waveshare ESP32-S3-Touch-LCD-2.1.
// Exposes the on-board peripherals through the hal:: interfaces.
#pragma once

#include <cstdint>

#include "gauge_hal/buzzer.h"
#include "gauge_hal/display.h"
#include "gauge_hal/storage.h"
#include "gauge_hal/touch.h"

namespace board {

/// Brings up I2C, the IO expander, the LCD panel, touch and backlight.
/// Must be called once, before lvgl_start() and before mounting the SD card.
bool init();

hal::IDisplay &display();
hal::ITouch &touch();
hal::IBuzzer &buzzer();
hal::IRemovableStorage &sdcard();

/// Creates the LVGL display (double frame buffer in PSRAM, direct mode),
/// the touch input device and the LVGL handler task pinned to core 1.
bool lvgl_start();

/// LVGL is not thread safe: every LVGL call from outside the LVGL task must be
/// wrapped in lvgl_lock()/lvgl_unlock().
bool lvgl_lock(uint32_t timeout_ms);
void lvgl_unlock();

} // namespace board
