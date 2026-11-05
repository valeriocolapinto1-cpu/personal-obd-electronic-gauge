// SPDX-License-Identifier: GPL-3.0-or-later
#include "bringup_ui.h"

#include <algorithm>
#include <cstdio>

#include "board/board.h"
#include "bus_task.h"
#include "lvgl.h"

namespace {

lv_obj_t *s_arc = nullptr;
lv_obj_t *s_value = nullptr;
lv_obj_t *s_touch = nullptr;
lv_obj_t *s_bus = nullptr;
int s_sweep = 0;
int s_dir = 1;

void sweep_timer_cb(lv_timer_t *)
{
    // Real RPM when the bus delivers it (slot of "engine_rpm"), demo sweep otherwise.
    const auto defs = app::active_signals();
    for (size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].id != "engine_rpm") {
            continue;
        }
        const core::signals::Sample rpm = app::signal_store().read(i);
        if (rpm.valid) {
            const int pct = static_cast<int>(100.0 * rpm.value / (defs[i].max > 0 ? defs[i].max : 8000));
            lv_arc_set_value(s_arc, pct < 0 ? 0 : (pct > 100 ? 100 : pct));
            lv_label_set_text_fmt(s_value, "%d", static_cast<int>(rpm.value));
            return;
        }
    }
    s_sweep += s_dir * 2;
    if (s_sweep >= 100 || s_sweep <= 0) {
        s_dir = -s_dir;
    }
    lv_arc_set_value(s_arc, s_sweep);
    lv_label_set_text_fmt(s_value, "%d", s_sweep * 70); // demo sweep 0..7000
}

void bus_timer_cb(lv_timer_t *)
{
    const app::BusStatus st = app::bus_status();
    const auto defs = app::active_signals();
    char text[256];
    int n = snprintf(text, sizeof(text), "%s %s\n", st.phase.c_str(), st.profile.c_str());
    for (size_t i = 0; i < defs.size() && i < 4 && n > 0 && static_cast<size_t>(n) < sizeof(text); ++i) {
        const core::signals::Sample s = app::signal_store().read(i);
        if (s.valid) {
            n += snprintf(text + n, sizeof(text) - n, "%s %.*f %s\n", defs[i].label.c_str(), defs[i].decimals, s.value,
                          defs[i].unit.c_str());
        } else {
            n += snprintf(text + n, sizeof(text) - n, "%s --\n", defs[i].label.c_str());
        }
    }
    lv_label_set_text(s_bus, text);
}

void screen_pressing_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_label_set_text_fmt(s_touch, "touch %d,%d", static_cast<int>(p.x), static_cast<int>(p.y));
    (void)e;
}

void beep_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    board::buzzer().set(code == LV_EVENT_PRESSED);
}

void backlight_cb(lv_event_t *e)
{
    lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    board::display().set_backlight(static_cast<uint8_t>(lv_slider_get_value(slider)));
}

} // namespace

void bringup_ui_create(const hal::DisplayInfo &info, bool sd_ok, unsigned sd_mib)
{
    const int32_t size = std::min(info.width, info.height);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_add_event_cb(scr, screen_pressing_cb, LV_EVENT_PRESSING, nullptr);

    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, size - 20, size - 20);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 100);
    lv_obj_remove_style(s_arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, size / 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, size / 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);

    s_value = lv_label_create(scr);
    lv_obj_set_style_text_font(s_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_value, lv_color_white(), 0);
    lv_obj_align(s_value, LV_ALIGN_CENTER, 0, -size / 10);

    s_touch = lv_label_create(scr);
    lv_obj_set_style_text_color(s_touch, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_touch, "touch -,-");
    lv_obj_align(s_touch, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sd = lv_label_create(scr);
    lv_obj_set_style_text_color(sd, lv_palette_main(LV_PALETTE_GREY), 0);
    if (sd_ok) {
        lv_label_set_text_fmt(sd, "SD %u MiB", sd_mib);
    } else {
        lv_label_set_text(sd, "SD not mounted");
    }
    lv_obj_align(sd, LV_ALIGN_CENTER, 0, size / 12);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, size / 5);
    lv_obj_add_event_cb(btn, beep_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(btn, beep_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "BEEP");

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, size / 2);
    lv_slider_set_range(slider, 5, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, size / 3);
    lv_obj_add_event_cb(slider, backlight_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    s_bus = lv_label_create(scr);
    lv_obj_set_style_text_font(s_bus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_bus, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_set_style_text_align(s_bus, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_bus, "bus: starting");
    lv_obj_align(s_bus, LV_ALIGN_TOP_MID, 0, size / 8);

    lv_timer_create(sweep_timer_cb, 20, nullptr);
    lv_timer_create(bus_timer_cb, 300, nullptr);
}
