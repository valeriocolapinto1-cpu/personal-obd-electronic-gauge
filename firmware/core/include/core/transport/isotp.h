// SPDX-License-Identifier: GPL-3.0-or-later
//
// ISO 15765-2 (ISO-TP) transport over classic CAN, normal addressing.
// Event driven and clock agnostic: the caller feeds received frames and the current time.
//
//  - Sender: single frame (<= 7 bytes) or first frame + consecutive frames paced by the
//    receiver's flow control (block size, STmin, WAIT, overflow).
//  - Receiver: single frame or first frame + consecutive frames, answers with flow control.
//
// The channel is the inner transport of a GatedRequestSink: diagnostic requests reach it only
// after the SafetyGate. Flow-control frames are transport protocol frames, not requests.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/bus/can_frame.h"
#include "core/transport/safety_gate.h"

namespace core::transport {

class ICanFrameSink {
public:
    virtual ~ICanFrameSink() = default;
    virtual bool send_frame(const core::bus::CanFrame &frame) = 0;
};

struct IsoTpConfig {
    uint32_t tx_id = 0x7E0;    ///< id used for requests (may be switched to the functional id)
    uint32_t fc_tx_id = 0x7E0; ///< id used for our flow-control frames (always physical)
    uint32_t rx_id = 0x7E8;    ///< id we receive from
    bool extended = false;     ///< 29-bit identifiers
    uint8_t padding = 0x55;    ///< frame padding byte (all frames are sent with DLC 8)
    uint32_t n_bs_ms = 1000;   ///< max wait for a flow control after FF / block
    uint32_t n_cr_ms = 1000;   ///< max gap between consecutive frames
    uint8_t rx_block_size = 0; ///< BS we announce (0 = no further FC)
    uint8_t rx_stmin_ms = 0;   ///< STmin we announce
    uint8_t max_wait_frames = 10;
};

enum class IsoTpError : uint8_t {
    None,
    Busy,          ///< transmit while a transmission is in progress
    TooLong,       ///< payload longer than the buffer
    TimeoutBs,     ///< no flow control
    TimeoutCr,     ///< consecutive frame gap too long
    WrongSequence, ///< unexpected sequence number
    Overflow,      ///< receiver reported overflow / message too large for us
    TooManyWaits,  ///< too many FC WAIT frames
    SinkFailed,    ///< the CAN driver refused a frame
};

class IsoTpChannel : public IRequestTransport {
public:
    static constexpr size_t kMaxMessage = 512;

    IsoTpChannel(ICanFrameSink &sink, const IsoTpConfig &cfg) : sink_(sink), cfg_(cfg) {}

    /// Starts sending a message. Single frames go out immediately.
    bool transmit(const uint8_t *payload, size_t len) override;

    /// Feeds a received CAN frame (frames with other ids are ignored).
    void on_frame(const core::bus::CanFrame &frame, uint32_t now_ms);

    /// Advances timers and sends pending consecutive frames.
    void poll(uint32_t now_ms);

    /// Switches the id used for requests (functional 0x7DF vs physical 0x7E0+n).
    void set_tx_id(uint32_t id) { cfg_.tx_id = id; }
    const IsoTpConfig &config() const { return cfg_; }

    bool tx_busy() const { return tx_state_ != TxState::Idle; }
    bool rx_busy() const { return rx_state_ == RxState::Receiving; }

    /// A complete message has been received and not consumed yet.
    bool has_message() const { return rx_state_ == RxState::Complete; }
    const uint8_t *message() const { return rx_buf_; }
    size_t message_len() const { return rx_len_; }
    void consume() { rx_state_ = RxState::Idle; }

    /// Last error since the previous call (reset on read).
    IsoTpError take_error()
    {
        const IsoTpError e = last_error_;
        last_error_ = IsoTpError::None;
        return e;
    }

    /// Drops any transfer in progress (e.g. on bus loss).
    void reset();

private:
    enum class TxState : uint8_t { Idle, WaitFc, SendingCf };
    enum class RxState : uint8_t { Idle, Receiving, Complete };

    bool send(uint32_t id, const uint8_t *data, uint8_t len);
    void send_flow_control(uint8_t flag, uint8_t bs, uint8_t stmin);
    void handle_flow_control(const core::bus::CanFrame &f, uint32_t now_ms);
    void fail_tx(IsoTpError e);
    static uint32_t stmin_to_ms(uint8_t stmin);

    ICanFrameSink &sink_;
    IsoTpConfig cfg_;
    uint32_t now_ms_ = 0;
    IsoTpError last_error_ = IsoTpError::None;

    // Transmit side
    TxState tx_state_ = TxState::Idle;
    uint8_t tx_buf_[kMaxMessage] = {};
    size_t tx_len_ = 0;
    size_t tx_pos_ = 0;
    uint8_t tx_sn_ = 0;
    uint8_t tx_bs_ = 0;        // block size granted by the receiver
    uint8_t tx_block_cnt_ = 0; // CFs sent in the current block
    uint32_t tx_stmin_ms_ = 0;
    uint32_t tx_deadline_ = 0; // FC timeout
    uint32_t tx_next_cf_ = 0;  // earliest time for the next CF
    uint8_t tx_waits_ = 0;

    // Receive side
    RxState rx_state_ = RxState::Idle;
    uint8_t rx_buf_[kMaxMessage] = {};
    size_t rx_len_ = 0;
    size_t rx_pos_ = 0;
    uint8_t rx_sn_ = 0;
    uint8_t rx_block_cnt_ = 0;
    uint32_t rx_deadline_ = 0;
};

const char *to_string(IsoTpError e);

} // namespace core::transport
