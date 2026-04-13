# Sharechain Data Model, Storage, and P2P Relay

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

Before the miner can produce shares or the block assembler can build reward windows, RNG needs a way to store, validate, and relay share records. After this plan, nodes can receive shares over P2P, validate their RandomX proofs, store them in LevelDB, track the best share tip by cumulative work, buffer orphans, and relay accepted shares to peers -- all gated behind the sharepool deployment activation.

## Requirements Trace

`R1`. `ShareRecord` must contain: version, parent_share, prev_block_hash, candidate_header, share_nBits, payout_script.

`R2`. Share validation must check: version, nBits bounds, RandomX proof against share target, parent chain anchoring.

`R3`. Best tip selection by cumulative share work with deterministic tie break.

`R4`. Orphan buffer of max 64 shares with FIFO eviction.

`R5`. P2P messages: `shareinv`, `getshare`, `share` with batch limits.

`R6`. All relay gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`.

## Scope Boundaries

This plan adds share storage and relay. It does not add reward-window computation, payout commitment, claim verification, share-producing mining, or wallet integration.

## Progress

- [x] (2026-04-13) POOL-05: Implemented `ShareRecord` serialization in `src/node/sharechain.h`
- [x] (2026-04-13) POOL-05: Implemented `SharechainStore` with LevelDB persistence, orphan handling, best-tip selection
- [x] (2026-04-13) POOL-05: Implemented `ValidateShare()` with RandomX proof checking
- [x] (2026-04-13) POOL-05: Added `shareinv`/`getshare`/`share` P2P handlers in `src/net_processing.cpp`
- [x] (2026-04-13) POOL-05: Added `sharechain_tests` and `feature_sharepool_relay.py`

## Surprises & Discoveries

- Observation: The functional P2P test framework uses network magic bytes that must match RNG's, not Bitcoin's. Tests that send raw P2P messages must use `0xB07C010E`.
  Evidence: `test/functional/feature_sharepool_relay.py`

## Decision Log

- Decision: Use LevelDB for share persistence (same as Bitcoin Core's block index and chainstate).
  Rationale: Already vendored, proven at scale, familiar API.
  Date/Author: 2026-04-13 / POOL-05

- Decision: Limit `shareinv` to 1000 IDs and `share` messages to 16 records per batch.
  Rationale: Prevents memory exhaustion from malicious peers. Constants defined in `sharechain.h`.
  Date/Author: 2026-04-13 / POOL-05

## Outcomes & Retrospective

Complete. The sharechain store and P2P relay work correctly on regtest. The relay benchmark (POOL-06-GATE) later confirmed acceptable latency and bandwidth at 10-second test intervals. The confirmed 1-second cadence still needs measurement (tracked in WORKLIST.md).

## Context and Orientation

`src/node/sharechain.h` defines `ShareRecord`, `ShareStoreResult`, and `SharechainStore`. `src/node/sharechain.cpp` implements validation, storage, orphan handling, and best-tip tracking. `src/net_processing.cpp` handles P2P share relay. `src/protocol.h` defines message type constants.

## Plan of Work

1. Define `ShareRecord` with serialization.
2. Implement `SharechainStore` with LevelDB backend.
3. Implement `ValidateShare()` with RandomX proof checking.
4. Add orphan buffer with FIFO eviction.
5. Add P2P message handlers gated on activation.
6. Add unit and functional tests.

## Implementation Units

### Unit 1: ShareRecord and SharechainStore
- Goal: Persistent share storage with validation
- Requirements advanced: R1, R2, R3, R4
- Dependencies: Plan 004 (deployment boundary)
- Files to create or modify: `src/node/sharechain.{h,cpp}` (create)
- Tests to add or modify: `build/bin/test_bitcoin --run_test=sharechain_tests`
- Approach: Define record struct, implement LevelDB I/O, cumulative-work tip selection, orphan buffer
- Specific test scenarios: Store and retrieve shares; orphan resolution on parent arrival; best-tip updates on new shares; eviction when buffer full

### Unit 2: P2P relay
- Goal: Activation-gated share relay
- Requirements advanced: R5, R6
- Dependencies: Unit 1
- Files to create or modify: `src/net_processing.cpp`, `src/protocol.h`
- Tests to add or modify: `python3 test/functional/feature_sharepool_relay.py --configfile=build/test/config.ini`
- Approach: Add message handlers for shareinv/getshare/share, check activation before processing
- Specific test scenarios: Share relayed between two activated nodes; share rejected before activation; orphan share triggers parent request

## Concrete Steps

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=sharechain_tests
    python3 test/functional/feature_sharepool_relay.py --configfile=build/test/config.ini

## Validation and Acceptance

`sharechain_tests` pass. `feature_sharepool_relay.py` demonstrates share relay between activated regtest nodes. Shares persist across daemon restart (LevelDB).

## Idempotence and Recovery

Share storage is additive. A corrupted sharechain DB can be wiped and rebuilt from peers (shares are relayed on request).

## Artifacts and Notes

- Source: `src/node/sharechain.{h,cpp}` (463 lines total)
- P2P handlers: `src/net_processing.cpp` lines 3965-4042
- Test artifacts: `src/test/sharechain_tests.cpp`, `test/functional/feature_sharepool_relay.py`

## Interfaces and Dependencies

- `src/node/sharechain.h`: `ShareRecord`, `SharechainStore` (public API for other subsystems)
- `src/crypto/randomx_hash.h`: `RandomXContext` for share proof validation
- `src/net_processing.cpp`: P2P message handlers
- `src/protocol.h`: Message type constants (`SHAREINV`, `GETSHARE`, `SHARE`)
- LevelDB (vendored)
