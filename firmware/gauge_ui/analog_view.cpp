// SPDX-License-Identifier: GPL-3.0-or-later
//
// OEM-like analog dial: 270 degree scale with major/minor ticks and labels, a needle,
// the value and unit in the lower half.
#include <algorithm>
#include <cmath>
#include <vector>

#include "views.h"

namespace ui {

namespace {

constexpr double kStartDeg = 135.0; // LVGL/screen angles: 0 = 3 o'clock, clockwise
constexpr double kSweepDeg = 270.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxTicks = 61;
constexpr int kMaxLabels = 13;

class AnalogView final : public IView {
public:
    AnalogView(lv_obj_t *parent, const hal::DisplayInfo &info)
    {
        size_ = std::min(info.width, info.height);
        cx_ = info.width / 2;
        cy_ = info.height / 2;

        root_ = lv_obj_create(parent);
        lv_obj_remove_style_all(root_);
        lv_obj_set_size(root_, info.width, info.height);
        lv_obj_center(root_);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

        for (int i = 0; i < kMaxTicks; ++i) {
            ticks_[i] = lv_line_create(root_);
            lv_obj_add_flag(ticks_[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int i = 0; i < kMaxLabels; ++i) {
            tick_labels_[i] = lv_label_create(root_);
            lv_obj_set_style_text_font(tick_labels_[i], &lv_font_montserrat_24, 0);
            lv_obj_add_flag(tick_labels_[i], LV_OBJ_FLAG_HIDDEN);
        }

        needle_ = lv_line_create(root_);
        lv_obj_set_style_line_width(needle_, std::max(4, size_ / 80), 0);
        lv_obj_set_style_line_rounded(needle_, true, 0);

        hub_ = lv_obj_create(root_);
        lv_obj_remove_style_all(hub_);
        const int hub = size_ / 14;
        lv_obj_set_size(hub_, hub, hub);
        lv_obj_set_style_radius(hub_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(hub_, LV_OPA_COVER, 0);
        lv_obj_center(hub_);

        value_ = lv_label_create(root_);
        lv_obj_set_style_text_font(value_, &lv_font_montserrat_48, 0);
        lv_obj_align(value_, LV_ALIGN_CENTER, 0, size_ / 5);

        unit_ = lv_label_create(root_);
        lv_obj_set_style_text_font(unit_, &lv_font_montserrat_24, 0);
        lv_obj_align(unit_, LV_ALIGN_CENTER, 0, size_ / 5 + 40);

        title_ = lv_label_create(root_);
        lv_obj_set_style_text_font(title_, &lv_font_montserrat_24, 0);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -size_ / 5);

        flag_ = lv_label_create(root_);
        lv_obj_set_style_text_font(flag_, &lv_font_montserrat_14, 0);
        lv_obj_align(flag_, LV_ALIGN_CENTER, 0, size_ / 5 + 70);
        lv_label_set_text(flag_, "");
    }

    void configure(const ViewModel &vm) override
    {
        lv_label_set_text(title_, vm.label.c_str());
        lv_label_set_text(unit_, vm.dv.unit.c_str());

        const double min = vm.dv.min;
        const double max = vm.dv.max;
        const double major = core::display::nice_step(min, max, 7);
        double minor = major / 5.0;
        if ((max - min) / minor > kMaxTicks - 1) {
            minor = major / 2.0;
        }
        if ((max - min) / minor > kMaxTicks - 1) {
            minor = major;
        }

        const double r_out = size_ * 0.47;
        const double r_major = size_ * 0.41;
        const double r_minor = size_ * 0.44;
        const double r_label = size_ * 0.33;

        int ti = 0;
        int li = 0;
        const double first = std::ceil(min / minor - 1e-9) * minor;
        for (double v = first; v <= max + 1e-9 && ti < kMaxTicks; v += minor, ++ti) {
            const bool is_major = std::fabs(std::remainder(v, major)) < minor * 0.01;
            const double a = core::display::needle_angle(v, min, max, kStartDeg, kSweepDeg) * kPi / 180.0;
            const double r_in = is_major ? r_major : r_minor;
            tick_pts_[ti][0] = {static_cast<lv_value_precise_t>(cx_ + r_in * std::cos(a)),
                                static_cast<lv_value_precise_t>(cy_ + r_in * std::sin(a))};
            tick_pts_[ti][1] = {static_cast<lv_value_precise_t>(cx_ + r_out * std::cos(a)),
                                static_cast<lv_value_precise_t>(cy_ + r_out * std::sin(a))};
            lv_line_set_points(ticks_[ti], tick_pts_[ti], 2);
            lv_obj_set_style_line_width(ticks_[ti], is_major ? std::max(3, size_ / 120) : 2, 0);
            tick_major_[ti] = is_major;
            lv_obj_remove_flag(ticks_[ti], LV_OBJ_FLAG_HIDDEN);

            if (is_major && li < kMaxLabels) {
                // Large ranges (rpm) are labelled in thousands like an OEM tachometer.
                const double shown = (max >= 2000) ? v / 1000.0 : v;
                const uint8_t dec = (major < 1.0 || (max >= 2000 && major < 1000)) ? 1 : 0;
                lv_label_set_text(tick_labels_[li], core::display::format_value(shown, dec, true).c_str());
                lv_obj_update_layout(tick_labels_[li]);
                const int w = lv_obj_get_width(tick_labels_[li]);
                const int h = lv_obj_get_height(tick_labels_[li]);
                lv_obj_set_pos(tick_labels_[li], static_cast<int32_t>(cx_ + r_label * std::cos(a) - w / 2),
                               static_cast<int32_t>(cy_ + r_label * std::sin(a) - h / 2));
                lv_obj_remove_flag(tick_labels_[li], LV_OBJ_FLAG_HIDDEN);
                ++li;
            }
        }
        for (int i = ti; i < kMaxTicks; ++i) {
            lv_obj_add_flag(ticks_[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int i = li; i < kMaxLabels; ++i) {
            lv_obj_add_flag(tick_labels_[i], LV_OBJ_FLAG_HIDDEN);
        }
        min_ = min;
        max_ = max;
        apply_palette(palette_);
    }

    void update(const ViewModel &vm, double shown) override
    {
        const double a = core::display::needle_angle(shown, min_, max_, kStartDeg, kSweepDeg) * kPi / 180.0;
        const double r_tip = size_ * 0.40;
        const double r_tail = size_ * 0.06;
        needle_pts_[0] = {static_cast<lv_value_precise_t>(cx_ - r_tail * std::cos(a)),
                          static_cast<lv_value_precise_t>(cy_ - r_tail * std::sin(a))};
        needle_pts_[1] = {static_cast<lv_value_precise_t>(cx_ + r_tip * std::cos(a)),
                          static_cast<lv_value_precise_t>(cy_ + r_tip * std::sin(a))};
        lv_line_set_points(needle_, needle_pts_, 2);

        if (vm.valid) {
            lv_label_set_text(value_, core::display::format_value(vm.dv.value, vm.dv.decimals, true).c_str());
            lv_label_set_text(flag_, vm.saturated ? "SENSOR LIMIT" : "");
        } else {
            lv_label_set_text(value_, "--");
            lv_label_set_text(flag_, vm.status.c_str());
        }
    }

    void apply_palette(const core::display::Palette &p) override
    {
        palette_ = p;
        for (int i = 0; i < kMaxTicks; ++i) {
            lv_obj_set_style_line_color(ticks_[i], color(tick_major_[i] ? p.primary : p.secondary), 0);
        }
        for (int i = 0; i < kMaxLabels; ++i) {
            lv_obj_set_style_text_color(tick_labels_[i], color(p.primary), 0);
        }
        lv_obj_set_style_line_color(needle_, color(p.needle), 0);
        lv_obj_set_style_bg_color(hub_, color(p.secondary), 0);
        lv_obj_set_style_text_color(value_, color(p.primary), 0);
        lv_obj_set_style_text_color(unit_, color(p.secondary), 0);
        lv_obj_set_style_text_color(title_, color(p.secondary), 0);
        lv_obj_set_style_text_color(flag_, color(p.warning), 0);
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
    int size_ = 0;
    int cx_ = 0;
    int cy_ = 0;
    double min_ = 0;
    double max_ = 1;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *ticks_[kMaxTicks] = {};
    lv_point_precise_t tick_pts_[kMaxTicks][2] = {};
    bool tick_major_[kMaxTicks] = {};
    lv_obj_t *tick_labels_[kMaxLabels] = {};
    lv_obj_t *needle_ = nullptr;
    lv_point_precise_t needle_pts_[2] = {};
    lv_obj_t *hub_ = nullptr;
    lv_obj_t *value_ = nullptr;
    lv_obj_t *unit_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *flag_ = nullptr;
    core::display::Palette palette_;
};

} // namespace

IView *create_analog_view(lv_obj_t *parent, const hal::DisplayInfo &info)
{
    return new AnalogView(parent, info);
}

} // namespace ui
