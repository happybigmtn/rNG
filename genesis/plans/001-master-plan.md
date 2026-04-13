# Master Plan: Protocol-Native Trustless Pooled Mining

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

RNG is a live CPU-mineable RandomX chain where the block finder currently receives the entire block reward. This plan sequences the work to replace that winner-takes-all model with protocol-native trustless pooled mining, so that every CPU miner earns proportional reward for every valid share they produce, and claims their committed amount after coinbase maturity without trusting any operator.

After this master plan is fully executed, a user running `rngd -mine -mineaddress=<addr> -minethreads=4` on an activated network will see their pending pooled reward grow with each share they contribute, and their wallet will auto-claim matured settlements. A 10% miner over 100 blocks will receive approximately 10% of total rewards with coefficient of variation below 10%.

This master plan is the sequencing index for plans 002 through 012. Each numbered plan is a self-contained ExecPlan following the root `PLANS.md` standard. The dependency order ensures that each plan produces a verifiable artifact before the next plan begins, and decision gates at plans 003, 006, and 010 provide explicit go/no-go checkpoints.

## Requirements Trace

`R1`. Pooled mining must be protocol-enforced, not an overlay service. No operator ledger, no operator-controlled payout decision point, no single control plane for share admission.

`R2`. The protocol must use public RandomX share work proofs that any upgraded node can verify independently.

`R3`. Reward splits must be deterministic: given the same accepted share history, every node computes the same payout commitment.

`R4`. Each block commits to its reward split via a compact settlement output, not one coinbase output per miner.

`R5`. Miners must see pending pooled reward as soon as their shares enter the reward window, before any block is found.

`R6`. Claims must be trustless: any party can construct a claim transaction, but funds can only emerge at the committed payout script.

`R7`. Solo mining is the degenerate case. A lone miner filling the reward window receives the full block reward through the same settlement mechanism.

`R8`. Activation uses BIP9 versionbits with explicit regtest proof, devnet adversarial testing, and mainnet activation preparation gates.

`R9`. Pre-activation behavior remains unchanged. Existing miners, wallets, and nodes continue operating normally until activation.

`R10`. Share relay bandwidth must stay under approximately 10 KB/s per node at the confirmed 1-second share cadence.

## Scope Boundaries

This master plan covers the sharepool protocol from specification through mainnet activation. It does not cover:

- Agent wallet or MCP server implementation (FUTURE-04)
- Atomic swap protocol (FUTURE-06)
- QSB operator support (already complete, DONE-08)
- Coinbase maturity changes (intentionally unchanged at 100 blocks)
- Batched multi-leaf claims (deferred to v2)
- Non-sharepool uses of witness version 2 (reserved for settlement in v1)

## Progress

- [x] (2026-04-13) POOL-01: Sharepool protocol specification written and committed
- [x] (2026-04-13) POOL-02: Offline economic simulator built and tested
- [x] (2026-04-13) POOL-03: Decision gate -- no-go on 10-second baseline (25.10% CV)
- [x] (2026-04-13) POOL-01R/02R/03R: Revised constants confirmed (1-second, 7200 shares)
- [x] (2026-04-13) CHKPT-02: Pre-consensus implementation review (GO for POOL-04)
- [x] (2026-04-13) POOL-04: BIP9 `DEPLOYMENT_SHAREPOOL` activation boundary
- [x] (2026-04-13) POOL-05: Sharechain data model, storage, and P2P relay
- [x] (2026-04-13) POOL-06-GATE: Share relay viability measurement (GO at 10-second test cadence)
- [x] (2026-04-13) POOL-07A: Settlement state machine specification locked
- [x] (2026-04-13) POOL-07B: Reference settlement model with deterministic test vectors
- [x] (2026-04-13) POOL-07C: Pre-consensus settlement design checkpoint (GO)
- [x] (2026-04-13) POOL-07D: Deterministic C++ consensus helpers with parity tests
- [x] (2026-04-13) POOL-07E: Solo-settlement coinbase wired into block assembly
- [ ] POOL-07: Witness-v2 claim verification + ConnectBlock enforcement + multi-leaf commitment
- [ ] POOL-08: Share-producing miner + wallet auto-claim + sharepool RPCs
- [ ] CHKPT-03: Regtest end-to-end proof
- [ ] FUTURE-01: Devnet deployment and adversarial testing
- [ ] FUTURE-02: Mainnet activation preparation

## Surprises & Discoveries

- Observation: The original 10-second share spacing with 720-share reward window failed the variance gate (25.10% CV for 10% miner). The 1-second spacing with 7200 shares passes (max CV 8.06% across 20 seeds).
  Evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`

- Observation: Share relay at 10-second intervals measured p50 latency 58ms, p99 79ms, max bandwidth 0.06 KB/s per node. The confirmed 1-second cadence will be approximately 10x higher bandwidth but still well under the 10 KB/s budget.
  Evidence: `contrib/sharepool/reports/pool-06-relay-viability.json`

- Observation: The settlement state machine requires two separate Merkle trees (payout tree for immutable leaf set, status tree for mutable claim flags). This was not obvious from the initial protocol sketch but fell out of the POOL-07A specification work.
  Evidence: `specs/sharepool-settlement.md`, sections "Payout Leaves" and "Claim-Status Tree"

## Decision Log

- Decision: Use 1-second share spacing with 7200 target-spacing shares as the confirmed mainnet constants.
  Rationale: Only candidate that passes both seed-42 and 20-seed stress variance gates (CV < 10%). The 2-second candidate fails stress seeds (10.33% max CV). The 10-second baseline is rejected (25.10%).
  Date/Author: 2026-04-13 / POOL-03R decision gate

- Decision: Reserve witness version 2 exclusively for sharepool settlement in v1.
  Rationale: Simplifies consensus verification. No need to disambiguate between sharepool and other witness-v2 uses. Future versions can relax this.
  Date/Author: 2026-04-13 / POOL-07A specification

- Decision: One claim per transaction in v1. No batched multi-leaf claims.
  Rationale: Simpler consensus code, easier to reason about correctness. Batched claims can be layered later if claim throughput becomes a bottleneck.
  Date/Author: 2026-04-13 / POOL-07A specification

- Decision: Claims are permissionless (no inner signature). The payout destination is consensus-locked to the committed payout script.
  Rationale: Removes the need for wallet-specific signature construction. Any party can build a claim transaction, but funds always go to the committed destination. Enables third-party claim helpers.
  Date/Author: 2026-04-13 / POOL-07A specification

- Decision: Fees for claims must come from non-settlement inputs.
  Rationale: Prevents the settlement pool from silently paying fees or being drained by fee-paying claims.
  Date/Author: 2026-04-13 / POOL-07A specification

## Outcomes & Retrospective

Not yet complete. The master plan is approximately 60% through its dependency chain, measured by implementation plan item completion. The specification phase and consensus helper phase are done. The critical remaining work is consensus enforcement (POOL-07), user-facing integration (POOL-08), and the testing gates (CHKPT-03, FUTURE-01).

## Context and Orientation

This RNG repository is a Bitcoin Core v30.2 fork with RandomX PoW, live on mainnet in the low-32,000 block range per committed docs. Root `EXECPLAN.md` records validator-02/04/05 healthy at height 32122 on 2026-04-13 and validator-01 crash-looping on a zero-byte `settings.json`; this plan does not depend on a fresh live-network probe. The root `PLANS.md` defines the ExecPlan standard. `IMPLEMENTATION_PLAN.md` at the repo root is the active task tracker for the sharepool implementation sequence. This generated `genesis/` corpus is a subordinate plan set and index, not a replacement control plane.

Key files and their roles:

- `specs/sharepool.md`: Top-level sharepool protocol spec with confirmed constants
- `specs/sharepool-settlement.md`: Settlement state machine specification
- `src/consensus/sharepool.{h,cpp}`: C++ settlement helpers (leaf hashing, Merkle trees, state hashing)
- `src/node/sharechain.{h,cpp}`: LevelDB-backed share storage with orphan buffering
- `src/node/miner.cpp`: Block assembly with activated solo-settlement coinbase
- `src/node/internal_miner.{h,cpp}`: Multi-threaded RandomX miner (block-only, not yet share-producing)
- `src/net_processing.cpp`: P2P share relay handlers (`shareinv`, `getshare`, `share`)
- `src/consensus/params.h`: `DEPLOYMENT_SHAREPOOL` definition
- `src/kernel/chainparams.cpp`: Network parameters including dormant sharepool deployment
- `contrib/sharepool/simulate.py`: Offline economic simulator
- `contrib/sharepool/settlement_model.py`: Reference settlement transition model

## Plan of Work

The plan sequences twelve numbered plans in dependency order:

1. **002**: Sharepool spec and simulator (DONE)
2. **003**: Decision gate on simulator results (DONE, revised via 003R)
3. **004**: BIP9 deployment skeleton (DONE)
4. **005**: Sharechain data model, storage, and relay (DONE)
5. **006**: Decision gate on relay viability (DONE)
6. **007**: Payout commitment and claim program (partially done through 07E; remaining: consensus enforcement)
7. **008**: Miner, wallet, and RPC integration (not started)
8. **009**: Regtest end-to-end proof (not started)
9. **010**: Decision gate on regtest proof (not started)
10. **011**: Devnet deployment and adversarial testing (not started)
11. **012**: Mainnet activation preparation (not started)

Checkpoint plans at 003, 006, and 010 are explicit decision gates.

## Implementation Units

### Unit 1: Maintain the sharepool plan index
- Goal: Keep the generated sharepool plan sequence reconciled to root `IMPLEMENTATION_PLAN.md`, source truth, and downstream plan status.
- Requirements advanced: R1, R2, R3, R4, R5, R6, R7, R8, R9, R10.
- Dependencies: Root `PLANS.md` standard; root `IMPLEMENTATION_PLAN.md`; numbered plans 002-012.
- Files to create or modify: `genesis/PLANS.md`, `genesis/GENESIS-REPORT.md`, and numbered plans under `genesis/plans/` when source evidence or root tracker status changes.
- Tests to add or modify: Test expectation: none -- this is a planning/index artifact. Validation is a corpus shape and evidence-link check.
- Approach: Keep this master plan as the sequencing map, not the implementation itself. Reconcile against the root tracker before editing statuses, and avoid citing deleted historical checkpoint files as current evidence.
- Specific test scenarios:
  - All numbered plans 001-012 contain every mandatory `PLANS.md` section.
  - `genesis/PLANS.md` maps each root tracker item to exactly one numbered plan or decision gate.
  - No absent historical checkpoint/report file is cited as current evidence.
  - Downstream dependency order remains topological: 007 before 008, 008 before 009, 010 before 011, 011 before 012.

## Concrete Steps

Build the project from the repository root:

    cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON
    cmake --build build -j$(nproc)

Run existing sharepool tests:

    build/bin/test_bitcoin --run_test=sharepool_commitment_tests
    build/bin/test_bitcoin --run_test=sharechain_tests
    python3 test/functional/feature_sharepool_relay.py --configfile=build/test/config.ini
    python3 -m pytest contrib/sharepool/test_simulate.py
    python3 contrib/sharepool/settlement_model.py --self-test

Start a regtest node with sharepool activation:

    build/bin/rngd -regtest -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<addr> -minethreads=1

## Validation and Acceptance

The master plan is complete when:

1. All twelve numbered plans are complete.
2. A 4-node regtest network demonstrates the full sharepool lifecycle: activate, produce shares, relay, commit, mine, claim.
3. A multi-day devnet run shows stability under adversarial conditions (withholding, eclipse, spam).
4. `DEPLOYMENT_SHAREPOOL` is changed from `NEVER_ACTIVE` to a mainnet activation window.
5. A release tag includes the activation parameters.

## Idempotence and Recovery

Each numbered plan is independently verifiable and can be rerun from its starting state. The BIP9 activation mechanism ensures that pre-activation nodes are unaffected. If a plan introduces a regression, the affected code can be reverted without breaking pre-activation behavior.

## Artifacts and Notes

- Simulator evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`
- Relay benchmark: `contrib/sharepool/reports/pool-06-relay-viability.json`
- Settlement vectors: `contrib/sharepool/reports/pool-07b-settlement-vectors.json`
- Decision gates: `genesis/plans/003-decision-gate-simulator-results.md`, `genesis/plans/006-decision-gate-share-relay-viability.md`, `genesis/plans/010-decision-gate-regtest-proof-review.md`
- Historical standalone checkpoint/report files (`chkpt-02-pre-consensus-review.md`, `chkpt-03a-settlement-design-review.md`, `003-decision-report.md`, `003r-decision-report.md`) are absent from the current generated tree; reconcile their root-doc references against current source/spec evidence before relying on them.

## Interfaces and Dependencies

The sharepool implementation touches these module boundaries:

- **Consensus**: `src/consensus/sharepool.{h,cpp}` (settlement helpers, already exists)
- **Script interpreter**: `src/script/interpreter.cpp` (witness v2 verification, to be added in POOL-07)
- **Validation**: `src/validation.cpp` (ConnectBlock enforcement, to be added in POOL-07)
- **Miner**: `src/node/miner.cpp` (multi-leaf commitment, to be extended in POOL-07) and `src/node/internal_miner.{h,cpp}` (dual-target production, POOL-08)
- **Sharechain**: `src/node/sharechain.{h,cpp}` (exists, to be extended for reward-window queries in POOL-08)
- **Wallet**: `src/wallet/` (pooled reward tracking, auto-claim, POOL-08)
- **RPC**: `src/rpc/mining.cpp` (sharepool RPCs, POOL-08)
- **P2P**: `src/net_processing.cpp` (share relay, exists)
- **Chain params**: `src/kernel/chainparams.cpp` (activation parameters, FUTURE-02)

External dependency: RandomX v1.2.1 (vendored, no changes needed).
