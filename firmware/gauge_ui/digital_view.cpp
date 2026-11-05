// SPDX-License-Identifier: GPL-3.0-or-later
//
// Racing-style digital gauge: big number, progress ring, peak value.
#include <algorithm>

#include "views.h"

namespace ui {

namespace {

class DigitalView final : public IView {
public:
    DigitalView(lv_obj_t *parent, const hal::DisplayInfo &info)
    {
        const int size = std::min(info.width, info.height);
        root_ = lv_obj_create(parent);
        lv_obj_remove_style_all(root_);
        lv_obj_set_size(root_, info.width, info.height);
        lv_obj_center(root_);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

        ring_ = lv_arc_create(root_);
        lv_obj_set_size(ring_, size - 16, size - 16);
        lv_obj_center(ring_);
        lv_arc_set_rotation(ring_, 135);
        lv_arc_set_bg_angles(ring_, 0, 270);
        lv_arc_set_range(ring_, 0, 1000);
        lv_obj_remove_style(ring_, nullptr, LV_PART_KNOB);
        lv_obj_remove_flag(ring_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(ring_, size / 16, LV_PART_MAIN);
        lv_obj_set_style_arc_width(ring_, size / 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(ring_, false, LV_PART_INDICATOR);

        value_ = lv_label_create(root_);
        lv_obj_set_style_text_font(value_, &lv_font_montserrat_48, 0);
        // Montserrat 48 is the largest built-in font: scale it for a big racing number.
        lv_obj_set_style_transform_scale(value_, 512, 0);
        lv_obj_set_style_transform_pivot_x(value_, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(value_, LV_PCT(50), 0);
        lv_obj_align(value_, LV_ALIGN_CENTER, 0, 0);

        unit_ = lv_label_create(root_);
        lv_obj_set_style_text_font(unit_, &lv_font_montserrat_24, 0);
        lv_obj_align(unit_, LV_ALIGN_CENTER, 0, size / 6);

        title_ = lv_label_create(root_);
        lv_obj_set_style_text_font(title_, &lv_font_montserrat_24, 0);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -size / 5);

        peak_ = lv_label_create(root_);
        lv_obj_set_style_text_font(peak_, &lv_font_montserrat_14, 0);
        lv_obj_align(peak_, LV_ALIGN_CENTER, 0, size / 4);
    }

    void configure(const ViewModel &vm) override
    {
        lv_label_set_text(title_, vm.label.c_str());
        lv_label_set_text(unit_, vm.dv.unit.c_str());
        lv_label_set_text(peak_, "");
        has_peak_ = false;
        min_ = vm.dv.min;
        max_ = vm.dv.max;
    }

    void update(const ViewModel &vm, double shown) override
    {
        lv_arc_set_value(ring_, static_cast<int32_t>(core::display::fraction(shown, min_, max_) * 1000.0));
        if (!vm.valid) {
            lv_label_set_text(value_, "--");
            lv_label_set_text(peak_, vm.status.c_str());
            return;
        }
        lv_label_set_text(value_, core::display::format_value(vm.dv.value, vm.dv.decimals, true).c_str());
        if (!has_peak_ || vm.dv.value > peak_value_) {
            peak_value_ = vm.dv.value;
            has_peak_ = true;
        }
        const std::string peak = "MAX " + core::display::format_value(peak_value_, vm.dv.decimals, true) +
                                 (vm.saturated ? "  SENSOR LIMIT" : "");
        lv_label_set_text(peak_, peak.c_str());
    }

    void apply_palette(const core::display::Palette &p) override
    {
        lv_obj_set_style_arc_color(ring_, color(p.secondary), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(ring_, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_arc_color(ring_, color(p.accent), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(value_, color(p.primary), 0);
        lv_obj_set_style_text_color(unit_, color(p.secondary), 0);
        lv_obj_set_style_text_color(title_, color(p.secondary), 0);
        lv_obj_set_style_text_color(peak_, color(p.warning), 0);
    }

    void set_visible(bool visible) override
    {
        if (visible) {
            lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        }
    }

private:
    lv_obj_t *root_ = nullptr;
    lv_obj_t *ring_ = nullptr;
    lv_obj_t *value_ = nullptr;
    lv_obj_t *unit_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *peak_ = nullptr;
    double min_ = 0;
    double max_ = 1;
    double peak_value_ = 0;
    bool has_peak_ = false;
};

} // namespace

IView *create_digital_view(lv_obj_t *parent, const hal::DisplayInfo &info)
{
    return new DigitalView(parent, info);
}

} // namespace ui
