/*
 * test_depth.cpp — Deep coverage tests for libantheos
 *
 * Covers gaps in existing suites: Frame class API, parser tail handling,
 * parser/SidPool move semantics, parser statistics, SidPool edge cases,
 * Context session_accept, Context session exhaustion, Context feed edge
 * cases, Context inbound dispatch for all verb types, word_decode
 * malformed inputs, DecodedWord operator bool.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace antheos;

/* ================================================================
 * 1. Frame class — value semantics & API
 * ================================================================ */

static void test_frame_default_empty() {
    Frame f;
    ASSERT_TRUE(f.empty());
    ASSERT_EQ(f.size(), 0u);
    ASSERT_TRUE(!static_cast<bool>(f));
    ASSERT_TRUE(f.begin() == f.end());
}

static void test_frame_from_ptr() {
    const uint8_t data[] = {0x02, 0x03};
    Frame f(data, 2);
    ASSERT_EQ(f.size(), 2u);
    ASSERT_TRUE(!f.empty());
    ASSERT_TRUE(static_cast<bool>(f));
    ASSERT_EQ(f[0], 0x02);
    ASSERT_EQ(f[1], 0x03);
    ASSERT_EQ(f.data()[0], 0x02);
}

static void test_frame_from_vector() {
    std::vector<uint8_t> v = {0x10, 0x20, 0x30};
    Frame f(std::move(v));
    ASSERT_EQ(f.size(), 3u);
    ASSERT_EQ(f[0], 0x10);
    ASSERT_EQ(f[2], 0x30);
}

static void test_frame_copy() {
    const uint8_t data[] = {0x01, 0x02, 0x03};
    Frame a(data, 3);
    Frame b = a;
    ASSERT_EQ(b.size(), 3u);
    ASSERT_MEM_EQ(b.data(), data, 3);
    /* Modify copy, original unchanged */
    b.bytes()[0] = 0xFF;
    ASSERT_EQ(a[0], 0x01);
    ASSERT_EQ(b[0], 0xFF);
}

static void test_frame_move() {
    const uint8_t data[] = {0xAA, 0xBB};
    Frame a(data, 2);
    Frame b = std::move(a);
    ASSERT_EQ(b.size(), 2u);
    ASSERT_EQ(b[0], 0xAA);
    ASSERT_EQ(b[1], 0xBB);
    /* Source is moved-from — empty */
    ASSERT_TRUE(a.empty());
}

static void test_frame_iterators() {
    const uint8_t data[] = {0x10, 0x20, 0x30};
    Frame f(data, 3);
    int count = 0;
    uint8_t sum = 0;
    for (auto it = f.begin(); it != f.end(); ++it) {
        sum += *it;
        count++;
    }
    ASSERT_EQ(count, 3);
    ASSERT_EQ(sum, 0x60);
}

static void test_frame_bytes_mutable() {
    const uint8_t data[] = {0x01};
    Frame f(data, 1);
    f.bytes().push_back(0x02);
    ASSERT_EQ(f.size(), 2u);
    ASSERT_EQ(f[1], 0x02);
}

/* ================================================================
 * 2. Parser tail handling
 * ================================================================ */

static void test_parser_tail_callback() {
    Parser p;
    int word_count = 0;
    int msg_count = 0;
    size_t tail_len = 0;
    uint8_t tail_data[64]{};

    p.on_word([&](wire::WordType, wire::Radix, wire::Unit,
                  const uint8_t*, size_t) {
        word_count++;
    });
    p.on_tail([&](const uint8_t* data, size_t len) {
        tail_len = len;
        if (len <= sizeof(tail_data))
            std::memcpy(tail_data, data, len);
    });
    p.on_message([&](size_t) { msg_count++; });

    /* Set tail length before feeding frame */
    p.set_tail_length(4);

    /* Feed: SOM !B "test EOM <4 tail bytes> */
    auto f = bus::broadcast("test");
    ASSERT_TRUE(f.has_value());

    /* Feed frame bytes */
    p.feed(f->data(), f->size());

    /* After EOM, parser should be in Tail state, no message yet */
    ASSERT_EQ(msg_count, 0);

    /* Feed 4 tail bytes */
    const uint8_t tail_in[] = {0xDE, 0xAD, 0xBE, 0xEF};
    p.feed(tail_in, 4);

    /* Now message should be complete */
    ASSERT_EQ(msg_count, 1);
    ASSERT_EQ(tail_len, 4u);
    ASSERT_MEM_EQ(tail_data, tail_in, 4);
}

static void test_parser_tail_zero_length() {
    /* tail_length=0: message completes immediately after EOM */
    Parser p;
    int msg_count = 0;
    p.on_message([&](size_t) { msg_count++; });
    p.set_tail_length(0);

    auto f = bus::broadcast("x");
    ASSERT_TRUE(f.has_value());
    p.feed(f->data(), f->size());
    ASSERT_EQ(msg_count, 1);
}

/* ================================================================
 * 3. Parser statistics accumulation
 * ================================================================ */

static void test_parser_stats_accumulate() {
    Parser p;
    p.on_word([](wire::WordType, wire::Radix, wire::Unit,
                 const uint8_t*, size_t) {});
    p.on_message([](size_t) {});

    ASSERT_EQ(p.total_words(), 0u);
    ASSERT_EQ(p.total_messages(), 0u);
    ASSERT_EQ(p.parse_errors(), 0u);

    /* Feed two valid frames */
    auto f1 = bus::broadcast("a");       /* 2 words: !B, "a */
    auto f2 = bus::ping_all();           /* 1 word: !P */
    ASSERT_TRUE(f1.has_value() && f2.has_value());

    p.feed(f1->data(), f1->size());
    ASSERT_EQ(p.total_words(), 2u);
    ASSERT_EQ(p.total_messages(), 1u);

    p.feed(f2->data(), f2->size());
    ASSERT_EQ(p.total_words(), 3u);
    ASSERT_EQ(p.total_messages(), 2u);
    ASSERT_EQ(p.parse_errors(), 0u);
}

static void test_parser_error_count() {
    Parser p;
    p.on_word([](wire::WordType, wire::Radix, wire::Unit,
                 const uint8_t*, size_t) {});
    p.on_message([](size_t) {});

    /* Feed invalid: SOM followed by garbage (not SOW or EOM) */
    const uint8_t bad[] = {wire::SOM, 0x99};
    p.feed(bad, 2);
    ASSERT_EQ(p.parse_errors(), 1u);
    ASSERT_EQ(p.total_messages(), 0u);

    /* Recovery: valid frame after error */
    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());
    p.feed(f->data(), f->size());
    ASSERT_EQ(p.total_messages(), 1u);
    ASSERT_EQ(p.parse_errors(), 1u); /* error count preserved */
}

static void test_parser_stats_survive_reset() {
    Parser p;
    p.on_word([](wire::WordType, wire::Radix, wire::Unit,
                 const uint8_t*, size_t) {});
    p.on_message([](size_t) {});

    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());
    p.feed(f->data(), f->size());
    ASSERT_EQ(p.total_words(), 1u);
    ASSERT_EQ(p.total_messages(), 1u);

    p.reset();
    ASSERT_EQ(static_cast<int>(p.state()), static_cast<int>(ParseState::WaitSom));
    /* Lifetime stats preserved across reset */
    ASSERT_EQ(p.total_words(), 1u);
    ASSERT_EQ(p.total_messages(), 1u);
}

/* ================================================================
 * 4. Parser move semantics
 * ================================================================ */

static void test_parser_move_construct() {
    Parser a;
    int msg_count = 0;
    a.on_message([&](size_t) { msg_count++; });

    /* Partially feed */
    a.feed(static_cast<uint8_t>(wire::SOM));

    Parser b = std::move(a);
    /* Continue feeding on b */
    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());
    /* Feed remaining bytes (skip SOM already fed) */
    for (size_t i = 1; i < f->size(); i++)
        b.feed(f->data()[i]);
    ASSERT_EQ(msg_count, 1);
}

static void test_parser_move_assign() {
    Parser a;
    int count_a = 0;
    a.on_message([&](size_t) { count_a++; });

    Parser b;
    int count_b = 0;
    b.on_message([&](size_t) { count_b++; });

    b = std::move(a);

    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());
    b.feed(f->data(), f->size());
    ASSERT_EQ(count_a, 1); /* a's callback transferred to b */
    ASSERT_EQ(count_b, 0);
}

/* ================================================================
 * 5. SidPool move semantics
 * ================================================================ */

static void test_sidpool_move_construct() {
    SidPool a("org", "dev", "iid");
    auto sid = a.acquire();
    ASSERT_TRUE(sid.has_value());

    SidPool b = std::move(a);
    /* b should be functional — can acquire fresh SIDs */
    auto sid2 = b.acquire();
    ASSERT_TRUE(sid2.has_value());
    ASSERT_TRUE(sid2->size() >= id::SID_MIN_LEN);
    ASSERT_TRUE(*sid != *sid2);
}

static void test_sidpool_move_assign() {
    SidPool a("org", "dev", "iid1");
    SidPool b("org", "dev", "iid2");

    auto sid_a = a.acquire();
    ASSERT_TRUE(sid_a.has_value());

    b = std::move(a);
    /* b now has a's state — acquire should continue a's counter */
    auto sid_b = b.acquire();
    ASSERT_TRUE(sid_b.has_value());
}

/* ================================================================
 * 6. SidPool edge cases
 * ================================================================ */

static void test_sidpool_acquire_unique_null_check() {
    SidPool pool("org", "dev", "iid");
    /* Null collision check returns nullopt */
    auto sid = pool.acquire_unique(nullptr);
    ASSERT_TRUE(!sid.has_value());
}

static void test_sidpool_different_identity_different_sids() {
    /* Different identity produces different SIDs */
    SidPool pool_a("org", "dev", "iid1");
    SidPool pool_b("org", "dev", "iid2");

    auto sid_a = pool_a.acquire();
    auto sid_b = pool_b.acquire();
    ASSERT_TRUE(sid_a.has_value());
    ASSERT_TRUE(sid_b.has_value());
    ASSERT_TRUE(*sid_a != *sid_b);
}

static void test_sidpool_invalid_args() {
    /* Empty OID */
    bool threw = false;
    try { SidPool pool("", "d", "i"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);

    /* Empty DID */
    threw = false;
    try { SidPool pool("o", "", "i"); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);

    /* Empty IID */
    threw = false;
    try { SidPool pool("o", "d", ""); }
    catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

/* ================================================================
 * 7. Context session_accept — inbound session handling
 * ================================================================ */

static void test_context_session_accept_basic() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    int slot = ctx.session_accept("XYZW1234", 5);
    ASSERT_TRUE(slot >= 0);
    ASSERT_TRUE(ctx.session_sid(slot) == "XYZW1234");
    ASSERT_EQ(ctx.session_mid(slot), 6u); /* inbound_mid + 1 */
    ASSERT_TRUE(ctx.session_state(slot) == SessionState::Active);
}

static void test_context_session_accept_empty_sid() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    int slot = ctx.session_accept("", 0);
    ASSERT_EQ(slot, -1);
}

static void test_context_session_accept_call() {
    /* Accept inbound session then send a call on it */
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    int slot = ctx.session_accept("MNPQ5678", 10);
    ASSERT_TRUE(slot >= 0);

    auto f = ctx.session_call(slot, "TARGET", "hello");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], wire::SOM);
    ASSERT_EQ(f->data()[f->size() - 1], wire::EOM);
}

/* ================================================================
 * 8. Context — 32-session exhaustion
 * ================================================================ */

static void test_context_session_exhaustion() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");

    /* Open all 32 slots */
    int slots[32];
    for (int i = 0; i < 32; i++) {
        slots[i] = ctx.session_open();
        ASSERT_TRUE(slots[i] >= 0);
    }

    /* 33rd open should fail */
    int overflow = ctx.session_open();
    ASSERT_EQ(overflow, -1);

    /* Close one, then open should succeed */
    ctx.session_close(slots[0]);
    int reused = ctx.session_open();
    ASSERT_TRUE(reused >= 0);
}

/* ================================================================
 * 9. Context feed edge cases
 * ================================================================ */

static void test_context_feed_null() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    ASSERT_EQ(ctx.feed(nullptr, 0), 0u);
    ASSERT_EQ(ctx.feed(nullptr, 100), 0u);
}

static void test_context_feed_empty() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    uint8_t data[] = {0x00};
    ASSERT_EQ(ctx.feed(data, 0), 0u);
}

/* ================================================================
 * 10. Context inbound dispatch — all verb types
 * ================================================================ */

struct DispatchState {
    int event_count = 0;
    std::string last_verb;
    std::string last_id;
    std::string last_id2;
    std::string last_detail;
    int msg_count = 0;
    std::string last_msg_verb;
    std::string last_msg_sid;
    uint32_t last_msg_mid = 0;
    std::string last_msg_body;
    int offer_count = 0;
    std::string last_offer_bid;
    std::string last_offer_desc;
};

static void wire_ctx(Context& ctx, DispatchState& s) {
    ctx.on_event([&s](std::string_view verb, std::string_view id,
                       std::string_view id2, std::string_view detail) {
        s.event_count++;
        s.last_verb   = std::string(verb);
        s.last_id     = std::string(id);
        s.last_id2    = std::string(id2);
        s.last_detail = std::string(detail);
    });
    ctx.on_message([&s](std::string_view verb, std::string_view sid,
                         std::string_view, uint32_t mid,
                         std::string_view body) {
        s.msg_count++;
        s.last_msg_verb = std::string(verb);
        s.last_msg_sid  = std::string(sid);
        s.last_msg_mid  = mid;
        s.last_msg_body = std::string(body);
    });
    ctx.on_offer([&s](std::string_view bid, std::string_view desc) {
        s.offer_count++;
        s.last_offer_bid  = std::string(bid);
        s.last_offer_desc = std::string(desc);
    });
}

static void test_dispatch_establish() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::establish("AB12");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "E");
    ASSERT_TRUE(s.last_id == "AB12");
}

static void test_dispatch_conflict() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::conflict("CD34");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "C");
    ASSERT_TRUE(s.last_id == "CD34");
}

static void test_dispatch_ping() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::ping("EF56");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "P");
    ASSERT_TRUE(s.last_id == "EF56");
}

static void test_dispatch_discover() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::discover("GH78");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "D");
    ASSERT_TRUE(s.last_id == "GH78");
}

static void test_dispatch_discover_response() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::discover_response("GH78", "JK9A");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "D");
    ASSERT_TRUE(s.last_id == "GH78");
    ASSERT_TRUE(s.last_id2 == "JK9A");
}

static void test_dispatch_verify() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::verify("LM2B");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "V");
    ASSERT_TRUE(s.last_id == "LM2B");
}

static void test_dispatch_acknowledge() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::acknowledge("NP3C");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "W");
    ASSERT_TRUE(s.last_id == "NP3C");
}

static void test_dispatch_exception() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::exception("OVERLOAD");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "X");
    ASSERT_TRUE(s.last_detail == "OVERLOAD");
}

static void test_dispatch_query() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = service::query("temperature");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "Q");
    ASSERT_TRUE(s.last_detail == "temperature");
}

static void test_dispatch_offer() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = service::offer("QR4D", "thermo");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.offer_count, 1);
    ASSERT_TRUE(s.last_offer_bid == "QR4D");
    ASSERT_TRUE(s.last_offer_desc == "thermo");
}

static void test_dispatch_accept() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = service::accept("ST5E");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "A");
    ASSERT_TRUE(s.last_id == "ST5E");
}

static void test_dispatch_session_notify() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = session::notify("UV6F", {}, 3, "temp_high");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.msg_count, 1);
    ASSERT_TRUE(s.last_msg_verb == "N");
    ASSERT_TRUE(s.last_msg_sid == "UV6F");
    ASSERT_EQ(s.last_msg_mid, 3u);
    ASSERT_TRUE(s.last_msg_body == "temp_high");
}

static void test_dispatch_session_status() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = session::status("WX7G", {}, 1);
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "T");
    ASSERT_TRUE(s.last_id == "WX7G");
}

static void test_dispatch_session_locate() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = session::locate("YZ8H");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "L");
    ASSERT_TRUE(s.last_id == "YZ8H");
}

static void test_dispatch_session_resume() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = session::resume("A2B3");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "U");
    ASSERT_TRUE(s.last_id == "A2B3");
}

static void test_dispatch_session_finish() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = session::finish("C4D5");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "F");
    ASSERT_TRUE(s.last_id == "C4D5");
}

/* ================================================================
 * 10b. Auth (Z-verb) dispatch
 * ================================================================ */

static void test_dispatch_auth_challenge() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::auth_challenge("4T9X2", "deadbeef01234567");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "Z");
    ASSERT_TRUE(s.last_id == "4T9X2");
    ASSERT_TRUE(s.last_id2.empty());                   /* no key_id */
    ASSERT_TRUE(s.last_detail == "deadbeef01234567");   /* nonce */
}

static void test_dispatch_auth_response() {
    DispatchState s;
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    wire_ctx(ctx, s);
    auto f = bus::auth_response("4T9X2", "kid123", "sig456");
    ASSERT_TRUE(f.has_value());
    ctx.feed(f->data(), f->size());
    ASSERT_EQ(s.event_count, 1);
    ASSERT_TRUE(s.last_verb == "Z");
    ASSERT_TRUE(s.last_id == "4T9X2");
    ASSERT_TRUE(s.last_id2 == "kid123");       /* key_id */
    ASSERT_TRUE(s.last_detail == "sig456");    /* signature */
}

/* ================================================================
 * 11. word_decode — malformed input rejection
 * ================================================================ */

static void test_decode_null_input() {
    auto dw = wire::word_decode(nullptr, 10);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_too_short() {
    uint8_t data[] = {wire::SOW, '"', wire::SOB};
    auto dw = wire::word_decode(data, 3);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_missing_sow() {
    uint8_t data[] = {0xFF, '"', wire::SOB, 'A', wire::EOW};
    auto dw = wire::word_decode(data, 5);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_invalid_word_type() {
    uint8_t data[] = {wire::SOW, 0x99, wire::SOB, 'A', wire::EOW};
    auto dw = wire::word_decode(data, 5);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_invalid_radix() {
    /* Valid word type, but radix flag is invalid */
    uint8_t data[] = {wire::SOW, '#', wire::SOR, 0x99, wire::SOB, '1', wire::EOW};
    auto dw = wire::word_decode(data, 7);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_invalid_unit() {
    /* Valid word type + valid radix, but unit flag is invalid */
    uint8_t data[] = {wire::SOW, '#', wire::SOR, 'D', wire::SOU, 0x99,
                      wire::SOB, '1', wire::EOW};
    auto dw = wire::word_decode(data, 9);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_missing_sob() {
    /* After type, expect SOR/SOU/SOB — instead get body byte */
    uint8_t data[] = {wire::SOW, '"', 'A', wire::EOW};
    auto dw = wire::word_decode(data, 4);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_reserved_in_body() {
    uint8_t data[] = {wire::SOW, '"', wire::SOB, wire::SOM, wire::EOW};
    auto dw = wire::word_decode(data, 5);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_truncated_radix() {
    /* SOR present but no radix byte follows (end of input) */
    uint8_t data[] = {wire::SOW, '#', wire::SOR};
    auto dw = wire::word_decode(data, 3);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_truncated_unit() {
    /* SOU present but no unit byte follows (end of input) */
    uint8_t data[] = {wire::SOW, '#', wire::SOR, 'D', wire::SOU};
    auto dw = wire::word_decode(data, 5);
    ASSERT_TRUE(!dw.has_value());
}

static void test_decode_no_eow() {
    /* Body data present but no EOW terminator (end of input) */
    uint8_t data[] = {wire::SOW, '"', wire::SOB, 'A', 'B'};
    auto dw = wire::word_decode(data, 5);
    ASSERT_TRUE(!dw.has_value());
}

/* ================================================================
 * 12. DecodedWord operator bool
 * ================================================================ */

static void test_decoded_word_bool() {
    /* Symbol with empty body: operator bool returns true (Symbol is special) */
    wire::DecodedWord dw;
    dw.type = wire::WordType::Symbol;
    dw.radix = wire::Radix::None;
    dw.unit = wire::Unit::None;
    ASSERT_TRUE(static_cast<bool>(dw));

    /* Text with non-empty body: true */
    dw.type = wire::WordType::Text;
    dw.body = {0x41};
    ASSERT_TRUE(static_cast<bool>(dw));

    /* Text with empty body: false */
    dw.body.clear();
    ASSERT_TRUE(!static_cast<bool>(dw));
}

/* ================================================================
 * 13. Context session_mid/state/sid for edge slots
 * ================================================================ */

static void test_context_session_accessors_invalid() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");

    /* Negative slot */
    ASSERT_TRUE(ctx.session_sid(-1).empty());
    ASSERT_EQ(ctx.session_mid(-1), 0u);
    ASSERT_TRUE(ctx.session_state(-1) == SessionState::Idle);

    /* Slot >= MAX_SESSIONS */
    ASSERT_TRUE(ctx.session_sid(32).empty());
    ASSERT_EQ(ctx.session_mid(32), 0u);
    ASSERT_TRUE(ctx.session_state(32) == SessionState::Idle);

    /* Idle (never opened) slot */
    ASSERT_TRUE(ctx.session_sid(0).empty());
    ASSERT_EQ(ctx.session_mid(0), 0u);
    ASSERT_TRUE(ctx.session_state(0) == SessionState::Idle);
}

static void test_context_session_close_idle() {
    /* Closing a never-opened slot returns nullopt */
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.session_close(0).has_value());
}

static void test_context_session_notify_invalid() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.session_notify(-1, {}, "ev").has_value());
    ASSERT_TRUE(!ctx.session_notify(32, {}, "ev").has_value());
    ASSERT_TRUE(!ctx.session_notify(0, {}, "ev").has_value()); /* idle */
}

static void test_context_session_status_invalid() {
    Context ctx("ORG", "Dev", "SN001", "ABCDEFGH");
    ASSERT_TRUE(!ctx.session_status(-1, {}).has_value());
    ASSERT_TRUE(!ctx.session_status(32, {}).has_value());
    ASSERT_TRUE(!ctx.session_status(0, {}).has_value()); /* idle */
}

/* ================================================================
 * 14. word_encode edge cases
 * ================================================================ */

static void test_encode_null_body_zero_len() {
    /* null body + 0 length: should succeed (empty body word) */
    auto result = wire::word_encode(wire::WordType::Text,
                                     wire::Radix::None, wire::Unit::None,
                                     nullptr, 0);
    ASSERT_TRUE(result.has_value());
    /* [SOW][WT][SOB][EOW] = 4 bytes */
    ASSERT_EQ(result->size(), 4u);
}

static void test_encode_id_decimal_zero() {
    auto result = wire::encode_id_decimal(0);
    ASSERT_TRUE(result.has_value());
    /* [SOW]@[SOR]D[SOB]0[EOW] = 7 bytes */
    ASSERT_EQ(result->size(), 7u);
    ASSERT_EQ((*result)[5], '0');
}

static void test_encode_id_decimal_max() {
    auto result = wire::encode_id_decimal(UINT32_MAX);
    ASSERT_TRUE(result.has_value());
    /* Body should be "4294967295" (10 digits) */
    ASSERT_EQ(result->size(), 16u); /* SOW @ SOR D SOB + 10 digits + EOW */
}

/* ================================================================
 * 15. Base-32 encoding edge cases
 * ================================================================ */

static void test_base32_single_byte() {
    uint8_t data = 0xFF;
    auto result = id::base32_encode(&data, 1);
    ASSERT_TRUE(result.has_value());
    /* 1 byte = 8 bits -> ceil(8/5) = 2 chars */
    ASSERT_EQ(result->size(), 2u);
    for (char c : *result) {
        ASSERT_TRUE(std::strchr(id::BASE32_ALPHABET, c) != nullptr);
    }
}

static void test_base32_deterministic() {
    const uint8_t data[] = {0xCA, 0xFE};
    auto a = id::base32_encode(data, 2);
    auto b = id::base32_encode(data, 2);
    ASSERT_TRUE(a.has_value() && b.has_value());
    ASSERT_TRUE(*a == *b);
}

static void test_base32_zero_length() {
    uint8_t data = 0;
    auto result = id::base32_encode(&data, 0);
    ASSERT_TRUE(!result.has_value());
}

/* ================================================================
 * Suite entry point
 * ================================================================ */

void test_depth_run(int& out_run, int& out_passed) {
    std::printf("\n[depth -- coverage gap tests]\n");

    /* 1. Frame class */
    TEST(test_frame_default_empty);
    TEST(test_frame_from_ptr);
    TEST(test_frame_from_vector);
    TEST(test_frame_copy);
    TEST(test_frame_move);
    TEST(test_frame_iterators);
    TEST(test_frame_bytes_mutable);

    /* 2. Parser tail handling */
    TEST(test_parser_tail_callback);
    TEST(test_parser_tail_zero_length);

    /* 3. Parser statistics */
    TEST(test_parser_stats_accumulate);
    TEST(test_parser_error_count);
    TEST(test_parser_stats_survive_reset);

    /* 4. Parser move semantics */
    TEST(test_parser_move_construct);
    TEST(test_parser_move_assign);

    /* 5. SidPool move semantics */
    TEST(test_sidpool_move_construct);
    TEST(test_sidpool_move_assign);

    /* 6. SidPool edge cases */
    TEST(test_sidpool_acquire_unique_null_check);
    TEST(test_sidpool_different_identity_different_sids);
    TEST(test_sidpool_invalid_args);

    /* 7. Context session_accept */
    TEST(test_context_session_accept_basic);
    TEST(test_context_session_accept_empty_sid);
    TEST(test_context_session_accept_call);

    /* 8. Context session exhaustion */
    TEST(test_context_session_exhaustion);

    /* 9. Context feed edge cases */
    TEST(test_context_feed_null);
    TEST(test_context_feed_empty);

    /* 10. Context inbound dispatch */
    TEST(test_dispatch_establish);
    TEST(test_dispatch_conflict);
    TEST(test_dispatch_ping);
    TEST(test_dispatch_discover);
    TEST(test_dispatch_discover_response);
    TEST(test_dispatch_verify);
    TEST(test_dispatch_acknowledge);
    TEST(test_dispatch_exception);
    TEST(test_dispatch_query);
    TEST(test_dispatch_offer);
    TEST(test_dispatch_accept);
    TEST(test_dispatch_session_notify);
    TEST(test_dispatch_session_status);
    TEST(test_dispatch_session_locate);
    TEST(test_dispatch_session_resume);
    TEST(test_dispatch_session_finish);

    /* 10b. Auth (Z-verb) dispatch */
    TEST(test_dispatch_auth_challenge);
    TEST(test_dispatch_auth_response);

    /* 11. word_decode malformed */
    TEST(test_decode_null_input);
    TEST(test_decode_too_short);
    TEST(test_decode_missing_sow);
    TEST(test_decode_invalid_word_type);
    TEST(test_decode_invalid_radix);
    TEST(test_decode_invalid_unit);
    TEST(test_decode_missing_sob);
    TEST(test_decode_reserved_in_body);
    TEST(test_decode_truncated_radix);
    TEST(test_decode_truncated_unit);
    TEST(test_decode_no_eow);

    /* 12. DecodedWord operator bool */
    TEST(test_decoded_word_bool);

    /* 13. Context session accessors */
    TEST(test_context_session_accessors_invalid);
    TEST(test_context_session_close_idle);
    TEST(test_context_session_notify_invalid);
    TEST(test_context_session_status_invalid);

    /* 14. word_encode edge cases */
    TEST(test_encode_null_body_zero_len);
    TEST(test_encode_id_decimal_zero);
    TEST(test_encode_id_decimal_max);

    /* 15. Base-32 encoding edge cases */
    TEST(test_base32_single_byte);
    TEST(test_base32_deterministic);
    TEST(test_base32_zero_length);

    out_run    = tests_run;
    out_passed = tests_passed;
}
