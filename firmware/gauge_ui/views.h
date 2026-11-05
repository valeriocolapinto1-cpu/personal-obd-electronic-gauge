// SPDX-License-Identifier: GPL-3.0-or-later
//
// Internal: the two gauge renderers. Each view owns its LVGL objects under `parent`.
#pragma once

#include <string>

#include "core/display/gauge_math.h"
#include "core/display/theme.h"
#include "gauge_hal/display.h"
#include "lvgl.h"

namespace ui {

struct ViewModel {
    std::string label; ///< "BOOST"
    core::display::DisplayValue dv;
    bool valid = false;
    bool saturated = false;
    std::string status; ///< shown small when there is no data
};

class IView {
public:
    virtual ~IView() = default;
    /// Rebuilds scale and labels for a new signal/range.
    virtual void configure(const ViewModel &vm) = 0;
    /// Per-frame update (needle position already smoothed by the controller).
    virtual void update(const ViewModel &vm, double shown_value) = 0;
    virtual void apply_palette(const core::display::Palette &p) = 0;
    virtual void set_visible(bool visible) = 0;
};

IView *create_analog_view(lv_obj_t *parent, const hal::DisplayInfo &info);
IView *create_digital_view(lv_obj_t *parent, const hal::DisplayInfo &info);

inline lv_color_t color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

} // namespace ui
