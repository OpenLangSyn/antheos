/*
 * test_service.cpp — Tests for antheos::service verb builders
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>

using namespace antheos;

/* ── Query: SOM [!Q] ["temperature] EOM ── */

static void test_service_query() {
    auto f = service::query("temperature");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x51, 0x10,              /* [!Q]            */
        0x12, 0x22, 0x1A,                           /* ["              */
        't',  'e',  'm',  'p',  'e',  'r',
        'a',  't',  'u',  'r',  'e',  0x10,         /*   temperature]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Offer: SOM [!O] [@U 4T9X2] ["temperature_celsius] EOM ── */

static void test_service_offer() {
    auto f = service::offer("4T9X2", "temperature_celsius");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x4F, 0x10,              /* [!O]                    */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,         /* [@U 4T9X2]              */
        0x12, 0x22, 0x1A,                           /* ["                      */
        't',  'e',  'm',  'p',  'e',  'r',
        'a',  't',  'u',  'r',  'e',  '_',
        'c',  'e',  'l',  's',  'i',  'u',  's',
        0x10,                                       /*   temperature_celsius]   */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Accept: SOM [!A] [@U 4T9X2] EOM ── */

static void test_service_accept() {
    auto f = service::accept("4T9X2");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x41, 0x10,              /* [!A]      */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Accept with sender BID: SOM [!A] [@U 4T9X2] [@U 7M3K9] EOM ── */

static void test_service_accept_sender() {
    auto f = service::accept("4T9X2", "7M3K9");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x41, 0x10,              /* [!A]        */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '4',  'T',  '9',  'X',  '2',  0x10,         /* [@U 4T9X2]  */
        0x12, 0x40, 0x04, 0x55, 0x1A,
        '7',  'M',  '3',  'K',  '9',  0x10,         /* [@U 7M3K9]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Query with empty capability: valid frame ── */

static void test_service_query_empty() {
    auto f = service::query("");
    ASSERT_TRUE(f.has_value());

    const uint8_t expected[] = {
        0x02,
        0x12, 0x21, 0x1A, 0x51, 0x10,   /* [!Q]  */
        0x12, 0x22, 0x1A, 0x10,          /* [""]  */
        0x03
    };
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

/* ── Empty BID: offer and accept return nullopt ── */

static void test_service_empty_bid() {
    ASSERT_TRUE(!service::offer("", "desc").has_value());
    ASSERT_TRUE(!service::accept("").has_value());
}

/* ── Offer with empty description: valid frame ── */

static void test_service_offer_empty_desc() {
    auto f = service::offer("4T9X2", "");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);
    ASSERT_EQ(f->data()[f->size()-1], 0x03);
    ASSERT_EQ(f->data()[4], 0x4F); /* O verb */
}

void test_service_run(int& out_run, int& out_passed) {
    std::printf("\n[service]\n");
    TEST(test_service_query);
    TEST(test_service_offer);
    TEST(test_service_accept);
    TEST(test_service_accept_sender);
    TEST(test_service_query_empty);
    TEST(test_service_empty_bid);
    TEST(test_service_offer_empty_desc);
    out_run    = tests_run;
    out_passed = tests_passed;
}
