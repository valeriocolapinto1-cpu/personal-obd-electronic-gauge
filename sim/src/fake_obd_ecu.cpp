// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/fake_obd_ecu.h"

namespace sim {

namespace {
constexpr uint32_t kFunctionalRequestId = 0x7DF;
constexpr uint8_t kSidCurrentData = 0x01;
constexpr uint8_t kPositiveOffset = 0x40;
} // namespace

std::vector<uint8_t> FakeObdEcu::support_bitmap(uint8_t base) const
{
    uint32_t bits = 0;
    for (const auto &kv : pids_) {
        const uint8_t pid = kv.first;
        if (pid > base && pid <= base + 0x20) {
            bits |= 1u << (32 - (pid - base));
        }
    }
    // Bit 0 (PID base+0x20) is set when any PID above this range is supported.
    for (const auto &kv : pids_) {
        if (kv.first > base + 0x20) {
            bits |= 1u;
            break;
        }
    }
    return {static_cast<uint8_t>(bits >> 24), static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 8),
            static_cast<uint8_t>(bits)};
}

void FakeObdEcu::on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame)
{
    if (frame.extended || (frame.id != kFunctionalRequestId && frame.id != request_id())) {
        return;
    }
    // Single frame: PCI high nibble 0, length in low nibble.
    const uint8_t pci = frame.data[0];
    if ((pci & 0xF0) != 0 || (pci & 0x0F) < 2 || frame.dlc < 3) {
        return;
    }
    const uint8_t sid = frame.data[1];
    const uint8_t pid = frame.data[2];
    if (sid != kSidCurrentData) {
        return; // other services are added in later phases
    }
    ++requests_seen_;

    std::vector<uint8_t> payload;
    uint32_t delay = 5;
    const bool is_bitmap = (pid % 0x20) == 0;
    auto it = pids_.find(pid);
    if (is_bitmap && it == pids_.end()) {
        payload = support_bitmap(pid);
        if (pid != 0 && payload == std::vector<uint8_t>{0, 0, 0, 0}) {
            return; // range not supported: no answer (functional request)
        }
    } else if (it == pids_.end()) {
        return; // unsupported PID: ECUs stay silent on functional requests
    } else {
        const PidScript &s = it->second;
        delay = s.delay_ms;
        if (s.behaviour == PidBehaviour::Ignore) {
            return;
        }
        if (s.behaviour == PidBehaviour::NegativeResponse) {
            const uint8_t nr[] = {0x03, 0x7F, sid, s.nrc, 0xAA, 0xAA, 0xAA, 0xAA};
            bus.send(this, core::bus::CanFrame::make(response_id(), false, nr, 8), delay);
            return;
        }
        payload = s.data;
    }

    uint8_t out[8] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // ISO 15765-4 padding
    const size_t n = payload.size() > 5 ? 5 : payload.size();
    out[0] = static_cast<uint8_t>(2 + n);
    out[1] = sid + kPositiveOffset;
    out[2] = pid;
    for (size_t i = 0; i < n; ++i) {
        out[3 + i] = payload[i];
    }
    bus.send(this, core::bus::CanFrame::make(response_id(), false, out, 8), delay);
}

} // namespace sim
