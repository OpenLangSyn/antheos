/*
 * test_edge.cpp — Edge case & robustness tests for Antheos Protocol v9
 *
 * Covers boundary conditions, malformed input handling, parser recovery,
 * reserved byte rejection, and C++ API semantics. Documents actual library
 * behavior without modifying it. C++17 port of test_edge.c.
 *
 * Wire format reference: Antheos Protocol 1.0 Level 1, v9.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>
#include <string>
#include <vector>

using namespace antheos;

/* -- Parser capture state -- */

static struct {
    int word_count;
    int msg_count;
    uint8_t types[16];
    uint8_t radixes[16];
    uint8_t units[16];
    char bodies[16][4096];
    size_t body_lens[16];
    size_t msg_word_counts[8];
} g_edge;

static Parser g_edge_parser;

static void edge_parse_reset() {
    std::memset(&g_edge, 0, sizeof(g_edge));
    g_edge_parser = Parser();
    g_edge_parser.on_word([](wire::WordType type, wire::Radix radix, wire::Unit unit,
                              const uint8_t* body, size_t len) {
        int i = g_edge.word_count;
        if (i < 16) {
            g_edge.types[i] = static_cast<uint8_t>(type);
            g_edge.radixes[i] = static_cast<uint8_t>(radix);
            g_edge.units[i] = static_cast<uint8_t>(unit);
            size_t cap = sizeof(g_edge.bodies[0]) - 1;
            if (len > cap) len = cap;
            std::memcpy(g_edge.bodies[i], body, len);
            g_edge.bodies[i][len] = '\0';
            g_edge.body_lens[i] = len;
        }
        g_edge.word_count++;
    });
    g_edge_parser.on_message([](size_t word_count) {
        int i = g_edge.msg_count;
        if (i < 8)
            g_edge.msg_word_counts[i] = word_count;
        g_edge.msg_count++;
    });
}

static void edge_parse_bytes(const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++)
        g_edge_parser.feed(bytes[i]);
}

/* ================================================================
 * 1. Empty and minimal inputs
 * ================================================================ */

static void test_edge_empty_bid_establish() {
    /* C++ rejects empty BID: FrameBuilder::id_base32("") returns false */
    auto f = bus::establish("");
    ASSERT_TRUE(!f.has_value());
}

static void test_edge_empty_text_broadcast() {
    /* Empty text: valid frame with zero-length TEXT body */
    auto f = bus::broadcast("");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 2);
    ASSERT_EQ(g_edge.types[1], '"');
    ASSERT_EQ((int)g_edge.body_lens[1], 0);
}

static void test_edge_empty_sid_call() {
    /* C++ rejects empty SID: id_base32("") returns false */
    auto f = session::call("", "", 1, "x");
    ASSERT_TRUE(!f.has_value());
}

static void test_edge_single_char_bid() {
    /* 1-char BID: below BID_MIN_LEN=2 but frame builder accepts (no policy check) */
    auto f = bus::establish("A");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[1], '@');
    ASSERT_EQ(g_edge.radixes[1], 'U');
    ASSERT_EQ((int)g_edge.body_lens[1], 1);
    ASSERT_EQ(g_edge.bodies[1][0], 'A');
}

static void test_edge_single_char_payload() {
    auto f = session::call("A7K2M", "", 1, "x");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[3], '"');
    ASSERT_EQ((int)g_edge.body_lens[3], 1);
    ASSERT_EQ(g_edge.bodies[3][0], 'x');
}

static void test_edge_empty_payload_notify() {
    /* Empty event string: produces TEXT word with zero-length body */
    auto f = session::notify("A7K2M", "", 1, "");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[3], '"');
    ASSERT_EQ((int)g_edge.body_lens[3], 0);
}

static void test_edge_empty_exception() {
    /* Empty reason: TEXT word with zero-length body */
    auto f = bus::exception("");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[1], '"');
    ASSERT_EQ((int)g_edge.body_lens[1], 0);
}

static void test_edge_minimal_frame_parse() {
    /* Smallest valid frame: [SOM][SOW]![SOB]E[EOW][EOM] = 7 bytes */
    const uint8_t frame[] = {0x02, 0x12, 0x21, 0x1A, 0x45, 0x10, 0x03};
    edge_parse_reset();
    edge_parse_bytes(frame, sizeof(frame));
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 1);
    ASSERT_EQ(g_edge.types[0], '!');
    ASSERT_EQ(g_edge.bodies[0][0], 'E');
}

/* ================================================================
 * 2. Maximum length inputs
 * ================================================================ */

static void test_edge_max_bid_length() {
    /* BID_MAX_LEN = 16 characters */
    const char bid16[] = "ABCDEFGHJKLMNPQR";
    auto f = bus::establish(bid16);
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[1], '@');
    ASSERT_EQ((int)g_edge.body_lens[1], 16);
    ASSERT_MEM_EQ(g_edge.bodies[1], bid16, 16);
}

static void test_edge_max_sid_length() {
    /* SID_MAX_LEN = 16 characters */
    const char sid16[] = "ABCDEFGHJKLMNPQR";
    auto f = session::call(sid16, "", 1, "x");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[1], '@');
    ASSERT_EQ(g_edge.radixes[1], 'U');
    ASSERT_EQ((int)g_edge.body_lens[1], 16);
    ASSERT_MEM_EQ(g_edge.bodies[1], sid16, 16);
}

static void test_edge_long_text_payload() {
    /* 4000-byte payload: exercises vector growth in parser word buffer */
    std::string payload(4000, 'Z');

    auto f = session::call("A7K2M", "", 1, payload);
    ASSERT_TRUE(f.has_value());

    /* Parse byte-by-byte */
    edge_parse_reset();
    for (size_t i = 0; i < f->size(); i++)
        g_edge_parser.feed(f->data()[i]);

    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[3], '"');
    ASSERT_EQ((int)g_edge.body_lens[3], 4000);
}

static void test_edge_long_path() {
    /* 10-hop path: "AA.BB.CC.DD.EE.FF.GG.HH.II.JJ" */
    const char* path = "AA.BB.CC.DD.EE.FF.GG.HH.II.JJ";
    auto f = bus::broadcast_path("x", path);
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[2], '/');
    ASSERT_EQ((int)g_edge.body_lens[2], (int)std::strlen(path));
    ASSERT_MEM_EQ(g_edge.bodies[2], path, std::strlen(path));
}

static void test_edge_large_mid() {
    /* MID=999999999: large decimal in @D word body */
    auto f = session::call("A7K2M", "", 999999999, "x");
    ASSERT_TRUE(f.has_value());
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[2], '@');
    ASSERT_EQ(g_edge.radixes[2], 'D');
    ASSERT_MEM_EQ(g_edge.bodies[2], "999999999", 9);
    ASSERT_EQ((int)g_edge.body_lens[2], 9);
}

/* ================================================================
 * 3. Reserved bytes in bodies
 * ================================================================ */

static void test_edge_reserved_in_text_body() {
    /* Attempt to encode TEXT with body containing 0x02 (SOM) */
    uint8_t body[] = {'H', 0x02, 'i'};
    auto result = wire::word_encode(wire::WordType::Text,
                                     wire::Radix::None, wire::Unit::None,
                                     body, 3);
    ASSERT_TRUE(!result.has_value());
}

static void test_edge_all_reserved_bytes() {
    /* Each of the 7 reserved bytes must be rejected in text body */
    const uint8_t reserved[] = {0x02, 0x03, 0x04, 0x07, 0x10, 0x12, 0x1A};
    for (size_t i = 0; i < sizeof(reserved); i++) {
        uint8_t body[1] = {reserved[i]};
        auto result = wire::word_encode(wire::WordType::Text,
                                         wire::Radix::None, wire::Unit::None,
                                         body, 1);
        ASSERT_TRUE(!result.has_value());
    }
}

static void test_edge_reserved_in_id_body() {
    /* encode_id_plain with body starting with reserved byte */
    auto result = wire::encode_id_plain("\x02test");
    ASSERT_TRUE(!result.has_value());
}

static void test_edge_high_bytes_in_body() {
    /* CP437 upper range (0x80-0xFF): valid body bytes, must pass through */
    uint8_t body[128];
    for (int i = 0; i < 128; i++)
        body[i] = static_cast<uint8_t>(0x80 + i);

    auto result = wire::word_encode(wire::WordType::Text,
                                     wire::Radix::None, wire::Unit::None,
                                     body, 128);
    ASSERT_TRUE(result.has_value());

    /* Verify body passes through unmodified by round-tripping */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), result->begin(), result->end());
    frame.push_back(wire::EOM);

    edge_parse_reset();
    edge_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ((int)g_edge.body_lens[0], 128);
    ASSERT_MEM_EQ(g_edge.bodies[0], body, 128);
}

/* ================================================================
 * 4. Parser robustness
 * ================================================================ */

static void test_edge_parse_eom_without_som() {
    /* [EOM] without preceding SOM: parser stays in WaitSom, no crash */
    edge_parse_reset();
    uint8_t stream[] = {0x03};
    edge_parse_bytes(stream, sizeof(stream));
    ASSERT_EQ(g_edge.msg_count, 0);
    ASSERT_EQ(g_edge.word_count, 0);
    ASSERT_EQ(static_cast<int>(g_edge_parser.state()),
              static_cast<int>(ParseState::WaitSom));
}

static void test_edge_parse_double_som() {
    /* [SOM][SOM]...: second SOM in WaitSow -> error. In error state,
     * subsequent non-SOM bytes are discarded. No message parsed. */
    edge_parse_reset();
    const uint8_t stream[] = {
        0x02,                                     /* SOM -- starts msg */
        0x02,                                     /* SOM in WaitSow -> error */
        0x12, 0x21, 0x1A, 0x45, 0x10, 0x03       /* SOW ! SOB E EOW EOM */
    };
    edge_parse_bytes(stream, sizeof(stream));
    ASSERT_EQ(g_edge.msg_count, 0);
}

static void test_edge_parse_eow_without_sow() {
    /* [SOM][EOW][EOM]: EOW in WaitSow -> error */
    edge_parse_reset();
    const uint8_t stream[] = {0x02, 0x10, 0x03};
    edge_parse_bytes(stream, sizeof(stream));
    ASSERT_EQ(g_edge.msg_count, 0);
}

static void test_edge_parse_sob_without_wt() {
    /* [SOM][SOW][SOB]E[EOW][EOM]: SOB where word type expected -> error */
    edge_parse_reset();
    const uint8_t stream[] = {0x02, 0x12, 0x1A, 0x45, 0x10, 0x03};
    edge_parse_bytes(stream, sizeof(stream));
    ASSERT_EQ(g_edge.msg_count, 0);
}

static void test_edge_parse_truncated_mid_frame() {
    /* [SOM][SOW]![SOB] then stop: no message callback fires, no crash */
    edge_parse_reset();
    const uint8_t stream[] = {0x02, 0x12, 0x21, 0x1A};
    edge_parse_bytes(stream, sizeof(stream));
    ASSERT_EQ(g_edge.msg_count, 0);
    ASSERT_EQ(g_edge.word_count, 0);
}

static void test_edge_parse_empty_word_body() {
    /* [SOM][SOW]"[SOB][EOW][EOM]: TEXT word with zero-length body */
    edge_parse_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x22, 0x1A, 0x10, 0x03};
    edge_parse_bytes(frame, sizeof(frame));
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 1);
    ASSERT_EQ(g_edge.types[0], '"');
    ASSERT_EQ((int)g_edge.body_lens[0], 0);
}

static void test_edge_parse_only_som_eom() {
    /* [SOM][EOM]: message with zero words */
    edge_parse_reset();
    const uint8_t frame[] = {0x02, 0x03};
    edge_parse_bytes(frame, sizeof(frame));
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 0);
    ASSERT_EQ(g_edge.msg_word_counts[0], (size_t)0);
}

static void test_edge_parse_interleaved_garbage() {
    /* Valid frame, 100 random non-SOM bytes, valid frame */
    edge_parse_reset();
    std::vector<uint8_t> stream;

    /* Frame 1: !E */
    const uint8_t f1[] = {0x02, 0x12, 0x21, 0x1A, 0x45, 0x10, 0x03};
    stream.insert(stream.end(), f1, f1 + sizeof(f1));

    /* 100 garbage bytes (0x20-0x7F range, no SOM) */
    for (int i = 0; i < 100; i++)
        stream.push_back(static_cast<uint8_t>(0x20 + (i % 96)));

    /* Frame 2: !P */
    const uint8_t f2[] = {0x02, 0x12, 0x21, 0x1A, 0x50, 0x10, 0x03};
    stream.insert(stream.end(), f2, f2 + sizeof(f2));

    edge_parse_bytes(stream.data(), stream.size());
    ASSERT_EQ(g_edge.msg_count, 2);
    ASSERT_EQ(g_edge.bodies[0][0], 'E');
    ASSERT_EQ(g_edge.bodies[1][0], 'P');
}

/* ================================================================
 * 5. C++ API semantics (replaces C11 buffer boundary tests)
 * ================================================================ */

static void test_edge_all_builders_frame_envelope() {
    /* Every builder's output starts with SOM and ends with EOM */
    auto check = [](const std::optional<Frame>& f) {
        ASSERT_TRUE(f.has_value());
        ASSERT_TRUE(f->size() >= 2);
        ASSERT_EQ(f->data()[0], wire::SOM);
        ASSERT_EQ(f->data()[f->size() - 1], wire::EOM);
    };

    check(bus::establish("AB"));
    check(bus::conflict("AB"));
    check(bus::broadcast("x"));
    check(bus::broadcast_path("x", "a"));
    check(bus::ping("AB"));
    check(bus::ping_all());
    check(bus::relay("x", 1, "a"));
    check(bus::discover("AB"));
    check(bus::discover_response("AB", "CD"));
    check(bus::verify("AB"));
    check(bus::verify_response("a", "b", "c"));
    check(bus::scaleback("!H", 0, 0));
    check(bus::acknowledge("AB"));
    check(bus::exception("x"));
    check(service::query("x"));
    check(service::offer("AB", "x"));
    check(service::accept("AB"));
    check(session::call("AB", "", 1, "x"));
    check(session::call_wrap("AB", ""));
    check(session::status("AB", "", 1));
    check(session::status_response("AB", "", 1, "OK"));
    check(session::notify("AB", "", 1, "x"));
    check(session::locate("AB"));
    check(session::locate_response("AB", "CD"));
    check(session::resume("AB"));
    check(session::finish("AB"));
}

static void test_edge_parser_feed_bulk() {
    /* Parser::feed(data, len) batch overload matches byte-at-a-time */
    auto f = bus::broadcast("bulk test");
    ASSERT_TRUE(f.has_value());

    /* Byte-at-a-time */
    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    int words_bab = g_edge.word_count;
    int msgs_bab = g_edge.msg_count;

    /* Bulk feed */
    edge_parse_reset();
    g_edge_parser.feed(f->data(), f->size());
    ASSERT_EQ(g_edge.word_count, words_bab);
    ASSERT_EQ(g_edge.msg_count, msgs_bab);
}

static void test_edge_builder_nullopt_empty_id() {
    /* All builders requiring a BID/SID reject empty string */
    ASSERT_TRUE(!bus::establish("").has_value());
    ASSERT_TRUE(!bus::conflict("").has_value());
    ASSERT_TRUE(!bus::ping("").has_value());
    ASSERT_TRUE(!bus::discover("").has_value());
    ASSERT_TRUE(!bus::verify("").has_value());
    ASSERT_TRUE(!bus::acknowledge("").has_value());
    ASSERT_TRUE(!bus::discover_response("", "CD").has_value());
    ASSERT_TRUE(!bus::discover_response("AB", "").has_value());
    ASSERT_TRUE(!service::offer("", "x").has_value());
    ASSERT_TRUE(!service::accept("").has_value());
    ASSERT_TRUE(!session::call("", "", 1, "x").has_value());
    ASSERT_TRUE(!session::locate("").has_value());
    ASSERT_TRUE(!session::resume("").has_value());
    ASSERT_TRUE(!session::finish("").has_value());
}

static void test_edge_builder_roundtrip_all() {
    /* Every verb builder produces a frame that parses cleanly */
    auto check = [](const std::optional<Frame>& f, int expected_words) {
        ASSERT_TRUE(f.has_value());
        edge_parse_reset();
        edge_parse_bytes(f->data(), f->size());
        ASSERT_EQ(g_edge.msg_count, 1);
        ASSERT_EQ(g_edge.word_count, expected_words);
    };

    check(bus::establish("AB"), 2);
    check(bus::conflict("AB"), 2);
    check(bus::broadcast("x"), 2);
    check(bus::broadcast_path("x", "a"), 3);
    check(bus::ping("AB"), 2);
    check(bus::ping_all(), 1);
    check(bus::relay("x", 1, "a"), 4);
    check(bus::discover("AB"), 2);
    check(bus::discover_response("AB", "CD"), 3);
    check(bus::verify("AB"), 2);
    check(bus::verify_response("a", "b", "c"), 4);
    check(bus::scaleback("!H", 0, 0), 2);
    check(bus::acknowledge("AB"), 2);
    check(bus::exception("x"), 2);
    check(service::query("x"), 2);
    check(service::offer("AB", "x"), 3);
    check(service::accept("AB"), 2);
    check(session::call("AB", "", 1, "x"), 4);
    check(session::status("AB", "", 1), 3);
    check(session::status_response("AB", "", 1, "OK"), 4);
    check(session::notify("AB", "", 1, "x"), 4);
    check(session::locate("AB"), 2);
    check(session::locate_response("AB", "CD"), 3);
    check(session::resume("AB"), 2);
    check(session::finish("AB"), 2);
}

/* ================================================================
 * 6. Word type coverage
 * ================================================================ */

static void test_edge_encode_timestamp() {
    /* TIMESTAMP word via wire::word_encode */
    const char* ts = "2026-02-08T12:00:00Z";
    auto out = wire::word_encode(wire::WordType::Timestamp,
                                  wire::Radix::None, wire::Unit::None,
                                  reinterpret_cast<const uint8_t*>(ts),
                                  std::strlen(ts));
    ASSERT_TRUE(out.has_value());

    /* Verify wire format: [SOW]&[SOB]2026-02-08T12:00:00Z[EOW] */
    ASSERT_EQ((*out)[0], wire::SOW);
    ASSERT_EQ((*out)[1], '&');
    ASSERT_EQ((*out)[2], wire::SOB);
    ASSERT_MEM_EQ(out->data() + 3, ts, std::strlen(ts));
    ASSERT_EQ((*out)[3 + std::strlen(ts)], wire::EOW);

    /* Round-trip via parser */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), out->begin(), out->end());
    frame.push_back(wire::EOM);

    edge_parse_reset();
    edge_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.types[0], '&');
    ASSERT_EQ(g_edge.radixes[0], 0);
    ASSERT_EQ(g_edge.units[0], 0);
    ASSERT_MEM_EQ(g_edge.bodies[0], ts, std::strlen(ts));
}

static void test_edge_encode_real() {
    /* REAL word: radix D, unit Q, body "3.14" */
    const char* val = "3.14";
    auto out = wire::word_encode(wire::WordType::Real,
                                  wire::Radix::Decimal, wire::Unit::Qword,
                                  reinterpret_cast<const uint8_t*>(val),
                                  std::strlen(val));
    ASSERT_TRUE(out.has_value());

    /* Wire: [SOW]$[SOR]D[SOU]Q[SOB]3.14[EOW] */
    ASSERT_EQ((*out)[0], wire::SOW);
    ASSERT_EQ((*out)[1], '$');
    ASSERT_EQ((*out)[2], wire::SOR);
    ASSERT_EQ((*out)[3], 'D');
    ASSERT_EQ((*out)[4], wire::SOU);
    ASSERT_EQ((*out)[5], 'Q');
    ASSERT_EQ((*out)[6], wire::SOB);
    ASSERT_MEM_EQ(out->data() + 7, val, std::strlen(val));
    ASSERT_EQ((*out)[7 + std::strlen(val)], wire::EOW);
}

static void test_edge_encode_scientific() {
    /* SCIENTIFIC word: radix D, unit B, body "1E10" */
    const char* val = "1E10";
    auto out = wire::word_encode(wire::WordType::Scientific,
                                  wire::Radix::Decimal, wire::Unit::Byte,
                                  reinterpret_cast<const uint8_t*>(val),
                                  std::strlen(val));
    ASSERT_TRUE(out.has_value());

    /* Wire: [SOW]%[SOR]D[SOU]B[SOB]1E10[EOW] */
    ASSERT_EQ((*out)[0], wire::SOW);
    ASSERT_EQ((*out)[1], '%');
    ASSERT_EQ((*out)[2], wire::SOR);
    ASSERT_EQ((*out)[3], 'D');
    ASSERT_EQ((*out)[4], wire::SOU);
    ASSERT_EQ((*out)[5], 'B');
    ASSERT_EQ((*out)[6], wire::SOB);
    ASSERT_MEM_EQ(out->data() + 7, val, std::strlen(val));
}

static void test_edge_encode_blob_declaration() {
    /* BLOB word: radix H, unit D, body "1A00" per spec ss7.3 */
    const char* val = "1A00";
    auto out = wire::word_encode(wire::WordType::Blob,
                                  wire::Radix::Hex, wire::Unit::Dword,
                                  reinterpret_cast<const uint8_t*>(val),
                                  std::strlen(val));
    ASSERT_TRUE(out.has_value());

    /* Wire: [SOW]*[SOR]H[SOU]D[SOB]1A00[EOW] */
    ASSERT_EQ((*out)[0], wire::SOW);
    ASSERT_EQ((*out)[1], '*');
    ASSERT_EQ((*out)[2], wire::SOR);
    ASSERT_EQ((*out)[3], 'H');
    ASSERT_EQ((*out)[4], wire::SOU);
    ASSERT_EQ((*out)[5], 'D');
    ASSERT_EQ((*out)[6], wire::SOB);
    ASSERT_MEM_EQ(out->data() + 7, val, std::strlen(val));
    ASSERT_EQ((*out)[7 + std::strlen(val)], wire::EOW);
}

/* ================================================================
 * 7. Target BID in session frames
 * ================================================================ */

static void test_edge_session_call_with_target() {
    /* K-frame with target_bid: SID, target BID, MID, TEXT */
    auto f = session::call("A7K2M", "4T9X2", 1, "payload");
    ASSERT_TRUE(f.has_value());

    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 5);
    /* Word 0: !K */
    ASSERT_EQ(g_edge.types[0], '!');
    ASSERT_EQ(g_edge.bodies[0][0], 'K');
    /* Word 1: @U SID */
    ASSERT_EQ(g_edge.types[1], '@');
    ASSERT_EQ(g_edge.radixes[1], 'U');
    ASSERT_MEM_EQ(g_edge.bodies[1], "A7K2M", 5);
    /* Word 2: @U target BID */
    ASSERT_EQ(g_edge.types[2], '@');
    ASSERT_EQ(g_edge.radixes[2], 'U');
    ASSERT_MEM_EQ(g_edge.bodies[2], "4T9X2", 5);
    /* Word 3: @D MID */
    ASSERT_EQ(g_edge.types[3], '@');
    ASSERT_EQ(g_edge.radixes[3], 'D');
    ASSERT_MEM_EQ(g_edge.bodies[3], "1", 1);
    /* Word 4: "payload */
    ASSERT_EQ(g_edge.types[4], '"');
    ASSERT_MEM_EQ(g_edge.bodies[4], "payload", 7);
}

static void test_edge_session_notify_with_target() {
    auto f = session::notify("A7K2M", "4T9X2", 1, "event");
    ASSERT_TRUE(f.has_value());

    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 5);
    ASSERT_EQ(g_edge.types[0], '!');
    ASSERT_EQ(g_edge.bodies[0][0], 'N');
    ASSERT_EQ(g_edge.types[2], '@');
    ASSERT_EQ(g_edge.radixes[2], 'U');
    ASSERT_MEM_EQ(g_edge.bodies[2], "4T9X2", 5);
}

static void test_edge_session_finish_with_target() {
    auto f = session::finish("A7K2M", "4T9X2");
    ASSERT_TRUE(f.has_value());

    edge_parse_reset();
    edge_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_edge.msg_count, 1);
    ASSERT_EQ(g_edge.word_count, 3);
    ASSERT_EQ(g_edge.types[0], '!');
    ASSERT_EQ(g_edge.bodies[0][0], 'F');
    /* Word 1: SID */
    ASSERT_EQ(g_edge.types[1], '@');
    ASSERT_EQ(g_edge.radixes[1], 'U');
    ASSERT_MEM_EQ(g_edge.bodies[1], "A7K2M", 5);
    /* Word 2: target BID */
    ASSERT_EQ(g_edge.types[2], '@');
    ASSERT_EQ(g_edge.radixes[2], 'U');
    ASSERT_MEM_EQ(g_edge.bodies[2], "4T9X2", 5);
}

/* ================================================================
 * Suite entry point
 * ================================================================ */

void test_edge_run(int& out_run, int& out_passed) {
    std::printf("\n[edge -- boundary conditions & robustness]\n");

    /* 1. Empty and minimal inputs */
    TEST(test_edge_empty_bid_establish);
    TEST(test_edge_empty_text_broadcast);
    TEST(test_edge_empty_sid_call);
    TEST(test_edge_single_char_bid);
    TEST(test_edge_single_char_payload);
    TEST(test_edge_empty_payload_notify);
    TEST(test_edge_empty_exception);
    TEST(test_edge_minimal_frame_parse);

    /* 2. Maximum length inputs */
    TEST(test_edge_max_bid_length);
    TEST(test_edge_max_sid_length);
    TEST(test_edge_long_text_payload);
    TEST(test_edge_long_path);
    TEST(test_edge_large_mid);

    /* 3. Reserved bytes in bodies */
    TEST(test_edge_reserved_in_text_body);
    TEST(test_edge_all_reserved_bytes);
    TEST(test_edge_reserved_in_id_body);
    TEST(test_edge_high_bytes_in_body);

    /* 4. Parser robustness */
    TEST(test_edge_parse_eom_without_som);
    TEST(test_edge_parse_double_som);
    TEST(test_edge_parse_eow_without_sow);
    TEST(test_edge_parse_sob_without_wt);
    TEST(test_edge_parse_truncated_mid_frame);
    TEST(test_edge_parse_empty_word_body);
    TEST(test_edge_parse_only_som_eom);
    TEST(test_edge_parse_interleaved_garbage);

    /* 5. C++ API semantics */
    TEST(test_edge_all_builders_frame_envelope);
    TEST(test_edge_parser_feed_bulk);
    TEST(test_edge_builder_nullopt_empty_id);
    TEST(test_edge_builder_roundtrip_all);

    /* 6. Word type coverage */
    TEST(test_edge_encode_timestamp);
    TEST(test_edge_encode_real);
    TEST(test_edge_encode_scientific);
    TEST(test_edge_encode_blob_declaration);

    /* 7. Target BID in session frames */
    TEST(test_edge_session_call_with_target);
    TEST(test_edge_session_notify_with_target);
    TEST(test_edge_session_finish_with_target);

    out_run    = tests_run;
    out_passed = tests_passed;
}
