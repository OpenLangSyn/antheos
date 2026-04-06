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

static void test_bus_auth_challenge() {
    auto f = bus::auth_challenge("4T9X2", "abcd1234");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);      /* SOM */
    ASSERT_EQ(f->data()[4], 0x5A);      /* Z verb */
    ASSERT_EQ(f->data()[f->size()-1], 0x03); /* EOM */
}

static void test_bus_auth_challenge_wire() {
    auto f = bus::auth_challenge("4T9X2", "ff00");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x5A,0x10,                        /* [!Z] */
        0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10, /* [@U 4T9X2] */
        0x12,0x22,0x1A,'f','f','0','0',0x10,              /* ["ff00] */
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_auth_response() {
    auto f = bus::auth_response("4T9X2", "abcd1234", "deadbeef");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);      /* SOM */
    ASSERT_EQ(f->data()[4], 0x5A);      /* Z verb */
    ASSERT_EQ(f->data()[f->size()-1], 0x03); /* EOM */
}

static void test_bus_auth_response_wire() {
    auto f = bus::auth_response("4T9X2", "k1d2", "s1g2");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x5A,0x10,                        /* [!Z] */
        0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10, /* [@U 4T9X2] */
        0x12,0x40,0x1A,'k','1','d','2',0x10,              /* [@ k1d2] */
        0x12,0x22,0x1A,'s','1','g','2',0x10,              /* ["s1g2] */
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_auth_empty_args() {
    ASSERT_TRUE(!bus::auth_challenge("", "nonce").has_value());
    ASSERT_TRUE(!bus::auth_response("", "kid", "sig").has_value());
}

/* ── Level 2: Relay + Auth (multi-hop Z-verb) ── */

static void test_bus_relay_auth_challenge() {
    auto f = bus::relay_auth_challenge("4T9X2", "ff00", 3, "AA.BB.CC.DD");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[0], 0x02);                   /* SOM */
    ASSERT_EQ(f->data()[4], 'R');                     /* R verb */
    ASSERT_EQ(f->data()[f->size()-1], 0x03);          /* EOM */
}

static void test_bus_relay_auth_challenge_wire() {
    auto f = bus::relay_auth_challenge("4T9X2", "ff00", 3, "AA.BB.CC");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,'R',0x10,                              /* [!R] */
        0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10,    /* [@U 4T9X2] */
        0x12,0x7E,0x1A,'Z',0x10,                              /* [~Z] */
        0x12,0x22,0x1A,'f','f','0','0',0x10,                  /* ["ff00] */
        0x12,0x23,0x04,0x44,0x07,0x42,0x1A,'3',0x10,          /* [#D B 3] */
        0x12,0x2F,0x1A,'A','A','.','B','B','.','C','C',0x10,  /* [/AA.BB.CC] */
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_relay_auth_response_wire() {
    auto f = bus::relay_auth_response("4T9X2", "k1d2", "s1g2", 2, "AA.BB.CC");
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,'R',0x10,                              /* [!R] */
        0x12,0x40,0x04,0x55,0x1A,'4','T','9','X','2',0x10,    /* [@U 4T9X2] */
        0x12,0x40,0x1A,'k','1','d','2',0x10,                  /* [@ k1d2] */
        0x12,0x7E,0x1A,'Z',0x10,                              /* [~Z] */
        0x12,0x22,0x1A,'s','1','g','2',0x10,                  /* ["s1g2] */
        0x12,0x23,0x04,0x44,0x07,0x42,0x1A,'2',0x10,          /* [#D B 2] */
        0x12,0x2F,0x1A,'A','A','.','B','B','.','C','C',0x10,  /* [/AA.BB.CC] */
        0x03};
    ASSERT_EQ(f->size(), sizeof(expected));
    ASSERT_MEM_EQ(f->data(), expected, sizeof(expected));
}

static void test_bus_relay_auth_empty_args() {
    ASSERT_TRUE(!bus::relay_auth_challenge("", "nonce", 1, "A.B").has_value());
    ASSERT_TRUE(!bus::relay_auth_challenge("BID", "", 1, "A.B").has_value());
    ASSERT_TRUE(!bus::relay_auth_challenge("BID", "nonce", 1, "").has_value());
    ASSERT_TRUE(!bus::relay_auth_response("", "kid", "sig", 1, "A.B").has_value());
    ASSERT_TRUE(!bus::relay_auth_response("BID", "", "sig", 1, "A.B").has_value());
    ASSERT_TRUE(!bus::relay_auth_response("BID", "kid", "", 1, "A.B").has_value());
    ASSERT_TRUE(!bus::relay_auth_response("BID", "kid", "sig", 1, "").has_value());
}

/* ── Exception code constants (spec §13 + Appendix A) ── */

static void test_exc_bus_codes() {
    /* Verify string values match spec §13 exactly */
    ASSERT_TRUE(std::strcmp(exc::BID_OVERFLOW, "BID_OVERFLOW") == 0);
    ASSERT_TRUE(std::strcmp(exc::BID_TIMEOUT, "BID_TIMEOUT") == 0);
    ASSERT_TRUE(std::strcmp(exc::RELAY_FAILED, "RELAY_FAILED") == 0);
    ASSERT_TRUE(std::strcmp(exc::PATH_BROKEN, "PATH_BROKEN") == 0);
    ASSERT_TRUE(std::strcmp(exc::INDEX_INVALID, "INDEX_INVALID") == 0);
    ASSERT_TRUE(std::strcmp(exc::UNKNOWN_BID, "UNKNOWN_BID") == 0);
    ASSERT_TRUE(std::strcmp(exc::MALFORMED_FRAME, "MALFORMED_FRAME") == 0);
    ASSERT_TRUE(std::strcmp(exc::UNSUPPORTED_TYPE, "UNSUPPORTED_TYPE") == 0);
}

static void test_exc_service_codes() {
    ASSERT_TRUE(std::strcmp(exc::SERVICE_UNKNOWN, "SERVICE_UNKNOWN") == 0);
    ASSERT_TRUE(std::strcmp(exc::OFFER_EXPIRED, "OFFER_EXPIRED") == 0);
}

static void test_exc_session_codes() {
    ASSERT_TRUE(std::strcmp(exc::SESSION_NOT_FOUND, "SESSION_NOT_FOUND") == 0);
    ASSERT_TRUE(std::strcmp(exc::SESSION_EXPIRED, "SESSION_EXPIRED") == 0);
    ASSERT_TRUE(std::strcmp(exc::RESUME_DENIED, "RESUME_DENIED") == 0);
    ASSERT_TRUE(std::strcmp(exc::MID_OUT_OF_ORDER, "MID_OUT_OF_ORDER") == 0);
}

static void test_exc_auth_code() {
    ASSERT_TRUE(std::strcmp(exc::AUTH_FAILED, "AUTH_FAILED") == 0);
}

static void test_exc_roundtrip_bus() {
    /* Each bus-scope code round-trips through bus::exception() */
    const char* codes[] = {
        exc::BID_OVERFLOW, exc::BID_TIMEOUT, exc::RELAY_FAILED,
        exc::PATH_BROKEN, exc::INDEX_INVALID, exc::UNKNOWN_BID,
        exc::MALFORMED_FRAME, exc::UNSUPPORTED_TYPE
    };
    for (auto code : codes) {
        auto f = bus::exception(code);
        ASSERT_TRUE(f.has_value());
        ASSERT_EQ(f->data()[0], 0x02);           /* SOM */
        ASSERT_EQ(f->data()[4], 0x58);           /* X verb */
        ASSERT_EQ(f->data()[f->size()-1], 0x03); /* EOM */
    }
}

static void test_exc_roundtrip_service() {
    const char* codes[] = { exc::SERVICE_UNKNOWN, exc::OFFER_EXPIRED };
    for (auto code : codes) {
        auto f = bus::exception(code);
        ASSERT_TRUE(f.has_value());
        ASSERT_EQ(f->data()[4], 0x58);
    }
}

static void test_exc_roundtrip_session() {
    const char* codes[] = {
        exc::SESSION_NOT_FOUND, exc::SESSION_EXPIRED,
        exc::RESUME_DENIED, exc::MID_OUT_OF_ORDER
    };
    for (auto code : codes) {
        auto f = bus::exception(code);
        ASSERT_TRUE(f.has_value());
        ASSERT_EQ(f->data()[4], 0x58);
    }
}

static void test_exc_roundtrip_auth() {
    auto f = bus::exception(exc::AUTH_FAILED);
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[4], 0x58);
}

static void test_exc_wire_format() {
    /* Verify wire format of exc::BID_OVERFLOW matches spec §9.9 example */
    auto f = bus::exception(exc::BID_OVERFLOW);
    ASSERT_TRUE(f.has_value());
    const uint8_t expected[] = {0x02,
        0x12,0x21,0x1A,0x58,0x10,
        0x12,0x22,0x1A,'B','I','D','_','O','V','E','R','F','L','O','W',0x10,
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
    TEST(test_bus_auth_challenge);
    TEST(test_bus_auth_challenge_wire);
    TEST(test_bus_auth_response);
    TEST(test_bus_auth_response_wire);
    TEST(test_bus_auth_empty_args);
    TEST(test_bus_relay_auth_challenge);
    TEST(test_bus_relay_auth_challenge_wire);
    TEST(test_bus_relay_auth_response_wire);
    TEST(test_bus_relay_auth_empty_args);
    TEST(test_exc_bus_codes);
    TEST(test_exc_service_codes);
    TEST(test_exc_session_codes);
    TEST(test_exc_auth_code);
    TEST(test_exc_roundtrip_bus);
    TEST(test_exc_roundtrip_service);
    TEST(test_exc_roundtrip_session);
    TEST(test_exc_roundtrip_auth);
    TEST(test_exc_wire_format);
    TEST(test_bus_empty_bid);
    out_run    = tests_run;
    out_passed = tests_passed;
}
