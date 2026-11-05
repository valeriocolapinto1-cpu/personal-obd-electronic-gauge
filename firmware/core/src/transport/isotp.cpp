// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/transport/isotp.h"

#include <cstring>

namespace core::transport {

namespace {
constexpr uint8_t kPciSingle = 0x00;
constexpr uint8_t kPciFirst = 0x10;
constexpr uint8_t kPciConsecutive = 0x20;
constexpr uint8_t kPciFlowControl = 0x30;

constexpr uint8_t kFcContinue = 0x0;
constexpr uint8_t kFcWait = 0x1;
constexpr uint8_t kFcOverflow = 0x2;

bool time_reached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}
} // namespace

const char *to_string(IsoTpError e)
{
    switch (e) {
        case IsoTpError::None:
            return "none";
        case IsoTpError::Busy:
            return "busy";
        case IsoTpError::TooLong:
            return "too-long";
        case IsoTpError::TimeoutBs:
            return "timeout-bs";
        case IsoTpError::TimeoutCr:
            return "timeout-cr";
        case IsoTpError::WrongSequence:
            return "wrong-sequence";
        case IsoTpError::Overflow:
            return "overflow";
        case IsoTpError::TooManyWaits:
            return "too-many-waits";
        case IsoTpError::SinkFailed:
            return "sink-failed";
    }
    return "?";
}

uint32_t IsoTpChannel::stmin_to_ms(uint8_t stmin)
{
    if (stmin <= 0x7F) {
        return stmin;
    }
    if (stmin >= 0xF1 && stmin <= 0xF9) {
        return 1; // 100..900 us, rounded up to our 1 ms resolution
    }
    return 0x7F; // reserved values: use the maximum (ISO 15765-2)
}

bool IsoTpChannel::send(uint32_t id, const uint8_t *data, uint8_t len)
{
    core::bus::CanFrame f;
    f.id = id;
    f.extended = cfg_.extended;
    f.dlc = 8;
    std::memset(f.data, cfg_.padding, sizeof(f.data));
    std::memcpy(f.data, data, len);
    return sink_.send_frame(f);
}

void IsoTpChannel::send_flow_control(uint8_t flag, uint8_t bs, uint8_t stmin)
{
    const uint8_t fc[3] = {static_cast<uint8_t>(kPciFlowControl | flag), bs, stmin};
    if (!send(cfg_.fc_tx_id, fc, sizeof(fc))) {
        last_error_ = IsoTpError::SinkFailed;
    }
}

void IsoTpChannel::fail_tx(IsoTpError e)
{
    last_error_ = e;
    tx_state_ = TxState::Idle;
}

void IsoTpChannel::reset()
{
    tx_state_ = TxState::Idle;
    if (rx_state_ == RxState::Receiving) {
        rx_state_ = RxState::Idle;
    }
}

bool IsoTpChannel::transmit(const uint8_t *payload, size_t len)
{
    if (tx_state_ != TxState::Idle) {
        last_error_ = IsoTpError::Busy;
        return false;
    }
    if (len == 0 || len > kMaxMessage || len > 4095) {
        last_error_ = IsoTpError::TooLong;
        return false;
    }
    if (len <= 7) {
        uint8_t sf[8];
        sf[0] = static_cast<uint8_t>(kPciSingle | len);
        std::memcpy(&sf[1], payload, len);
        if (!send(cfg_.tx_id, sf, static_cast<uint8_t>(len + 1))) {
            last_error_ = IsoTpError::SinkFailed;
            return false;
        }
        return true;
    }

    std::memcpy(tx_buf_, payload, len);
    tx_len_ = len;
    uint8_t ff[8];
    ff[0] = static_cast<uint8_t>(kPciFirst | ((len >> 8) & 0x0F));
    ff[1] = static_cast<uint8_t>(len & 0xFF);
    std::memcpy(&ff[2], payload, 6);
    // Enter WaitFc before sending: with a synchronous driver the flow control can
    // arrive while send() is still on the stack.
    tx_pos_ = 6;
    tx_sn_ = 1;
    tx_waits_ = 0;
    tx_state_ = TxState::WaitFc;
    tx_deadline_ = now_ms_ + cfg_.n_bs_ms;
    if (!send(cfg_.tx_id, ff, 8)) {
        tx_state_ = TxState::Idle;
        last_error_ = IsoTpError::SinkFailed;
        return false;
    }
    return true;
}

void IsoTpChannel::handle_flow_control(const core::bus::CanFrame &f, uint32_t now_ms)
{
    if (tx_state_ != TxState::WaitFc || f.dlc < 3) {
        return;
    }
    const uint8_t flag = f.data[0] & 0x0F;
    switch (flag) {
        case kFcContinue:
            tx_bs_ = f.data[1];
            tx_stmin_ms_ = stmin_to_ms(f.data[2]);
            tx_block_cnt_ = 0;
            tx_state_ = TxState::SendingCf;
            tx_next_cf_ = now_ms; // first CF right away
            poll(now_ms);
            break;
        case kFcWait:
            if (++tx_waits_ > cfg_.max_wait_frames) {
                fail_tx(IsoTpError::TooManyWaits);
            } else {
                tx_deadline_ = now_ms + cfg_.n_bs_ms;
            }
            break;
        case kFcOverflow:
            fail_tx(IsoTpError::Overflow);
            break;
        default:
            break; // reserved: ignore
    }
}

void IsoTpChannel::on_frame(const core::bus::CanFrame &f, uint32_t now_ms)
{
    now_ms_ = now_ms;
    if (f.id != cfg_.rx_id || f.extended != cfg_.extended || f.dlc == 0) {
        return;
    }
    const uint8_t type = f.data[0] & 0xF0;

    if (type == kPciFlowControl) {
        handle_flow_control(f, now_ms);
        return;
    }

    if (type == kPciSingle) {
        const uint8_t len = f.data[0] & 0x0F;
        if (len == 0 || len > 7 || len + 1 > f.dlc) {
            return; // invalid / CAN FD escape: ignore
        }
        std::memcpy(rx_buf_, &f.data[1], len);
        rx_len_ = len;
        rx_state_ = RxState::Complete;
        return;
    }

    if (type == kPciFirst) {
        if (f.dlc < 8) {
            return;
        }
        const size_t len = (static_cast<size_t>(f.data[0] & 0x0F) << 8) | f.data[1];
        if (len < 8) {
            return; // must have been a single frame
        }
        if (len > kMaxMessage) {
            send_flow_control(kFcOverflow, 0, 0);
            last_error_ = IsoTpError::Overflow;
            rx_state_ = RxState::Idle;
            return;
        }
        std::memcpy(rx_buf_, &f.data[2], 6);
        rx_len_ = len;
        rx_pos_ = 6;
        rx_sn_ = 1;
        rx_block_cnt_ = 0;
        rx_state_ = RxState::Receiving;
        rx_deadline_ = now_ms + cfg_.n_cr_ms;
        send_flow_control(kFcContinue, cfg_.rx_block_size, cfg_.rx_stmin_ms);
        return;
    }

    if (type == kPciConsecutive) {
        if (rx_state_ != RxState::Receiving) {
            return; // unsolicited CF (another tester's transfer): ignore
        }
        const uint8_t sn = f.data[0] & 0x0F;
        if (sn != rx_sn_) {
            last_error_ = IsoTpError::WrongSequence;
            rx_state_ = RxState::Idle;
            return;
        }
        rx_sn_ = static_cast<uint8_t>((rx_sn_ + 1) & 0x0F);
        size_t n = rx_len_ - rx_pos_;
        if (n > 7) {
            n = 7;
        }
        if (n + 1 > f.dlc) {
            last_error_ = IsoTpError::WrongSequence;
            rx_state_ = RxState::Idle;
            return;
        }
        std::memcpy(&rx_buf_[rx_pos_], &f.data[1], n);
        rx_pos_ += n;
        if (rx_pos_ >= rx_len_) {
            rx_state_ = RxState::Complete;
            return;
        }
        rx_deadline_ = now_ms + cfg_.n_cr_ms;
        if (cfg_.rx_block_size != 0 && ++rx_block_cnt_ >= cfg_.rx_block_size) {
            rx_block_cnt_ = 0;
            send_flow_control(kFcContinue, cfg_.rx_block_size, cfg_.rx_stmin_ms);
        }
    }
}

void IsoTpChannel::poll(uint32_t now_ms)
{
    now_ms_ = now_ms;

    if (rx_state_ == RxState::Receiving && time_reached(now_ms, rx_deadline_)) {
        last_error_ = IsoTpError::TimeoutCr;
        rx_state_ = RxState::Idle;
    }

    if (tx_state_ == TxState::WaitFc && time_reached(now_ms, tx_deadline_)) {
        fail_tx(IsoTpError::TimeoutBs);
        return;
    }

    while (tx_state_ == TxState::SendingCf && time_reached(now_ms, tx_next_cf_)) {
        uint8_t cf[8];
        cf[0] = static_cast<uint8_t>(kPciConsecutive | tx_sn_);
        size_t n = tx_len_ - tx_pos_;
        if (n > 7) {
            n = 7;
        }
        std::memcpy(&cf[1], &tx_buf_[tx_pos_], n);
        if (!send(cfg_.tx_id, cf, static_cast<uint8_t>(n + 1))) {
            fail_tx(IsoTpError::SinkFailed);
            return;
        }
        tx_pos_ += n;
        tx_sn_ = static_cast<uint8_t>((tx_sn_ + 1) & 0x0F);
        if (tx_pos_ >= tx_len_) {
            tx_state_ = TxState::Idle;
            return;
        }
        if (tx_bs_ != 0 && ++tx_block_cnt_ >= tx_bs_) {
            tx_state_ = TxState::WaitFc;
            tx_deadline_ = now_ms + cfg_.n_bs_ms;
            return;
        }
        tx_next_cf_ = now_ms + tx_stmin_ms_;
        if (tx_stmin_ms_ != 0) {
            return; // next CF on a later poll
        }
    }
}

} // namespace core::transport
