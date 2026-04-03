/*
 * basic_session.cpp — Minimal Antheos session example
 *
 * Demonstrates: init, establish, query, session open/call/close.
 * This example builds frames in memory. In a real system,
 * you would send these bytes over your chosen transport
 * (shared memory, serial, TCP, etc.) and feed received bytes
 * into the parser.
 *
 * Build: g++ -std=c++17 -o basic_session basic_session.cpp -lantheos
 * (requires: sudo make install in the libantheos root)
 *
 * Copyright (c) 2025-2026 Are Bjørby <are.bjorby@langsyn.org>
 * SPDX-License-Identifier: MIT
 */

#include <antheos/antheos.hpp>
#include <cstdio>

static void print_hex(const char* label, const antheos::Frame& frame) {
    std::printf("%s (%zu bytes): ", label, frame.size());
    for (size_t i = 0; i < frame.size(); i++)
        std::printf("%02X ", frame[i]);
    std::printf("\n");
}

int main() {
    /* Step 1: Create context with identity (OID, DID, IID, BID)
     * BID is caller-provided — generate from your platform's entropy source,
     * or use antheos::id::bid_generate() with raw entropy bytes. */
    uint8_t entropy[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA};
    auto bid = antheos::id::bid_generate(8, entropy, sizeof(entropy));
    if (!bid) { std::fprintf(stderr, "BID generation failed\n"); return 1; }
    antheos::Context ctx("example", "demo", "main", *bid);
    std::printf("Initialized: BID=%.*s\n",
        static_cast<int>(ctx.bid().size()), ctx.bid().data());

    /* Step 2: Establish a BID on the bus */
    auto est = ctx.establish();
    if (est) {
        print_hex("ESTABLISH", *est);
        std::printf("BID: %.*s\n\n",
            static_cast<int>(ctx.bid().size()), ctx.bid().data());
    }

    /* Step 3: Query for a service */
    auto qry = ctx.query("knowledge");
    if (qry)
        print_hex("QUERY 'knowledge'", *qry);
    std::printf("\n");

    /* Step 4: Open a session */
    int slot = ctx.session_open();
    if (slot < 0) {
        std::fprintf(stderr, "Failed to open session\n");
        return 1;
    }
    auto sid = ctx.session_sid(slot);
    std::printf("Session opened: slot=%d SID=%.*s\n\n",
        slot, static_cast<int>(sid.size()), sid.data());

    /* Step 5: Make a Call within the session */
    auto call = ctx.session_call(slot, {}, "PING");
    if (call)
        print_hex("CALL 'PING'", *call);
    std::printf("\n");

    /* Step 6: Send a Notify */
    auto notif = ctx.session_notify(slot, {}, "HEARTBEAT");
    if (notif)
        print_hex("NOTIFY 'HEARTBEAT'", *notif);
    std::printf("\n");

    /* Step 7: Close the session */
    auto fin = ctx.session_close(slot);
    if (fin)
        print_hex("FINISH", *fin);
    std::printf("\n");

    /* Context destructor handles cleanup (RAII) */
    std::printf("Done. All frames are valid Antheos v9 wire format.\n");

    return 0;
}
