// SPDX-License-Identifier: GPL-3.0-or-later
//
// Gauge UI: boot sequence, one-signal screens (analog / digital), swipe + button navigation,
// day/night themes. All LVGL calls must be made with the LVGL lock held.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/display/gauge_math.h"
#include "core/display/screens.h"
#include "core/display/theme.h"
#include "core/input/input.h"
#include "core/signals/signal.h"
#include "core/signals/signal_store.h"
#include "gauge_hal/display.h"

namespace ui {

/// What the UI needs from the rest of the firmware.
class IDataSource {
public:
    virtual ~IDataSource() = default;
    /// Increments whenever the active signal list changes (new profile).
    virtual uint32_t signals_generation() const = 0;
    virtual std::vector<core::signals::SignalDef> signals() const = 0;
    virtual core::signals::Sample read(size_t slot) const = 0;
    virtual std::string car_name() const = 0;
    /// Short connection status, e.g. "bus silent", "polling".
    virtual std::string status() const = 0;
    virtual void set_focus(size_t slot) = 0;
};

struct UiSettings {
    core::display::GaugeStyle default_style = core::display::GaugeStyle::Analog;
    core::display::UnitPrefs units;
    std::string theme_id = "oem_red";
};

/// Builds the UI and starts the boot animation.
void start(const hal::DisplayInfo &display, IDataSource &data, const UiSettings &settings);

/// Physical button / other input sources.
void handle_input(core::input::InputEvent ev);

/// Switches day/night palette.
void set_night(bool night);

} // namespace ui
