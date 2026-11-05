// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/demo_car.h"

#include <cmath>

namespace sim {

namespace {
constexpr uint32_t kCycleMs = 20000;
constexpr uint32_t kUpdateMs = 20;
constexpr double kBaroKpa = 101.0;
} // namespace

DemoCar::DemoCar(uint16_t limiter_rpm) : limiter_(limiter_rpm)
{
    ecu_.set_vin("WVWZZZ13ZAV000001"); // placeholder VIN: selects the Scirocco profile
    ecu_.set_pid(0x33, {static_cast<uint8_t>(kBaroKpa)});
    ecu_.set_pid(0x0F, {70}); // IAT 30 degC
    ecu_.set_pid(0x42, {0x37, 0x6E});
    apply(at(0));
}

DemoCar::Snapshot DemoCar::at(uint32_t now_ms) const
{
    const uint32_t t = now_ms % kCycleMs;
    Snapshot s{};
    double load = 0.0; // 0 idle .. 1 full throttle
    if (t < 3000) {
        s.rpm = 800 + 30 * std::sin(t / 150.0);
        load = 0.0;
    } else if (t < 15000) {
        // three pulls to the limiter with gear changes
        const uint32_t g = (t - 3000) % 4000;
        const double x = g / 4000.0;
        s.rpm = 2500 + (limiter_ - 2500 + 150) * x;
        if (s.rpm > limiter_) {
            s.rpm = limiter_ - 50 + 50 * std::sin(g / 20.0); // bouncing on the limiter
        }
        load = 1.0;
    } else {
        const double x = (t - 15000) / 5000.0;
        s.rpm = 3500 - 2700 * x;
        load = 0.05;
    }
    // MAP: vacuum at idle (~35 kPa), up to ~270 kPa at full load (beyond the PID 0x0B range).
    const double rpm_factor = s.rpm < 1500 ? 0.3 : (s.rpm < 3000 ? 0.7 : 1.0);
    s.map_kpa = 35 + (270 - 35) * load * rpm_factor;
    // Warm-up over the first 3 minutes, then thermostat at ~90 degC
    s.coolant_c = now_ms < 180000 ? 20 + 70 * (now_ms / 180000.0) : 90 + 2 * std::sin(now_ms / 30000.0);
    s.speed_kmh = s.rpm / 40.0;
    return s;
}

void DemoCar::apply(const Snapshot &s)
{
    const unsigned rpm_raw = static_cast<unsigned>(s.rpm * 4.0);
    ecu_.set_pid(0x0C, {static_cast<uint8_t>(rpm_raw >> 8), static_cast<uint8_t>(rpm_raw & 0xFF)});
    const double map = s.map_kpa > 255 ? 255 : s.map_kpa;
    ecu_.set_pid(0x0B, {static_cast<uint8_t>(map)});
    const unsigned map32 = static_cast<unsigned>(s.map_kpa * 32.0);
    ecu_.set_pid(0x87, {0x01, static_cast<uint8_t>(map32 >> 8), static_cast<uint8_t>(map32 & 0xFF), 0, 0});
    ecu_.set_pid(0x05, {static_cast<uint8_t>(s.coolant_c + 40)});
    ecu_.set_pid(0x0D, {static_cast<uint8_t>(s.speed_kmh > 255 ? 255 : s.speed_kmh)});
}

void DemoCar::on_tick(VirtualCanBus &bus, uint32_t now_ms)
{
    if (now_ms - last_update_ >= kUpdateMs) {
        last_update_ = now_ms;
        apply(at(now_ms));
    }
    ecu_.on_tick(bus, now_ms);
}

} // namespace sim
