# Changelog

## v1.0.1 — 2026-04-03

SID entropy — unpredictable session IDs.

- `sid_generate()` accepts optional entropy bytes, mixed into FNV-1a hash
- `SidPool` reads 8 bytes from `/dev/urandom` on each `acquire()` (POSIX dependency)
- `bid_generate(len)` overload with internal `/dev/urandom` entropy
- `O:<BID>\n` body-header convention for session ownership declaration
- Protocol spec §16 updated: SIDs now entropy-mixed, `O:` header documented
- 300 tests across 10 suites (+4 entropy tests)

## v1.0.0 — 2026-04-03

Open-source release under MIT license.

- Added `antheos::exc` namespace with 15 named exception codes (spec §13 + Appendix A)
- MIT license (was proprietary)
- SPDX-License-Identifier headers in all source files
- README rewrite for external audience
- CONTRIBUTING.md (was CLAUDE.md)
- 296 tests across 10 suites

## Pre-release History

| Date | Change |
|------|--------|
| 2026-03-30 | AP-12: Multi-bus-hop Z-verb relay. MESSAGE (~) word, relay_auth builders, Context relay dispatch. 287 tests. |
| 2026-03-21 | AC-10: SID rotation — removed recycling, fresh SID per session, little-endian byte order fix. 268 tests. |
| 2026-03-16 | AC-09: Purity audit + depth test suite. 268 tests. |
| 2026-03-16 | AC-08: Standalone purity — caller-provided entropy, removed POSIX deps. |
| 2026-03-16 | AC-06: Context — stateful codec with session management. |
| 2026-03-16 | AC-05: Conformance + edge test suites (71 tests). |
| 2026-03-16 | AC-04: Verb builders — bus/service/session (39 tests). |
| 2026-03-16 | AC-03: Identity — base-32, BID/SID, SidPool (12 tests). |
| 2026-03-16 | AC-02: Stream parser (12 tests). |
| 2026-03-16 | AC-01: Wire encoding + Makefile skeleton (24 tests). |
