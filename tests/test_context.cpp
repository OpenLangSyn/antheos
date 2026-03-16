/*
 * test_context.cpp — Integration tests for Context (high-level API)
 *
 * Tests lifecycle, outbound frame building, session management,
 * and inbound feed with callback dispatch.
 * Port of test_antheos.c — 43 tests.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace antheos;

/* ── Frame scanning helpers ── */

static bool frame_valid(const Frame& f)
{
    return f.size() >= 2 &&
           f[0] == wire::SOM &&
           f[f.size() - 1] == wire::EOM;
}

static bool frame_has_verb(const Frame& f, char verb)
{
    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 4 < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Symbol) &&
            d[i+2] == wire::SOB &&
            d[i+3] == static_cast<uint8_t>(verb) &&
            d[i+4] == wire::EOW)
            return true;
    }
    return false;
}

static bool frame_has_any_id_base32(const Frame& f)
{
    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 4 < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Id) &&
            d[i+2] == wire::SOR &&
            d[i+3] == static_cast<uint8_t>(wire::Radix::Base32) &&
            d[i+4] == wire::SOB)
            return true;
    }
    return false;
}

static bool frame_has_id_base32(const Frame& f, std::string_view id)
{
    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 5 + id.size() < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Id) &&
            d[i+2] == wire::SOR &&
            d[i+3] == static_cast<uint8_t>(wire::Radix::Base32) &&
            d[i+4] == wire::SOB &&
            std::memcmp(d + i + 5, id.data(), id.size()) == 0 &&
            d[i + 5 + id.size()] == wire::EOW)
            return true;
    }
    return false;
}

static bool frame_has_id_plain(const Frame& f, std::string_view id)
{
    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 3 + id.size() < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Id) &&
            d[i+2] == wire::SOB &&
            std::memcmp(d + i + 3, id.data(), id.size()) == 0 &&
            d[i + 3 + id.size()] == wire::EOW)
            return true;
    }
    return false;
}

static bool frame_has_text(const Frame& f, std::string_view text)
{
    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 3 + text.size() < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Text) &&
            d[i+2] == wire::SOB &&
            std::memcmp(d + i + 3, text.data(), text.size()) == 0 &&
            d[i + 3 + text.size()] == wire::EOW)
            return true;
    }
    return false;
}

static bool frame_has_id_decimal(const Frame& f, uint32_t value)
{
    char digits[16];
    std::snprintf(digits, sizeof(digits), "%u", value);
    size_t d_len = std::strlen(digits);

    auto d = f.data();
    size_t len = f.size();
    for (size_t i = 0; i + 5 + d_len < len; i++) {
        if (d[i]   == wire::SOW &&
            d[i+1] == static_cast<uint8_t>(wire::WordType::Id) &&
            d[i+2] == wire::SOR &&
            d[i+3] == static_cast<uint8_t>(wire::Radix::Decimal) &&
            d[i+4] == wire::SOB &&
            std::memcmp(d + i + 5, digits, d_len) == 0 &&
            d[i + 5 + d_len] == wire::EOW)
            return true;
    }
    return false;
}

/* ── Callback test infrastructure ── */

struct CbState {
    int event_count = 0;
    std::string last_verb;
    std::string last_id;
    std::string last_id2;
    std::string last_detail;

    int msg_count = 0;
    std::string last_msg_verb;
    std::string last_msg_sid;
    std::string last_msg_target_bid;
    uint32_t last_msg_mid = 0;
    std::string last_msg_body;

    int offer_count = 0;
    std::string last_offer_bid;
    std::string last_offer_desc;
};

static void wire_event_cb(Context& ctx, CbState& state)
{
    ctx.on_event([&state](std::string_view verb, std::string_view id,
                          std::string_view id2, std::string_view detail) {
        state.event_count++;
        state.last_id2.clear();
        state.last_verb   = std::string(verb);
        state.last_id     = std::string(id);
        state.last_id2    = std::string(id2);
        state.last_detail = std::string(detail);
    });
}

static void wire_msg_cb(Context& ctx, CbState& state)
{
    ctx.on_message([&state](std::string_view verb, std::string_view sid,
                            std::string_view target_bid, uint32_t mid,
                            std::string_view body) {
        state.msg_count++;
        state.last_msg_target_bid.clear();
        state.last_msg_verb       = std::string(verb);
        state.last_msg_sid        = std::string(sid);
        state.last_msg_target_bid = std::string(target_bid);
        state.last_msg_mid        = mid;
        state.last_msg_body       = std::string(body);
    });
}

static void wire_offer_cb(Context& ctx, CbState& state)
{
    ctx.on_offer([&state](std::string_view bid,
                          std::string_view description) {
        state.offer_count++;
        state.last_offer_bid  = std::string(bid);
        state.last_offer_desc = std::string(description);
    });
}

/* ══════════════════════════════════════════════════════════════
 * Lifecycle tests (1–7)
 * ══════════════════════════════════════════════════════════════ */

/* 1. Init success */
static void test_init_success(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.bid().empty());
}

/* 2. Init generates BID — establish frame contains radix U BID */
static void test_init_generates_bid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.establish();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_any_id_base32(*f));
}

/* 3. Init empty OID — throws */
static void test_init_empty_oid(void)
{
    bool threw = false;
    try { Context ctx("", "Oracle", "SN001", "ABCDEFGH"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

/* 4. Init empty DID — throws */
static void test_init_empty_did(void)
{
    bool threw = false;
    try { Context ctx("VERUS", "", "SN001", "ABCDEFGH"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

/* 5. Init empty IID — throws */
static void test_init_empty_iid(void)
{
    bool threw = false;
    try { Context ctx("VERUS", "Oracle", "", "ABCDEFGH"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

/* 6. Init empty BID — throws */
static void test_init_empty_bid(void)
{
    bool threw = false;
    try { Context ctx("VERUS", "Oracle", "SN001", ""); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

/* 7. Destroy with active sessions — no crash */
static void test_destroy_with_sessions(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int s0 = ctx.session_open();
    int s1 = ctx.session_open();
    ASSERT_TRUE(s0 >= 0);
    ASSERT_TRUE(s1 >= 0);
    /* ctx goes out of scope with active sessions — must not crash */
}

/* 7. Destroy valid — no crash */
static void test_destroy_valid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.bid().empty());
    /* scope exit — no crash */
}

/* ══════════════════════════════════════════════════════════════
 * Outbound bus scope tests (8–17)
 * ══════════════════════════════════════════════════════════════ */

/* 8. Establish — verb E, our BID */
static void test_establish(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.establish();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'E'));
    ASSERT_TRUE(frame_has_any_id_base32(*f));
}

/* 9. Broadcast — verb B, TEXT word */
static void test_broadcast(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.broadcast("hello world");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'B'));
    ASSERT_TRUE(frame_has_text(*f, "hello world"));
}

/* 10. Ping specific — verb P, target BID */
static void test_ping_specific(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.ping("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'P'));
    ASSERT_TRUE(frame_has_id_base32(*f, "4T9X2"));
}

/* 11. Ping broadcast — verb P, no ID word */
static void test_ping_broadcast(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.ping();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'P'));
    ASSERT_TRUE(!frame_has_any_id_base32(*f));
}

/* 12. Exception — verb X, TEXT reason */
static void test_exception(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.exception("overload");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'X'));
    ASSERT_TRUE(frame_has_text(*f, "overload"));
}

/* 13. Verify request — verb V, target BID */
static void test_verify_request(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.verify("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'V'));
    ASSERT_TRUE(frame_has_id_base32(*f, "4T9X2"));
}

/* 14. Verify response — verb V, OID/DID/IID as plain ASCII IDs */
static void test_verify_response(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.verify_response();
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'V'));
    ASSERT_TRUE(frame_has_id_plain(*f, "VERUS"));
    ASSERT_TRUE(frame_has_id_plain(*f, "Oracle"));
    ASSERT_TRUE(frame_has_id_plain(*f, "SN001"));
}

/* 15. Discover — verb D, target BID */
static void test_discover(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.discover("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'D'));
    ASSERT_TRUE(frame_has_id_base32(*f, "4T9X2"));
}

/* 16. Acknowledge — verb W, target BID */
static void test_acknowledge(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.acknowledge("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'W'));
    ASSERT_TRUE(frame_has_id_base32(*f, "4T9X2"));
}

/* 17. BID is valid base-32, length 8 */
static void test_bid_is_base32(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto bid = ctx.bid();
    ASSERT_TRUE(!bid.empty());
    ASSERT_EQ(static_cast<int>(bid.size()), 8);
    for (char c : bid) {
        bool valid = std::strchr(id::BASE32_ALPHABET, c) != nullptr;
        ASSERT_TRUE(valid);
    }
}

/* ══════════════════════════════════════════════════════════════
 * Outbound service scope tests (18–20)
 * ══════════════════════════════════════════════════════════════ */

/* 18. Query — verb Q, TEXT capability */
static void test_query(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.query("thermometer");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'Q'));
    ASSERT_TRUE(frame_has_text(*f, "thermometer"));
}

/* 19. Offer — verb O, our BID, TEXT description */
static void test_offer(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.offer("temp_sensor");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'O'));
    ASSERT_TRUE(frame_has_any_id_base32(*f));
    ASSERT_TRUE(frame_has_text(*f, "temp_sensor"));
}

/* 20. Accept — verb A, target BID */
static void test_accept(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    auto f = ctx.accept("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'A'));
    ASSERT_TRUE(frame_has_id_base32(*f, "4T9X2"));
}

/* ══════════════════════════════════════════════════════════════
 * Session lifecycle tests (21–33)
 * ══════════════════════════════════════════════════════════════ */

/* 21. Session open — returns slot >= 0 */
static void test_session_open(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
}

/* 22. Session SID — non-empty */
static void test_session_sid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    auto sid = ctx.session_sid(slot);
    ASSERT_TRUE(!sid.empty());
}

/* 23. Session initial MID — returns 1 */
static void test_session_initial_mid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    ASSERT_EQ(ctx.session_mid(slot), 1);
}

/* 24. Session initial state — Active */
static void test_session_initial_state(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    ASSERT_TRUE(ctx.session_state(slot) == SessionState::Active);
}

/* 25. Session call — verb K, SID radix U, MID=1, TEXT payload */
static void test_session_call_frame(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    auto sid = ctx.session_sid(slot);

    auto f = ctx.session_call(slot, {}, "hello");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'K'));
    ASSERT_TRUE(frame_has_id_base32(*f, sid));
    ASSERT_TRUE(frame_has_id_decimal(*f, 1));
    ASSERT_TRUE(frame_has_text(*f, "hello"));
}

/* 26. Session call MID increment — 2nd=2, 3rd=3 */
static void test_session_call_mid_increment(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);

    auto f1 = ctx.session_call(slot, {}, "a");
    ASSERT_TRUE(f1.has_value());
    ASSERT_TRUE(frame_has_id_decimal(*f1, 1));

    auto f2 = ctx.session_call(slot, {}, "b");
    ASSERT_TRUE(f2.has_value());
    ASSERT_TRUE(frame_has_id_decimal(*f2, 2));

    auto f3 = ctx.session_call(slot, {}, "c");
    ASSERT_TRUE(f3.has_value());
    ASSERT_TRUE(frame_has_id_decimal(*f3, 3));
}

/* 27. Session notify — verb N, SID, MID, TEXT. MID increments */
static void test_session_notify_frame(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    auto sid = ctx.session_sid(slot);

    auto f = ctx.session_notify(slot, {}, "event1");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'N'));
    ASSERT_TRUE(frame_has_id_base32(*f, sid));
    ASSERT_TRUE(frame_has_id_decimal(*f, 1));
    ASSERT_TRUE(frame_has_text(*f, "event1"));

    ASSERT_EQ(ctx.session_mid(slot), 2);
}

/* 28. Session status — verb T, SID, MID. MID increments */
static void test_session_status_frame(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);
    auto sid = ctx.session_sid(slot);

    auto f = ctx.session_status(slot, {});
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'T'));
    ASSERT_TRUE(frame_has_id_base32(*f, sid));
    ASSERT_TRUE(frame_has_id_decimal(*f, 1));

    ASSERT_EQ(ctx.session_mid(slot), 2);
}

/* 29. Session close — verb F, SID. State goes Idle */
static void test_session_close_frame(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot = ctx.session_open();
    ASSERT_TRUE(slot >= 0);

    std::string saved_sid(ctx.session_sid(slot));

    auto f = ctx.session_close(slot);
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'F'));
    ASSERT_TRUE(frame_has_id_base32(*f, saved_sid));

    ASSERT_TRUE(ctx.session_state(slot) == SessionState::Idle);
}

/* 30. Session close releases SID — new open succeeds after close */
static void test_session_close_releases_sid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int slot1 = ctx.session_open();
    ASSERT_TRUE(slot1 >= 0);

    ctx.session_close(slot1);

    int slot2 = ctx.session_open();
    ASSERT_TRUE(slot2 >= 0);
}

/* 31. Session invalid slot — returns nullopt */
static void test_session_invalid_slot(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.session_call(-1, {}, "x").has_value());
    ASSERT_TRUE(!ctx.session_call(32, {}, "x").has_value());
}

/* 32. Session closed slot — call on never-opened slot returns nullopt */
static void test_session_closed_slot(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.session_call(0, {}, "x").has_value());
}

/* 33. Multiple sessions — 3 sessions, each gets a unique SID */
static void test_multiple_sessions(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    int s0 = ctx.session_open();
    int s1 = ctx.session_open();
    int s2 = ctx.session_open();
    ASSERT_TRUE(s0 >= 0);
    ASSERT_TRUE(s1 >= 0);
    ASSERT_TRUE(s2 >= 0);

    auto sid0 = ctx.session_sid(s0);
    auto sid1 = ctx.session_sid(s1);
    auto sid2 = ctx.session_sid(s2);
    ASSERT_TRUE(!sid0.empty() && !sid1.empty() && !sid2.empty());
    ASSERT_TRUE(sid0 != sid1);
    ASSERT_TRUE(sid0 != sid2);
    ASSERT_TRUE(sid1 != sid2);
}

/* ══════════════════════════════════════════════════════════════
 * Inbound feed + callback dispatch tests (34–39)
 * ══════════════════════════════════════════════════════════════ */

/* 34. Feed broadcast — event_cb fires with verb "B" */
static void test_feed_broadcast(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_event_cb(ctx, state);

    auto f = bus::broadcast("hello");
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.event_count, 1);
    ASSERT_TRUE(state.last_verb == "B");
}

/* 35. Feed session call — msg_cb fires with correct SID, MID, payload */
static void test_feed_session_call(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_msg_cb(ctx, state);

    auto f = session::call("A7K2M", {}, 5, "payload");
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.msg_count, 1);
    ASSERT_TRUE(state.last_msg_verb == "K");
    ASSERT_TRUE(state.last_msg_sid == "A7K2M");
    ASSERT_EQ(state.last_msg_mid, 5);
    ASSERT_TRUE(state.last_msg_body == "payload");
}

/* 36. Feed service offer — offer_cb fires with correct BID and description */
static void test_feed_service_offer(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_offer_cb(ctx, state);

    auto f = service::offer("4T9X2", "thermometer");
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.offer_count, 1);
    ASSERT_TRUE(state.last_offer_bid == "4T9X2");
    ASSERT_TRUE(state.last_offer_desc == "thermometer");
}

/* 37. Feed two messages — callbacks fire twice */
static void test_feed_two_messages(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_event_cb(ctx, state);

    auto f1 = bus::broadcast("first");
    auto f2 = bus::broadcast("second");
    ASSERT_TRUE(f1.has_value() && f2.has_value());

    ctx.feed(f1->data(), f1->size());
    ctx.feed(f2->data(), f2->size());

    ASSERT_EQ(state.event_count, 2);
}

/* 38. Feed partial — one byte at a time, callback only after complete frame */
static void test_feed_partial(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_event_cb(ctx, state);

    auto f = bus::broadcast("test");
    ASSERT_TRUE(f.has_value());

    /* Feed all bytes except last — no callback yet */
    for (size_t i = 0; i < f->size() - 1; i++) {
        ctx.feed(f->data() + i, 1);
        ASSERT_EQ(state.event_count, 0);
    }

    /* Feed final byte (EOM) — callback fires */
    ctx.feed(f->data() + f->size() - 1, 1);
    ASSERT_EQ(state.event_count, 1);
}

/* 39. No callback registered — feed valid frame, no crash */
static void test_feed_no_callback(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");

    auto f = bus::broadcast("test");
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());
    ASSERT_TRUE(true);
}

/* 40. Feed large session call — payload >256 bytes parses correctly */
static void test_feed_large_payload(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_msg_cb(ctx, state);

    std::string payload(2000, 'X');

    auto f = session::call("B3Q7R", {}, 1, payload);
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.msg_count, 1);
    ASSERT_TRUE(state.last_msg_verb == "K");
    ASSERT_TRUE(state.last_msg_sid == "B3Q7R");
    ASSERT_EQ(state.last_msg_mid, 1);
    ASSERT_EQ(static_cast<int>(state.last_msg_body.size()), 2000);
    ASSERT_TRUE(state.last_msg_body[0] == 'X');
    ASSERT_TRUE(state.last_msg_body[1999] == 'X');
}

/* ══════════════════════════════════════════════════════════════
 * Accept sender BID tests (41–42)
 * ══════════════════════════════════════════════════════════════ */

/* 41. Accept with sender BID — event_cb receives id2 */
static void test_feed_accept_with_sender(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_event_cb(ctx, state);

    auto f = service::accept("TARG1", "SNDR2");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'A'));
    ASSERT_TRUE(frame_has_id_base32(*f, "TARG1"));
    ASSERT_TRUE(frame_has_id_base32(*f, "SNDR2"));

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.event_count, 1);
    ASSERT_TRUE(state.last_verb == "A");
    ASSERT_TRUE(state.last_id == "TARG1");
    ASSERT_TRUE(state.last_id2 == "SNDR2");
}

/* 42. Accept without sender BID — event_cb receives empty id2 */
static void test_feed_accept_no_sender(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_event_cb(ctx, state);

    auto f = service::accept("TARG1");
    ASSERT_TRUE(f.has_value());
    ASSERT_TRUE(frame_valid(*f));
    ASSERT_TRUE(frame_has_verb(*f, 'A'));
    ASSERT_TRUE(frame_has_id_base32(*f, "TARG1"));

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.event_count, 1);
    ASSERT_TRUE(state.last_verb == "A");
    ASSERT_TRUE(state.last_id == "TARG1");
    ASSERT_TRUE(state.last_id2.empty());
}

/* ══════════════════════════════════════════════════════════════
 * K-frame target BID feed-through test (43)
 * ══════════════════════════════════════════════════════════════ */

/* 43. K-frame with target BID — msg_cb receives target_bid */
static void test_feed_session_call_target_bid(void)
{
    Context ctx("VERUS", "Oracle", "SN001", "ABCDEFGH");
    CbState state;
    wire_msg_cb(ctx, state);

    auto f = session::call("A7K2M", "4T9X2", 5, "payload");
    ASSERT_TRUE(f.has_value());

    ctx.feed(f->data(), f->size());

    ASSERT_EQ(state.msg_count, 1);
    ASSERT_TRUE(state.last_msg_verb == "K");
    ASSERT_TRUE(state.last_msg_sid == "A7K2M");
    ASSERT_TRUE(state.last_msg_target_bid == "4T9X2");
    ASSERT_EQ(state.last_msg_mid, 5);
    ASSERT_TRUE(state.last_msg_body == "payload");
}

/* ══════════════════════════════════════════════════════════════
 * Suite entry point
 * ══════════════════════════════════════════════════════════════ */

void test_context_run(int& out_run, int& out_passed)
{
    std::printf("\n[context]\n");

    /* Lifecycle */
    TEST(test_init_success);
    TEST(test_init_generates_bid);
    TEST(test_init_empty_oid);
    TEST(test_init_empty_did);
    TEST(test_init_empty_iid);
    TEST(test_init_empty_bid);
    TEST(test_destroy_with_sessions);
    TEST(test_destroy_valid);

    /* Outbound — bus scope */
    TEST(test_establish);
    TEST(test_broadcast);
    TEST(test_ping_specific);
    TEST(test_ping_broadcast);
    TEST(test_exception);
    TEST(test_verify_request);
    TEST(test_verify_response);
    TEST(test_discover);
    TEST(test_acknowledge);
    TEST(test_bid_is_base32);

    /* Outbound — service scope */
    TEST(test_query);
    TEST(test_offer);
    TEST(test_accept);

    /* Session lifecycle */
    TEST(test_session_open);
    TEST(test_session_sid);
    TEST(test_session_initial_mid);
    TEST(test_session_initial_state);
    TEST(test_session_call_frame);
    TEST(test_session_call_mid_increment);
    TEST(test_session_notify_frame);
    TEST(test_session_status_frame);
    TEST(test_session_close_frame);
    TEST(test_session_close_releases_sid);
    TEST(test_session_invalid_slot);
    TEST(test_session_closed_slot);
    TEST(test_multiple_sessions);

    /* Inbound feed + callback dispatch */
    TEST(test_feed_broadcast);
    TEST(test_feed_session_call);
    TEST(test_feed_service_offer);
    TEST(test_feed_two_messages);
    TEST(test_feed_partial);
    TEST(test_feed_no_callback);
    TEST(test_feed_large_payload);

    /* Accept sender BID */
    TEST(test_feed_accept_with_sender);
    TEST(test_feed_accept_no_sender);

    /* K-frame target BID feed-through */
    TEST(test_feed_session_call_target_bid);

    out_run    = tests_run;
    out_passed = tests_passed;
}
