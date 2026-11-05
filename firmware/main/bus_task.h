// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vehicle bus task: CAN bitrate detection (listen-only), OBD session, profile selection.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/signals/signal.h"
#include "core/signals/signal_store.h"

namespace app {

struct BusStatus {
    std::string phase;   ///< e.g. "detecting", "polling", "bus silent"
    std::string profile; ///< active profile name
    bool vin_read = false;
    uint32_t bitrate = 0;
    uint32_t requests = 0;
    uint32_t responses = 0;
    uint32_t rejected_by_gate = 0;
};

/// Starts the bus task on core 0.
void bus_task_start();

core::signals::SignalStore &signal_store();
/// Copy of the active signal definitions (empty until a profile is selected).
std::vector<core::signals::SignalDef> active_signals();
BusStatus bus_status();
/// Tells the bus task which slot is on screen (polled fast).
void set_focus_slot(size_t slot);

} // namespace app
