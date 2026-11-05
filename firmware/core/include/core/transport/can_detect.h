// SPDX-License-Identifier: GPL-3.0-or-later
//
// CAN bitrate detection in LISTEN-ONLY mode (CLAUDE.md safety rule 2).
// The controller is opened listen-only at each candidate bitrate for an observation
// window; nothing is transmitted (no ACK, no error frames, no requests).
//
// Silent bus (e.g. a gateway that only forwards diagnostic traffic when asked): the
// detector reports Silent. Transmitting a probe on a silent bus is only allowed when the
// profile or the user explicitly names the bitrate (policy in decide_silent_bus()).
#pragma once

#include <cstddef>
#include <cstdint>

namespace core::transport {

struct BusObservation {
    uint32_t valid_frames = 0; ///< frames received without error during the window
    uint32_t bus_errors = 0;   ///< bus error count delta during the window
};

enum class DetectState : uint8_t {
    Listening, ///< observing at current_bitrate()
    Detected,  ///< valid traffic at detected_bitrate()
    Silent,    ///< no traffic at any candidate
    Failed,    ///< only errors (no candidate bitrate matches)
};

class CanBitrateDetector {
public:
    static constexpr uint32_t kCandidates[] = {500000, 250000}; ///< ISO 15765-4 bitrates
    static constexpr size_t kNumCandidates = 2;
    static constexpr uint32_t kMinValidFrames = 3;

    /// Restarts from the first candidate.
    void start();
    DetectState state() const { return state_; }
    /// Bitrate the controller must be opened at (listen-only) while Listening.
    uint32_t current_bitrate() const { return kCandidates[index_]; }
    uint32_t detected_bitrate() const { return detected_; }

    /// Feeds the result of one observation window. Returns the new state.
    DetectState feed(const BusObservation &obs);

private:
    DetectState state_ = DetectState::Listening;
    size_t index_ = 0;
    uint32_t detected_ = 0;
    bool saw_errors_ = false;
};

/// What to do when the bus looked silent at every bitrate.
enum class SilentBusAction : uint8_t {
    DoNotTransmit, ///< default: show "bus silent", try K-line only if allowed, ask the user
    ProbeAtHint,   ///< send ONE functional OBD request at the hinted bitrate
};

/// `hint_bitrate` = bitrate named by the selected profile or by the user (0 = none).
/// `user_allows_probe` = explicit opt-in from the app.
SilentBusAction decide_silent_bus(uint32_t hint_bitrate, bool user_allows_probe);

} // namespace core::transport
