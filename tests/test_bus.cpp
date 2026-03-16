/*
 * test_bus.cpp — Tests for antheos::bus verb builders
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>

using namespace antheos;

static void test_bus_establish() {
    auto f = bus::establish("4T9X2");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02, 0x12,0x21,0x1A,0x45,0x10,
                                0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10,
                                0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_conflict() {
    auto f = bus::conflict("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);
    ASSERT_EQ(f->data()[4], 0x43); /* C verb */
}

static void test_bus_broadcast() {
    auto f = bus::broadcast("Hello");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02, 0x12,0x21,0x1A,0x42,0x10,
                                0x12,0x22,0x1A,'H','e','l','l','o',0x10,
                                0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_broadcast_path() {
    auto f = bus::broadcast_path("Hello", "a.b");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);
    ASSERT_EQ(f->data()[f->size()-1], 0x03);
}

static void test_bus_ping_specific() {
    auto f = bus::ping("4T9X2");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02, 0x12,0x21,0x1A,0x50,0x10,
                                0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10,
                                0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_ping_all() {
    auto f = bus::ping_all();
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02, 0x12,0x21,0x1A,0x50,0x10, 0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_discover() {
    auto f = bus::discover("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[4], 0x44); /* D verb */
}

static void test_bus_discover_response() {
    auto f = bus::discover_response("4T9X2", "7M3K9");
    ASSERT_TRUE(f.has_value());
    /* SOM + !D + @U 4T9X2 + @U 7M3K9 + EOM */
    ASSERT_EQ(f->data()[0], 0x02);
    ASSERT_EQ(f->data()[f->size()-1], 0x03);
}

static void test_bus_verify() {
    auto f = bus::verify("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[4], 0x56); /* V verb */
}

static void test_bus_verify_response() {
    auto f = bus::verify_response("langsyn", "Thermostat", "SN00482");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x56,0x10,
        0x12,0x40,0x1A,'l','a','n','g','s','y','n',0x10,
        0x12,0x40,0x1A,'T','h','e','r','m','o','s','t','a','t',0x10,
        0x12,0x40,0x1A,'S','N','0','0','4','8','2',0x10,
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_acknowledge() {
    auto f = bus::acknowledge("4T9X2");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[4], 0x57); /* W verb */
}

static void test_bus_exception() {
    auto f = bus::exception("BID_OVERFLOW");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x58,0x10,
        0x12,0x22,0x1A,'B','I','D','_','O','V','E','R','F','L','O','W',0x10,
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_scaleback_full() {
    auto f = bus::scaleback("!H&!Q", 32, 0);
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);
    ASSERT_EQ(f->data()[f->size()-1], 0x03);
}

static void test_bus_scaleback_sizes_only() {
    auto f = bus::scaleback("", 32, 16);
    ASSERT_TRUE(f.has_value());
}

static void test_bus_scaleback_flags_only() {
    auto f = bus::scaleback("!H", 0, 0);
    ASSERT_TRUE(f.has_value());
}

static void test_bus_scaleback_both_absent() {
    auto f = bus::scaleback("", 0, 0);
    ASSERT_TRUE(!f.has_value());
}

static void test_bus_relay() {
    auto f = bus::relay("ping", 5, "hub.A");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x52,0x10,
        0x12,0x22,0x1A,'p','i','n','g',0x10,
        0x12,0x23,0x04,0x44,0x07,0x42,0x1A,'5',0x10,
        0x12,0x2F,0x1A,'h','u','b','.','A',0x10,
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_empty_bid() {
    ASSERT_TRUE(!bus::establish("").has_value());
    ASSERT_TRUE(!bus::conflict("").has_value());
    ASSERT_TRUE(!bus::ping("").has_value());
    ASSERT_TRUE(!bus::discover("").has_value());
    ASSERT_TRUE(!bus::verify("").has_value());
    ASSERT_TRUE(!bus::acknowledge("").has_value());
}

void test_bus_run(int& out_run, int& out_passed) {
    std::printf("\n[bus]\n");
    TEST(test_bus_establish);
    TEST(test_bus_conflict);
    TEST(test_bus_broadcast);
    TEST(test_bus_broadcast_path);
    TEST(test_bus_ping_specific);
    TEST(test_bus_ping_all);
    TEST(test_bus_discover);
    TEST(test_bus_discover_response);
    TEST(test_bus_verify);
    TEST(test_bus_verify_response);
    TEST(test_bus_acknowledge);
    TEST(test_bus_exception);
    TEST(test_bus_scaleback_full);
    TEST(test_bus_scaleback_sizes_only);
    TEST(test_bus_scaleback_flags_only);
    TEST(test_bus_scaleback_both_absent);
    TEST(test_bus_relay);
    TEST(test_bus_empty_bid);
    out_run    = tests_run;
    out_passed = tests_passed;
}
