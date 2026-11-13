// SPDX-License-Identifier: GPL-3.0-or-later
#include "bus_task.h"

#include <memory>

#include "board/board.h"
#include "core/obd/obd_session.h"
#include "core/profiles/profile.h"
#include "core/transport/can_detect.h"
#include "embedded_profiles.h"
#include "sdkconfig.h"

#if CONFIG_GAUGE_DEMO_MODE
#include "sim/demo_car.h"
#include "sim/virtual_can_bus.h"
#endif

#include <atomic>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace app {

namespace {

const char *TAG = "bus";

constexpr uint32_t kListenWindowMs = 500;
constexpr uint32_t kRetryAfterSilentMs = 5000;
constexpr uint32_t kRetryAfterLostMs = 2000;
constexpr uint32_t kLogPeriodMs = 2000;

core::signals::SignalStore s_store;
SemaphoreHandle_t s_mutex = nullptr;
BusStatus s_status;
std::vector<core::signals::SignalDef> s_signals;
std::vector<core::profiles::Profile> s_profiles;
volatile int s_focus_slot = -1;
std::atomic<uint32_t> s_generation{0};

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void set_phase(const char *phase)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.phase = phase;
    xSemaphoreGive(s_mutex);
}

/// ISO-TP output: the only path from the OBD session to the CAN driver.
class TwaiSink : public core::transport::ICanFrameSink {
public:
    bool send_frame(const core::bus::CanFrame &f) override { return board::can().transmit(f, 10); }
};

void load_profiles()
{
    for (size_t i = 0; i < kEmbeddedProfileCount; ++i) {
        const EmbeddedProfile &e = kEmbeddedProfiles[i];
        const size_t len = static_cast<size_t>(e.end - e.start) - 1; // drop the NUL
        core::profiles::Profile p;
        const core::profiles::LoadResult r = core::profiles::load_profile(e.start, len, p);
        if (r.ok) {
            ESP_LOGI(TAG, "profile %s (%s)", p.id.c_str(), p.name.c_str());
            s_profiles.push_back(std::move(p));
        } else {
            ESP_LOGE(TAG, "profile %s rejected: %s", e.file_name, r.error.c_str());
        }
    }
}

/// Listen-only observation of the bus at one bitrate. Never transmits.
core::transport::BusObservation observe(uint32_t bitrate)
{
    core::transport::BusObservation obs;
    hal::ICanBus &can = board::can();
    if (!can.start(bitrate, hal::CanMode::ListenOnly)) {
        return obs;
    }
    const uint32_t errors_before = can.status().bus_errors;
    const uint32_t end = now_ms() + kListenWindowMs;
    core::bus::CanFrame f;
    while (static_cast<int32_t>(end - now_ms()) > 0) {
        if (can.receive(f, 20)) {
            ++obs.valid_frames;
        }
    }
    obs.bus_errors = can.status().bus_errors - errors_before;
    can.stop();
    ESP_LOGI(TAG, "listen %u bit/s: %u frames, %u errors", static_cast<unsigned>(bitrate),
             static_cast<unsigned>(obs.valid_frames), static_cast<unsigned>(obs.bus_errors));
    return obs;
}

/// Returns the detected bitrate, or 0.
uint32_t detect_bitrate()
{
    set_phase("detecting CAN");
    core::transport::CanBitrateDetector det;
    det.start();
    while (det.state() == core::transport::DetectState::Listening) {
        det.feed(observe(det.current_bitrate()));
    }
    switch (det.state()) {
        case core::transport::DetectState::Detected:
            return det.detected_bitrate();
        case core::transport::DetectState::Silent: {
            // No profile is known before the first VIN, and the user opt-in does not exist yet
            // (web app, phase 8): the policy says do not transmit.
            const auto action = core::transport::decide_silent_bus(0, false);
            (void)action;
            set_phase("bus silent");
            ESP_LOGW(TAG, "CAN silent at every bitrate: not transmitting");
            return 0;
        }
        case core::transport::DetectState::Failed:
        default:
            set_phase("CAN errors");
            ESP_LOGW(TAG, "CAN errors at every bitrate");
            return 0;
    }
}

void log_values(const std::vector<core::signals::SignalDef> &defs)
{
    for (size_t i = 0; i < defs.size(); ++i) {
        const core::signals::Sample s = s_store.read(i);
        if (s.valid) {
            ESP_LOGI(TAG, "%-16s %10.2f %s%s", defs[i].id.c_str(), s.value, defs[i].unit.c_str(),
                     (s.flags & core::signals::kFlagSaturated) ? " (saturated)" : "");
        } else {
            ESP_LOGI(TAG, "%-16s %10s", defs[i].id.c_str(), "--");
        }
    }
}

/// Per-iteration housekeeping shared by the real and the demo bus. Returns false when the
/// session has ended.
bool service_session(core::obd::ObdSession &session, uint32_t t, uint32_t &next_log, int &focus)
{
    const core::obd::SessionState st = session.state();
    if (st == core::obd::SessionState::WaitingForSignals) {
        const core::obd::Vin *vin = session.has_vin() ? &session.vin() : nullptr;
        const core::profiles::Profile *p = core::profiles::select_profile(s_profiles, vin, "");
        if (p == nullptr) {
            ESP_LOGE(TAG, "no profiles available");
            return false;
        }
        ESP_LOGI(TAG, "VIN %s -> profile %s", vin ? "read" : "not available", p->id.c_str());
        session.set_signals(p->signals, t);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_signals = p->signals;
        s_status.profile = p->name;
        s_status.vin_read = vin != nullptr;
        xSemaphoreGive(s_mutex);
        ++s_generation;
    }
    if (st == core::obd::SessionState::NoResponse || st == core::obd::SessionState::Lost) {
        ESP_LOGW(TAG, "session ended: %s", core::obd::to_string(st));
        return false;
    }
    if (s_focus_slot != focus) {
        focus = s_focus_slot;
        session.set_focus(focus < 0 ? SIZE_MAX : static_cast<size_t>(focus));
    }
    if (static_cast<int32_t>(t - next_log) >= 0) {
        next_log = t + kLogPeriodMs;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_status.phase = core::obd::to_string(st);
        s_status.requests = session.stats().requests;
        s_status.responses = session.stats().responses;
        s_status.rejected_by_gate = session.gate_stats().rejected;
        const auto defs = s_signals;
        xSemaphoreGive(s_mutex);
        log_values(defs);
    }
    return true;
}

void clear_values()
{
    for (size_t i = 0; i < core::signals::SignalStore::kCapacity; ++i) {
        s_store.invalidate(i);
    }
}

void run_session(uint32_t bitrate)
{
    hal::ICanBus &can = board::can();
    if (!can.start(bitrate, hal::CanMode::Normal)) {
        ESP_LOGE(TAG, "cannot open CAN in normal mode");
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.bitrate = bitrate;
    s_status.phase = "connecting";
    xSemaphoreGive(s_mutex);

    TwaiSink sink;
    core::obd::ObdSession session(sink, s_store);
    session.start(now_ms());
    uint32_t next_log = now_ms() + kLogPeriodMs;
    int focus = -1;
    for (;;) {
        core::bus::CanFrame f;
        while (can.receive(f, 0)) {
            session.on_frame(f, now_ms());
        }
        const uint32_t t = now_ms();
        session.poll(t);
        if (!service_session(session, t, next_log, focus)) {
            break;
        }
        vTaskDelay(1);
    }
    can.stop();
    clear_values();
}

#if CONFIG_GAUGE_DEMO_MODE
/// The gauge's side of the in-memory bus used by demo builds.
class DemoPort : public sim::INode, public core::transport::ICanFrameSink {
public:
    explicit DemoPort(sim::VirtualCanBus &bus) : bus_(bus) {}
    void set_session(core::obd::ObdSession *s) { session_ = s; }
    bool send_frame(const core::bus::CanFrame &f) override
    {
        bus_.send(this, f);
        return true;
    }
    void on_frame(sim::VirtualCanBus &b, const core::bus::CanFrame &f) override
    {
        if (session_ != nullptr) {
            session_->on_frame(f, b.now_ms());
        }
    }

private:
    sim::VirtualCanBus &bus_;
    core::obd::ObdSession *session_ = nullptr;
};

/// Demo build: the full OBD stack (SafetyGate, ISO-TP, session) runs against a simulated
/// car in memory. The CAN controller is never started.
void run_demo()
{
    ESP_LOGW(TAG, "DEMO MODE: simulated car, CAN bus not used");
    sim::VirtualCanBus vbus;
    vbus.set_trace_enabled(false);
    sim::DemoCar car(6500);
    DemoPort port(vbus);
    core::obd::ObdSession session(port, s_store);
    port.set_session(&session);
    vbus.attach(&car);
    vbus.attach(&port);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status.phase = "demo";
    xSemaphoreGive(s_mutex);

    session.start(vbus.now_ms());
    uint32_t next_log = kLogPeriodMs;
    int focus = -1;
    uint32_t last = now_ms();
    for (;;) {
        const uint32_t real = now_ms();
        for (uint32_t i = 0; i < real - last; ++i) {
            vbus.advance(1);
            session.poll(vbus.now_ms());
        }
        last = real;
        if (!service_session(session, vbus.now_ms(), next_log, focus)) {
            session.start(vbus.now_ms());
        }
        vTaskDelay(1);
    }
}
#endif

void bus_task(void *)
{
    load_profiles();
#if CONFIG_GAUGE_DEMO_MODE
    run_demo();
#endif
    for (;;) {
        const uint32_t bitrate = detect_bitrate();
        if (bitrate == 0) {
            vTaskDelay(pdMS_TO_TICKS(kRetryAfterSilentMs));
            continue;
        }
        run_session(bitrate);
        set_phase("no data");
        vTaskDelay(pdMS_TO_TICKS(kRetryAfterLostMs));
    }
}

} // namespace

void bus_task_start()
{
    s_mutex = xSemaphoreCreateMutex();
    s_status.phase = "starting";
    xTaskCreatePinnedToCore(bus_task, "bus", 8192, nullptr, 5, nullptr, 0);
}

core::signals::SignalStore &signal_store()
{
    return s_store;
}

uint32_t signals_generation()
{
    return s_generation.load();
}

std::vector<core::signals::SignalDef> active_signals()
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    std::vector<core::signals::SignalDef> copy = s_signals;
    xSemaphoreGive(s_mutex);
    return copy;
}

BusStatus bus_status()
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    BusStatus copy = s_status;
    xSemaphoreGive(s_mutex);
    return copy;
}

void set_focus_slot(size_t slot)
{
    s_focus_slot = slot == SIZE_MAX ? -1 : static_cast<int>(slot);
}

} // namespace app
