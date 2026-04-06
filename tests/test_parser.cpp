/*
 * test_parser.cpp — Tests for antheos::Parser (stream parser)
 *
 * Verified against Antheos Protocol v9 specification.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>

using namespace antheos;
using namespace antheos::wire;

/* ── Capture state ── */

static struct {
    int word_count;
    int msg_count;
    WordType types[16];
    Radix radixes[16];
    Unit units[16];
    char bodies[16][256];
    size_t body_lens[16];
    size_t msg_word_counts[8];
} g_parse;

static Parser g_parser;

static void capture_reset() {
    std::memset(&g_parse, 0, sizeof(g_parse));
    g_parser = Parser();
    g_parser.on_word([](WordType type, Radix radix, Unit unit,
                        const uint8_t* body, size_t body_len) {
        int i = g_parse.word_count;
        if (i < 16) {
            g_parse.types[i] = type;
            g_parse.radixes[i] = radix;
            g_parse.units[i] = unit;
            if (body_len > 255) body_len = 255;
            std::memcpy(g_parse.bodies[i], body, body_len);
            g_parse.bodies[i][body_len] = '\0';
            g_parse.body_lens[i] = body_len;
        }
        g_parse.word_count++;
    });
    g_parser.on_message([](size_t word_count) {
        int i = g_parse.msg_count;
        if (i < 8)
            g_parse.msg_word_counts[i] = word_count;
        g_parse.msg_count++;
    });
}

static void feed(const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++)
        g_parser.feed(bytes[i]);
}

/* ── Simple symbol: [SOM][SOW]![SOB]E[EOW][EOM] ── */

static void test_parse_symbol() {
    capture_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x21, 0x1A, 0x45, 0x10, 0x03};
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '!');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[0]), 0);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.units[0]), 0);
    ASSERT_EQ(static_cast<int>(g_parse.body_lens[0]), 1);
    ASSERT_EQ(g_parse.bodies[0][0], 'E');
}

/* ── Text word: [SOM][SOW]"[SOB]Hello[EOW][EOM] ── */

static void test_parse_text() {
    capture_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x22, 0x1A,
                              'H','e','l','l','o',
                              0x10, 0x03};
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '"');
    ASSERT_EQ(static_cast<int>(g_parse.body_lens[0]), 5);
    ASSERT_MEM_EQ(g_parse.bodies[0], "Hello", 5);
}

/* ── ID with base32 radix: [SOW]@[SOR]U[SOB]4T9X2[EOW] ── */

static void test_parse_id_base32() {
    capture_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x40, 0x04, 0x55, 0x1A,
                              '4','T','9','X','2',
                              0x10, 0x03};
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '@');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[0]), 'U');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.units[0]), 0);
    ASSERT_EQ(static_cast<int>(g_parse.body_lens[0]), 5);
    ASSERT_MEM_EQ(g_parse.bodies[0], "4T9X2", 5);
}

/* ── ID with decimal radix (MID): [SOW]@[SOR]D[SOB]1[EOW] ── */

static void test_parse_id_decimal() {
    capture_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x40, 0x04, 0x44, 0x1A,
                              '1',
                              0x10, 0x03};
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '@');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[0]), 'D');
    ASSERT_EQ(static_cast<int>(g_parse.body_lens[0]), 1);
    ASSERT_EQ(g_parse.bodies[0][0], '1');
}

/* ── INTEGER with radix + unit: [SOW]#[SOR]D[SOU]B[SOB]32[EOW] ── */

static void test_parse_integer_with_flags() {
    capture_reset();
    const uint8_t frame[] = {0x02, 0x12, 0x23, 0x04, 0x44, 0x07, 0x42, 0x1A,
                              '3','2',
                              0x10, 0x03};
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '#');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[0]), 'D');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.units[0]), 'B');
    ASSERT_EQ(static_cast<int>(g_parse.body_lens[0]), 2);
    ASSERT_MEM_EQ(g_parse.bodies[0], "32", 2);
}

/* ── Full Establish frame: [SOM] !E @U 4T9X2 [EOM] — 2 words ── */

static void test_parse_establish() {
    capture_reset();
    const uint8_t frame[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x45, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4','T','9','X','2', 0x10,
        0x03
    };
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 2);
    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[0]), '!');
    ASSERT_EQ(g_parse.bodies[0][0], 'E');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[1]), '@');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[1]), 'U');
    ASSERT_MEM_EQ(g_parse.bodies[1], "4T9X2", 5);
}

/* ── Session Call frame: 4 words (!K, @U A7K2M, @D 1, "get_reading) ── */

static void test_parse_session_call() {
    capture_reset();
    const uint8_t frame[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A','7','K','2','M', 0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '1', 0x10,
        0x12, 0x22, 0x1A,
        'g','e','t','_','r','e','a','d','i','n','g',
        0x10,
        0x03
    };
    feed(frame, sizeof(frame));

    ASSERT_EQ(g_parse.word_count, 4);
    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(g_parse.bodies[0][0], 'K');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[1]), 'U');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.radixes[2]), 'D');
    ASSERT_EQ(g_parse.bodies[2][0], '1');
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[3]), '"');
    ASSERT_MEM_EQ(g_parse.bodies[3], "get_reading", 11);
}

/* ── Garbage bytes before SOM are discarded ── */

static void test_parse_garbage_before_som() {
    capture_reset();
    const uint8_t stream[] = {0xFF, 0xAB, 0x00, 0x7F,
                               0x02, 0x12, 0x21, 0x1A, 0x45, 0x10, 0x03};
    feed(stream, sizeof(stream));

    ASSERT_EQ(g_parse.word_count, 1);
    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(g_parse.bodies[0][0], 'E');
}

/* ── Two back-to-back complete frames ── */

static void test_parse_two_messages() {
    capture_reset();
    const uint8_t two[] = {
        0x02, 0x12, 0x21, 0x1A, 0x45, 0x10, 0x03,
        0x02, 0x12, 0x21, 0x1A, 0x50, 0x10, 0x03
    };
    feed(two, sizeof(two));

    ASSERT_EQ(g_parse.word_count, 2);
    ASSERT_EQ(g_parse.msg_count, 2);
    ASSERT_EQ(g_parse.bodies[0][0], 'E');
    ASSERT_EQ(g_parse.bodies[1][0], 'P');
}

/* ── Reserved byte mid-body → ERROR; parser recovers on next SOM ── */

static void test_parse_som_resets() {
    capture_reset();
    const uint8_t stream[] = {
        0x02, 0x12, 0x21, 0x1A,
        0x02,
        0x12, 0x21,
        0x02, 0x12, 0x21, 0x1A, 0x43, 0x10, 0x03
    };
    feed(stream, sizeof(stream));

    ASSERT_EQ(g_parse.msg_count, 1);
    ASSERT_EQ(static_cast<uint8_t>(g_parse.types[g_parse.word_count - 1]), '!');
    ASSERT_EQ(g_parse.bodies[g_parse.word_count - 1][0], 'C');
}

/* ── Initial parser state is WaitSom ── */

static void test_parser_state_initial() {
    capture_reset();
    ASSERT_EQ(static_cast<int>(g_parser.state()), static_cast<int>(ParseState::WaitSom));
}

/* ── reset() returns to WaitSom mid-parse ── */

static void test_parser_reset() {
    capture_reset();
    g_parser.feed(static_cast<uint8_t>(0x02));
    g_parser.feed(static_cast<uint8_t>(0x12));
    ASSERT_TRUE(g_parser.state() != ParseState::WaitSom);

    g_parser.reset();
    ASSERT_EQ(static_cast<int>(g_parser.state()), static_cast<int>(ParseState::WaitSom));
}

/* ── Suite entry point ── */

void test_parser_run(int& out_run, int& out_passed) {
    std::printf("\n[parser]\n");
    TEST(test_parse_symbol);
    TEST(test_parse_text);
    TEST(test_parse_id_base32);
    TEST(test_parse_id_decimal);
    TEST(test_parse_integer_with_flags);
    TEST(test_parse_establish);
    TEST(test_parse_session_call);
    TEST(test_parse_garbage_before_som);
    TEST(test_parse_two_messages);
    TEST(test_parse_som_resets);
    TEST(test_parser_state_initial);
    TEST(test_parser_reset);
    out_run    = tests_run;
    out_passed = tests_passed;
}
