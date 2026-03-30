# libantheos C++17

Updated: 2026-03-30

## What It Is

Pure C++17 implementation of the Antheos Protocol v1.0 (Level 1, v9 spec). Transport-agnostic
stateful codec for peer-to-peer messaging over any byte stream. Zero dependencies beyond libc.
No C11 layer underneath.

## Location

- Header: `include/antheos.hpp` (single public header)
- Implementation: `src/` (5 files)
- Tests: `tests/` (10 suites + test_common.hpp, 287 tests)
- Spec (canon): `docs/ANTHEOS_PROTOCOL_SPEC.md`
- Compiled: `libantheos.a` (static), `libantheos.so` (shared)

## Current State

### Source Files

| File | Lines | Purpose |
|------|-------|---------|
| wire.cpp | ~150 | Control chars, word types, validation, encode/decode |
| parser.cpp | ~260 | 11-state byte-at-a-time stream parser |
| identity.cpp | ~195 | FNV-1a, base-32, BID/SID generation, SidPool |
| bus.cpp | ~460 | All 28 frame builders: 14 bus + 2 relay-auth + 3 service + 9 session (19 protocol verbs + convenience variants + Level 2 relay) |
| context.cpp | ~370 | High-level stateful API: identity, sessions, feed, dispatch (incl. relay dispatch) |

### Protocol Verbs (19 total)

- **Bus (10):** E (establish), C (conflict), B (broadcast), P (ping), R (relay),
  D (discover), V (verify), S (scaleback), W (acknowledge), X (exception)
- **Service (3):** Q (query), O (offer), A (accept)
- **Session (6):** K (call), T (status), N (notify), L (locate), U (resume), F (finish)

### Identity Model

| ID | Scope | Format | Lifetime |
|----|-------|--------|----------|
| OID | Registry | ASCII | Registry-defined |
| DID | Origin | ASCII | Hardwired device type |
| IID | Origin | ASCII | Serial number |
| BID | Bus | Base-32 | Ephemeral (per connection) |
| SID | Instance | Base-32 | Session duration |
| MID | Session | Decimal | Per-message counter |

### Wire Format

7 reserved control characters (CP437): SOM (0x02), EOM (0x03), SOR (0x04),
SOU (0x07), EOW (0x10), SOW (0x12), SOB (0x1A).

11 word types: Symbol (!), Text ("), Integer (#), Real ($), Scientific (%),
Timestamp (&), Blob (*), Path (/), Logical (?), ID (@), Message (~).

Frame: `[SOM] Word₁ Word₂ ... Wordₙ [EOM] Tail`
Word: `[SOW] [WT] [SOR RF]? [SOU UF]? [SOB] Body [EOW]`

### API Design

- **Single public header** `antheos.hpp` — namespaces organize the modules
- **Stateless verb builders** are free functions returning `std::optional<Frame>`
- **Stateful components** are pimpl classes: `Parser`, `SidPool`, `Context`
- **`Frame`** — value type wrapping `std::vector<uint8_t>`
- **Error handling**: `std::optional<Frame>` for builders, exceptions for API misuse
- **Callbacks**: `std::function<>` with `std::string_view` parameters

### Build

```bash
make              # Build libantheos.a
make test         # Run all 287 tests
make install      # Install to /usr/local/lib + /usr/local/include/antheos/
# Flags: -std=c++17 -Wall -Wextra -Werror -O2 -fPIC
```

### Test Suites

| Suite | Tests | Purpose |
|-------|-------|---------|
| test_context | 50 | High-level API lifecycle, sessions, feed+callbacks, relay dispatch |
| test_conformance | 35 | Every wire example from v9 spec verified byte-for-byte |
| test_edge | 36 | Boundary conditions, C++ API semantics |
| test_wire | 26 | Encoding/decoding round-trips incl. MESSAGE word |
| test_bus | 27 | Bus frame builders incl. relay_auth_challenge/response |
| test_session | 14 | K/T/N/L/U/F session verbs, MID wrap |
| test_parser | 12 | Stream parser state machine |
| test_identity | 15 | Base-32, BID/SID generation, rotation |
| test_service | 7 | Q/O/A service verbs |
| test_depth | 63 | Frame class, tail handling, move semantics, session_accept, dispatch, decode edge cases |

## Interfaces

- **Consumed by:** Any C++17 code that needs Antheos protocol framing
- **Verb builders** return `std::optional<Frame>` (nullopt on error)
- **Callback-driven:** `std::function` for Parser (word/tail/message) and Context (message/offer/event/relay)

## Design Notes

| Aspect | Choice |
|--------|--------|
| Header count | 1 single header (`antheos.hpp`) |
| Error handling | `optional<Frame>` for builders, exceptions for API misuse |
| Callbacks | `std::function` with `string_view` parameters |
| Memory | RAII (pimpl, vector, string) |
| Buffer model | Frame value type (heap-backed) |
| Build output | libantheos.a (static) |
| Platform deps | None — pure C++17 standard library only |
| Entropy | Caller-provided (`bid_generate` takes raw bytes) |
| Tests | 287 across 10 suites |

## Level 2 Extensions

### Auth (Z-verb, Appendix A)
- `bus::auth_challenge()` / `bus::auth_response()` — Ed25519 challenge-response frames
- Context dispatches Z-verb through `on_event` callback

### Relay + Auth (multi-hop Z-verb)
- `wire::encode_message()` — MESSAGE (~) word encoder for embedded verb references
- `bus::relay_auth_challenge()` / `bus::relay_auth_response()` — Z-verb wrapped in Relay
  - Wire: `[!R] [@U bid] [~Z] ["nonce] [#index] [/path]` (challenge)
  - Wire: `[!R] [@U bid] [@ key_id] [~Z] ["sig] [#index] [/path]` (response)
- `Context::on_relay(RelayCb)` — dispatches relay frames with MESSAGE word
  - Callback receives: message_ref (char), id, id2, body, index, path
  - Plain text relay (no MESSAGE word) falls through to `on_event`

## Known Issues

- Parser enters ERROR state on unexpected SOM mid-parse (recovers by scanning for next SOM)
- SID hash collisions theoretically possible at short lengths (FNV-1a on "OID:DID:IID:counter", mitigated by LE byte order and length growth on collision)
- Empty capability string accepted in Q-verb (no validation)
- No trace integration yet (deferred)
