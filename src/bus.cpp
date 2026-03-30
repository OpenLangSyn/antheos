/*
 * bus.cpp — Antheos Protocol v9 Bus Frame Construction
 *
 * Copyright (c) 2025-2026 Are Bjorby <are.bjorby@proton.me>
 * Proprietary — all rights reserved.
 */

#include "antheos.hpp"
#include <cstdio>

namespace antheos {

namespace {

/* Shared frame builder: appends SOM, words, EOM to a growing vector */
struct FrameBuilder {
    std::vector<uint8_t> buf;

    void som() { buf.push_back(wire::SOM); }
    void eom() { buf.push_back(wire::EOM); }

    bool symbol(char verb) {
        auto w = wire::encode_symbol(verb);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool id_base32(std::string_view id) {
        if (id.empty()) return false;
        auto w = wire::encode_id_base32(id);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool id_plain(std::string_view id) {
        auto w = wire::encode_id_plain(id);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool id_decimal(uint32_t value) {
        auto w = wire::encode_id_decimal(value);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool text(std::string_view t) {
        auto w = wire::encode_text(t);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool integer(wire::Radix r, wire::Unit u, std::string_view v) {
        auto w = wire::encode_integer(r, u, v);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool path(std::string_view p) {
        auto w = wire::encode_path(p);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool logical(std::string_view expr) {
        auto w = wire::encode_logical(expr);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    bool message(std::string_view ref) {
        auto w = wire::encode_message(ref);
        if (!w) return false;
        buf.insert(buf.end(), w->begin(), w->end());
        return true;
    }

    std::optional<Frame> finish() {
        return Frame(std::move(buf));
    }
};

} // anonymous namespace

namespace bus {

std::optional<Frame> establish(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('E')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> conflict(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('C')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> broadcast(std::string_view text) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('B')) return std::nullopt;
    if (!fb.text(text)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> broadcast_path(std::string_view text, std::string_view path) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('B')) return std::nullopt;
    if (!fb.text(text)) return std::nullopt;
    if (!fb.path(path)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> ping(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('P')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> ping_all() {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('P')) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> relay(std::string_view text, uint32_t index, std::string_view path) {
    char idx_str[16];
    std::snprintf(idx_str, sizeof(idx_str), "%u", index);

    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('R')) return std::nullopt;
    if (!fb.text(text)) return std::nullopt;
    if (!fb.integer(wire::Radix::Decimal, wire::Unit::Byte, idx_str)) return std::nullopt;
    if (!fb.path(path)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> discover(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('D')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> discover_response(std::string_view target, std::string_view via) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('D')) return std::nullopt;
    if (!fb.id_base32(target)) return std::nullopt;
    if (!fb.id_base32(via)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> verify(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('V')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> verify_response(
    std::string_view oid, std::string_view did, std::string_view iid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('V')) return std::nullopt;
    if (!fb.id_plain(oid)) return std::nullopt;
    if (!fb.id_plain(did)) return std::nullopt;
    if (!fb.id_plain(iid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> scaleback(
    std::string_view exclusions, uint16_t head_max, uint16_t tail_max) {
    if (exclusions.empty() && head_max == 0 && tail_max == 0)
        return std::nullopt;

    char head_str[8], tail_str[8];
    std::snprintf(head_str, sizeof(head_str), "%u", static_cast<unsigned>(head_max));
    std::snprintf(tail_str, sizeof(tail_str), "%u", static_cast<unsigned>(tail_max));

    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('S')) return std::nullopt;
    if (!exclusions.empty()) {
        if (!fb.logical(exclusions)) return std::nullopt;
    }
    if (head_max > 0 || tail_max > 0) {
        if (!fb.integer(wire::Radix::Decimal, wire::Unit::Word, head_str)) return std::nullopt;
        if (!fb.integer(wire::Radix::Decimal, wire::Unit::Word, tail_str)) return std::nullopt;
    }
    fb.eom();
    return fb.finish();
}

std::optional<Frame> acknowledge(std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('W')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

/* ── Level 2: Auth (Z-verb) ────────────────────────────────────── */

std::optional<Frame> auth_challenge(std::string_view target_bid,
                                    std::string_view nonce_hex) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('Z')) return std::nullopt;
    if (!fb.id_base32(target_bid)) return std::nullopt;
    if (!fb.text(nonce_hex)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> auth_response(std::string_view target_bid,
                                   std::string_view key_id,
                                   std::string_view sig_hex) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('Z')) return std::nullopt;
    if (!fb.id_base32(target_bid)) return std::nullopt;
    if (!fb.id_plain(key_id)) return std::nullopt;
    if (!fb.text(sig_hex)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

/* ── Level 2: Relay + Auth (multi-hop Z-verb) ─────────────────── */

std::optional<Frame> relay_auth_challenge(std::string_view target_bid,
    std::string_view nonce_hex, uint32_t index, std::string_view path) {
    if (target_bid.empty() || nonce_hex.empty() || path.empty())
        return std::nullopt;

    char idx_str[16];
    std::snprintf(idx_str, sizeof(idx_str), "%u", index);

    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('R')) return std::nullopt;
    if (!fb.id_base32(target_bid)) return std::nullopt;
    if (!fb.message("Z")) return std::nullopt;
    if (!fb.text(nonce_hex)) return std::nullopt;
    if (!fb.integer(wire::Radix::Decimal, wire::Unit::Byte, idx_str)) return std::nullopt;
    if (!fb.path(path)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> relay_auth_response(std::string_view target_bid,
    std::string_view key_id, std::string_view sig_hex,
    uint32_t index, std::string_view path) {
    if (target_bid.empty() || key_id.empty() || sig_hex.empty() || path.empty())
        return std::nullopt;

    char idx_str[16];
    std::snprintf(idx_str, sizeof(idx_str), "%u", index);

    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('R')) return std::nullopt;
    if (!fb.id_base32(target_bid)) return std::nullopt;
    if (!fb.id_plain(key_id)) return std::nullopt;
    if (!fb.message("Z")) return std::nullopt;
    if (!fb.text(sig_hex)) return std::nullopt;
    if (!fb.integer(wire::Radix::Decimal, wire::Unit::Byte, idx_str)) return std::nullopt;
    if (!fb.path(path)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> exception(std::string_view reason) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('X')) return std::nullopt;
    if (!fb.text(reason)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

} // namespace bus

namespace service {

std::optional<Frame> query(std::string_view capability) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('Q')) return std::nullopt;
    if (!fb.text(capability)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> offer(std::string_view bid, std::string_view description) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('O')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    if (!fb.text(description)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> accept(std::string_view bid, std::string_view sender_bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('A')) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    if (!sender_bid.empty()) {
        if (!fb.id_base32(sender_bid)) return std::nullopt;
    }
    fb.eom();
    return fb.finish();
}

} // namespace service

namespace session {

std::optional<Frame> call(std::string_view sid, std::string_view target_bid,
                          uint32_t mid, std::string_view payload) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('K')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    if (!fb.id_decimal(mid)) return std::nullopt;
    if (!fb.text(payload)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> call_wrap(std::string_view sid, std::string_view target_bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('K')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    if (!fb.id_decimal(0)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> status(std::string_view sid, std::string_view target_bid,
                            uint32_t mid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('T')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    if (!fb.id_decimal(mid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> status_response(std::string_view sid,
    std::string_view target_bid, uint32_t mid, std::string_view state) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('T')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    if (!fb.id_decimal(mid)) return std::nullopt;
    if (!fb.text(state)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> notify(std::string_view sid, std::string_view target_bid,
                            uint32_t mid, std::string_view event) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('N')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    if (!fb.id_decimal(mid)) return std::nullopt;
    if (!fb.text(event)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> locate(std::string_view sid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('L')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> locate_response(std::string_view sid, std::string_view bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('L')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!fb.id_base32(bid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> resume(std::string_view sid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('U')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    fb.eom();
    return fb.finish();
}

std::optional<Frame> finish(std::string_view sid, std::string_view target_bid) {
    FrameBuilder fb;
    fb.som();
    if (!fb.symbol('F')) return std::nullopt;
    if (!fb.id_base32(sid)) return std::nullopt;
    if (!target_bid.empty()) {
        if (!fb.id_base32(target_bid)) return std::nullopt;
    }
    fb.eom();
    return fb.finish();
}

} // namespace session

} // namespace antheos
