# libantheos

C++17 implementation of the Antheos Protocol v1.0 (Level 1).

Transport-agnostic frame codec: bytes in, bytes out. Zero dependencies beyond libc.

## Build

```bash
make              # Build libantheos.a
make test         # Build and run all 201 tests
make install      # Install to /usr/local (sudo required)
make clean        # Remove build artifacts
```

Requires: `g++` with C++17 support.

## API

Single public header: `#include <antheos/antheos.hpp>`

```
antheos::wire       Control chars, word types, encode/decode, validation
antheos::id         Base-32 encoding, BID/SID generation
antheos::bus        10 bus verb builders (E, C, B, P, R, D, V, S, W, X)
antheos::service    3 service verb builders (Q, O, A)
antheos::session    6 session verb builders (K, T, N, L, U, F)
antheos::Frame      Value type wrapping vector<uint8_t>
antheos::Parser     Byte-at-a-time stream parser (pimpl)
antheos::SidPool    Session ID recycling pool (pimpl)
antheos::Context    Stateful codec — owns identity, sessions, parser (pimpl)
```

Verb builders return `std::optional<Frame>` (nullopt on error).
`Context` provides the high-level stateful API: identity, session management,
outbound frame building, and inbound feed with callback dispatch.

## Protocol

See `docs/ANTHEOS_PROTOCOL_SPEC.md` for the canonical protocol specification.

## License

Copyright (c) 2025-2026 Are Bjorby. All rights reserved. See `LICENSE`.
