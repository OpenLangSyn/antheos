# Contributing to libantheos

## Architecture

```
antheos.hpp (single public header)

Namespaces:
  wire::      Control chars, word types, encode/decode, validation
  exc::       15 standard exception codes (spec §13 + Appendix A)
  id::        Base-32 encoding, BID/SID generation (caller-provided entropy)
  bus::       Bus verb builders (14 Level 1 + Level 2 auth + relay variants)
  service::   Service verb builders (Q, O, A)
  session::   Session verb builders (K, T, N, L, U, F + MID wrap)

Classes:
  Frame       Value type wrapping vector<uint8_t>
  Parser      Byte-at-a-time stream parser (pimpl)
  SidPool     Session ID generator — fresh per session, never reused (pimpl)
  Context     Stateful codec — identity, sessions, feed, dispatch (pimpl)
```

## Source Files

| File | Purpose |
|------|---------|
| wire.cpp | Control chars, word types, validation, encode/decode |
| parser.cpp | 11-state byte-at-a-time stream parser |
| identity.cpp | FNV-1a, base-32, BID/SID generation, SidPool |
| bus.cpp | All frame builders: bus + service + session verbs |
| context.cpp | High-level stateful API: identity, sessions, feed, dispatch |

## Canonical Spec

[`docs/ANTHEOS_PROTOCOL_SPEC.md`](docs/ANTHEOS_PROTOCOL_SPEC.md) is the protocol specification.
All implementation decisions defer to this document.

## Build

```bash
make              # Build libantheos.a + libantheos.so
make test         # Build and run all test suites
make install      # Install library + header to /usr/local (sudo)
make uninstall    # Remove installed files (sudo)
make clean        # Remove build artifacts
```

## Coding Standards

- **C++17**, compiled with `-Wall -Wextra -Werror -O2 -fPIC`
- **No external dependencies** — C++17 standard library only
- **POSIX dependency**: `/dev/urandom` for BID/SID entropy generation
- **Single public header**: `antheos.hpp`
- Frame builders return `std::optional<Frame>` (nullopt on error)
- Parser is byte-at-a-time with `std::function` callbacks
- Context takes OID, DID, IID, BID — `id::bid_generate(len)` generates internally
- `bid_generate()` and `sid_generate()` have overloads for caller-provided entropy (testing)
- Reserved bytes (7 total: 0x02 0x03 0x04 0x07 0x10 0x12 0x1A) rejected in word bodies
- No commented-out code, no bare TODOs, no debug prints
- Library is transport-agnostic: no sockets, no threads

## Adding a New Verb Builder

1. Add the function declaration to `include/antheos.hpp` in the appropriate namespace
2. Implement in `src/bus.cpp` (all verb builders live here)
3. Add wire-format tests to the matching test file (`tests/test_bus.cpp`, etc.)
4. If the verb has a spec wire example, add a conformance test to `tests/test_conformance.cpp`
5. If Context should dispatch it, update the dispatch logic in `src/context.cpp`

## Adding Tests

Tests use a lightweight custom framework (`tests/test_common.hpp`):

```cpp
static void test_my_feature() {
    auto f = bus::some_verb("arg");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->data()[4], 0x58);  // Expected verb byte
}
```

Register in the suite runner function (e.g., `test_bus_run`) and it will be included
in `make test`.

## Test Suites

| Suite | Tests | What It Tests |
|-------|------:|--------------|
| test_wire | 26 | Wire encoding constants, encode/decode round-trips |
| test_parser | 12 | Stream parser state machine, recovery from malformed input |
| test_identity | 15 | Base-32 encoding, BID/SID generation, entropy, SID pool |
| test_bus | 36 | All bus frame builders, exception code constants |
| test_service | 7 | Q/O/A service verb frame builders |
| test_session | 14 | K/T/N/L/U/F session verb builders, MID wrap |
| test_conformance | 35 | Every wire example from v9 spec verified byte-for-byte |
| test_edge | 36 | Boundary conditions, C++ API semantics |
| test_context | 50 | High-level API: lifecycle, sessions, feed+callbacks |
| test_depth | 65 | Frame class, tail handling, move semantics, decode edge cases |

## Before Submitting

- Run `make clean && make test` — all tests must pass
- Wire format changes require updating `test_conformance.cpp` to match
- New public API requires documentation in `antheos.hpp` header comments
