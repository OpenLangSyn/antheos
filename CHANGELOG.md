# Changelog

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
