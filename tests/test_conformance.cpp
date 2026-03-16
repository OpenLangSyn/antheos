/*
 * test_conformance.cpp — Spec conformance tests for Antheos Protocol v9
 *
 * Verifies every wire example from Antheos_Protocol_1_0_v9.md byte-for-byte
 * using build->parse round-trips. C++17 port of test_conformance.c.
 *
 * Wire format reference: Antheos Protocol 1.0 Level 1, v9 ss6.5, ss9, ss10, ss11.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>
#include <vector>

using namespace antheos;

/* -- Parser capture state -- shared by all round-trip tests -- */

static struct {
    int word_count;
    int msg_count;
    uint8_t types[16];
    uint8_t radixes[16];
    uint8_t units[16];
    char bodies[16][256];
    size_t body_lens[16];
    size_t msg_word_counts[8];
} g_conf;

static Parser g_conf_parser;

static void conf_parse_reset() {
    std::memset(&g_conf, 0, sizeof(g_conf));
    g_conf_parser = Parser();
    g_conf_parser.on_word([](wire::WordType type, wire::Radix radix, wire::Unit unit,
                              const uint8_t* body, size_t len) {
        int i = g_conf.word_count;
        if (i < 16) {
            g_conf.types[i] = static_cast<uint8_t>(type);
            g_conf.radixes[i] = static_cast<uint8_t>(radix);
            g_conf.units[i] = static_cast<uint8_t>(unit);
            if (len > 255) len = 255;
            std::memcpy(g_conf.bodies[i], body, len);
            g_conf.bodies[i][len] = '\0';
            g_conf.body_lens[i] = len;
        }
        g_conf.word_count++;
    });
    g_conf_parser.on_message([](size_t word_count) {
        int i = g_conf.msg_count;
        if (i < 8)
            g_conf.msg_word_counts[i] = word_count;
        g_conf.msg_count++;
    });
}

static void conf_parse_bytes(const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++)
        g_conf_parser.feed(bytes[i]);
}

/* ================================================================
 * ss6.5 Word Encoding Examples -- round-trip tests
 * ================================================================ */

/* ss6.5: SYMBOL 'E' -> [SOW]![SOB]E[EOW] = 12 21 1A 45 10 */

static void test_spec_word_symbol_E() {
    auto enc = wire::encode_symbol('E');
    ASSERT_TRUE(enc.has_value());

    const uint8_t expected[] = {0x12, 0x21, 0x1A, 0x45, 0x10};
    ASSERT_EQ(enc->size(), sizeof(expected));
    ASSERT_MEM_EQ(enc->data(), expected, sizeof(expected));

    /* Round-trip: wrap in SOM/EOM and parse */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), enc->begin(), enc->end());
    frame.push_back(wire::EOM);

    conf_parse_reset();
    conf_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_conf.word_count, 1);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.radixes[0], 0);
    ASSERT_EQ(g_conf.units[0], 0);
    ASSERT_EQ((int)g_conf.body_lens[0], 1);
    ASSERT_EQ(g_conf.bodies[0][0], 'E');
}

/* ss6.5: ID plain "langsyn" -> [SOW]@[SOB]langsyn[EOW] */

static void test_spec_word_id_oid() {
    auto enc = wire::encode_id_plain("langsyn");
    ASSERT_TRUE(enc.has_value());

    const uint8_t expected[] = {
        0x12, 0x40, 0x1A,
        'l', 'a', 'n', 'g', 's', 'y', 'n',
        0x10
    };
    ASSERT_EQ(enc->size(), sizeof(expected));
    ASSERT_MEM_EQ(enc->data(), expected, sizeof(expected));

    /* Round-trip */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), enc->begin(), enc->end());
    frame.push_back(wire::EOM);

    conf_parse_reset();
    conf_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_conf.word_count, 1);
    ASSERT_EQ(g_conf.types[0], '@');
    ASSERT_EQ(g_conf.radixes[0], 0);
    ASSERT_EQ(g_conf.units[0], 0);
    ASSERT_EQ((int)g_conf.body_lens[0], 7);
    ASSERT_MEM_EQ(g_conf.bodies[0], "langsyn", 7);
}

/* ss6.5: ID base32 "4T9X2" -> [SOW]@[SOR]U[SOB]4T9X2[EOW] */

static void test_spec_word_id_bid() {
    auto enc = wire::encode_id_base32("4T9X2");
    ASSERT_TRUE(enc.has_value());

    const uint8_t expected[] = {
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2',
        0x10
    };
    ASSERT_EQ(enc->size(), sizeof(expected));
    ASSERT_MEM_EQ(enc->data(), expected, sizeof(expected));

    /* Round-trip */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), enc->begin(), enc->end());
    frame.push_back(wire::EOM);

    conf_parse_reset();
    conf_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_conf.word_count, 1);
    ASSERT_EQ(g_conf.types[0], '@');
    ASSERT_EQ(g_conf.radixes[0], 'U');
    ASSERT_EQ(g_conf.units[0], 0);
    ASSERT_EQ((int)g_conf.body_lens[0], 5);
    ASSERT_MEM_EQ(g_conf.bodies[0], "4T9X2", 5);
}

/* ss6.5: INTEGER radix=D unit=B body="32" */

static void test_spec_word_integer_decimal_byte() {
    auto enc = wire::encode_integer(wire::Radix::Decimal, wire::Unit::Byte, "32");
    ASSERT_TRUE(enc.has_value());

    const uint8_t expected[] = {
        0x12, 0x23, 0x04, 0x44, 0x07, 0x42, 0x1A,
        '3', '2',
        0x10
    };
    ASSERT_EQ(enc->size(), sizeof(expected));
    ASSERT_MEM_EQ(enc->data(), expected, sizeof(expected));

    /* Round-trip */
    std::vector<uint8_t> frame;
    frame.push_back(wire::SOM);
    frame.insert(frame.end(), enc->begin(), enc->end());
    frame.push_back(wire::EOM);

    conf_parse_reset();
    conf_parse_bytes(frame.data(), frame.size());
    ASSERT_EQ(g_conf.word_count, 1);
    ASSERT_EQ(g_conf.types[0], '#');
    ASSERT_EQ(g_conf.radixes[0], 'D');
    ASSERT_EQ(g_conf.units[0], 'B');
    ASSERT_EQ((int)g_conf.body_lens[0], 2);
    ASSERT_MEM_EQ(g_conf.bodies[0], "32", 2);
}

/* ================================================================
 * ss9.1 BID Establishment -- 3 wire examples
 * ================================================================ */

/* ss9.1.4 example 1: Establish BID "4T9X2" */

static void test_spec_establish_bid() {
    auto f = bus::establish("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x45, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    /* Round-trip parse */
    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'E');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.1.4 example 2: Conflict BID "4T9X2" */

static void test_spec_conflict_bid() {
    auto f = bus::conflict("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x43, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'C');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.1.4 example 3: Exception "BID_OVERFLOW" */

static void test_spec_establish_failed() {
    auto f = bus::exception("BID_OVERFLOW");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x58, 0x10,
        0x12, 0x22, 0x1A,
        'B', 'I', 'D', '_', 'O', 'V', 'E', 'R', 'F', 'L', 'O', 'W', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'X');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "BID_OVERFLOW", 12);
}

/* ================================================================
 * ss9.2 Scaleback -- 3 wire examples
 * ================================================================ */

/* ss9.2.3 example 1: Full scaleback -- exclusions "!H&!Q", head=32, tail=0 */

static void test_spec_scaleback_full() {
    auto f = bus::scaleback("!H&!Q", 32, 0);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x53, 0x10,
        0x12, 0x3F, 0x1A,
        '!', 'H', '&', '!', 'Q', 0x10,
        0x12, 0x23, 0x04, 0x44, 0x07, 0x57, 0x1A,
        '3', '2', 0x10,
        0x12, 0x23, 0x04, 0x44, 0x07, 0x57, 0x1A,
        '0', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'S');
    ASSERT_EQ(g_conf.types[1], '?');
    ASSERT_MEM_EQ(g_conf.bodies[1], "!H&!Q", 5);
    ASSERT_EQ(g_conf.types[2], '#');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_EQ(g_conf.units[2], 'W');
    ASSERT_MEM_EQ(g_conf.bodies[2], "32", 2);
    ASSERT_EQ(g_conf.types[3], '#');
    ASSERT_EQ(g_conf.radixes[3], 'D');
    ASSERT_EQ(g_conf.units[3], 'W');
    ASSERT_MEM_EQ(g_conf.bodies[3], "0", 1);
}

/* ss9.2.3 example 2: Sizes only -- no exclusions, head=64, tail=0 */

static void test_spec_scaleback_sizes_only() {
    auto f = bus::scaleback("", 64, 0);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x53, 0x10,
        0x12, 0x23, 0x04, 0x44, 0x07, 0x57, 0x1A,
        '6', '4', 0x10,
        0x12, 0x23, 0x04, 0x44, 0x07, 0x57, 0x1A,
        '0', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'S');
    ASSERT_EQ(g_conf.types[1], '#');
    ASSERT_EQ(g_conf.radixes[1], 'D');
    ASSERT_EQ(g_conf.units[1], 'W');
    ASSERT_MEM_EQ(g_conf.bodies[1], "64", 2);
    ASSERT_EQ(g_conf.types[2], '#');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_EQ(g_conf.units[2], 'W');
    ASSERT_MEM_EQ(g_conf.bodies[2], "0", 1);
}

/* ss9.2.3 example 3: Flags only -- exclusions "!Q&!T", no sizes */

static void test_spec_scaleback_flags_only() {
    auto f = bus::scaleback("!Q&!T", 0, 0);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x53, 0x10,
        0x12, 0x3F, 0x1A,
        '!', 'Q', '&', '!', 'T', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'S');
    ASSERT_EQ(g_conf.types[1], '?');
    ASSERT_MEM_EQ(g_conf.bodies[1], "!Q&!T", 5);
}

/* ================================================================
 * ss9.3 Path Addressing -- 2 wire examples
 * ================================================================ */

/* ss9.3.1: Broadcast "Hello" with path "AA" */

static void test_spec_broadcast_path_origin() {
    auto f = bus::broadcast_path("Hello", "AA");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x42, 0x10,
        0x12, 0x22, 0x1A,
        'H', 'e', 'l', 'l', 'o', 0x10,
        0x12, 0x2F, 0x1A,
        'A', 'A', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'B');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "Hello", 5);
    ASSERT_EQ(g_conf.types[2], '/');
    ASSERT_MEM_EQ(g_conf.bodies[2], "AA", 2);
}

/* ss9.3.2: Relay "Reply" index=3 path="AA.BB.CC.DD" */

static void test_spec_relay_d_to_a() {
    auto f = bus::relay("Reply", 3, "AA.BB.CC.DD");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x52, 0x10,
        0x12, 0x22, 0x1A,
        'R', 'e', 'p', 'l', 'y', 0x10,
        0x12, 0x23, 0x04, 0x44, 0x07, 0x42, 0x1A,
        '3', 0x10,
        0x12, 0x2F, 0x1A,
        'A', 'A', '.', 'B', 'B', '.', 'C', 'C', '.', 'D', 'D', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'R');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "Reply", 5);
    ASSERT_EQ(g_conf.types[2], '#');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_EQ(g_conf.units[2], 'B');
    ASSERT_MEM_EQ(g_conf.bodies[2], "3", 1);
    ASSERT_EQ(g_conf.types[3], '/');
    ASSERT_MEM_EQ(g_conf.bodies[3], "AA.BB.CC.DD", 11);
}

/* ================================================================
 * ss9.4--9.9 Bus Verbs -- wire examples
 * ================================================================ */

/* ss9.4: Broadcast "Hello all" */

static void test_spec_broadcast_simple() {
    auto f = bus::broadcast("Hello all");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x42, 0x10,
        0x12, 0x22, 0x1A,
        'H', 'e', 'l', 'l', 'o', ' ', 'a', 'l', 'l', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'B');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "Hello all", 9);
}

/* ss9.5 example 1: Ping specific BID "4T9X2" */

static void test_spec_ping_specific() {
    auto f = bus::ping("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x50, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'P');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.5 example 3: Ping broadcast (no BID) */

static void test_spec_ping_broadcast() {
    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x50, 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 1);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'P');
}

/* ss9.6: Discover request -- BID "4T9X2" */

static void test_spec_discover_request() {
    auto f = bus::discover("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x44, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'D');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.6: Discover response -- "4T9X2" via "7M3K9" */

static void test_spec_discover_response() {
    auto f = bus::discover_response("4T9X2", "7M3K9");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x44, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '7', 'M', '3', 'K', '9', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'D');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[2], "7M3K9", 5);
}

/* ss9.7: Verify request -- BID "4T9X2" */

static void test_spec_verify_request() {
    auto f = bus::verify("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x56, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'V');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.7: Verify response -- OID="langsyn" DID="Thermostat" IID="SN00482" */

static void test_spec_verify_response() {
    auto f = bus::verify_response("langsyn", "Thermostat", "SN00482");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x56, 0x10,
        0x12, 0x40, 0x1A,
        'l', 'a', 'n', 'g', 's', 'y', 'n', 0x10,
        0x12, 0x40, 0x1A,
        'T', 'h', 'e', 'r', 'm', 'o', 's', 't', 'a', 't', 0x10,
        0x12, 0x40, 0x1A,
        'S', 'N', '0', '0', '4', '8', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'V');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 0);
    ASSERT_MEM_EQ(g_conf.bodies[1], "langsyn", 7);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 0);
    ASSERT_MEM_EQ(g_conf.bodies[2], "Thermostat", 10);
    ASSERT_EQ(g_conf.types[3], '@');
    ASSERT_EQ(g_conf.radixes[3], 0);
    ASSERT_MEM_EQ(g_conf.bodies[3], "SN00482", 7);
}

/* ss9.8: Acknowledge BID "4T9X2" */

static void test_spec_acknowledge() {
    auto f = bus::acknowledge("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x57, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'W');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ss9.9: Exception "BID_OVERFLOW" */

static void test_spec_exception() {
    auto f = bus::exception("BID_OVERFLOW");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x58, 0x10,
        0x12, 0x22, 0x1A,
        'B', 'I', 'D', '_', 'O', 'V', 'E', 'R', 'F', 'L', 'O', 'W', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'X');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "BID_OVERFLOW", 12);
}

/* ================================================================
 * ss10 Service Operations -- 3 wire examples
 * ================================================================ */

/* ss10.1: Query "temperature" */

static void test_spec_query() {
    auto f = service::query("temperature");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x51, 0x10,
        0x12, 0x22, 0x1A,
        't', 'e', 'm', 'p', 'e', 'r', 'a', 't', 'u', 'r', 'e', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'Q');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "temperature", 11);
}

/* ss10.2: Offer BID="4T9X2" desc="temperature_celsius" */

static void test_spec_offer() {
    auto f = service::offer("4T9X2", "temperature_celsius");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4F, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x12, 0x22, 0x1A,
        't', 'e', 'm', 'p', 'e', 'r', 'a', 't', 'u', 'r', 'e',
        '_', 'c', 'e', 'l', 's', 'i', 'u', 's', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'O');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
    ASSERT_EQ(g_conf.types[2], '"');
    ASSERT_MEM_EQ(g_conf.bodies[2], "temperature_celsius", 19);
}

/* ss10.3: Accept BID="4T9X2" */

static void test_spec_accept() {
    auto f = service::accept("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x41, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'A');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "4T9X2", 5);
}

/* ================================================================
 * ss11 Session Operations -- wire examples
 * ================================================================ */

/* ss11.3: Call SID="A7K2M" MID=1 "get_reading" */

static void test_spec_session_call() {
    auto f = session::call("A7K2M", "", 1, "get_reading");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '1', 0x10,
        0x12, 0x22, 0x1A,
        'g', 'e', 't', '_', 'r', 'e', 'a', 'd', 'i', 'n', 'g', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'K');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "1", 1);
    ASSERT_EQ(g_conf.types[3], '"');
    ASSERT_MEM_EQ(g_conf.bodies[3], "get_reading", 11);
}

/* ss11.3: Call response SID="A7K2M" MID=2 "23.5" */

static void test_spec_session_call_response() {
    auto f = session::call("A7K2M", "", 2, "23.5");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '2', 0x10,
        0x12, 0x22, 0x1A,
        '2', '3', '.', '5', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'K');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "2", 1);
    ASSERT_EQ(g_conf.types[3], '"');
    ASSERT_MEM_EQ(g_conf.bodies[3], "23.5", 4);
}

/* ss11.4: Status request SID="A7K2M" MID=2 */

static void test_spec_session_status_request() {
    auto f = session::status("A7K2M", "", 2);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x54, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'T');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "2", 1);
}

/* ss11.4: Status response SID="A7K2M" MID=3 "ACTIVE" */

static void test_spec_session_status_response() {
    auto f = session::status_response("A7K2M", "", 3, "ACTIVE");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x54, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '3', 0x10,
        0x12, 0x22, 0x1A,
        'A', 'C', 'T', 'I', 'V', 'E', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'T');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "3", 1);
    ASSERT_EQ(g_conf.types[3], '"');
    ASSERT_MEM_EQ(g_conf.bodies[3], "ACTIVE", 6);
}

/* ss11.5: Notify SID="A7K2M" MID=3 "threshold_exceeded" */

static void test_spec_session_notify() {
    auto f = session::notify("A7K2M", "", 3, "threshold_exceeded");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4E, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '3', 0x10,
        0x12, 0x22, 0x1A,
        't', 'h', 'r', 'e', 's', 'h', 'o', 'l', 'd',
        '_', 'e', 'x', 'c', 'e', 'e', 'd', 'e', 'd', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'N');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "3", 1);
    ASSERT_EQ(g_conf.types[3], '"');
    ASSERT_MEM_EQ(g_conf.bodies[3], "threshold_exceeded", 18);
}

/* ss11.6: Locate request SID="A7K2M" */

static void test_spec_session_locate_request() {
    auto f = session::locate("A7K2M");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4C, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'L');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
}

/* ss11.6: Locate response SID="A7K2M" at BID="4T9X2" */

static void test_spec_session_locate_response() {
    auto f = session::locate_response("A7K2M", "4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4C, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4', 'T', '9', 'X', '2', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 3);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'L');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[2], "4T9X2", 5);
}

/* ss11.7: Resume request SID="A7K2M" */

static void test_spec_session_resume() {
    auto f = session::resume("A7K2M");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x55, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'U');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
}

/* ss11.7: Resume success -- Status response SID="A7K2M" MID=47 "ACTIVE" */

static void test_spec_session_resume_success() {
    auto f = session::status_response("A7K2M", "", 47, "ACTIVE");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x54, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '4', '7', 0x10,
        0x12, 0x22, 0x1A,
        'A', 'C', 'T', 'I', 'V', 'E', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 4);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'T');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
    ASSERT_EQ(g_conf.types[2], '@');
    ASSERT_EQ(g_conf.radixes[2], 'D');
    ASSERT_MEM_EQ(g_conf.bodies[2], "47", 2);
    ASSERT_EQ(g_conf.types[3], '"');
    ASSERT_MEM_EQ(g_conf.bodies[3], "ACTIVE", 6);
}

/* ss11.7: Resume failure -- Exception "SESSION_EXPIRED" */

static void test_spec_session_resume_failure() {
    auto f = bus::exception("SESSION_EXPIRED");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x58, 0x10,
        0x12, 0x22, 0x1A,
        'S', 'E', 'S', 'S', 'I', 'O', 'N', '_',
        'E', 'X', 'P', 'I', 'R', 'E', 'D', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'X');
    ASSERT_EQ(g_conf.types[1], '"');
    ASSERT_MEM_EQ(g_conf.bodies[1], "SESSION_EXPIRED", 15);
}

/* ss11.8: Finish SID="A7K2M" */

static void test_spec_session_finish() {
    auto f = session::finish("A7K2M");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x46, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A', '7', 'K', '2', 'M', 0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));

    conf_parse_reset();
    conf_parse_bytes(f->data(), f->size());
    ASSERT_EQ(g_conf.word_count, 2);
    ASSERT_EQ(g_conf.msg_count, 1);
    ASSERT_EQ(g_conf.types[0], '!');
    ASSERT_EQ(g_conf.bodies[0][0], 'F');
    ASSERT_EQ(g_conf.types[1], '@');
    ASSERT_EQ(g_conf.radixes[1], 'U');
    ASSERT_MEM_EQ(g_conf.bodies[1], "A7K2M", 5);
}

/* ================================================================
 * Suite entry point
 * ================================================================ */

void test_conformance_run(int& out_run, int& out_passed) {
    std::printf("\n[conformance -- v9 spec wire examples]\n");

    /* ss6.5 Word Encoding */
    TEST(test_spec_word_symbol_E);
    TEST(test_spec_word_id_oid);
    TEST(test_spec_word_id_bid);
    TEST(test_spec_word_integer_decimal_byte);

    /* ss9.1 BID Establishment */
    TEST(test_spec_establish_bid);
    TEST(test_spec_conflict_bid);
    TEST(test_spec_establish_failed);

    /* ss9.2 Scaleback */
    TEST(test_spec_scaleback_full);
    TEST(test_spec_scaleback_sizes_only);
    TEST(test_spec_scaleback_flags_only);

    /* ss9.3 Path Addressing */
    TEST(test_spec_broadcast_path_origin);
    TEST(test_spec_relay_d_to_a);

    /* ss9.4-9.9 Bus Verbs */
    TEST(test_spec_broadcast_simple);
    TEST(test_spec_ping_specific);
    TEST(test_spec_ping_broadcast);
    TEST(test_spec_discover_request);
    TEST(test_spec_discover_response);
    TEST(test_spec_verify_request);
    TEST(test_spec_verify_response);
    TEST(test_spec_acknowledge);
    TEST(test_spec_exception);

    /* ss10 Service Operations */
    TEST(test_spec_query);
    TEST(test_spec_offer);
    TEST(test_spec_accept);

    /* ss11 Session Operations */
    TEST(test_spec_session_call);
    TEST(test_spec_session_call_response);
    TEST(test_spec_session_status_request);
    TEST(test_spec_session_status_response);
    TEST(test_spec_session_notify);
    TEST(test_spec_session_locate_request);
    TEST(test_spec_session_locate_response);
    TEST(test_spec_session_resume);
    TEST(test_spec_session_resume_success);
    TEST(test_spec_session_resume_failure);
    TEST(test_spec_session_finish);

    out_run    = tests_run;
    out_passed = tests_passed;
}
