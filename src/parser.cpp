/*
 * parser.cpp — Antheos Protocol v9 Stream Parser
 *
 * Byte-at-a-time state machine for v9 wire format:
 *   [SOM] Word₁ Word₂ ... Wordₙ [EOM] Tail
 *
 * Copyright (c) 2025-2026 Are Bjørby <are.bjorby@langsyn.org>
 * SPDX-License-Identifier: MIT
 */

#include "antheos.hpp"
#include <cstring>

namespace antheos {

static constexpr size_t WORD_BUF_INIT = 256;
static constexpr size_t TAIL_BUF_SIZE = 4096;

struct Parser::Impl {
    ParseState state = ParseState::WaitSom;

    /* Current word being parsed */
    wire::WordType current_word_type{};
    wire::Radix radix_flag = wire::Radix::None;
    wire::Unit unit_flag = wire::Unit::None;

    /* Word body buffer */
    std::vector<uint8_t> word_buffer;

    /* Tail handling */
    size_t tail_bytes_expected = 0;
    size_t tail_bytes_received = 0;
    uint8_t tail_buffer[TAIL_BUF_SIZE]{};

    /* Per-message counter */
    size_t word_count = 0;

    /* Lifetime statistics */
    size_t total_words_parsed = 0;
    size_t total_messages = 0;
    size_t parse_errors_ = 0;

    /* Callbacks */
    Parser::WordCb word_cb;
    Parser::TailCb tail_cb;
    Parser::MessageCb message_cb;

    Impl() { word_buffer.reserve(WORD_BUF_INIT); }

    void enter_error() {
        parse_errors_++;
        state = ParseState::Error;
    }

    void complete_word() {
        if (word_cb) {
            word_cb(current_word_type, radix_flag, unit_flag,
                    word_buffer.data(), word_buffer.size());
        }
        word_count++;
        total_words_parsed++;
        state = ParseState::WaitSow;
    }

    void complete_message() {
        if (message_cb) {
            message_cb(word_count);
        }
        total_messages++;
        word_count = 0;
        tail_bytes_expected = 0;
        tail_bytes_received = 0;
        state = ParseState::WaitSom;
    }

    ParseState feed_byte(uint8_t byte) {
        using namespace wire;

        switch (state) {

        case ParseState::WaitSom:
            if (byte == SOM) {
                word_count = 0;
                state = ParseState::WaitSow;
            }
            break;

        case ParseState::WaitSow:
            if (byte == SOW) {
                current_word_type = static_cast<wire::WordType>(0);
                radix_flag = Radix::None;
                unit_flag = Unit::None;
                word_buffer.clear();
                state = ParseState::WordType;
            } else if (byte == EOM) {
                if (tail_bytes_expected > 0) {
                    tail_bytes_received = 0;
                    state = ParseState::Tail;
                } else {
                    complete_message();
                }
            } else {
                enter_error();
            }
            break;

        case ParseState::WordType:
            if (is_valid_word_type(byte)) {
                current_word_type = static_cast<wire::WordType>(byte);
                state = ParseState::AfterType;
            } else {
                enter_error();
            }
            break;

        case ParseState::AfterType:
            if (byte == SOR) {
                state = ParseState::RadixFlag;
            } else if (byte == SOU) {
                state = ParseState::UnitFlag;
            } else if (byte == SOB) {
                state = ParseState::WordBody;
            } else if (byte == EOW) {
                complete_word();
            } else {
                enter_error();
            }
            break;

        case ParseState::RadixFlag:
            if (is_radix_flag(byte)) {
                radix_flag = static_cast<Radix>(byte);
                state = ParseState::AfterRadix;
            } else {
                enter_error();
            }
            break;

        case ParseState::AfterRadix:
            if (byte == SOU) {
                state = ParseState::UnitFlag;
            } else if (byte == SOB) {
                state = ParseState::WordBody;
            } else {
                enter_error();
            }
            break;

        case ParseState::UnitFlag:
            if (is_unit_flag(byte)) {
                unit_flag = static_cast<Unit>(byte);
                state = ParseState::WaitSob;
            } else {
                enter_error();
            }
            break;

        case ParseState::WaitSob:
            if (byte == SOB) {
                state = ParseState::WordBody;
            } else {
                enter_error();
            }
            break;

        case ParseState::WordBody:
            if (byte == EOW) {
                complete_word();
            } else if (is_reserved(byte)) {
                enter_error();
            } else {
                word_buffer.push_back(byte);
            }
            break;

        case ParseState::Tail:
            if (tail_bytes_received < TAIL_BUF_SIZE) {
                tail_buffer[tail_bytes_received++] = byte;
            } else {
                enter_error();
                break;
            }
            if (tail_bytes_received == tail_bytes_expected) {
                if (tail_cb) {
                    tail_cb(tail_buffer, tail_bytes_received);
                }
                complete_message();
            }
            break;

        case ParseState::Error:
            if (byte == SOM) {
                word_count = 0;
                word_buffer.clear();
                radix_flag = Radix::None;
                unit_flag = Unit::None;
                state = ParseState::WaitSow;
            }
            break;
        }

        return state;
    }
};

/* ── Public API ── */

Parser::Parser() : impl_(std::make_unique<Impl>()) {}
Parser::~Parser() = default;
Parser::Parser(Parser&&) noexcept = default;
Parser& Parser::operator=(Parser&&) noexcept = default;

void Parser::on_word(WordCb cb) { impl_->word_cb = std::move(cb); }
void Parser::on_tail(TailCb cb) { impl_->tail_cb = std::move(cb); }
void Parser::on_message(MessageCb cb) { impl_->message_cb = std::move(cb); }

ParseState Parser::feed(uint8_t byte) {
    return impl_->feed_byte(byte);
}

ParseState Parser::feed(const uint8_t* data, size_t len) {
    ParseState s = impl_->state;
    for (size_t i = 0; i < len; i++)
        s = impl_->feed_byte(data[i]);
    return s;
}

void Parser::reset() {
    /* Preserve callbacks and lifetime counters */
    auto word_cb = std::move(impl_->word_cb);
    auto tail_cb = std::move(impl_->tail_cb);
    auto message_cb = std::move(impl_->message_cb);
    size_t tw = impl_->total_words_parsed;
    size_t tm = impl_->total_messages;
    size_t te = impl_->parse_errors_;

    impl_ = std::make_unique<Impl>();

    impl_->word_cb = std::move(word_cb);
    impl_->tail_cb = std::move(tail_cb);
    impl_->message_cb = std::move(message_cb);
    impl_->total_words_parsed = tw;
    impl_->total_messages = tm;
    impl_->parse_errors_ = te;
}

void Parser::set_tail_length(size_t len) {
    impl_->tail_bytes_expected = len;
    impl_->tail_bytes_received = 0;
}

ParseState Parser::state() const { return impl_->state; }

size_t Parser::total_words() const { return impl_->total_words_parsed; }
size_t Parser::total_messages() const { return impl_->total_messages; }
size_t Parser::parse_errors() const { return impl_->parse_errors_; }

} // namespace antheos
