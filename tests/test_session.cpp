/*
 * test_session.cpp — Tests for antheos::session verb builders
 *
 * Verifies exact v9 wire format for every session verb.
 * Wire format reference: Antheos Protocol v9 §8.3, §11.
 *
 * SID encoded as ID word radix U (base-32).
 * MID encoded as ID word radix D (decimal). NOT integer.
 * MID=0 + no payload is the wrap signal (v9 §11.1.2) — use call_wrap().
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>

using namespace antheos;

static constexpr const char* SID = "A7K2M";
static constexpr const char* BID = "4T9X2";

/* ── Call: SOM [!K] [@U A7K2M] [@D 1] ["get_reading] EOM ── */

static void test_session_call() {
    auto f = session::call(SID, {}, 1, "get_reading");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,              /* [!K]           */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,         /* [@U A7K2M]     */
        0x12, 0x40, 0x04, 0x44, 0x1A, '1', 0x10,   /* [@D 1]         */
        0x12, 0x22, 0x1A,                           /* ["             */
        'g',  'e',  't',  '_',
        'r',  'e',  'a',  'd',  'i',  'n',  'g',
        0x10,                                       /*   get_reading]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Call response: same verb K, different MID and payload ── */

static void test_session_call_response() {
    auto f = session::call(SID, {}, 2, "23.5");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,              /* [!K]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,         /* [@U A7K2M]  */
        0x12, 0x40, 0x04, 0x44, 0x1A, '2', 0x10,   /* [@D 2]      */
        0x12, 0x22, 0x1A,
        '2',  '3',  '.',  '5',  0x10,               /* ["23.5]     */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Call with target BID: SOM [!K] [@U A7K2M] [@U 4T9X2] [@D 1] ["x] EOM ── */

static void test_session_call_target() {
    auto f = session::call(SID, BID, 1, "x");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,              /* [!K]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,         /* [@U A7K2M]  */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,         /* [@U 4T9X2]  */
        0x12, 0x40, 0x04, 0x44, 0x1A, '1', 0x10,   /* [@D 1]      */
        0x12, 0x22, 0x1A, 'x', 0x10,               /* ["x]        */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Status request: SOM [!T] [@U A7K2M] [@D 2] EOM ── */

static void test_session_status() {
    auto f = session::status(SID, {}, 2);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x54, 0x10,              /* [!T]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '2', 0x10,   /* [@D 2]      */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Status response: SOM [!T] [@U A7K2M] [@D 3] ["ACTIVE] EOM ── */

static void test_session_status_response() {
    auto f = session::status_response(SID, {}, 3, "ACTIVE");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x54, 0x10,              /* [!T]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '3', 0x10,   /* [@D 3]      */
        0x12, 0x22, 0x1A,
        'A',  'C',  'T',  'I',  'V',  'E',  0x10,  /* ["ACTIVE]   */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Notify: SOM [!N] [@U A7K2M] [@D 3] ["threshold_exceeded] EOM ── */

static void test_session_notify() {
    auto f = session::notify(SID, {}, 3, "threshold_exceeded");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4E, 0x10,              /* [!N]                    */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '3', 0x10,   /* [@D 3]                  */
        0x12, 0x22, 0x1A,
        't',  'h',  'r',  'e',  's',  'h',  'o',
        'l',  'd',  '_',  'e',  'x',  'c',  'e',
        'e',  'd',  'e',  'd',  0x10,               /* ["threshold_exceeded]   */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Locate request: SOM [!L] [@U A7K2M] EOM ── */

static void test_session_locate() {
    auto f = session::locate(SID);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4C, 0x10,              /* [!L]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Locate response: SOM [!L] [@U A7K2M] [@U 4T9X2] EOM ── */

static void test_session_locate_response() {
    auto f = session::locate_response(SID, BID);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4C, 0x10,              /* [!L]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,         /* [@U A7K2M]  */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,         /* [@U 4T9X2]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Resume: SOM [!U] [@U A7K2M] EOM ── */

static void test_session_resume() {
    auto f = session::resume(SID);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x55, 0x10,              /* [!U]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Finish: SOM [!F] [@U A7K2M] EOM ── */

static void test_session_finish() {
    auto f = session::finish(SID);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x46, 0x10,              /* [!F]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Finish with target BID: SOM [!F] [@U A7K2M] [@U 4T9X2] EOM ── */

static void test_session_finish_target() {
    auto f = session::finish(SID, BID);
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x46, 0x10,              /* [!F]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,         /* [@U A7K2M]  */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,         /* [@U 4T9X2]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── MID wrap: call_wrap → SOM [!K] [@U A7K2M] [@D 0] EOM
 *
 * Dedicated MID wrap signal per v9 §11.1.2. No payload word. ── */

static void test_session_mid_wrap() {
    auto f = session::call_wrap(SID, {});
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4B, 0x10,              /* [!K]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        'A',  '7',  'K',  '2',  'M',  0x10,
        0x12, 0x40, 0x04, 0x44, 0x1A, '0', 0x10,   /* [@D 0]      */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── MID sequencing: verify mid=1, 2, 47 encode as decimal strings ──
 *
 * Single-digit MIDs produce a 7-byte @D word; "47" (two digits) is 8 bytes.
 * Each additional decimal digit adds one byte to the @D word. ── */

static void test_session_mid_sequencing() {
    auto f1  = session::call(SID, {}, 1,  "x");
    auto f2  = session::call(SID, {}, 2,  "x");
    auto f47 = session::call(SID, {}, 47, "x");

    ASSERT_TRUE(f1.has_value());
    ASSERT_TRUE(f2.has_value());
    ASSERT_TRUE(f47.has_value());

    /* mid=1 and mid=2 are both single digits → same frame size */
    ASSERT_EQ(f1->size(), f2->size());

    /* mid=47 is two digits → one byte larger than mid=1 */
    ASSERT_EQ(f47->size(), f1->size() + 1);

    /* Spot-check mid=47: the @D word body should contain "47"
     * @D word is at offset 17 in the frame (after SOM + !K + @U A7K2M):
     *   SOW @ SOR D SOB 4 7 EOW
     *   [17] [18][19][20][21][22][23][24]
     */
    ASSERT_EQ(f47->data()[17], wire::SOW);
    ASSERT_EQ(f47->data()[18], 0x40);       /* '@' */
    ASSERT_EQ(f47->data()[19], wire::SOR);
    ASSERT_EQ(f47->data()[20], 0x44);       /* 'D' */
    ASSERT_EQ(f47->data()[21], wire::SOB);
    ASSERT_EQ(f47->data()[22], '4');
    ASSERT_EQ(f47->data()[23], '7');
    ASSERT_EQ(f47->data()[24], wire::EOW);
}

/* ── Empty SID: all verbs return nullopt ── */

static void test_session_empty_sid() {
    ASSERT_TRUE(!session::call("", {}, 1, "x").has_value());
    ASSERT_TRUE(!session::call_wrap("", {}).has_value());
    ASSERT_TRUE(!session::status("", {}, 1).has_value());
    ASSERT_TRUE(!session::status_response("", {}, 1, "OK").has_value());
    ASSERT_TRUE(!session::notify("", {}, 1, "ev").has_value());
    ASSERT_TRUE(!session::locate("").has_value());
    ASSERT_TRUE(!session::locate_response("", BID).has_value());
    ASSERT_TRUE(!session::resume("").has_value());
    ASSERT_TRUE(!session::finish("").has_value());
}

/* ── Suite entry point ── */

void test_session_run(int& out_run, int& out_passed) {
    std::printf("\n[session]\n");
    TEST(test_session_call);
    TEST(test_session_call_response);
    TEST(test_session_call_target);
    TEST(test_session_status);
    TEST(test_session_status_response);
    TEST(test_session_notify);
    TEST(test_session_locate);
    TEST(test_session_locate_response);
    TEST(test_session_resume);
    TEST(test_session_finish);
    TEST(test_session_finish_target);
    TEST(test_session_mid_wrap);
    TEST(test_session_mid_sequencing);
    TEST(test_session_empty_sid);
    out_run    = tests_run;
    out_passed = tests_passed;
}
