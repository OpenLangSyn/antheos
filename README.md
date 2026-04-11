# libantheos

C++17 reference implementation of the [Antheos Protocol](docs/ANTHEOS_PROTOCOL_SPEC.md) v1.0 Level 1.

Transport-agnostic frame codec: bytes in, bytes out. Zero dependencies beyond the C++17 standard library. No POSIX, no platform-specific code. Runs on anything from 8-bit MCUs to server clusters.

**Author:** Are Bjørby <are.bjorby@langsyn.org>

## Features

- **19 Level 1 verbs** across 3 scopes (bus, service, session)
- **Level 2 Z-verb** (Ed25519 challenge-response authentication)
- **11 word types** with full radix/unit flag support
- **15 named exception codes** (`antheos::exc::` namespace, spec section 13)
- **11-state stream parser** (byte-at-a-time, no message buffering required)
- **Session management** (32 concurrent sessions, MID sequencing with wrap)
- **SID generation** (FNV-1a hash, collision-safe length growth)
- **Relay routing** (path expansion, index decrement, multi-hop auth)
- **296 tests** including 35 byte-for-byte spec conformance checks

## Requirements

- C++17 compiler (g++ or clang++)
- GNU Make
- No external libraries

## Quick Start

```bash
make              # Build libantheos.a + libantheos.so
make test         # Run all 296 tests
sudo make install # Install to /usr/local/lib + /usr/local/include/antheos/
make clean        # Remove build artifacts
```

## Usage

```cpp
#include <antheos/antheos.hpp>

// Generate a BID (reads /dev/urandom internally)
auto bid = antheos::id::bid_generate(4);

// Create a context (OID, DID, IID, BID)
antheos::Context ctx("myorg", "sensor", "unit1", *bid);

// Build frames — send the bytes over your transport
auto est = ctx.establish();                      // Claim BID on bus
auto qry = ctx.query("temperature");             // Find a service

// Open a session and make calls
int slot = ctx.session_open();
auto call = ctx.session_call(slot, {}, "READ");  // K-verb with payload
auto fin  = ctx.session_close(slot);             // F-verb

// Parse inbound bytes
ctx.on_message([](auto verb, auto sid, auto bid, uint32_t mid, auto body) {
    // Handle inbound session messages (K, N, T)
});
ctx.on_event([](auto verb, auto id, auto id2, auto detail) {
    // Handle bus/service events (E, C, B, P, D, V, S, W, X, Q, A, L, U, F)
});
ctx.feed(received_bytes, len);

// Use named exception codes
auto err = ctx.exception(antheos::exc::SESSION_NOT_FOUND);
```

## API Overview

```
antheos::wire       Control characters, word types, encode/decode, validation
antheos::exc        15 standard exception codes (spec section 13 + Appendix A)
antheos::id         Base-32 encoding, BID/SID generation
antheos::bus        Bus verb builders (E, C, B, P, R, D, V, S, W, X, Z)
antheos::service    Service verb builders (Q, O, A)
antheos::session    Session verb builders (K, T, N, L, U, F)
antheos::Frame      Value type wrapping encoded wire bytes
antheos::Parser     Byte-at-a-time stream parser (pimpl)
antheos::SidPool    Session ID generator with collision handling (pimpl)
antheos::Context    Stateful codec — identity, sessions, parser, dispatch (pimpl)
```

All verb builders return `std::optional<Frame>` (`nullopt` on validation error).

## Examples

See [`examples/basic_session.cpp`](examples/basic_session.cpp) for a complete working example.

```bash
# Build and run the example (after make install)
g++ -std=c++17 -o basic_session examples/basic_session.cpp -lantheos
./basic_session
```

## Protocol

The canonical protocol specification is at [`docs/ANTHEOS_PROTOCOL_SPEC.md`](docs/ANTHEOS_PROTOCOL_SPEC.md).

Key concepts:
- **Bus**: A shared byte stream. Instances claim ephemeral BIDs via collision detection.
- **Service**: Demand-driven discovery via Query/Offer/Accept (no advertising).
- **Session**: Stateful message exchange with sequential MIDs and suspend/resume.
- **Wire format**: CP437-encoded head + optional binary tail. 7 reserved control characters.

## Testing

```bash
make test    # Runs all 10 test suites
```

| Suite | Tests | Coverage |
|-------|------:|----------|
| test_wire | 26 | Control chars, word encoding/decoding, flag rules |
| test_parser | 12 | Stream parser state machine, error recovery |
| test_identity | 15 | Base-32, BID/SID generation, collision handling |
| test_bus | 36 | All bus verb builders + exception code constants |
| test_service | 7 | Q/O/A service verbs |
| test_session | 14 | K/T/N/L/U/F session verbs, MID wrap |
| test_conformance | 35 | Byte-for-byte verification of spec wire examples |
| test_edge | 36 | Boundary conditions, reserved bytes, C++ semantics |
| test_context | 50 | High-level API lifecycle, sessions, dispatch |
| test_depth | 65 | Frame class, tail handling, move semantics, decode edge cases |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for architecture, coding standards, and how to add new features.

## License

MIT License. Copyright (c) 2025-2026 Are Bjorby. See [LICENSE](LICENSE).
