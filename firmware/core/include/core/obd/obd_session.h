// SPDX-License-Identifier: GPL-3.0-or-later
//
// OBD-II session over ISO 15765-4 (CAN): ECU discovery, PID support, VIN, cyclic polling.
// Runs after the bitrate is known (CanBitrateDetector). Clock-agnostic and host-testable.
//
// Every request goes through a GatedRequestSink (SafetyGate) into an IsoTpChannel.
// Every response is validated (service + PID echo); unsolicited or mismatched frames,
// e.g. answers to another dongle's requests, are dropped.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/bus/can_frame.h"
#include "core/obd/vin.h"
#include "core/scheduler/poll_scheduler.h"
#include "core/signals/signal.h"
#include "core/signals/signal_store.h"
#include "core/transport/isotp.h"
#include "core/transport/safety_gate.h"

namespace core::obd {

struct SessionConfig {
    bool allow_29bit = true;            ///< try 29-bit ids if 11-bit gets no answer
    uint32_t discovery_window_ms = 150; ///< collect answers to the functional "01 00"
    uint32_t response_timeout_ms = 100; ///< P2CAN (50 ms) + margin
    uint32_t pending_timeout_ms = 5000; ///< after NRC 0x78 (response pending)
    uint32_t vin_timeout_ms = 1000;
    uint32_t lost_after_timeouts = 10; ///< consecutive timeouts => bus lost
    core::scheduler::SchedulerConfig scheduler;
};

enum class SessionState : uint8_t {
    Idle,
    Discovering,
    ReadingSupport,
    ReadingVin,
    WaitingForSignals, ///< VIN known (or not available): caller picks the profile
    Polling,
    NoResponse, ///< no ECU answered on 11-bit nor 29-bit
    Lost,       ///< too many consecutive timeouts while polling
};

const char *to_string(SessionState s);

struct SessionStats {
    uint32_t requests = 0;
    uint32_t responses = 0;
    uint32_t mismatched = 0; ///< valid frames that were not the answer we wait for
    uint32_t negative = 0;
    uint32_t timeouts = 0;
};

class ObdSession {
public:
    ObdSession(core::transport::ICanFrameSink &can, core::signals::SignalStore &store,
               const SessionConfig &cfg = SessionConfig());

    void start(uint32_t now_ms);
    void on_frame(const core::bus::CanFrame &frame, uint32_t now_ms);
    void poll(uint32_t now_ms);

    /// Installs the signals to poll (slot = index in `defs`). Moves to Polling.
    void set_signals(const std::vector<core::signals::SignalDef> &defs, uint32_t now_ms);
    /// Promotes the signal shown on screen to the fast rate.
    void set_focus(size_t slot);

    SessionState state() const { return state_; }
    bool extended_ids() const { return extended_; }
    uint32_t ecu_response_id() const { return ecu_rx_id_; }
    bool has_vin() const { return has_vin_; }
    const Vin &vin() const { return vin_; }
    bool pid_supported(uint8_t pid) const { return supported_[pid]; }
    const SessionStats &stats() const { return stats_; }
    const core::transport::GateStats &gate_stats() const { return gated_.stats(); }

    /// Source chosen for a slot: OBD PID, or 0 when derived / unavailable.
    uint8_t slot_pid(size_t slot) const;

private:
    enum class Pending : uint8_t { None, Support, Vin, Pid };

    struct SlotPlan {
        uint8_t pid = 0; ///< 0 = not polled
        bool derived_boost = false;
        int poll_index = -1; ///< scheduler item
    };

    void begin_discovery(bool extended, uint32_t now_ms);
    void finish_discovery(uint32_t now_ms);
    void request_next_support(uint32_t now_ms);
    void request_vin(uint32_t now_ms);
    /// Sends now if the global rate limit allows it, otherwise defers to poll().
    bool send_request(const uint8_t *req, size_t len, Pending what, uint8_t id, uint32_t now_ms);
    bool transmit_now(const uint8_t *req, size_t len, Pending what, uint8_t id, uint32_t now_ms);
    uint32_t min_gap_ms() const;
    void handle_message(uint32_t now_ms);
    void handle_timeout(uint32_t now_ms);
    void publish_pid(size_t slot, uint8_t pid, const uint8_t *data, uint32_t now_ms);
    void update_derived(uint32_t now_ms);
    uint32_t functional_id() const;
    uint32_t physical_tx_for(uint32_t rx_id) const;

    core::signals::SignalStore &store_;
    SessionConfig cfg_;
    core::transport::IsoTpChannel channel_;
    core::transport::GatedRequestSink gated_;
    core::scheduler::PollScheduler scheduler_;

    SessionState state_ = SessionState::Idle;
    bool extended_ = false;
    uint32_t deadline_ms_ = 0;
    uint32_t ecu_rx_id_ = 0;
    std::vector<uint32_t> responders_;
    bool supported_[256] = {};
    uint8_t next_support_base_ = 0;

    Pending pending_ = Pending::None;
    uint8_t pending_id_ = 0; ///< PID or info type
    int pending_item_ = -1;  ///< scheduler item
    uint32_t consecutive_timeouts_ = 0;
    bool has_last_request_ = false;
    uint32_t last_request_ms_ = 0;

    struct Deferred {
        bool active = false;
        uint8_t req[2] = {};
        size_t len = 0;
        Pending what = Pending::None;
        uint8_t id = 0;
    } deferred_;

    bool has_vin_ = false;
    Vin vin_;

    std::vector<SlotPlan> plan_;
    std::vector<int> item_slot_; ///< scheduler item -> slot
    int map_slot_ = -1;
    int baro_slot_ = -1;
    SessionStats stats_;
};

} // namespace core::obd
