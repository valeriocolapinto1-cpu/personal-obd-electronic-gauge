// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scripted fake OBD-II ECU (ISO 15765-4) for the simulator.
//  - 11-bit (0x7E0+n / 0x7E8+n) or 29-bit (0x18DAxxF1 / 0x18DAF1xx) addressing;
//  - functional and physical requests;
//  - service 0x01 PIDs with scripted data, timeouts and negative responses;
//  - service 0x09 PID 0x02 (VIN) as an ISO-TP multi-frame answer;
//  - automatic "supported PIDs" bitmaps.
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "core/transport/isotp.h"
#include "sim/virtual_can_bus.h"

namespace sim {

enum class PidBehaviour : uint8_t {
    Respond,          ///< answer with the scripted bytes
    Ignore,           ///< never answer (timeout)
    NegativeResponse, ///< 7F <sid> <nrc>
};

struct PidScript {
    std::vector<uint8_t> data; ///< bytes after "41 <pid>"
    PidBehaviour behaviour = PidBehaviour::Respond;
    uint8_t nrc = 0x12;    ///< negative response code
    uint32_t delay_ms = 5; ///< response latency
};

class FakeObdEcu : public INode, private core::transport::ICanFrameSink {
public:
    /// `index`: 11-bit ECU number 0..7, or the 29-bit ECU address (e.g. 0x10).
    explicit FakeObdEcu(uint8_t index = 0, bool extended = false);

    void set_pid(uint8_t pid, PidScript script) { pids_[pid] = std::move(script); }
    void set_pid(uint8_t pid, std::vector<uint8_t> data) { pids_[pid] = PidScript{std::move(data)}; }
    /// Enables service 09 PID 02. Empty = not supported (negative response 0x12).
    void set_vin(const std::string &vin) { vin_ = vin; }
    /// When false the ECU stops answering anything (unplugged / ignition off).
    void set_online(bool online) { online_ = online; }

    uint32_t request_id() const;
    uint32_t response_id() const;
    uint32_t requests_seen() const { return requests_seen_; }
    /// Highest request rate seen over any 1 s window (for rate-limit tests).
    uint32_t max_requests_per_second() const { return max_rate_; }

    void on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame) override;
    void on_tick(VirtualCanBus &bus, uint32_t now_ms) override;

private:
    struct Queued {
        uint32_t due_ms;
        std::vector<uint8_t> payload;
    };

    bool send_frame(const core::bus::CanFrame &frame) override;
    void handle_request(const uint8_t *req, size_t len, uint32_t now_ms);
    void queue(std::vector<uint8_t> payload, uint32_t delay_ms, uint32_t now_ms);
    std::vector<uint8_t> support_bitmap(uint8_t base) const;
    uint32_t functional_id() const { return extended_ ? 0x18DB33F1u : 0x7DFu; }

    uint8_t index_;
    bool extended_;
    bool online_ = true;
    std::map<uint8_t, PidScript> pids_;
    std::string vin_;
    core::transport::IsoTpChannel channel_;
    VirtualCanBus *bus_ = nullptr;
    std::deque<Queued> out_;
    uint32_t requests_seen_ = 0;
    std::deque<uint32_t> recent_;
    uint32_t max_rate_ = 0;
};

} // namespace sim
