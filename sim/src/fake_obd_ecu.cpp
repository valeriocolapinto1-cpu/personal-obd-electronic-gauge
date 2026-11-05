// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/fake_obd_ecu.h"

namespace sim {

namespace {
constexpr uint8_t kSidCurrentData = 0x01;
constexpr uint8_t kSidVehicleInfo = 0x09;
constexpr uint8_t kPositiveOffset = 0x40;

core::transport::IsoTpConfig ecu_isotp(uint8_t index, bool extended)
{
    core::transport::IsoTpConfig c;
    c.extended = extended;
    if (extended) {
        c.rx_id = 0x18DA00F1u | (static_cast<uint32_t>(index) << 8);
        c.tx_id = 0x18DAF100u | index;
    } else {
        c.rx_id = 0x7E0u + index;
        c.tx_id = 0x7E8u + index;
    }
    c.fc_tx_id = c.tx_id;
    c.padding = 0xAA;
    return c;
}
} // namespace

FakeObdEcu::FakeObdEcu(uint8_t index, bool extended)
    : index_(index), extended_(extended), channel_(*this, ecu_isotp(index, extended))
{}

uint32_t FakeObdEcu::request_id() const
{
    return channel_.config().rx_id;
}

uint32_t FakeObdEcu::response_id() const
{
    return channel_.config().tx_id;
}

bool FakeObdEcu::send_frame(const core::bus::CanFrame &frame)
{
    if (bus_ != nullptr) {
        bus_->send(this, frame);
    }
    return true;
}

std::vector<uint8_t> FakeObdEcu::support_bitmap(uint8_t base) const
{
    uint32_t bits = 0;
    bool more = false;
    for (const auto &kv : pids_) {
        const unsigned p = kv.first;
        if (p > base && p <= base + 0x20u) {
            bits |= 1u << (32 - (p - base));
        }
        more = more || p > base + 0x20u;
    }
    if (more) {
        bits |= 1u;
    }
    return {static_cast<uint8_t>(bits >> 24), static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 8),
            static_cast<uint8_t>(bits)};
}

void FakeObdEcu::queue(std::vector<uint8_t> payload, uint32_t delay_ms, uint32_t now_ms)
{
    out_.push_back(Queued{now_ms + delay_ms, std::move(payload)});
}

void FakeObdEcu::handle_request(const uint8_t *req, size_t len, uint32_t now_ms)
{
    if (len < 2) {
        return;
    }
    ++requests_seen_;
    recent_.push_back(now_ms);
    while (!recent_.empty() && now_ms - recent_.front() >= 1000) {
        recent_.pop_front();
    }
    if (recent_.size() > max_rate_) {
        max_rate_ = static_cast<uint32_t>(recent_.size());
    }

    const uint8_t sid = req[0];
    const uint8_t id = req[1];
    if (sid == kSidVehicleInfo && id == 0x02) {
        if (vin_.empty()) {
            queue({0x7F, sid, 0x12}, 5, now_ms);
            return;
        }
        std::vector<uint8_t> p = {static_cast<uint8_t>(sid + kPositiveOffset), id, 0x01};
        p.insert(p.end(), vin_.begin(), vin_.end());
        queue(std::move(p), 10, now_ms);
        return;
    }
    if (sid != kSidCurrentData) {
        queue({0x7F, sid, 0x11}, 5, now_ms); // service not supported
        return;
    }

    auto it = pids_.find(id);
    if ((id % 0x20) == 0 && it == pids_.end()) {
        std::vector<uint8_t> bitmap = support_bitmap(id);
        if (id != 0 && bitmap == std::vector<uint8_t>{0, 0, 0, 0}) {
            return; // range not supported
        }
        std::vector<uint8_t> p = {static_cast<uint8_t>(sid + kPositiveOffset), id};
        p.insert(p.end(), bitmap.begin(), bitmap.end());
        queue(std::move(p), 5, now_ms);
        return;
    }
    if (it == pids_.end()) {
        return; // unsupported PID: silence (as on functional requests)
    }
    const PidScript &s = it->second;
    switch (s.behaviour) {
        case PidBehaviour::Ignore:
            return;
        case PidBehaviour::NegativeResponse:
            queue({0x7F, sid, s.nrc}, s.delay_ms, now_ms);
            return;
        case PidBehaviour::Respond: {
            std::vector<uint8_t> p = {static_cast<uint8_t>(sid + kPositiveOffset), id};
            p.insert(p.end(), s.data.begin(), s.data.end());
            queue(std::move(p), s.delay_ms, now_ms);
            return;
        }
    }
}

void FakeObdEcu::on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame)
{
    bus_ = &bus;
    if (!online_ || frame.extended != extended_) {
        return;
    }
    if (frame.id == functional_id()) {
        const uint8_t pci = frame.data[0];
        if ((pci & 0xF0) == 0 && (pci & 0x0F) >= 1 && (pci & 0x0F) <= 7) {
            handle_request(&frame.data[1], pci & 0x0F, bus.now_ms());
        }
        return;
    }
    channel_.on_frame(frame, bus.now_ms());
    if (channel_.has_message()) {
        handle_request(channel_.message(), channel_.message_len(), bus.now_ms());
        channel_.consume();
    }
}

void FakeObdEcu::on_tick(VirtualCanBus &bus, uint32_t now_ms)
{
    bus_ = &bus;
    channel_.poll(now_ms);
    if (!online_) {
        out_.clear();
        return;
    }
    if (!out_.empty() && static_cast<int32_t>(now_ms - out_.front().due_ms) >= 0 && !channel_.tx_busy()) {
        const Queued q = out_.front();
        out_.pop_front();
        channel_.transmit(q.payload.data(), q.payload.size());
    }
}

} // namespace sim
