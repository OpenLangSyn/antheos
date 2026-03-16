/*
 * test_wire.cpp — Tests for wire encoding (antheos::wire)
 *
 * Verified against Antheos Protocol v9 specification.
 */

#include "test_common.hpp"
#include "antheos.hpp"

using namespace antheos;
using namespace antheos::wire;

/* ── Control character values (v9 §4.1) ── */

static void test_control_char_values() {
    ASSERT_EQ(SOM, 0x02);
    ASSERT_EQ(EOM, 0x03);
    ASSERT_EQ(SOR, 0x04);
    ASSERT_EQ(SOU, 0x07);
    ASSERT_EQ(EOW, 0x10);
    ASSERT_EQ(SOW, 0x12);
    ASSERT_EQ(SOB, 0x1A);
}

/* ── Word type values (v9 §6.2) — ASCII printable ── */

static void test_word_type_values() {
    ASSERT_EQ(static_cast<uint8_t>(WordType::Symbol),     '!');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Text),       '"');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Integer),    '#');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Real),       '$');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Scientific), '%');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Timestamp),  '&');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Blob),       '*');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Path),       '/');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Logical),    '?');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Id),         '@');
    ASSERT_EQ(static_cast<uint8_t>(WordType::Message),    '~');
}

/* ── Radix flag values (v9 §6.3) ── */

static void test_radix_flag_values() {
    ASSERT_EQ(static_cast<uint8_t>(Radix::Binary),  'I');
    ASSERT_EQ(static_cast<uint8_t>(Radix::Octal),   'O');
    ASSERT_EQ(static_cast<uint8_t>(Radix::Decimal), 'D');
    ASSERT_EQ(static_cast<uint8_t>(Radix::Hex),     'H');
    ASSERT_EQ(static_cast<uint8_t>(Radix::Base32),  'U');
}

/* ── Unit flag values (v9 §6.4) ── */

static void test_unit_flag_values() {
    ASSERT_EQ(static_cast<uint8_t>(Unit::Byte),     'B');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Word),     'W');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Dword),    'D');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Qword),    'Q');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Megabyte), 'M');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Gigabyte), 'G');
    ASSERT_EQ(static_cast<uint8_t>(Unit::Terabyte), 'T');
}

/* ── is_reserved ── */

static void test_is_reserved() {
    ASSERT_TRUE(is_reserved(SOM));
    ASSERT_TRUE(is_reserved(EOM));
    ASSERT_TRUE(is_reserved(SOR));
    ASSERT_TRUE(is_reserved(SOU));
    ASSERT_TRUE(is_reserved(EOW));
    ASSERT_TRUE(is_reserved(SOW));
    ASSERT_TRUE(is_reserved(SOB));
    ASSERT_TRUE(!is_reserved('A'));
    ASSERT_TRUE(!is_reserved(0x41));
    ASSERT_TRUE(!is_reserved(0x00));
}

/* ── is_valid_word_type ── */

static void test_is_valid_word_type() {
    ASSERT_TRUE(is_valid_word_type('!'));
    ASSERT_TRUE(is_valid_word_type('"'));
    ASSERT_TRUE(is_valid_word_type('#'));
    ASSERT_TRUE(is_valid_word_type('$'));
    ASSERT_TRUE(is_valid_word_type('%'));
    ASSERT_TRUE(is_valid_word_type('&'));
    ASSERT_TRUE(is_valid_word_type('*'));
    ASSERT_TRUE(is_valid_word_type('/'));
    ASSERT_TRUE(is_valid_word_type('?'));
    ASSERT_TRUE(is_valid_word_type('@'));
    ASSERT_TRUE(is_valid_word_type('~'));
    ASSERT_TRUE(!is_valid_word_type('A'));
    ASSERT_TRUE(!is_valid_word_type(0x00));
    ASSERT_TRUE(!is_valid_word_type(0x02));
}

/* ── is_radix_flag ── */

static void test_is_radix_flag() {
    ASSERT_TRUE(is_radix_flag('I'));
    ASSERT_TRUE(is_radix_flag('O'));
    ASSERT_TRUE(is_radix_flag('D'));
    ASSERT_TRUE(is_radix_flag('H'));
    ASSERT_TRUE(is_radix_flag('U'));
    ASSERT_TRUE(!is_radix_flag('A'));
    ASSERT_TRUE(!is_radix_flag('B'));
    ASSERT_TRUE(!is_radix_flag(0x00));
}

/* ── is_unit_flag ── */

static void test_is_unit_flag() {
    ASSERT_TRUE(is_unit_flag('B'));
    ASSERT_TRUE(is_unit_flag('W'));
    ASSERT_TRUE(is_unit_flag('D'));
    ASSERT_TRUE(is_unit_flag('Q'));
    ASSERT_TRUE(is_unit_flag('M'));
    ASSERT_TRUE(is_unit_flag('G'));
    ASSERT_TRUE(is_unit_flag('T'));
    ASSERT_TRUE(!is_unit_flag('A'));
    ASSERT_TRUE(!is_unit_flag('I'));
    ASSERT_TRUE(!is_unit_flag(0x00));
}

/* ── Flag requirement rules (v9 §6.1) ── */

static void test_word_flag_rules() {
    ASSERT_TRUE(needs_radix(WordType::Integer));
    ASSERT_TRUE(needs_unit(WordType::Integer));
    ASSERT_TRUE(allows_radix(WordType::Integer));

    ASSERT_TRUE(!needs_radix(WordType::Id));
    ASSERT_TRUE(!needs_unit(WordType::Id));
    ASSERT_TRUE(allows_radix(WordType::Id));

    ASSERT_TRUE(!needs_radix(WordType::Text));
    ASSERT_TRUE(!needs_unit(WordType::Text));
    ASSERT_TRUE(!allows_radix(WordType::Text));

    ASSERT_TRUE(!needs_radix(WordType::Symbol));
    ASSERT_TRUE(!needs_unit(WordType::Symbol));
    ASSERT_TRUE(!allows_radix(WordType::Symbol));

    ASSERT_TRUE(needs_radix(WordType::Blob));
    ASSERT_TRUE(needs_unit(WordType::Blob));
}

/* ── encode_symbol: [SOW]![SOB]E[EOW] ──
 * Wire: 0x12 0x21 0x1A 0x45 0x10  (5 bytes)
 */

static void test_encode_symbol() {
    auto result = encode_symbol('E');
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x21, 0x1A, 0x45, 0x10};
    ASSERT_EQ(result->size(), 5u);
    ASSERT_MEM_EQ(result->data(), expected, 5);
}

/* ── encode_text: [SOW]"[SOB]Hello[EOW] ──
 * Wire: 0x12 0x22 0x1A H e l l o 0x10  (9 bytes)
 */

static void test_encode_text() {
    auto result = encode_text("Hello");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x22, 0x1A,
                                0x48, 0x65, 0x6C, 0x6C, 0x6F,
                                0x10};
    ASSERT_EQ(result->size(), 9u);
    ASSERT_MEM_EQ(result->data(), expected, 9);
}

/* ── encode_id_plain: [SOW]@[SOB]langsyn[EOW] ──
 * Wire: 0x12 0x40 0x1A l a n g s y n 0x10  (11 bytes)
 */

static void test_encode_id_plain() {
    auto result = encode_id_plain("langsyn");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x40, 0x1A,
                                'l','a','n','g','s','y','n',
                                0x10};
    ASSERT_EQ(result->size(), 11u);
    ASSERT_MEM_EQ(result->data(), expected, 11);
}

/* ── encode_id_base32: [SOW]@[SOR]U[SOB]4T9X2[EOW] ──
 * Wire: 0x12 0x40 0x04 0x55 0x1A 4 T 9 X 2 0x10  (11 bytes)
 */

static void test_encode_id_base32() {
    auto result = encode_id_base32("4T9X2");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x40, 0x04, 0x55, 0x1A,
                                '4','T','9','X','2',
                                0x10};
    ASSERT_EQ(result->size(), 11u);
    ASSERT_MEM_EQ(result->data(), expected, 11);
}

/* ── encode_id_decimal: [SOW]@[SOR]D[SOB]1[EOW] ──
 * Wire: 0x12 0x40 0x04 0x44 0x1A 0x31 0x10  (7 bytes)
 */

static void test_encode_id_decimal() {
    auto result = encode_id_decimal(1);
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x40, 0x04, 0x44, 0x1A, 0x31, 0x10};
    ASSERT_EQ(result->size(), 7u);
    ASSERT_MEM_EQ(result->data(), expected, 7);
}

/* ── encode_integer: [SOW]#[SOR]D[SOU]B[SOB]32[EOW] ──
 * Wire: 0x12 0x23 0x04 0x44 0x07 0x42 0x1A 3 2 0x10  (10 bytes)
 */

static void test_encode_integer() {
    auto result = encode_integer(Radix::Decimal, Unit::Byte, "32");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x23, 0x04, 0x44, 0x07, 0x42, 0x1A,
                                '3','2',
                                0x10};
    ASSERT_EQ(result->size(), 10u);
    ASSERT_MEM_EQ(result->data(), expected, 10);
}

/* ── encode_logical: [SOW]?[SOB]x>0[EOW] ──
 * Wire: 0x12 0x3F 0x1A x > 0 0x10  (7 bytes)
 */

static void test_encode_logical() {
    auto result = encode_logical("x>0");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x3F, 0x1A, 'x','>','0', 0x10};
    ASSERT_EQ(result->size(), 7u);
    ASSERT_MEM_EQ(result->data(), expected, 7);
}

/* ── encode_path: [SOW]/[SOB]a/b/c[EOW] ──
 * Wire: 0x12 0x2F 0x1A a / b / c 0x10  (9 bytes)
 */

static void test_encode_path() {
    auto result = encode_path("a/b/c");
    ASSERT_TRUE(result.has_value());
    const uint8_t expected[] = {0x12, 0x2F, 0x1A, 'a','/','b','/','c', 0x10};
    ASSERT_EQ(result->size(), 9u);
    ASSERT_MEM_EQ(result->data(), expected, 9);
}

/* ── word_encode / word_decode round-trip — no flags ── */

static void test_word_encode_decode_roundtrip() {
    const uint8_t body_in[] = {'H', 'i'};
    auto wire = word_encode(WordType::Text, Radix::None, Unit::None,
                            body_in, 2);
    ASSERT_TRUE(wire.has_value());
    ASSERT_EQ(wire->size(), 6u);

    auto dw = word_decode(wire->data(), wire->size());
    ASSERT_TRUE(dw.has_value());
    ASSERT_EQ(static_cast<uint8_t>(dw->type), static_cast<uint8_t>(WordType::Text));
    ASSERT_EQ(static_cast<uint8_t>(dw->radix), 0);
    ASSERT_EQ(static_cast<uint8_t>(dw->unit), 0);
    ASSERT_EQ(dw->body.size(), 2u);
    ASSERT_MEM_EQ(dw->body.data(), body_in, 2);
}

/* ── word_encode / word_decode round-trip — radix + unit ── */

static void test_word_encode_decode_with_flags() {
    const uint8_t body_in[] = {'3','.','1','4'};
    auto wire = word_encode(WordType::Real, Radix::Decimal, Unit::Qword,
                            body_in, 4);
    ASSERT_TRUE(wire.has_value());
    ASSERT_EQ(wire->size(), 12u);

    auto dw = word_decode(wire->data(), wire->size());
    ASSERT_TRUE(dw.has_value());
    ASSERT_EQ(static_cast<uint8_t>(dw->type), static_cast<uint8_t>(WordType::Real));
    ASSERT_EQ(static_cast<uint8_t>(dw->radix), 'D');
    ASSERT_EQ(static_cast<uint8_t>(dw->unit), 'Q');
    ASSERT_EQ(dw->body.size(), 4u);
    ASSERT_MEM_EQ(dw->body.data(), body_in, 4);
}

/* ── Reserved byte in body rejected ── */

static void test_encode_reserved_in_body() {
    uint8_t body[] = {'A', SOM, 'B'};
    auto result = word_encode(WordType::Text, Radix::None, Unit::None,
                              body, 3);
    ASSERT_TRUE(!result.has_value());
}

/* ── Invalid word type rejected ── */

static void test_encode_invalid_word_type() {
    uint8_t body[] = {'X'};
    auto result = word_encode(static_cast<WordType>(0x99), Radix::None, Unit::None,
                              body, 1);
    ASSERT_TRUE(!result.has_value());
}

/* ── Radix not allowed on TEXT ── */

static void test_encode_radix_on_text() {
    uint8_t body[] = {'A'};
    auto result = word_encode(WordType::Text, Radix::Decimal, Unit::None,
                              body, 1);
    ASSERT_TRUE(!result.has_value());
}

/* ── Unit not allowed on ID ── */

static void test_encode_unit_on_id() {
    uint8_t body[] = {'A'};
    auto result = word_encode(WordType::Id, Radix::None, Unit::Byte,
                              body, 1);
    ASSERT_TRUE(!result.has_value());
}

/* ── Decode with truncated input ── */

static void test_decode_truncated() {
    uint8_t wire[] = {SOW, '"'};
    auto dw = word_decode(wire, 2);
    ASSERT_TRUE(!dw.has_value());
}

/* ── Suite entry point ── */

void test_wire_run(int& out_run, int& out_passed) {
    std::printf("\n[wire]\n");
    TEST(test_control_char_values);
    TEST(test_word_type_values);
    TEST(test_radix_flag_values);
    TEST(test_unit_flag_values);
    TEST(test_is_reserved);
    TEST(test_is_valid_word_type);
    TEST(test_is_radix_flag);
    TEST(test_is_unit_flag);
    TEST(test_word_flag_rules);
    TEST(test_encode_symbol);
    TEST(test_encode_text);
    TEST(test_encode_id_plain);
    TEST(test_encode_id_base32);
    TEST(test_encode_id_decimal);
    TEST(test_encode_integer);
    TEST(test_encode_logical);
    TEST(test_encode_path);
    TEST(test_word_encode_decode_roundtrip);
    TEST(test_word_encode_decode_with_flags);
    TEST(test_encode_reserved_in_body);
    TEST(test_encode_invalid_word_type);
    TEST(test_encode_radix_on_text);
    TEST(test_encode_unit_on_id);
    TEST(test_decode_truncated);
    out_run    = tests_run;
    out_passed = tests_passed;
}
