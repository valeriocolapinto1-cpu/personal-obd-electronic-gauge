// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gauge_hal/display.h"

/// Phase 0 bench test screen: arc sweep, touch read-out, buzzer and backlight controls.
/// Call with the LVGL lock held.
void bringup_ui_create(const hal::DisplayInfo &info, bool sd_ok, unsigned sd_mib);
