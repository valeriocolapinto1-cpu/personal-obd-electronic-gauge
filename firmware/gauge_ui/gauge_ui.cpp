// SPDX-License-Identifier: GPL-3.0-or-later
#include "gauge_ui/gauge_ui.h"

#include <algorithm>
#include <cmath>

#include "core/display/boot_sequence.h"
#include "lvgl.h"
#include "views.h"

namespace ui {

namespace {

constexpr uint32_t kFramePeriodMs = 33; // ~30 fps
constexpr double kNeedleSmoothing = 0.35;

struct State {
    hal::DisplayInfo info{};
    IDataSource *data = nullptr;
    UiSettings settings;
    const core::display::Theme *theme = nullptr;
    bool night = false;

    lv_obj_t *screen = nullptr;
    lv_obj_t *boot_arc = nullptr;
    lv_obj_t *boot_name = nullptr;
    lv_obj_t *boot_logo = nullptr;
    lv_obj_t *status = nullptr;
    IView *analog = nullptr;
    IView *digital = nullptr;
    IView *active = nullptr;

    uint32_t boot_start = 0;
    bool booting = true;

    uint32_t generation = UINT32_MAX;
    std::vector<core::signals::SignalDef> signals;
    core::display::ScreenList screens;
    size_t configured_index = SIZE_MAX;
    core::display::GaugeStyle configured_style = core::display::GaugeStyle::Analog;
    double shown = 0;

    core::input::SwipeDetector swipe;
};

State s;

const core::display::Palette &palette()
{
    return s.night ? s.theme->night : s.theme->day;
}

void apply_palette()
{
    const core::display::Palette &p = palette();
    lv_obj_set_style_bg_color(s.screen, color(p.background), 0);
    lv_obj_set_style_arc_color(s.boot_arc, color(p.accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s.boot_arc, color(p.secondary), LV_PART_MAIN);
    lv_obj_set_style_text_color(s.boot_name, color(p.primary), 0);
    lv_obj_set_style_border_color(s.boot_logo, color(p.accent), 0);
    lv_obj_set_style_text_color(s.status, color(p.secondary), 0);
    s.analog->apply_palette(p);
    s.digital->apply_palette(p);
}

ViewModel make_model(size_t slot, bool *ok)
{
    ViewModel vm;
    *ok = slot < s.signals.size();
    if (!*ok) {
        vm.label = "NO DATA";
        vm.status = s.data->status();
        return vm;
    }
    const core::signals::SignalDef &def = s.signals[slot];
    const core::signals::Sample sample = s.data->read(slot);
    vm.label = def.label;
    vm.valid = sample.valid;
    vm.saturated = (sample.flags & core::signals::kFlagSaturated) != 0;
    vm.dv = core::display::to_display(def, sample.valid ? sample.value : def.min, s.settings.units);
    vm.status = s.data->status();
    return vm;
}

void configure_current()
{
    if (s.screens.empty()) {
        s.active = s.analog;
        s.digital->set_visible(false);
        s.analog->set_visible(true);
        bool ok = false;
        const ViewModel vm = make_model(SIZE_MAX, &ok);
        s.analog->configure(vm);
        s.configured_index = SIZE_MAX;
        return;
    }
    const core::display::ScreenEntry &e = s.screens.current();
    s.active = (e.style == core::display::GaugeStyle::Analog) ? s.analog : s.digital;
    s.analog->set_visible(s.active == s.analog);
    s.digital->set_visible(s.active == s.digital);
    bool ok = false;
    const ViewModel vm = make_model(e.slot, &ok);
    s.active->configure(vm);
    s.shown = vm.dv.min;
    s.configured_index = s.screens.index();
    s.configured_style = e.style;
    s.data->set_focus(e.slot);
}

void refresh_signals()
{
    const uint32_t gen = s.data->signals_generation();
    if (gen == s.generation) {
        return;
    }
    s.generation = gen;
    s.signals = s.data->signals();
    s.screens.set_default(s.signals.size(), s.settings.default_style);
    configure_current();
}

void update_gauge()
{
    refresh_signals();
    if (!s.screens.empty() &&
        (s.screens.index() != s.configured_index || s.screens.current().style != s.configured_style)) {
        configure_current();
    }
    const size_t slot = s.screens.empty() ? SIZE_MAX : s.screens.current().slot;
    bool ok = false;
    const ViewModel vm = make_model(slot, &ok);
    const double target = vm.valid ? vm.dv.value : vm.dv.min;
    s.shown += (target - s.shown) * kNeedleSmoothing;
    if (std::fabs(target - s.shown) < (vm.dv.max - vm.dv.min) * 1e-4) {
        s.shown = target;
    }
    s.active->update(vm, s.shown);
    lv_label_set_text(s.status, vm.valid ? "" : vm.status.c_str());
}

void update_boot(uint32_t now)
{
    const core::display::BootFrame f = core::display::boot_frame(now - s.boot_start);
    switch (f.phase) {
        case core::display::BootPhase::SweepUp:
        case core::display::BootPhase::SweepDown:
            lv_arc_set_value(s.boot_arc, static_cast<int32_t>(f.sweep * 1000.0));
            break;
        case core::display::BootPhase::Splash: {
            lv_obj_add_flag(s.boot_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(s.boot_logo, LV_OBJ_FLAG_HIDDEN);
            const std::string name = s.data->car_name();
            lv_label_set_text(s.boot_name, name.empty() ? "Universal OBD Gauge" : name.c_str());
            lv_obj_remove_flag(s.boot_name, LV_OBJ_FLAG_HIDDEN);
            break;
        }
        case core::display::BootPhase::Done:
            lv_obj_add_flag(s.boot_logo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s.boot_name, LV_OBJ_FLAG_HIDDEN);
            s.booting = false;
            s.generation = UINT32_MAX; // force configuration
            break;
    }
}

void frame_cb(lv_timer_t *)
{
    const uint32_t now = lv_tick_get();
    if (s.booting) {
        update_boot(now);
        return;
    }
    update_gauge();
}

void touch_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t p{0, 0};
    if (indev != nullptr) {
        lv_indev_get_point(indev, &p);
    }
    core::input::InputEvent ev = core::input::InputEvent::None;
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        ev = s.swipe.update(true, static_cast<uint16_t>(p.x), static_cast<uint16_t>(p.y), lv_tick_get());
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ev = s.swipe.update(false, 0, 0, lv_tick_get());
    }
    if (ev != core::input::InputEvent::None) {
        handle_input(ev);
    }
}

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

} // namespace

void start(const hal::DisplayInfo &display, IDataSource &data, const UiSettings &settings)
{
    s.info = display;
    s.data = &data;
    s.settings = settings;
    s.theme = core::display::find_theme(settings.theme_id);
    if (s.theme == nullptr) {
        s.theme = &core::display::default_theme();
    }
    const int size = std::min(display.width, display.height);
    s.swipe = core::input::SwipeDetector(static_cast<uint16_t>(size * 15 / 100));

    s.screen = lv_screen_active();
    lv_obj_clean(s.screen);
    lv_obj_set_style_bg_opa(s.screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s.screen, LV_OBJ_FLAG_SCROLLABLE);

    s.analog = create_analog_view(s.screen, display);
    s.digital = create_digital_view(s.screen, display);
    s.analog->set_visible(false);
    s.digital->set_visible(false);

    s.boot_arc = lv_arc_create(s.screen);
    lv_obj_set_size(s.boot_arc, size - 16, size - 16);
    lv_obj_center(s.boot_arc);
    lv_arc_set_rotation(s.boot_arc, 135);
    lv_arc_set_bg_angles(s.boot_arc, 0, 270);
    lv_arc_set_range(s.boot_arc, 0, 1000);
    lv_obj_remove_style(s.boot_arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(s.boot_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s.boot_arc, size / 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s.boot_arc, size / 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s.boot_arc, LV_OPA_30, LV_PART_MAIN);

    // Logo placeholder: the user's logo (PNG upload) replaces it in phase 8.
    s.boot_logo = lv_obj_create(s.screen);
    lv_obj_remove_style_all(s.boot_logo);
    lv_obj_set_size(s.boot_logo, size / 3, size / 3);
    lv_obj_set_style_radius(s.boot_logo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s.boot_logo, std::max(3, size / 60), 0);
    lv_obj_align(s.boot_logo, LV_ALIGN_CENTER, 0, -size / 10);
    lv_obj_add_flag(s.boot_logo, LV_OBJ_FLAG_HIDDEN);

    s.boot_name = make_label(s.screen, &lv_font_montserrat_24);
    lv_obj_set_width(s.boot_name, size * 3 / 4);
    lv_obj_set_style_text_align(s.boot_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s.boot_name, LV_LABEL_LONG_WRAP);
    lv_obj_align(s.boot_name, LV_ALIGN_CENTER, 0, size / 6);
    lv_obj_add_flag(s.boot_name, LV_OBJ_FLAG_HIDDEN);

    s.status = make_label(s.screen, &lv_font_montserrat_14);
    lv_obj_align(s.status, LV_ALIGN_BOTTOM_MID, 0, -size / 12);
    lv_label_set_text(s.status, "");

    lv_obj_add_event_cb(s.screen, touch_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s.screen, touch_cb, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(s.screen, touch_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(s.screen, touch_cb, LV_EVENT_PRESS_LOST, nullptr);

    s.active = s.analog;
    apply_palette();
    s.booting = true;
    s.boot_start = lv_tick_get();
    lv_timer_create(frame_cb, kFramePeriodMs, nullptr);
}

void handle_input(core::input::InputEvent ev)
{
    if (s.booting || s.screens.empty()) {
        return;
    }
    switch (ev) {
        case core::input::InputEvent::Next:
            s.screens.next();
            break;
        case core::input::InputEvent::Previous:
            s.screens.prev();
            break;
        case core::input::InputEvent::Tap:
            s.screens.toggle_style();
            break;
        case core::input::InputEvent::LongPress: // menu / Wi-Fi AP: phase 8
        case core::input::InputEvent::None:
            break;
    }
}

void set_night(bool night)
{
    if (night != s.night && s.theme != nullptr) {
        s.night = night;
        apply_palette();
    }
}

} // namespace ui
