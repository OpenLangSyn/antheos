/*
 * test_identity.cpp — Tests for antheos::id and antheos::SidPool
 *
 * Verifies base-32 encoding, BID/SID generation, and SID pool recycling
 * per Antheos Protocol v9 §5.4-5.6, §11.2.
 */

#include "test_common.hpp"
#include "antheos.hpp"
#include <cstring>

using namespace antheos;

/* ── Helper: check if string contains only valid base-32 characters ── */

static bool is_valid_base32(std::string_view s) {
    for (char c : s) {
        if (std::strchr(id::BASE32_ALPHABET, c) == nullptr) return false;
    }
    return true;
}

/* ── 1. BID generation — valid base-32 string ── */

static void test_bid_generate_valid() {
    auto bid = id::bid_generate(8);
    ASSERT_TRUE(bid.has_value());
    ASSERT_EQ(bid->size(), 8u);
    ASSERT_TRUE(is_valid_base32(*bid));
}

/* ── 2. BID length — different lengths (4, 8, 16) ── */

static void test_bid_length() {
    auto b4 = id::bid_generate(4);
    ASSERT_TRUE(b4.has_value());
    ASSERT_EQ(b4->size(), 4u);

    auto b8 = id::bid_generate(8);
    ASSERT_TRUE(b8.has_value());
    ASSERT_EQ(b8->size(), 8u);

    auto b16 = id::bid_generate(16);
    ASSERT_TRUE(b16.has_value());
    ASSERT_EQ(b16->size(), 16u);
}

/* ── 3. BID uniqueness — 100 BIDs, no duplicates ── */

static void test_bid_uniqueness() {
    std::string bids[100];
    for (int i = 0; i < 100; i++) {
        auto b = id::bid_generate(8);
        ASSERT_TRUE(b.has_value());
        bids[i] = *b;
    }
    for (int i = 0; i < 100; i++) {
        for (int j = i + 1; j < 100; j++) {
            ASSERT_TRUE(bids[i] != bids[j]);
        }
    }
}

/* ── 4. SID generation — valid base-32 SID of expected length ── */

static void test_sid_generate_valid() {
    auto sid = id::sid_generate("langsyn", "Thermo", "SN00482", 0, 8);
    ASSERT_TRUE(sid.has_value());
    ASSERT_EQ(sid->size(), 8u);
    ASSERT_TRUE(is_valid_base32(*sid));
}

/* ── 5. SID determinism — same inputs produce same SID ── */

static void test_sid_determinism() {
    auto sid1 = id::sid_generate("langsyn", "Thermo", "SN00482", 0, 8);
    auto sid2 = id::sid_generate("langsyn", "Thermo", "SN00482", 0, 8);
    ASSERT_TRUE(sid1.has_value());
    ASSERT_TRUE(sid2.has_value());
    ASSERT_TRUE(*sid1 == *sid2);

    auto sid3 = id::sid_generate("langsyn", "Thermo", "SN00482", 1, 8);
    ASSERT_TRUE(sid3.has_value());
    ASSERT_TRUE(*sid1 != *sid3);
}

/* ── 6. SID pool init ── */

static void test_sid_pool_init_basic() {
    SidPool pool(16, "langsyn", "Thermo", "SN00482");
    ASSERT_EQ(pool.size(), 0u);
}

/* ── 7. SID pool acquire — valid non-empty string ── */

static void test_sid_pool_acquire() {
    SidPool pool(16, "langsyn", "Thermo", "SN00482");
    auto sid = pool.acquire();
    ASSERT_TRUE(sid.has_value());
    ASSERT_TRUE(sid->size() >= id::SID_MIN_LEN);
    ASSERT_TRUE(is_valid_base32(*sid));
}

/* ── 8. SID pool acquire multiple — all unique via acquire_unique ── */

static void test_sid_pool_acquire_multiple() {
    SidPool pool(16, "langsyn", "Thermo", "SN00482");
    std::string sids[10];

    for (int i = 0; i < 10; i++) {
        auto sid = pool.acquire_unique([&](std::string_view candidate) -> bool {
            for (int j = 0; j < i; j++) {
                if (sids[j] == candidate) return true;
            }
            return false;
        });
        ASSERT_TRUE(sid.has_value());
        sids[i] = *sid;
    }

    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            ASSERT_TRUE(sids[i] != sids[j]);
        }
    }
}

/* ── 9. SID pool release and reacquire — oldest-first recycling ── */

static void test_sid_pool_release_reacquire() {
    SidPool pool(16, "langsyn", "Thermo", "SN00482");

    auto sid_orig = pool.acquire();
    ASSERT_TRUE(sid_orig.has_value());

    ASSERT_TRUE(pool.release(*sid_orig));
    ASSERT_EQ(pool.size(), 1u);

    auto sid_reused = pool.acquire();
    ASSERT_TRUE(sid_reused.has_value());
    ASSERT_TRUE(*sid_orig == *sid_reused);
    ASSERT_EQ(pool.size(), 0u);
}

/* ── 10. SID pool exhaustion — fill pool to capacity, drain, then fresh ── */

static void test_sid_pool_exhaustion() {
    size_t capacity = 4;
    SidPool pool(capacity, "langsyn", "Thermo", "SN00482");
    std::string sids[4];

    for (size_t i = 0; i < capacity; i++) {
        auto s = pool.acquire();
        ASSERT_TRUE(s.has_value());
        sids[i] = *s;
    }
    for (size_t i = 0; i < capacity; i++) {
        ASSERT_TRUE(pool.release(sids[i]));
    }
    ASSERT_EQ(pool.size(), capacity);

    std::string out;
    for (size_t i = 0; i < capacity; i++) {
        auto s = pool.acquire();
        ASSERT_TRUE(s.has_value());
        out = *s;
    }
    ASSERT_EQ(pool.size(), 0u);

    auto fresh = pool.acquire();
    ASSERT_TRUE(fresh.has_value());
    ASSERT_TRUE(fresh->size() >= id::SID_MIN_LEN);
}

/* ── 11. SID pool release then acquire — FIFO order ── */

static void test_sid_pool_release_then_acquire() {
    SidPool pool(16, "langsyn", "Thermo", "SN00482");

    auto sid_a = pool.acquire();
    auto sid_b = pool.acquire();
    ASSERT_TRUE(sid_a.has_value());
    ASSERT_TRUE(sid_b.has_value());

    ASSERT_TRUE(pool.release(*sid_a));
    ASSERT_TRUE(pool.release(*sid_b));
    ASSERT_EQ(pool.size(), 2u);

    auto out1 = pool.acquire();
    ASSERT_TRUE(out1.has_value());
    ASSERT_TRUE(*out1 == *sid_a);

    auto out2 = pool.acquire();
    ASSERT_TRUE(out2.has_value());
    ASSERT_TRUE(*out2 == *sid_b);

    ASSERT_EQ(pool.size(), 0u);
}

/* ── 12. Invalid argument handling ── */

static void test_invalid_arguments() {
    ASSERT_TRUE(!id::bid_generate(0).has_value());
    ASSERT_TRUE(!id::bid_generate(20).has_value());

    ASSERT_TRUE(!id::sid_generate("", "d", "i", 0, 8).has_value());
    ASSERT_TRUE(!id::sid_generate("o", "", "i", 0, 8).has_value());
    ASSERT_TRUE(!id::sid_generate("o", "d", "", 0, 8).has_value());
    ASSERT_TRUE(!id::sid_generate("o", "d", "i", 0, 2).has_value());

    ASSERT_TRUE(!id::base32_encode(nullptr, 1).has_value());
}

/* ── Suite entry point ── */

void test_identity_run(int& out_run, int& out_passed) {
    std::printf("\n[identity]\n");
    TEST(test_bid_generate_valid);
    TEST(test_bid_length);
    TEST(test_bid_uniqueness);
    TEST(test_sid_generate_valid);
    TEST(test_sid_determinism);
    TEST(test_sid_pool_init_basic);
    TEST(test_sid_pool_acquire);
    TEST(test_sid_pool_acquire_multiple);
    TEST(test_sid_pool_release_reacquire);
    TEST(test_sid_pool_exhaustion);
    TEST(test_sid_pool_release_then_acquire);
    TEST(test_invalid_arguments);
    out_run    = tests_run;
    out_passed = tests_passed;
}
