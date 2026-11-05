// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/obd/obd_session.h"

#include "core/obd/obd.h"
#include "core/util/log.h"

namespace core::obd {

using core::bus::CanFrame;
using core::signals::SignalDef;
using core::signals::SourceType;
using core::transport::RequestContext;

namespace {
const char *TAG = "obd";

constexpr uint32_t kFunctional11 = 0x7DF;
constexpr uint32_t kFunctional29 = 0x18DB33F1;
constexpr uint8_t kNrcResponsePending = 0x78;

bool reached(uint32_t now, uint32_t t)
{
    return static_cast<int32_t>(now - t) >= 0;
}

bool is_response_id(uint32_t id, bool extended)
{
    if (extended) {
        return (id & 0x1FFFFF00u) == 0x18DAF100u;
    }
    return id >= 0x7E8 && id <= 0x7EF;
}

core::transport::IsoTpConfig initial_isotp()
{
    core::transport::IsoTpConfig c;
    c.tx_id = kFunctional11;
    c.fc_tx_id = 0x7E0;
    c.rx_id = 0x7E8;
    return c;
}
} // namespace

const char *to_string(SessionState s)
{
    switch (s) {
        case SessionState::Idle:
            return "idle";
        case SessionState::Discovering:
            return "discovering";
        case SessionState::ReadingSupport:
            return "reading-support";
        case SessionState::ReadingVin:
            return "reading-vin";
        case SessionState::WaitingForSignals:
            return "waiting-for-signals";
        case SessionState::Polling:
            return "polling";
        case SessionState::NoResponse:
            return "no-response";
        case SessionState::Lost:
            return "lost";
    }
    return "?";
}

ObdSession::ObdSession(core::transport::ICanFrameSink &can, core::signals::SignalStore &store, const SessionConfig &cfg)
    : store_(store), cfg_(cfg), channel_(can, initial_isotp()), gated_(channel_, RequestContext::Obd),
      scheduler_(cfg.scheduler)
{}

uint32_t ObdSession::functional_id() const
{
    return extended_ ? kFunctional29 : kFunctional11;
}

uint32_t ObdSession::physical_tx_for(uint32_t rx_id) const
{
    if (extended_) {
        const uint32_t ecu = rx_id & 0xFFu;
        return 0x18DA00F1u | (ecu << 8);
    }
    return rx_id - 8;
}

uint8_t ObdSession::slot_pid(size_t slot) const
{
    return slot < plan_.size() ? plan_[slot].pid : 0;
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------
void ObdSession::start(uint32_t now_ms)
{
    has_vin_ = false;
    for (bool &b : supported_) {
        b = false;
    }
    plan_.clear();
    item_slot_.clear();
    scheduler_.clear();
    map_slot_ = baro_slot_ = -1;
    consecutive_timeouts_ = 0;
    deferred_ = Deferred{};
    begin_discovery(false, now_ms);
}

void ObdSession::begin_discovery(bool extended, uint32_t now_ms)
{
    extended_ = extended;
    responders_.clear();
    core::transport::IsoTpConfig c = channel_.config();
    c.extended = extended;
    c.tx_id = functional_id();
    channel_.reconfigure(c);
    state_ = SessionState::Discovering;
    uint8_t req[2];
    build_request_01(pid::kSupported01To20, req, sizeof(req));
    send_request(req, sizeof(req), Pending::Support, 0x00, now_ms);
    deadline_ms_ = now_ms + cfg_.discovery_window_ms;
}

void ObdSession::finish_discovery(uint32_t now_ms)
{
    pending_ = Pending::None;
    if (responders_.empty()) {
        if (!extended_ && cfg_.allow_29bit) {
            CORE_LOGI(TAG, "no answer on 11-bit ids, trying 29-bit");
            begin_discovery(true, now_ms);
            return;
        }
        CORE_LOGW(TAG, "no ECU answered");
        state_ = SessionState::NoResponse;
        return;
    }
    // Engine ECU: 0x7E8 / 0x18DAF110 by convention, otherwise the lowest responder.
    uint32_t chosen = responders_.front();
    for (uint32_t id : responders_) {
        const uint32_t preferred = extended_ ? 0x18DAF110u : 0x7E8u;
        if (id == preferred) {
            chosen = id;
            break;
        }
        if (id < chosen) {
            chosen = id;
        }
    }
    ecu_rx_id_ = chosen;
    core::transport::IsoTpConfig c = channel_.config();
    c.rx_id = chosen;
    c.tx_id = physical_tx_for(chosen);
    c.fc_tx_id = c.tx_id;
    channel_.reconfigure(c);
    CORE_LOGI(TAG, "ECU 0x%X (%u responder(s), %s ids)", static_cast<unsigned>(chosen),
              static_cast<unsigned>(responders_.size()), extended_ ? "29-bit" : "11-bit");
    state_ = SessionState::ReadingSupport;
    request_next_support(now_ms);
}

void ObdSession::request_next_support(uint32_t now_ms)
{
    // supported_[base] tells whether the range starting at `base` exists.
    while (next_support_base_ != 0 && next_support_base_ < 0xE0 + 1 && supported_[next_support_base_]) {
        uint8_t req[2];
        build_request_01(next_support_base_, req, sizeof(req));
        if (send_request(req, sizeof(req), Pending::Support, next_support_base_, now_ms)) {
            return;
        }
        break;
    }
    state_ = SessionState::ReadingVin;
    request_vin(now_ms);
}

void ObdSession::request_vin(uint32_t now_ms)
{
    uint8_t req[2];
    build_request_09(kInfoTypeVin, req, sizeof(req));
    if (!send_request(req, sizeof(req), Pending::Vin, kInfoTypeVin, now_ms)) {
        state_ = SessionState::WaitingForSignals;
        return;
    }
    deadline_ms_ = now_ms + cfg_.vin_timeout_ms;
}

uint32_t ObdSession::min_gap_ms() const
{
    const uint32_t rps = cfg_.scheduler.max_requests_per_s;
    return rps ? (1000 + rps - 1) / rps : 1000;
}

bool ObdSession::send_request(const uint8_t *req, size_t len, Pending what, uint8_t id, uint32_t now_ms)
{
    // The global rate limit covers every request, not only the cyclic ones.
    if (has_last_request_ && !reached(now_ms, last_request_ms_ + min_gap_ms())) {
        if (len > sizeof(deferred_.req)) {
            return false; // the gate still checks it when it is actually sent
        }
        deferred_.active = true;
        deferred_.len = len;
        for (size_t i = 0; i < len; ++i) {
            deferred_.req[i] = req[i];
        }
        deferred_.what = what;
        deferred_.id = id;
        pending_ = what; // reserved: nothing else is sent meanwhile
        pending_id_ = id;
        deadline_ms_ = last_request_ms_ + min_gap_ms() + cfg_.response_timeout_ms;
        return true;
    }
    return transmit_now(req, len, what, id, now_ms);
}

bool ObdSession::transmit_now(const uint8_t *req, size_t len, Pending what, uint8_t id, uint32_t now_ms)
{
    channel_.poll(now_ms);
    if (!gated_.send(req, len)) {
        return false;
    }
    ++stats_.requests;
    has_last_request_ = true;
    last_request_ms_ = now_ms;
    pending_ = what;
    pending_id_ = id;
    deadline_ms_ = now_ms + cfg_.response_timeout_ms;
    return true;
}

// ---------------------------------------------------------------------------
// Frames and timers
// ---------------------------------------------------------------------------
void ObdSession::on_frame(const CanFrame &frame, uint32_t now_ms)
{
    if (state_ == SessionState::Discovering) {
        // Answers to the functional request: single frames "41 00 a b c d" from any ECU.
        if (frame.extended != extended_ || !is_response_id(frame.id, extended_) || frame.dlc < 7) {
            return;
        }
        const uint8_t len = frame.data[0];
        const ParsedResponse r = parse_response_01(pid::kSupported01To20, &frame.data[1], len <= 7 ? len : 0);
        if (r.kind != ResponseKind::Positive) {
            ++stats_.mismatched;
            return;
        }
        for (uint32_t id : responders_) {
            if (id == frame.id) {
                return; // duplicate
            }
        }
        responders_.push_back(frame.id);
        if (responders_.size() == 1 || frame.id == (extended_ ? 0x18DAF110u : 0x7E8u)) {
            for (bool &b : supported_) {
                b = false;
            }
            next_support_base_ = 0x20;
            supported_[0x20] = decode_support_bitmap(0x00, r.data, supported_);
        }
        ++stats_.responses;
        return;
    }
    channel_.on_frame(frame, now_ms);
    if (channel_.has_message()) {
        handle_message(now_ms);
    }
}

void ObdSession::poll(uint32_t now_ms)
{
    channel_.poll(now_ms);
    if (deferred_.active && reached(now_ms, last_request_ms_ + min_gap_ms())) {
        deferred_.active = false;
        if (!transmit_now(deferred_.req, deferred_.len, deferred_.what, deferred_.id, now_ms)) {
            pending_ = Pending::None;
        }
    }
    if (deferred_.active) {
        return;
    }
    switch (state_) {
        case SessionState::Discovering:
            if (reached(now_ms, deadline_ms_)) {
                finish_discovery(now_ms);
            }
            return;
        case SessionState::ReadingSupport:
        case SessionState::ReadingVin:
        case SessionState::Polling:
            if (pending_ != Pending::None && reached(now_ms, deadline_ms_)) {
                handle_timeout(now_ms);
            }
            break;
        case SessionState::Lost:
        case SessionState::Idle:
        case SessionState::NoResponse:
        case SessionState::WaitingForSignals:
            return;
    }
    if (state_ == SessionState::Polling && pending_ == Pending::None &&
        (!has_last_request_ || reached(now_ms, last_request_ms_ + min_gap_ms()))) {
        const int item = scheduler_.next(now_ms);
        if (item >= 0) {
            const size_t slot = static_cast<size_t>(item_slot_[static_cast<size_t>(item)]);
            uint8_t req[2];
            build_request_01(plan_[slot].pid, req, sizeof(req));
            if (send_request(req, sizeof(req), Pending::Pid, plan_[slot].pid, now_ms)) {
                pending_item_ = item;
            } else {
                // Should never happen for 01 xx; keep the scheduler consistent.
                scheduler_.on_negative(static_cast<size_t>(item), 0x11, now_ms);
            }
        }
    }
}

void ObdSession::handle_timeout(uint32_t now_ms)
{
    ++stats_.timeouts;
    const Pending what = pending_;
    pending_ = Pending::None;
    switch (what) {
        case Pending::Support:
            CORE_LOGW(TAG, "support bitmap 0x%02X timed out", pending_id_);
            state_ = SessionState::ReadingVin;
            request_vin(now_ms);
            return;
        case Pending::Vin:
            CORE_LOGW(TAG, "VIN not available");
            channel_.reset();
            state_ = SessionState::WaitingForSignals;
            return;
        case Pending::Pid:
            if (pending_item_ >= 0) {
                scheduler_.on_timeout(static_cast<size_t>(pending_item_), now_ms);
                const size_t slot = static_cast<size_t>(item_slot_[static_cast<size_t>(pending_item_)]);
                if (scheduler_.item(static_cast<size_t>(pending_item_)).state != core::scheduler::ItemState::Active) {
                    store_.invalidate(slot);
                }
            }
            pending_item_ = -1;
            if (++consecutive_timeouts_ >= cfg_.lost_after_timeouts) {
                CORE_LOGW(TAG, "bus lost after %u timeouts", static_cast<unsigned>(consecutive_timeouts_));
                for (size_t i = 0; i < plan_.size(); ++i) {
                    store_.invalidate(i);
                }
                state_ = SessionState::Lost;
            }
            return;
        case Pending::None:
            return;
    }
}

void ObdSession::handle_message(uint32_t now_ms)
{
    const uint8_t *msg = channel_.message();
    const size_t len = channel_.message_len();

    if (pending_ == Pending::None) {
        ++stats_.mismatched; // unsolicited (another tester's answer)
        channel_.consume();
        return;
    }

    if (pending_ == Pending::Vin) {
        const ParsedResponse r = parse_response_09(kInfoTypeVin, msg, len);
        if (r.kind == ResponseKind::Negative && r.nrc == kNrcResponsePending) {
            deadline_ms_ = now_ms + cfg_.pending_timeout_ms;
        } else if (r.kind == ResponseKind::Positive) {
            ++stats_.responses;
            has_vin_ = parse_vin_response(r.data, r.data_len, vin_) == VinStatus::Ok;
            pending_ = Pending::None;
            state_ = SessionState::WaitingForSignals;
        } else if (r.kind == ResponseKind::Negative) {
            ++stats_.negative;
            pending_ = Pending::None;
            state_ = SessionState::WaitingForSignals;
        } else {
            ++stats_.mismatched;
        }
        channel_.consume();
        return;
    }

    const ParsedResponse r = parse_response_01(pending_id_, msg, len);
    if (r.kind == ResponseKind::Mismatch || r.kind == ResponseKind::Malformed) {
        ++stats_.mismatched;
        channel_.consume();
        return; // keep waiting for our answer until the timeout
    }
    if (r.kind == ResponseKind::Negative && r.nrc == kNrcResponsePending) {
        deadline_ms_ = now_ms + cfg_.pending_timeout_ms;
        channel_.consume();
        return;
    }

    const Pending what = pending_;
    pending_ = Pending::None;
    consecutive_timeouts_ = 0;

    if (what == Pending::Support) {
        if (r.kind == ResponseKind::Positive) {
            ++stats_.responses;
            const uint8_t base = pending_id_;
            const bool more = decode_support_bitmap(base, r.data, supported_);
            next_support_base_ = static_cast<uint8_t>(base + 0x20);
            if (base == 0xE0) {
                next_support_base_ = 0;
            }
            if (next_support_base_ != 0) {
                supported_[next_support_base_] = more;
            }
        } else {
            ++stats_.negative;
            next_support_base_ = 0;
        }
        channel_.consume();
        request_next_support(now_ms);
        return;
    }

    // Pending::Pid
    const size_t item = static_cast<size_t>(pending_item_);
    pending_item_ = -1;
    if (r.kind == ResponseKind::Negative) {
        ++stats_.negative;
        scheduler_.on_negative(item, r.nrc, now_ms);
        channel_.consume();
        return;
    }
    ++stats_.responses;
    const size_t slot = static_cast<size_t>(item_slot_[item]);
    publish_pid(slot, pending_id_, r.data, now_ms);
    scheduler_.on_success(item, now_ms);
    channel_.consume();
}

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------
void ObdSession::set_signals(const std::vector<SignalDef> &defs, uint32_t now_ms)
{
    scheduler_.clear();
    scheduler_.set_config(cfg_.scheduler);
    plan_.assign(defs.size(), SlotPlan{});
    item_slot_.clear();
    map_slot_ = baro_slot_ = -1;

    for (size_t slot = 0; slot < defs.size() && slot < core::signals::SignalStore::kCapacity; ++slot) {
        const SignalDef &d = defs[slot];
        if (d.id == "intake_map") {
            map_slot_ = static_cast<int>(slot);
        } else if (d.id == "barometric") {
            baro_slot_ = static_cast<int>(slot);
        }
        for (const auto &src : d.sources) {
            if (src.type == SourceType::ObdPid && supported_[src.pid] && find_pid(src.pid) != nullptr) {
                plan_[slot].pid = src.pid;
                plan_[slot].poll_index = static_cast<int>(scheduler_.add(static_cast<uint32_t>(slot), d.priority));
                item_slot_.push_back(static_cast<int>(slot));
                break;
            }
            if (src.type == SourceType::Derived && src.ref == "intake_map-barometric") {
                plan_[slot].derived_boost = true;
                break;
            }
            // Manufacturer and module sources are handled outside this session.
        }
        store_.invalidate(slot);
    }
    pending_ = Pending::None;
    pending_item_ = -1;
    consecutive_timeouts_ = 0;
    deferred_ = Deferred{};
    state_ = SessionState::Polling;
    (void)now_ms;
}

void ObdSession::set_focus(size_t slot)
{
    if (slot < plan_.size() && plan_[slot].poll_index >= 0) {
        scheduler_.set_focus(static_cast<size_t>(plan_[slot].poll_index));
    } else if (slot < plan_.size() && plan_[slot].derived_boost && map_slot_ >= 0 &&
               plan_[static_cast<size_t>(map_slot_)].poll_index >= 0) {
        // Boost on screen: poll its MAP input fast.
        scheduler_.set_focus(static_cast<size_t>(plan_[static_cast<size_t>(map_slot_)].poll_index));
    } else {
        scheduler_.set_focus(SIZE_MAX);
    }
}

void ObdSession::publish_pid(size_t slot, uint8_t p, const uint8_t *data, uint32_t now_ms)
{
    const PidInfo *info = find_pid(p);
    double value = 0;
    if (info == nullptr || !info->decode(data, value)) {
        store_.invalidate(slot);
        return;
    }
    uint8_t flags = core::signals::kFlagNone;
    if (p == pid::kIntakeMap && data[0] == 0xFF) {
        flags |= core::signals::kFlagSaturated; // PID 0x0B cannot show more than 255 kPa
    }
    store_.publish(slot, value, now_ms, flags);
    if (static_cast<int>(slot) == map_slot_ || static_cast<int>(slot) == baro_slot_) {
        update_derived(now_ms);
    }
}

void ObdSession::update_derived(uint32_t now_ms)
{
    if (map_slot_ < 0 || baro_slot_ < 0) {
        return;
    }
    const core::signals::Sample map = store_.read(static_cast<size_t>(map_slot_));
    const core::signals::Sample baro = store_.read(static_cast<size_t>(baro_slot_));
    if (!map.valid || !baro.valid) {
        return;
    }
    for (size_t slot = 0; slot < plan_.size(); ++slot) {
        if (plan_[slot].derived_boost) {
            store_.publish(slot, map.value - baro.value, now_ms, map.flags);
        }
    }
}

} // namespace core::obd
