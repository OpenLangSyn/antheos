/*
 * antheos.hpp — Antheos Protocol v1.0 Level 1
 *
 * Pure C++17 implementation. Transport-agnostic stateful codec
 * for peer-to-peer messaging over any byte stream.
 *
 * Copyright (c) 2025-2026 Are Bjorby <are.bjorby@proton.me>
 * Proprietary — all rights reserved.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace antheos {

/* ══════════════════════════════════════════════════════════════════
 *  Frame — value type holding encoded wire data
 * ══════════════════════════════════════════════════════════════════ */

class Frame {
    std::vector<uint8_t> data_;
public:
    Frame() = default;
    Frame(const uint8_t* d, size_t n) : data_(d, d + n) {}
    explicit Frame(std::vector<uint8_t>&& v) : data_(std::move(v)) {}

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    explicit operator bool() const { return !data_.empty(); }

    const uint8_t* begin() const { return data_.data(); }
    const uint8_t* end() const { return data_.data() + data_.size(); }
    uint8_t operator[](size_t i) const { return data_[i]; }

    std::vector<uint8_t>& bytes() { return data_; }
    const std::vector<uint8_t>& bytes() const { return data_; }
};

/* ══════════════════════════════════════════════════════════════════
 *  Wire — control characters, word types, encoding/decoding
 *         (v9 spec §4–§6)
 * ══════════════════════════════════════════════════════════════════ */

namespace wire {

/* ── Control Characters (v9 §4.1) ── exactly 7 ── */

inline constexpr uint8_t SOM = 0x02;   /* Start of Message */
inline constexpr uint8_t EOM = 0x03;   /* End of Message */
inline constexpr uint8_t SOR = 0x04;   /* Start of Radix qualifier */
inline constexpr uint8_t SOU = 0x07;   /* Start of Unit qualifier */
inline constexpr uint8_t EOW = 0x10;   /* End of Word */
inline constexpr uint8_t SOW = 0x12;   /* Start of Word */
inline constexpr uint8_t SOB = 0x1A;   /* Start of Body */

/* ── Word Types (v9 §6.2) ── */

enum class WordType : uint8_t {
    Symbol     = '!',   // 0x21 — Protocol verbs
    Text       = '"',   // 0x22 — Plain text
    Integer    = '#',   // 0x23 — Integer values
    Real       = '$',   // 0x24 — Floating-point
    Scientific = '%',   // 0x25 — Exponential notation
    Timestamp  = '&',   // 0x26 — ISO 8601
    Blob       = '*',   // 0x2A — Tail size declaration
    Path       = '/',   // 0x2F — Routing sequences
    Logical    = '?',   // 0x3F — Boolean expressions
    Id         = '@',   // 0x40 — Identifiers
    Message    = '~',   // 0x7E — Embedded message ref
};

/* ── Radix Flags (v9 §6.3) ── */

enum class Radix : uint8_t {
    None    = 0,
    Binary  = 'I',   // Base-2
    Octal   = 'O',   // Base-8
    Decimal = 'D',   // Base-10
    Hex     = 'H',   // Base-16
    Base32  = 'U',   // Base-32 (duotrigesimal)
};

/* ── Unit Flags (v9 §6.4) ── */

enum class Unit : uint8_t {
    None     = 0,
    Byte     = 'B',   // 8 bits
    Word     = 'W',   // 16 bits
    Dword    = 'D',   // 32 bits
    Qword    = 'Q',   // 64 bits
    Megabyte = 'M',   // 2^20 bytes
    Gigabyte = 'G',   // 2^30 bytes
    Terabyte = 'T',   // 2^40 bytes
};

/* ── Validation ── */

bool is_reserved(uint8_t byte);
bool is_valid_word_type(uint8_t byte);
bool is_radix_flag(uint8_t byte);
bool is_unit_flag(uint8_t byte);

/* ── Flag requirement rules (v9 §6.1) ──
 *
 * Radix + Unit required:           INTEGER, REAL, SCIENTIFIC, BLOB
 * Radix optional, Unit forbidden:  ID
 * Both forbidden:                  SYMBOL, PATH, TEXT, LOGICAL, TIMESTAMP, MESSAGE
 */

bool needs_radix(WordType type);
bool needs_unit(WordType type);
bool allows_radix(WordType type);

/* ── Decoded word ── */

struct DecodedWord {
    WordType type;
    Radix radix;
    Unit unit;
    std::vector<uint8_t> body;

    explicit operator bool() const { return !body.empty() || type == WordType::Symbol; }
};

/* ── Word encoding/decoding — v9 wire format ──
 *
 * Wire: [SOW][WT]([SOR][RF])([SOU][UF])[SOB]Body[EOW]
 */

std::optional<std::vector<uint8_t>> word_encode(
    WordType type, Radix radix, Unit unit,
    const uint8_t* body, size_t body_len);

std::optional<DecodedWord> word_decode(
    const uint8_t* in, size_t in_len);

/* ── Convenience encoders ── */

std::optional<std::vector<uint8_t>> encode_symbol(char verb);
std::optional<std::vector<uint8_t>> encode_text(std::string_view text);
std::optional<std::vector<uint8_t>> encode_id_plain(std::string_view id);
std::optional<std::vector<uint8_t>> encode_id_base32(std::string_view id);
std::optional<std::vector<uint8_t>> encode_id_decimal(uint32_t value);
std::optional<std::vector<uint8_t>> encode_integer(
    Radix radix, Unit unit, std::string_view value);
std::optional<std::vector<uint8_t>> encode_logical(std::string_view expr);
std::optional<std::vector<uint8_t>> encode_path(std::string_view path);

} // namespace wire

/* ══════════════════════════════════════════════════════════════════
 *  Identity — Base-32, BID/SID generation
 *             (v9 spec §5, §11.2)
 * ══════════════════════════════════════════════════════════════════ */

namespace id {

inline constexpr char BASE32_ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

inline constexpr size_t BID_MIN_LEN = 2;
inline constexpr size_t BID_MAX_LEN = 16;
inline constexpr size_t SID_MIN_LEN = 4;
inline constexpr size_t SID_MAX_LEN = 16;

constexpr size_t bid_entropy_needed(size_t len) { return (len * 5 + 7) / 8; }

std::optional<std::string> base32_encode(const uint8_t* data, size_t len);
std::optional<std::string> bid_generate(size_t len,
    const uint8_t* entropy, size_t entropy_len);
std::optional<std::string> sid_generate(
    std::string_view oid, std::string_view did,
    std::string_view iid, uint32_t counter, size_t len);

} // namespace id

/* ══════════════════════════════════════════════════════════════════
 *  SidPool — Session ID generator with length optimization (§11.2)
 *
 *  Every session gets a fresh SID. SIDs are never reused.
 *  Length starts at SID_MIN_LEN and grows only on collision.
 * ══════════════════════════════════════════════════════════════════ */

class SidPool {
public:
    using CollisionCheck = std::function<bool(std::string_view sid)>;

    SidPool(std::string_view oid, std::string_view did,
            std::string_view iid);
    ~SidPool();

    SidPool(const SidPool&) = delete;
    SidPool& operator=(const SidPool&) = delete;
    SidPool(SidPool&&) noexcept;
    SidPool& operator=(SidPool&&) noexcept;

    std::optional<std::string> acquire();
    std::optional<std::string> acquire_unique(CollisionCheck check);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* ══════════════════════════════════════════════════════════════════
 *  Parser — Byte-at-a-time stream parser (v9 §7)
 * ══════════════════════════════════════════════════════════════════ */

enum class ParseState {
    WaitSom, WaitSow, WordType, AfterType,
    RadixFlag, AfterRadix, UnitFlag, WaitSob,
    WordBody, Tail, Error,
};

class Parser {
public:
    using WordCb = std::function<void(
        wire::WordType type, wire::Radix radix, wire::Unit unit,
        const uint8_t* body, size_t len)>;
    using TailCb = std::function<void(const uint8_t* data, size_t len)>;
    using MessageCb = std::function<void(size_t word_count)>;

    Parser();
    ~Parser();

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) noexcept;
    Parser& operator=(Parser&&) noexcept;

    void on_word(WordCb cb);
    void on_tail(TailCb cb);
    void on_message(MessageCb cb);

    ParseState feed(uint8_t byte);
    ParseState feed(const uint8_t* data, size_t len);
    void reset();
    void set_tail_length(size_t len);
    ParseState state() const;

    size_t total_words() const;
    size_t total_messages() const;
    size_t parse_errors() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* ══════════════════════════════════════════════════════════════════
 *  Verb Builders — stateless frame construction
 *  (v9 spec §9–§11)
 * ══════════════════════════════════════════════════════════════════ */

namespace bus {

std::optional<Frame> establish(std::string_view bid);
std::optional<Frame> conflict(std::string_view bid);
std::optional<Frame> broadcast(std::string_view text);
std::optional<Frame> broadcast_path(std::string_view text, std::string_view path);
std::optional<Frame> ping(std::string_view bid);
std::optional<Frame> ping_all();
std::optional<Frame> relay(std::string_view text, uint32_t index, std::string_view path);
std::optional<Frame> discover(std::string_view bid);
std::optional<Frame> discover_response(std::string_view target, std::string_view via);
std::optional<Frame> verify(std::string_view bid);
std::optional<Frame> verify_response(
    std::string_view oid, std::string_view did, std::string_view iid);
std::optional<Frame> scaleback(
    std::string_view exclusions, uint16_t head_max, uint16_t tail_max);
std::optional<Frame> acknowledge(std::string_view bid);
std::optional<Frame> exception(std::string_view reason);

} // namespace bus

namespace service {

std::optional<Frame> query(std::string_view capability);
std::optional<Frame> offer(std::string_view bid, std::string_view description);
std::optional<Frame> accept(std::string_view bid, std::string_view sender_bid = {});

} // namespace service

namespace session {

std::optional<Frame> call(std::string_view sid, std::string_view target_bid,
                          uint32_t mid, std::string_view payload);
std::optional<Frame> call_wrap(std::string_view sid, std::string_view target_bid);
std::optional<Frame> status(std::string_view sid, std::string_view target_bid,
                            uint32_t mid);
std::optional<Frame> status_response(std::string_view sid,
    std::string_view target_bid, uint32_t mid, std::string_view state);
std::optional<Frame> notify(std::string_view sid, std::string_view target_bid,
                            uint32_t mid, std::string_view event);
std::optional<Frame> locate(std::string_view sid);
std::optional<Frame> locate_response(std::string_view sid, std::string_view bid);
std::optional<Frame> resume(std::string_view sid);
std::optional<Frame> finish(std::string_view sid, std::string_view target_bid = {});

} // namespace session

/* ══════════════════════════════════════════════════════════════════
 *  Context — Stateful codec (v9 §8–§11)
 * ══════════════════════════════════════════════════════════════════ */

enum class SessionState { Idle, Active, Suspended };

inline constexpr int MAX_SESSIONS = 32;

class Context {
public:
    using MessageCb = std::function<void(
        std::string_view verb, std::string_view sid,
        std::string_view target_bid, uint32_t mid,
        std::string_view body)>;
    using OfferCb = std::function<void(
        std::string_view bid, std::string_view description)>;
    using EventCb = std::function<void(
        std::string_view verb, std::string_view id,
        std::string_view id2, std::string_view detail)>;

    Context(std::string_view oid, std::string_view did,
            std::string_view iid, std::string_view bid);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    std::string_view bid() const;

    void on_message(MessageCb cb);
    void on_offer(OfferCb cb);
    void on_event(EventCb cb);

    size_t feed(const uint8_t* data, size_t len);

    // Bus scope
    std::optional<Frame> establish();
    std::optional<Frame> broadcast(std::string_view text);
    std::optional<Frame> ping(std::string_view bid = {});
    std::optional<Frame> exception(std::string_view reason);
    std::optional<Frame> verify(std::string_view bid);
    std::optional<Frame> verify_response();
    std::optional<Frame> discover(std::string_view bid);
    std::optional<Frame> acknowledge(std::string_view bid);

    // Service scope
    std::optional<Frame> query(std::string_view capability);
    std::optional<Frame> offer(std::string_view description);
    std::optional<Frame> accept(std::string_view bid, std::string_view sender_bid = {});

    // Session scope
    int session_open();
    int session_accept(std::string_view sid, uint32_t inbound_mid);
    std::string_view session_sid(int slot) const;
    uint32_t session_mid(int slot) const;
    SessionState session_state(int slot) const;

    std::optional<Frame> session_call(int slot, std::string_view target_bid,
                                      std::string_view payload);
    std::optional<Frame> session_notify(int slot, std::string_view target_bid,
                                        std::string_view event);
    std::optional<Frame> session_status(int slot, std::string_view target_bid);
    std::optional<Frame> session_close(int slot, std::string_view target_bid = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace antheos
