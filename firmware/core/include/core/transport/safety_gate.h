// SPDX-License-Identifier: GPL-3.0-or-later
//
// SafetyGate: the single compile-time allow-list for every diagnostic request
// the gauge transmits on a vehicle bus (CAN or K-line).
//
// Policy (CLAUDE.md, "Vehicle bus safety"):
//  - default deny: a service ID that is not explicitly allowed is dropped and logged;
//  - OBD-II context: 0x01 and 0x09 only (0x03/0x07 behind kDtcReadEnabled, off);
//  - manufacturer context (extensions only): additionally 0x10 default/extended session,
//    0x1A, 0x22, 0x21, 0x3E;
//  - some services are forbidden forever, in every context, whatever the configuration.
//
// Transports never get a raw transmit handle: they get a GatedRequestSink, which calls
// SafetyGate::check() before anything reaches the bus.
#pragma once

#include <cstddef>
#include <cstdint>

namespace core::transport {

/// Who is sending the request. Manufacturer services are only allowed from extensions.
enum class RequestContext : uint8_t {
    Obd,          ///< standard OBD-II (core)
    Manufacturer, ///< manufacturer extension (ext/vag, ext/hyundai)
};

enum class GateVerdict : uint8_t {
    Allowed,
    Empty,            ///< zero-length payload
    ForbiddenForever, ///< on the permanent deny list (clear DTC, write, security, reset, ...)
    NotAllowed,       ///< not on the allow-list for this context (default deny)
    ForbiddenSession, ///< 0x10 with a session other than default/extended
    MalformedRequest, ///< allowed service but missing mandatory bytes
    DtcReadDisabled,  ///< 0x03/0x07 while DTC reading is not enabled
};

const char *to_string(GateVerdict v);

/// Compile-time switch for DTC *reading* (services 0x03/0x07). A later phase may enable it.
inline constexpr bool kDtcReadEnabled = false;

class SafetyGate {
public:
    /// Checks a request payload (first byte = service ID). Pure function, no side effects.
    static GateVerdict check(RequestContext ctx, const uint8_t *payload, size_t len);

    /// True if the service is on the permanent deny list.
    static bool is_forbidden_forever(uint8_t sid);
};

/// Statistics of rejected requests, readable by diagnostics.
struct GateStats {
    uint32_t allowed = 0;
    uint32_t rejected = 0;
    uint8_t last_rejected_sid = 0;
    GateVerdict last_verdict = GateVerdict::Allowed;
};

/// Lowest transmit layer seen by transports. Implementations put bytes on the bus.
class IRequestTransport {
public:
    virtual ~IRequestTransport() = default;
    /// Sends one complete request payload (segmentation is the transport's job).
    virtual bool transmit(const uint8_t *payload, size_t len) = 0;
};

/// Wraps a transport so that every request is checked by the SafetyGate.
/// Rejected requests are dropped, logged and counted; the inner transport never sees them.
class GatedRequestSink {
public:
    GatedRequestSink(IRequestTransport &inner, RequestContext ctx) : inner_(inner), ctx_(ctx) {}

    /// Returns false if the gate rejected the request or the transport failed.
    bool send(const uint8_t *payload, size_t len);

    const GateStats &stats() const { return stats_; }
    RequestContext context() const { return ctx_; }

private:
    IRequestTransport &inner_;
    RequestContext ctx_;
    GateStats stats_;
};

} // namespace core::transport
