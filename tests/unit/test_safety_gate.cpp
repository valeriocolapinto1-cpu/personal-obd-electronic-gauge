// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "core/transport/safety_gate.h"

using core::transport::GatedRequestSink;
using core::transport::GateVerdict;
using core::transport::IRequestTransport;
using core::transport::RequestContext;
using core::transport::SafetyGate;

namespace {

GateVerdict check(RequestContext ctx, std::vector<uint8_t> p)
{
    return SafetyGate::check(ctx, p.data(), p.size());
}

constexpr RequestContext kAllContexts[] = {RequestContext::Obd, RequestContext::Manufacturer};

// The list from CLAUDE.md "Explicitly forbidden, forever".
const std::vector<uint8_t> kClaudeMdForbidden = {0x04, 0x14, 0x27, 0x2E, 0x3B, 0x2F, 0x31,
                                                 0x11, 0x28, 0x34, 0x35, 0x36, 0x37};

class RecordingTransport : public IRequestTransport {
public:
    bool transmit(const uint8_t *payload, size_t len) override
    {
        sent.emplace_back(payload, payload + len);
        return true;
    }
    std::vector<std::vector<uint8_t>> sent;
};

} // namespace

TEST(SafetyGate, EveryForbiddenServiceIsRejectedInEveryContext)
{
    for (RequestContext ctx : kAllContexts) {
        for (uint8_t sid : kClaudeMdForbidden) {
            // Try with a range of plausible sub-function / parameter bytes.
            for (int sub = 0; sub <= 0xFF; ++sub) {
                EXPECT_NE(check(ctx, {sid, static_cast<uint8_t>(sub), 0x00, 0x00}), GateVerdict::Allowed)
                    << "sid=0x" << std::hex << int(sid) << " sub=0x" << sub;
            }
            EXPECT_EQ(check(ctx, {sid}), GateVerdict::ForbiddenForever);
            EXPECT_TRUE(SafetyGate::is_forbidden_forever(sid));
        }
    }
}

TEST(SafetyGate, ProgrammingSessionIsRejected)
{
    // UDS programming session 0x02 (and with suppress-positive-response bit), KWP 0x85.
    for (uint8_t s : std::vector<uint8_t>{0x02, 0x82, 0x85, 0x86, 0x87, 0x89, 0x40, 0x60, 0x00}) {
        EXPECT_EQ(check(RequestContext::Manufacturer, {0x10, s}), GateVerdict::ForbiddenSession) << int(s);
    }
}

TEST(SafetyGate, DefaultAndExtendedSessionsOnlyFromManufacturerContext)
{
    for (uint8_t s : std::vector<uint8_t>{0x01, 0x81, 0x03, 0x83}) {
        EXPECT_EQ(check(RequestContext::Manufacturer, {0x10, s}), GateVerdict::Allowed) << int(s);
        EXPECT_EQ(check(RequestContext::Obd, {0x10, s}), GateVerdict::NotAllowed) << int(s);
    }
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x10}), GateVerdict::MalformedRequest);
}

TEST(SafetyGate, ObdServicesAllowed)
{
    for (RequestContext ctx : kAllContexts) {
        EXPECT_EQ(check(ctx, {0x01, 0x00}), GateVerdict::Allowed);
        EXPECT_EQ(check(ctx, {0x01, 0x0C}), GateVerdict::Allowed);
        EXPECT_EQ(check(ctx, {0x09, 0x02}), GateVerdict::Allowed);
        EXPECT_EQ(check(ctx, {0x01}), GateVerdict::MalformedRequest);
    }
}

TEST(SafetyGate, DtcReadingDisabledByDefault)
{
    static_assert(!core::transport::kDtcReadEnabled, "DTC reading must stay off until a phase enables it");
    for (RequestContext ctx : kAllContexts) {
        EXPECT_EQ(check(ctx, {0x03}), GateVerdict::DtcReadDisabled);
        EXPECT_EQ(check(ctx, {0x07}), GateVerdict::DtcReadDisabled);
    }
}

TEST(SafetyGate, ManufacturerReadServices)
{
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x22, 0xF1, 0x90}), GateVerdict::Allowed);
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x22, 0xF1}), GateVerdict::MalformedRequest);
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x21, 0x01}), GateVerdict::Allowed);
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x1A, 0x9B}), GateVerdict::Allowed);
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x3E}), GateVerdict::Allowed);
    EXPECT_EQ(check(RequestContext::Manufacturer, {0x3E, 0x80}), GateVerdict::Allowed);
    for (uint8_t sid : std::vector<uint8_t>{0x22, 0x21, 0x1A, 0x3E}) {
        EXPECT_EQ(check(RequestContext::Obd, {sid, 0x00, 0x00}), GateVerdict::NotAllowed) << int(sid);
    }
}

TEST(SafetyGate, ExhaustiveAllowListIsExactlyTheDocumentedOne)
{
    const std::set<uint8_t> obd_allowed = {0x01, 0x09};
    const std::set<uint8_t> mfr_allowed = {0x01, 0x09, 0x10, 0x1A, 0x21, 0x22, 0x3E};
    for (int sid = 0; sid <= 0xFF; ++sid) {
        // Use a payload that satisfies every length/session rule so only the SID matters.
        std::vector<uint8_t> p = {static_cast<uint8_t>(sid), 0x01, 0x00, 0x00};
        const bool obd_ok = check(RequestContext::Obd, p) == GateVerdict::Allowed;
        const bool mfr_ok = check(RequestContext::Manufacturer, p) == GateVerdict::Allowed;
        EXPECT_EQ(obd_ok, obd_allowed.count(static_cast<uint8_t>(sid)) == 1) << "OBD sid=0x" << std::hex << sid;
        EXPECT_EQ(mfr_ok, mfr_allowed.count(static_cast<uint8_t>(sid)) == 1) << "MFR sid=0x" << std::hex << sid;
        // Negative-response SID and positive-response range can never be sent.
        if (sid == 0x7F || sid >= 0x40) {
            EXPECT_FALSE(obd_ok || mfr_ok) << "sid=0x" << std::hex << sid;
        }
    }
}

TEST(SafetyGate, EmptyPayloadRejected)
{
    EXPECT_EQ(SafetyGate::check(RequestContext::Obd, nullptr, 0), GateVerdict::Empty);
    const uint8_t b = 0x01;
    EXPECT_EQ(SafetyGate::check(RequestContext::Obd, &b, 0), GateVerdict::Empty);
}

TEST(GatedRequestSink, RejectedRequestsNeverReachTheTransport)
{
    RecordingTransport t;
    GatedRequestSink sink(t, RequestContext::Manufacturer);
    const uint8_t clear_dtc[] = {0x04};
    const uint8_t security[] = {0x27, 0x01};
    const uint8_t reset[] = {0x11, 0x01};
    const uint8_t prog[] = {0x10, 0x02};
    const uint8_t rpm[] = {0x01, 0x0C};

    EXPECT_FALSE(sink.send(clear_dtc, sizeof(clear_dtc)));
    EXPECT_FALSE(sink.send(security, sizeof(security)));
    EXPECT_FALSE(sink.send(reset, sizeof(reset)));
    EXPECT_FALSE(sink.send(prog, sizeof(prog)));
    EXPECT_TRUE(sink.send(rpm, sizeof(rpm)));

    ASSERT_EQ(t.sent.size(), 1u);
    EXPECT_EQ(t.sent[0], (std::vector<uint8_t>{0x01, 0x0C}));
    EXPECT_EQ(sink.stats().rejected, 4u);
    EXPECT_EQ(sink.stats().allowed, 1u);
    EXPECT_EQ(sink.stats().last_rejected_sid, 0x10);
    EXPECT_EQ(sink.stats().last_verdict, GateVerdict::ForbiddenSession);
}
