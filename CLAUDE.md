# libantheos — Claude Code Context

## What is this?
Standalone C++17 implementation of the Antheos Protocol v1.0 Level 1 specification.
Transport-agnostic frame codec: bytes in, bytes out. Zero external dependencies —
pure C++17 standard library only. No POSIX, no platform-specific code.

## Canonical spec
`docs/ANTHEOS_PROTOCOL_SPEC.md` — the protocol specification. All implementation decisions defer to this document.

## Build
```bash
make              # Build libantheos.a
make test         # Build and run all test suites (205 tests)
make install      # Install library + header to /usr/local (sudo)
make uninstall    # Remove installed files (sudo)
make clean        # Remove build artifacts
```

## Directory structure
- `include/antheos.hpp` — Single public header
- `src/` — Implementation (5 files)
- `tests/` — Test suites (9 files + test_common.hpp, 205 tests)
- `docs/` — Baseline, changes, recovery

## Architecture
```
antheos.hpp (single header) — all public API in antheos:: namespace

Namespaces:
  wire::      Control chars, word types, encode/decode, validation
  id::        Base-32 encoding, BID/SID generation (caller-provided entropy)
  bus::       14 stateless bus verb builders
  service::   3 stateless service verb builders
  session::   9 stateless session verb builders

Classes:
  Frame       Value type wrapping vector<uint8_t>
  Parser      Byte-at-a-time stream parser (pimpl)
  SidPool     Session ID recycling pool (pimpl)
  Context     Stateful codec — caller provides BID, owns SidPool, Parser, 32 sessions (pimpl)
```

## Source files
| File | Purpose |
|------|---------|
| wire.cpp | Control chars, word types, encode/decode, validation |
| parser.cpp | 11-state byte-at-a-time stream parser |
| identity.cpp | FNV-1a, base-32, BID/SID generation, SidPool |
| bus.cpp | All 19 verb frame builders (bus + service + session) |
| context.cpp | High-level stateful API: identity, sessions, feed, dispatch |

## Coding standards
- C++17, compiled with `-Wall -Wextra -Werror -O2 -fPIC`
- No external dependencies (C++17 standard library only)
- Single public header: `antheos.hpp`
- Frame builders return `std::optional<Frame>` (nullopt on error)
- Parser is byte-at-a-time, `std::function` callbacks
- Context takes OID, DID, IID, BID — caller generates the BID via `id::bid_generate()`
- `bid_generate()` takes caller-provided entropy bytes (platform-agnostic)
- Reserved bytes (7 total: 0x02 0x03 0x04 0x07 0x10 0x12 0x1A) rejected in word bodies

## Test suites
| File | Tests | What it tests |
|------|-------|--------------|
| test_wire.cpp | 24 | Wire encoding constants, encode/decode round-trips |
| test_parser.cpp | 12 | Stream parser state machine, recovery from malformed input |
| test_identity.cpp | 15 | Base-32 encoding, BID/SID generation, entropy, SID pool |
| test_bus.cpp | 18 | All 10 bus verb frame builders, wire format verification |
| test_service.cpp | 7 | Q/O/A service verb frame builders |
| test_session.cpp | 14 | K/T/N/L/U/F session verb builders, MID wrap |
| test_conformance.cpp | 35 | Every wire example from v9 spec verified byte-for-byte |
| test_edge.cpp | 36 | Boundary conditions, C++ API semantics |
| test_context.cpp | 44 | High-level API: lifecycle, sessions, feed+callbacks |

## Rules for modifications
- Run `make clean && make test` before every commit — all 205 tests must pass
- Wire format changes require updating test_conformance.cpp to match
- No commented-out code, no bare TODOs, no debug prints
- Library is transport-agnostic: no sockets, no file I/O, no threads
- No platform-specific code: no POSIX headers, no `/dev/*`, no `#ifdef` for OS
