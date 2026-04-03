/*
 * wire.cpp — Antheos Protocol v9 Wire Encoding
 *
 * Control characters, word types, radix/unit flags, and wire-format
 * encoding/decoding per Antheos Protocol 1.0 Level 1, v9 specification.
 *
 * Copyright (c) 2025-2026 Are Bjørby <are.bjorby@langsyn.org>
 * SPDX-License-Identifier: MIT
 */

#include "antheos.hpp"
#include <cstring>

namespace antheos::wire {

/* ── Validation ── */

bool is_reserved(uint8_t byte) {
    switch (byte) {
        case SOM: case EOM: case SOR: case SOU:
        case EOW: case SOW: case SOB:
            return true;
        default:
            return false;
    }
}

bool is_valid_word_type(uint8_t byte) {
    switch (byte) {
        case static_cast<uint8_t>(WordType::Symbol):
        case static_cast<uint8_t>(WordType::Text):
        case static_cast<uint8_t>(WordType::Integer):
        case static_cast<uint8_t>(WordType::Real):
        case static_cast<uint8_t>(WordType::Scientific):
        case static_cast<uint8_t>(WordType::Timestamp):
        case static_cast<uint8_t>(WordType::Blob):
        case static_cast<uint8_t>(WordType::Path):
        case static_cast<uint8_t>(WordType::Logical):
        case static_cast<uint8_t>(WordType::Id):
        case static_cast<uint8_t>(WordType::Message):
            return true;
        default:
            return false;
    }
}

bool is_radix_flag(uint8_t byte) {
    switch (byte) {
        case static_cast<uint8_t>(Radix::Binary):
        case static_cast<uint8_t>(Radix::Octal):
        case static_cast<uint8_t>(Radix::Decimal):
        case static_cast<uint8_t>(Radix::Hex):
        case static_cast<uint8_t>(Radix::Base32):
            return true;
        default:
            return false;
    }
}

bool is_unit_flag(uint8_t byte) {
    switch (byte) {
        case static_cast<uint8_t>(Unit::Byte):
        case static_cast<uint8_t>(Unit::Word):
        case static_cast<uint8_t>(Unit::Dword):
        case static_cast<uint8_t>(Unit::Qword):
        case static_cast<uint8_t>(Unit::Megabyte):
        case static_cast<uint8_t>(Unit::Gigabyte):
        case static_cast<uint8_t>(Unit::Terabyte):
            return true;
        default:
            return false;
    }
}

/* ── Flag requirement rules (v9 §6.1) ── */

bool needs_radix(WordType type) {
    switch (type) {
        case WordType::Integer:
        case WordType::Real:
        case WordType::Scientific:
        case WordType::Blob:
            return true;
        default:
            return false;
    }
}

bool needs_unit(WordType type) {
    switch (type) {
        case WordType::Integer:
        case WordType::Real:
        case WordType::Scientific:
        case WordType::Blob:
            return true;
        default:
            return false;
    }
}

bool allows_radix(WordType type) {
    switch (type) {
        case WordType::Integer:
        case WordType::Real:
        case WordType::Scientific:
        case WordType::Blob:
        case WordType::Id:
            return true;
        default:
            return false;
    }
}

/* ── Word encoding — v9 wire format ──
 *
 *  [SOW][WT]([SOR][RF])([SOU][UF])[SOB]Body[EOW]
 */

std::optional<std::vector<uint8_t>> word_encode(
    WordType type, Radix radix, Unit unit,
    const uint8_t* body, size_t body_len) {

    if (!is_valid_word_type(static_cast<uint8_t>(type)))
        return std::nullopt;

    uint8_t rf = static_cast<uint8_t>(radix);
    uint8_t uf = static_cast<uint8_t>(unit);

    if (rf != 0) {
        if (!allows_radix(type)) return std::nullopt;
        if (!is_radix_flag(rf))  return std::nullopt;
    }

    if (uf != 0) {
        if (!needs_unit(type))  return std::nullopt;
        if (!is_unit_flag(uf))  return std::nullopt;
    }

    for (size_t i = 0; i < body_len; i++) {
        if (is_reserved(body[i])) return std::nullopt;
    }

    /* SOW + WT + SOB + EOW + body + optional SOR/RF + optional SOU/UF */
    size_t cap = 4 + body_len;
    if (rf != 0) cap += 2;
    if (uf != 0) cap += 2;

    std::vector<uint8_t> out;
    out.reserve(cap);

    out.push_back(SOW);
    out.push_back(static_cast<uint8_t>(type));

    if (rf != 0) {
        out.push_back(SOR);
        out.push_back(rf);
    }

    if (uf != 0) {
        out.push_back(SOU);
        out.push_back(uf);
    }

    out.push_back(SOB);

    if (body_len > 0 && body != nullptr)
        out.insert(out.end(), body, body + body_len);

    out.push_back(EOW);

    return out;
}

/* ── Word decoding — v9 wire format ── */

std::optional<DecodedWord> word_decode(const uint8_t* in, size_t in_len) {
    if (in == nullptr || in_len < 4)
        return std::nullopt;

    size_t pos = 0;

    if (in[pos++] != SOW)
        return std::nullopt;

    uint8_t wt = in[pos++];
    if (!is_valid_word_type(wt))
        return std::nullopt;

    DecodedWord dw;
    dw.type = static_cast<WordType>(wt);
    dw.radix = Radix::None;
    dw.unit = Unit::None;

    if (pos < in_len && in[pos] == SOR) {
        pos++;
        if (pos >= in_len) return std::nullopt;
        if (!is_radix_flag(in[pos])) return std::nullopt;
        dw.radix = static_cast<Radix>(in[pos++]);
    }

    if (pos < in_len && in[pos] == SOU) {
        pos++;
        if (pos >= in_len) return std::nullopt;
        if (!is_unit_flag(in[pos])) return std::nullopt;
        dw.unit = static_cast<Unit>(in[pos++]);
    }

    if (pos >= in_len || in[pos] != SOB)
        return std::nullopt;
    pos++;

    size_t body_start = pos;
    while (pos < in_len && in[pos] != EOW) {
        if (is_reserved(in[pos])) return std::nullopt;
        pos++;
    }

    if (pos >= in_len)
        return std::nullopt;

    dw.body.assign(in + body_start, in + pos);

    return dw;
}

/* ── Convenience encoders ── */

std::optional<std::vector<uint8_t>> encode_symbol(char verb) {
    uint8_t body = static_cast<uint8_t>(verb);
    return word_encode(WordType::Symbol, Radix::None, Unit::None, &body, 1);
}

std::optional<std::vector<uint8_t>> encode_text(std::string_view text) {
    return word_encode(WordType::Text, Radix::None, Unit::None,
                       reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::optional<std::vector<uint8_t>> encode_id_plain(std::string_view id) {
    return word_encode(WordType::Id, Radix::None, Unit::None,
                       reinterpret_cast<const uint8_t*>(id.data()), id.size());
}

std::optional<std::vector<uint8_t>> encode_id_base32(std::string_view id) {
    return word_encode(WordType::Id, Radix::Base32, Unit::None,
                       reinterpret_cast<const uint8_t*>(id.data()), id.size());
}

std::optional<std::vector<uint8_t>> encode_id_decimal(uint32_t value) {
    auto s = std::to_string(value);
    return word_encode(WordType::Id, Radix::Decimal, Unit::None,
                       reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

std::optional<std::vector<uint8_t>> encode_integer(
    Radix radix, Unit unit, std::string_view value) {
    return word_encode(WordType::Integer, radix, unit,
                       reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

std::optional<std::vector<uint8_t>> encode_logical(std::string_view expr) {
    return word_encode(WordType::Logical, Radix::None, Unit::None,
                       reinterpret_cast<const uint8_t*>(expr.data()), expr.size());
}

std::optional<std::vector<uint8_t>> encode_path(std::string_view path) {
    return word_encode(WordType::Path, Radix::None, Unit::None,
                       reinterpret_cast<const uint8_t*>(path.data()), path.size());
}

std::optional<std::vector<uint8_t>> encode_message(std::string_view ref) {
    if (ref.empty()) return std::nullopt;
    return word_encode(WordType::Message, Radix::None, Unit::None,
                       reinterpret_cast<const uint8_t*>(ref.data()), ref.size());
}

} // namespace antheos::wire
