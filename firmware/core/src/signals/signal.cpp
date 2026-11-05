// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/signals/signal.h"

#include "core/obd/obd.h"

namespace core::signals {

namespace {

SignalSource obd(uint8_t pid)
{
    SignalSource s;
    s.type = SourceType::ObdPid;
    s.pid = pid;
    s.verified = true; // standard J1979 formula; car support is checked with the bitmaps
    return s;
}

SignalSource derived(const char *ref)
{
    SignalSource s;
    s.type = SourceType::Derived;
    s.ref = ref;
    s.verified = true;
    return s;
}

SignalDef def(const char *id, const char *label, const char *unit, double min, double max, uint8_t decimals,
              PollPriority prio, std::vector<SignalSource> sources)
{
    SignalDef d;
    d.id = id;
    d.label = label;
    d.unit = unit;
    d.min = min;
    d.max = max;
    d.decimals = decimals;
    d.priority = prio;
    d.sources = std::move(sources);
    return d;
}

std::vector<SignalDef> make_builtins()
{
    namespace p = core::obd::pid;
    std::vector<SignalDef> v;
    v.push_back(def("engine_rpm", "RPM", "rpm", 0, 8000, 0, PollPriority::Fast, {obd(p::kEngineRpm)}));
    v.push_back(def("coolant_temp", "COOLANT", "degC", -40, 140, 0, PollPriority::Slow, {obd(p::kCoolantTemp)}));
    v.push_back(def("oil_temp", "OIL", "degC", -40, 160, 0, PollPriority::Slow, {obd(p::kOilTemp)}));
    v.push_back(def("intake_air_temp", "IAT", "degC", -40, 100, 0, PollPriority::Slow, {obd(p::kIntakeAirTemp)}));
    v.push_back(def("intake_map", "MAP", "kPa", 0, 300, 0, PollPriority::Normal,
                    {obd(p::kIntakeMapExtended), obd(p::kIntakeMap)}));
    v.push_back(def("barometric", "BARO", "kPa", 50, 110, 0, PollPriority::Slow, {obd(p::kBarometric)}));
    v.push_back(def("boost", "BOOST", "kPa", -100, 200, 2, PollPriority::Normal, {derived("intake_map-barometric")}));
    v.push_back(def("vehicle_speed", "SPEED", "km/h", 0, 260, 0, PollPriority::Normal, {obd(p::kVehicleSpeed)}));
    v.push_back(def("engine_load", "LOAD", "%", 0, 100, 0, PollPriority::Normal, {obd(p::kEngineLoad)}));
    v.push_back(def("throttle", "THROTTLE", "%", 0, 100, 0, PollPriority::Normal, {obd(p::kThrottle)}));
    v.push_back(def("maf_rate", "MAF", "g/s", 0, 300, 1, PollPriority::Normal, {obd(p::kMafRate)}));
    v.push_back(def("module_voltage", "VOLT", "V", 8, 16, 1, PollPriority::Slow, {obd(p::kModuleVoltage)}));
    v.push_back(def("ambient_temp", "AMBIENT", "degC", -40, 60, 0, PollPriority::Slow, {obd(p::kAmbientTemp)}));
    v.push_back(def("fuel_rate", "FUEL", "L/h", 0, 40, 1, PollPriority::Normal, {obd(p::kFuelRate)}));
    return v;
}

} // namespace

const std::vector<SignalDef> &builtin_signals()
{
    static const std::vector<SignalDef> kBuiltins = make_builtins();
    return kBuiltins;
}

const SignalDef *find_builtin(const std::string &id)
{
    for (const SignalDef &d : builtin_signals()) {
        if (d.id == id) {
            return &d;
        }
    }
    return nullptr;
}

} // namespace core::signals
