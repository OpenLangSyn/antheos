/*
 * identity.cpp — Antheos Protocol v9 Identity
 *
 * Base-32 encoding, BID/SID generation, and SID generator
 * per Antheos Protocol 1.0 Level 1, v9 specification (§5.4-5.6, §11.2).
 *
 * Copyright (c) 2025-2026 Are Bjørby <are.bjorby@langsyn.org>
 * SPDX-License-Identifier: MIT
 */

#include "antheos.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace antheos {

/* ── FNV-1a 64-bit hash ── */

namespace {

constexpr uint64_t FNV1A_OFFSET = UINT64_C(14695981039346656037);
constexpr uint64_t FNV1A_PRIME  = UINT64_C(1099511628211);

uint64_t fnv1a_64(const uint8_t* data, size_t len) {
    uint64_t hash = FNV1A_OFFSET;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= FNV1A_PRIME;
    }
    return hash;
}

} // anonymous namespace

/* ── id:: free functions ── */

namespace id {

std::optional<std::string> base32_encode(const uint8_t* data, size_t data_len) {
    if (data == nullptr || data_len == 0)
        return std::nullopt;

    size_t num_chars = (data_len * 8 + 4) / 5;
    std::string out;
    out.reserve(num_chars);

    uint32_t bits = 0;
    int nbits = 0;

    for (size_t i = 0; i < data_len; i++) {
        bits = (bits << 8) | static_cast<uint32_t>(data[i]);
        nbits += 8;
        while (nbits >= 5) {
            nbits -= 5;
            out += BASE32_ALPHABET[(bits >> static_cast<uint32_t>(nbits)) & 0x1Fu];
        }
    }

    if (nbits > 0) {
        out += BASE32_ALPHABET[(bits << static_cast<uint32_t>(5 - nbits)) & 0x1Fu];
    }

    return out;
}

std::optional<std::string> bid_generate(size_t len,
    const uint8_t* entropy, size_t entropy_len) {
    if (len < BID_MIN_LEN || len > BID_MAX_LEN)
        return std::nullopt;

    size_t nbytes = bid_entropy_needed(len);
    if (entropy == nullptr || entropy_len < nbytes)
        return std::nullopt;

    auto encoded = base32_encode(entropy, nbytes);
    if (!encoded) return std::nullopt;

    return encoded->substr(0, len);
}

std::optional<std::string> sid_generate(
    std::string_view oid, std::string_view did,
    std::string_view iid, uint32_t counter, size_t len) {

    if (oid.empty() || did.empty() || iid.empty())
        return std::nullopt;
    if (len < SID_MIN_LEN || len > SID_MAX_LEN)
        return std::nullopt;

    /* Build hash input: "oid:did:iid:<counter>" */
    char input[256];
    int n = std::snprintf(input, sizeof(input), "%.*s:%.*s:%.*s:%u",
                          static_cast<int>(oid.size()), oid.data(),
                          static_cast<int>(did.size()), did.data(),
                          static_cast<int>(iid.size()), iid.data(),
                          counter);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(input))
        return std::nullopt;

    /* Primary FNV-1a hash → 8 bytes (little-endian for truncation quality) */
    uint64_t h1 = fnv1a_64(reinterpret_cast<const uint8_t*>(input),
                            static_cast<size_t>(n));
    uint8_t hash_bytes[16];
    for (int i = 0; i < 8; i++) {
        hash_bytes[i] = static_cast<uint8_t>(h1 & 0xFFu);
        h1 >>= 8;
    }

    /* Secondary hash for extension — needed for len > 12 */
    uint64_t h2 = fnv1a_64(hash_bytes, 8);
    for (int i = 0; i < 8; i++) {
        hash_bytes[8 + i] = static_cast<uint8_t>(h2 & 0xFFu);
        h2 >>= 8;
    }

    auto encoded = base32_encode(hash_bytes, 16);
    if (!encoded) return std::nullopt;

    return encoded->substr(0, len);
}

} // namespace id

/* ── SidPool ── */

struct SidPool::Impl {
    std::string oid;
    std::string did;
    std::string iid;
    uint32_t next_counter = 0;

    size_t initial_len = id::SID_MIN_LEN;
    size_t current_len = id::SID_MIN_LEN;
    size_t max_len = id::SID_MAX_LEN;

    Impl(std::string_view o, std::string_view d, std::string_view i)
        : oid(o), did(d), iid(i) {}
};

SidPool::SidPool(std::string_view oid,
                 std::string_view did, std::string_view iid) {
    if (oid.empty() || did.empty() || iid.empty())
        throw std::invalid_argument("SidPool: OID, DID, IID must be non-empty");
    impl_ = std::make_unique<Impl>(oid, did, iid);
}

SidPool::~SidPool() = default;
SidPool::SidPool(SidPool&&) noexcept = default;
SidPool& SidPool::operator=(SidPool&&) noexcept = default;

std::optional<std::string> SidPool::acquire() {
    auto& p = *impl_;
    auto sid = id::sid_generate(p.oid, p.did, p.iid,
                                p.next_counter, p.current_len);
    if (!sid) return std::nullopt;
    p.next_counter++;
    return sid;
}

std::optional<std::string> SidPool::acquire_unique(CollisionCheck check) {
    if (!check) return std::nullopt;
    auto& p = *impl_;

    /* Generate fresh per §11.2 — up to 3 full cycles */
    size_t full_cycles = 0;
    while (full_cycles < 3) {
        auto sid = id::sid_generate(p.oid, p.did, p.iid,
                                    p.next_counter, p.current_len);
        if (!sid) return std::nullopt;

        if (!check(*sid)) {
            p.next_counter++;
            return sid;
        }

        /* Collision: grow length or roll over */
        if (p.current_len < p.max_len) {
            p.current_len++;
        } else {
            p.next_counter++;
            p.current_len = p.initial_len;
            full_cycles++;
        }
    }

    return std::nullopt;
}

} // namespace antheos
