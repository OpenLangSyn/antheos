/*
 * context.cpp — Antheos Protocol v1.0 Level 1 Context
 *
 * Stateful codec: owns identity, SidPool, Parser, and 32 session slots.
 * Inbound dispatch via internal parser callbacks.
 * Outbound via bus/service/session verb builders.
 *
 * Copyright (c) 2025-2026 Are Bjorby <are.bjorby@proton.me>
 * Proprietary — all rights reserved.
 */

#include "antheos.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace antheos {

struct Context::Impl {
    std::string oid, did, iid, bid_str;
    SidPool sid_pool;
    Parser parser;

    struct SessionSlot {
        std::string sid;
        uint32_t mid = 0;
        SessionState state = SessionState::Idle;
    };
    SessionSlot sessions[MAX_SESSIONS];

    Context::MessageCb msg_cb;
    Context::OfferCb offer_cb;
    Context::EventCb event_cb;

    /* Parser scratch — accumulated during word callbacks, dispatched on EOM */
    static constexpr int MAX_SCRATCH_IDS = 4;
    char scratch_verb = 0;
    std::string scratch_ids[MAX_SCRATCH_IDS];
    wire::Radix scratch_id_radix[MAX_SCRATCH_IDS]{};
    int scratch_id_count = 0;
    uint32_t scratch_mid = 0;
    bool scratch_has_mid = false;
    std::string scratch_body;
    bool scratch_has_body = false;

    Impl(std::string_view o, std::string_view d, std::string_view i,
         std::string_view b)
        : oid(o), did(d), iid(i), bid_str(b),
          sid_pool(MAX_SESSIONS, o, d, i)
    {
        parser.on_word([this](wire::WordType type, wire::Radix radix,
                              wire::Unit unit,
                              const uint8_t* body, size_t len) {
            on_parser_word(type, radix, unit, body, len);
        });
        parser.on_message([this](size_t word_count) {
            on_parser_message(word_count);
        });
    }

    void scratch_reset() {
        scratch_verb = 0;
        scratch_id_count = 0;
        scratch_has_mid = false;
        scratch_mid = 0;
        scratch_has_body = false;
        scratch_body.clear();
    }

    void on_parser_word(wire::WordType type, wire::Radix radix,
                        wire::Unit /*unit*/,
                        const uint8_t* body, size_t len) {
        switch (type) {
        case wire::WordType::Symbol:
            if (len >= 1)
                scratch_verb = static_cast<char>(body[0]);
            break;

        case wire::WordType::Id:
            if (radix == wire::Radix::Decimal) {
                std::string tmp(reinterpret_cast<const char*>(body), len);
                scratch_mid = static_cast<uint32_t>(
                    std::strtoul(tmp.c_str(), nullptr, 10));
                scratch_has_mid = true;
            } else if (scratch_id_count < MAX_SCRATCH_IDS) {
                scratch_ids[scratch_id_count].assign(
                    reinterpret_cast<const char*>(body), len);
                scratch_id_radix[scratch_id_count] = radix;
                scratch_id_count++;
            }
            break;

        case wire::WordType::Text:
            scratch_body.assign(
                reinterpret_cast<const char*>(body), len);
            scratch_has_body = true;
            break;

        default:
            break;
        }
    }

    void on_parser_message(size_t /*word_count*/) {
        std::string_view id0 = scratch_id_count > 0
            ? std::string_view(scratch_ids[0]) : std::string_view{};
        std::string_view id1 = scratch_id_count > 1
            ? std::string_view(scratch_ids[1]) : std::string_view{};
        std::string_view body = scratch_has_body
            ? std::string_view(scratch_body) : std::string_view{};

        char verb_buf[2] = { scratch_verb, '\0' };
        std::string_view verb(verb_buf, 1);

        switch (scratch_verb) {
        /* Bus events: ID-carrying verbs */
        case 'E': case 'C': case 'P': case 'D': case 'V': case 'W':
            if (event_cb)
                event_cb(verb, id0, id1, {});
            break;

        /* Bus events: text-carrying verbs */
        case 'B': case 'X':
            if (event_cb)
                event_cb(verb, id0, {}, body);
            break;

        /* Service: query */
        case 'Q':
            if (event_cb)
                event_cb(verb, {}, {}, body);
            break;

        /* Service: offer */
        case 'O':
            if (offer_cb)
                offer_cb(id0, body);
            break;

        /* Service: accept */
        case 'A':
            if (event_cb)
                event_cb(verb, id0, id1, {});
            break;

        /* Session messages: call, notify */
        case 'K': case 'N':
            if (msg_cb) {
                uint32_t mid = scratch_has_mid ? scratch_mid : 0;
                msg_cb(verb, id0, id1, mid, body);
            }
            break;

        /* Session events: status, locate, resume, finish */
        case 'T': case 'L': case 'U': case 'F':
            if (event_cb) {
                std::string_view detail = scratch_has_body ? body : id1;
                event_cb(verb, id0, {}, detail);
            }
            break;

        default:
            break;
        }

        scratch_reset();
    }

    /* MID advance with wrap detection */
    struct MidResult { uint32_t mid; bool wrap; };

    MidResult advance_mid(SessionSlot& s) {
        if (s.mid == 0) {
            s.mid = 1;
            return {0, true};
        }
        uint32_t mid = s.mid;
        if (s.mid == UINT32_MAX)
            s.mid = 0;
        else
            s.mid++;
        return {mid, false};
    }
};

/* ── Lifecycle ── */

Context::Context(std::string_view oid, std::string_view did,
                 std::string_view iid, std::string_view bid) {
    if (oid.empty() || did.empty() || iid.empty() || bid.empty())
        throw std::invalid_argument(
            "Context: OID, DID, IID, BID must be non-empty");
    impl_ = std::make_unique<Impl>(oid, did, iid, bid);
}

Context::~Context() = default;

std::string_view Context::bid() const {
    return impl_->bid_str;
}

void Context::on_message(MessageCb cb) { impl_->msg_cb = std::move(cb); }
void Context::on_offer(OfferCb cb) { impl_->offer_cb = std::move(cb); }
void Context::on_event(EventCb cb) { impl_->event_cb = std::move(cb); }

/* ── Inbound ── */

size_t Context::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    impl_->parser.feed(data, len);
    return len;
}

/* ── Outbound — Bus Scope ── */

std::optional<Frame> Context::establish() {
    return bus::establish(impl_->bid_str);
}

std::optional<Frame> Context::broadcast(std::string_view text) {
    return bus::broadcast(text);
}

std::optional<Frame> Context::ping(std::string_view bid) {
    if (bid.empty())
        return bus::ping_all();
    return bus::ping(bid);
}

std::optional<Frame> Context::exception(std::string_view reason) {
    return bus::exception(reason);
}

std::optional<Frame> Context::verify(std::string_view bid) {
    return bus::verify(bid);
}

std::optional<Frame> Context::verify_response() {
    return bus::verify_response(impl_->oid, impl_->did, impl_->iid);
}

std::optional<Frame> Context::discover(std::string_view bid) {
    return bus::discover(bid);
}

std::optional<Frame> Context::acknowledge(std::string_view bid) {
    return bus::acknowledge(bid);
}

/* ── Outbound — Service Scope ── */

std::optional<Frame> Context::query(std::string_view capability) {
    return service::query(capability);
}

std::optional<Frame> Context::offer(std::string_view description) {
    return service::offer(impl_->bid_str, description);
}

std::optional<Frame> Context::accept(std::string_view bid,
                                     std::string_view sender_bid) {
    return service::accept(bid, sender_bid);
}

/* ── Session helpers ── */

static bool slot_valid(int slot) {
    return slot >= 0 && slot < MAX_SESSIONS;
}

/* ── Outbound — Session Scope ── */

int Context::session_open() {
    auto& p = *impl_;

    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (p.sessions[i].state == SessionState::Idle) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    auto sid = p.sid_pool.acquire_unique(
        [&p](std::string_view candidate) {
            for (int i = 0; i < MAX_SESSIONS; i++) {
                if (p.sessions[i].state != SessionState::Idle &&
                    p.sessions[i].sid == candidate)
                    return true;
            }
            return false;
        });

    if (!sid) return -1;

    p.sessions[slot].sid = std::move(*sid);
    p.sessions[slot].state = SessionState::Active;
    p.sessions[slot].mid = 1;

    return slot;
}

int Context::session_accept(std::string_view sid, uint32_t inbound_mid) {
    if (sid.empty()) return -1;
    auto& p = *impl_;

    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (p.sessions[i].state == SessionState::Idle) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    p.sessions[slot].sid = std::string(sid);
    p.sessions[slot].state = SessionState::Active;
    p.sessions[slot].mid = inbound_mid + 1;

    return slot;
}

std::string_view Context::session_sid(int slot) const {
    if (!slot_valid(slot)) return {};
    auto& s = impl_->sessions[slot];
    if (s.state == SessionState::Idle) return {};
    return s.sid;
}

uint32_t Context::session_mid(int slot) const {
    if (!slot_valid(slot)) return 0;
    auto& s = impl_->sessions[slot];
    if (s.state == SessionState::Idle) return 0;
    return s.mid;
}

SessionState Context::session_state(int slot) const {
    if (!slot_valid(slot)) return SessionState::Idle;
    return impl_->sessions[slot].state;
}

std::optional<Frame> Context::session_call(int slot,
                                           std::string_view target_bid,
                                           std::string_view payload) {
    if (!slot_valid(slot)) return std::nullopt;
    auto& s = impl_->sessions[slot];
    if (s.state != SessionState::Active) return std::nullopt;

    auto [mid, wrap] = impl_->advance_mid(s);
    if (wrap)
        return session::call_wrap(s.sid, target_bid);
    return session::call(s.sid, target_bid, mid, payload);
}

std::optional<Frame> Context::session_notify(int slot,
                                             std::string_view target_bid,
                                             std::string_view event) {
    if (!slot_valid(slot)) return std::nullopt;
    auto& s = impl_->sessions[slot];
    if (s.state != SessionState::Active) return std::nullopt;

    auto [mid, wrap] = impl_->advance_mid(s);
    if (wrap)
        return session::call_wrap(s.sid, target_bid);
    return session::notify(s.sid, target_bid, mid, event);
}

std::optional<Frame> Context::session_status(int slot,
                                             std::string_view target_bid) {
    if (!slot_valid(slot)) return std::nullopt;
    auto& s = impl_->sessions[slot];
    if (s.state != SessionState::Active) return std::nullopt;

    auto [mid, wrap] = impl_->advance_mid(s);
    if (wrap)
        return session::call_wrap(s.sid, target_bid);
    return session::status(s.sid, target_bid, mid);
}

std::optional<Frame> Context::session_close(int slot,
                                            std::string_view target_bid) {
    if (!slot_valid(slot)) return std::nullopt;
    auto& s = impl_->sessions[slot];
    if (s.state == SessionState::Idle) return std::nullopt;

    auto frame = session::finish(s.sid, target_bid);

    impl_->sid_pool.release(s.sid);
    s.state = SessionState::Idle;
    s.mid = 0;
    s.sid.clear();

    return frame;
}

} // namespace antheos
