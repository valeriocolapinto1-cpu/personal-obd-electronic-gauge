// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/transport/safety_gate.h"

#include "core/util/log.h"

namespace core::transport {

namespace {

const char *TAG = "safety_gate";

// Service IDs (ISO 15031-5 / ISO 14229 UDS / ISO 14230-3 KWP2000).
constexpr uint8_t kSidObdCurrentData = 0x01;
constexpr uint8_t kSidObdStoredDtc = 0x03;
constexpr uint8_t kSidObdPendingDtc = 0x07;
constexpr uint8_t kSidObdVehicleInfo = 0x09;
constexpr uint8_t kSidSessionControl = 0x10;
constexpr uint8_t kSidReadEcuId = 0x1A;
constexpr uint8_t kSidReadByLocalId = 0x21;
constexpr uint8_t kSidReadDataById = 0x22;
constexpr uint8_t kSidTesterPresent = 0x3E;

// Permanent deny list. Kept explicit (on top of default deny) so that a future edit of the
// allow-list can never re-enable one of these by mistake: check() tests this list first.
constexpr uint8_t kForbiddenForever[] = {
    0x04, // OBD clear DTCs
    0x11, // ECU reset
    0x14, // clear diagnostic information
    0x27, // security access
    0x28, // communication control
    0x2E, // write data by identifier
    0x2F, // input/output control by identifier
    0x30, // KWP input/output control by local identifier
    0x31, // routine control (UDS) / start routine by local id (KWP)
    0x32, // KWP stop routine by local identifier
    0x33, // KWP request routine results by local identifier
    0x34, // request download
    0x35, // request upload
    0x36, // transfer data
    0x37, // request transfer exit
    0x38, // request file transfer
    0x3B, // KWP write data by local identifier
    0x3D, // write memory by address
    0x85, // control DTC setting
    0x87, // link control
};

// Session sub-functions (bit 7 = suppress positive response, ignored for the check).
constexpr uint8_t kSessionDefault = 0x01;
constexpr uint8_t kSessionExtended = 0x03;

} // namespace

const char *to_string(GateVerdict v)
{
    switch (v) {
        case GateVerdict::Allowed:
            return "allowed";
        case GateVerdict::Empty:
            return "empty";
        case GateVerdict::ForbiddenForever:
            return "forbidden-forever";
        case GateVerdict::NotAllowed:
            return "not-allowed";
        case GateVerdict::ForbiddenSession:
            return "forbidden-session";
        case GateVerdict::MalformedRequest:
            return "malformed";
        case GateVerdict::DtcReadDisabled:
            return "dtc-read-disabled";
    }
    return "?";
}

bool SafetyGate::is_forbidden_forever(uint8_t sid)
{
    for (uint8_t f : kForbiddenForever) {
        if (f == sid) {
            return true;
        }
    }
    return false;
}

GateVerdict SafetyGate::check(RequestContext ctx, const uint8_t *payload, size_t len)
{
    if (payload == nullptr || len == 0) {
        return GateVerdict::Empty;
    }
    const uint8_t sid = payload[0];

    if (is_forbidden_forever(sid)) {
        return GateVerdict::ForbiddenForever;
    }

    // OBD-II services: allowed from both contexts.
    switch (sid) {
        case kSidObdCurrentData:
        case kSidObdVehicleInfo:
            return len >= 2 ? GateVerdict::Allowed : GateVerdict::MalformedRequest;
        case kSidObdStoredDtc:
        case kSidObdPendingDtc:
            return kDtcReadEnabled ? GateVerdict::Allowed : GateVerdict::DtcReadDisabled;
        default:
            break;
    }

    if (ctx != RequestContext::Manufacturer) {
        return GateVerdict::NotAllowed;
    }

    switch (sid) {
        case kSidSessionControl: {
            if (len < 2) {
                return GateVerdict::MalformedRequest;
            }
            const uint8_t session = payload[1] & 0x7F;
            if (session == kSessionDefault || session == kSessionExtended) {
                return GateVerdict::Allowed;
            }
            return GateVerdict::ForbiddenSession;
        }
        case kSidTesterPresent:
            return GateVerdict::Allowed; // KWP allows the bare SID, UDS adds a sub-function
        case kSidReadEcuId:
        case kSidReadByLocalId:
            return len >= 2 ? GateVerdict::Allowed : GateVerdict::MalformedRequest;
        case kSidReadDataById:
            return len >= 3 ? GateVerdict::Allowed : GateVerdict::MalformedRequest;
        default:
            return GateVerdict::NotAllowed;
    }
}

bool GatedRequestSink::send(const uint8_t *payload, size_t len)
{
    const GateVerdict v = SafetyGate::check(ctx_, payload, len);
    if (v != GateVerdict::Allowed) {
        ++stats_.rejected;
        stats_.last_verdict = v;
        stats_.last_rejected_sid = (payload != nullptr && len > 0) ? payload[0] : 0;
        CORE_LOGW(TAG, "dropped request sid=0x%02X len=%u: %s", stats_.last_rejected_sid, static_cast<unsigned>(len),
                  to_string(v));
        return false;
    }
    ++stats_.allowed;
    return inner_.transmit(payload, len);
}

} // namespace core::transport
