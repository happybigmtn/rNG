# Payout Commitment and Claim Program

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan implements the consensus enforcement layer for sharepool payout commitments and claims. After this plan, an activated regtest node will: (a) build coinbase settlement outputs with multi-leaf payout commitments derived from the reward window, (b) reject blocks with missing or invalid commitments, (c) accept valid claim transactions that spend settlement outputs after coinbase maturity, (d) reject duplicate claims and settlement-draining claims. This is the plan that gives share work economic meaning.

The plan builds on completed preparatory work: POOL-07A locked the settlement specification in `specs/sharepool-settlement.md`, POOL-07B produced reference test vectors in `contrib/sharepool/reports/pool-07b-settlement-vectors.json`, POOL-07C recorded the interpreter-vs-validation split in the root `IMPLEMENTATION_PLAN.md`, POOL-07D landed deterministic C++ helpers in `src/consensus/sharepool.{h,cpp}` with parity tests, and POOL-07E wired the solo-settlement coinbase into `src/node/miner.cpp`. The historical standalone POOL-07C checkpoint file is absent from the current generated tree, so this plan treats the root tracker, settlement spec, reference model, and vectors as current evidence.

The remaining work is three integration slices: (1) witness-v2 settlement program verification in the script interpreter, (2) `ConnectBlock` commitment enforcement in validation, and (3) multi-leaf reward-window commitment in the block assembler.

## Requirements Trace

`R1`. After activation, `CreateNewBlock()` must include a settlement output whose state hash commits to the full reward-window-derived payout leaf set, not just the single-miner solo fallback.

`R2`. After activation, `ConnectBlock()` must reject blocks whose coinbase lacks a valid settlement output or whose settlement output value does not equal the full block reward.

`R3`. The script interpreter must verify witness-v2 settlement programs: validate the claim witness (descriptor, leaf index, leaf data, payout branch, status branch), reconstruct the old state hash, and verify it matches the prevout program.

`R4`. Transaction-level validation must enforce claim shape: payout output at index 0 matching the committed leaf exactly, successor settlement output at index 1 with updated claim-status root, and exact value conservation.

`R5`. Duplicate claims (leaf already marked claimed) must be rejected.

`R6`. Settlement value cannot fund transaction fees. All fees must come from non-settlement inputs.

`R7`. Pre-activation nodes must treat witness-v2 outputs as valid unknown witness programs (soft-fork compatibility).

`R8`. The solo miner case must continue working: a single payout script in the reward window produces a one-leaf settlement.

## Scope Boundaries

This plan does not add the share-producing miner (Plan 008). It does not add wallet auto-claim or pooled reward tracking (Plan 008). It does not add sharepool RPCs (Plan 008). It does not change the share relay protocol (Plan 005). It builds on the existing `src/consensus/sharepool.{h,cpp}` helpers without replacing them.

The reward-window walk for multi-leaf commitment requires access to the sharechain store. This plan wires that access into the block assembler but does not change `SharechainStore` internals.

## Progress

- [x] (2026-04-13) POOL-07A: Settlement state machine specification locked
- [x] (2026-04-13) POOL-07B: Reference settlement model with deterministic test vectors
- [x] (2026-04-13) POOL-07C: Pre-consensus settlement design checkpoint (GO)
- [x] (2026-04-13) POOL-07D: C++ consensus helpers with parity tests
- [x] (2026-04-13) POOL-07E: Solo-settlement coinbase in miner
- [ ] Unit A: Witness-v2 settlement program verification in script interpreter
- [ ] Unit B: ConnectBlock commitment enforcement in validation
- [ ] Unit C: Multi-leaf reward-window commitment in block assembler

## Surprises & Discoveries

- Observation: The settlement design review (POOL-07C) concluded that witness-program verification and transaction-level conservation checking are two separate validation layers. The witness program proves "this leaf exists and is unclaimed under this state." The transaction-level check proves "the payout output and successor output are correct and conserve value." Both are required.
  Evidence: root `IMPLEMENTATION_PLAN.md` POOL-07C status plus `specs/sharepool-settlement.md`; the previous standalone genesis checkpoint artifact is absent from the current tree.

- Observation: The solo-settlement coinbase wiring (POOL-07E) uses a synthetic solo leaf with `first_share_id` and `last_share_id` derived from the previous block hash and height. This is a placeholder that must be replaced by real share IDs when the reward window walk is wired.
  Evidence: `src/node/miner.cpp` lines 188-194, `src/consensus/sharepool.cpp` `MakeSoloSettlementLeaf()`

## Decision Log

- Decision: Split the remaining POOL-07 work into three units (interpreter, validation, assembler) rather than one monolithic implementation.
  Rationale: Each unit is independently testable. The interpreter can be tested with crafted settlement prevouts. The validation can be tested with crafted blocks. The assembler can be tested with existing miner_tests patterns.
  Date/Author: 2026-04-13 / this plan

- Decision: Wire reward-window access into the block assembler via `SharechainStore` pointer in `BlockAssembler`, not by copying shares into the template.
  Rationale: The share store already exists and handles concurrency. Copying would be wasteful and introduce staleness.
  Date/Author: 2026-04-13 / this plan

- Decision: Claim transactions entering the mempool must pass the same witness-v2 and conservation checks as mined transactions. Standard mempool policy should accept claims for outputs that are mature.
  Rationale: Prevents invalid claims from propagating and wasting relay bandwidth.
  Date/Author: 2026-04-13 / this plan

## Outcomes & Retrospective

Not yet started. The preparatory work (07A-07E) is complete.

## Context and Orientation

The settlement protocol is specified in `specs/sharepool-settlement.md`. The immutable payout leaf set, claim-status tree, state-hash construction, claim witness format, and conservation rules are all defined there. The reference Python model at `contrib/sharepool/settlement_model.py` implements the same transitions and produces deterministic vectors at `contrib/sharepool/reports/pool-07b-settlement-vectors.json`. The C++ helpers at `src/consensus/sharepool.{h,cpp}` reproduce those vectors in `src/test/sharepool_commitment_tests.cpp`.

Current code state:
- `src/script/interpreter.cpp`: Handles witness v0 (SegWit) and v1 (Taproot). Witness versions 2-16 pass through as "unknown witness programs" (anyone-can-spend for soft-fork compatibility). After this plan, v2 with 32-byte programs will be recognized as sharepool settlement programs.
- `src/validation.cpp`: `ConnectBlock()` validates coinbase structure, witness commitments, and subsidy. After this plan, it also validates the sharepool settlement commitment.
- `src/node/miner.cpp`: `CreateNewBlock()` currently inserts a solo-settlement output when sharepool is active. After this plan, it queries the sharechain store for the reward window and builds multi-leaf commitments.
- `src/node/sharechain.{h,cpp}`: Stores shares and tracks the best tip. After this plan, it exposes a reward-window query method used by the block assembler.

## Plan of Work

### Unit A: Witness-v2 Settlement Program Verification

In `src/script/interpreter.cpp`, add a handler for witness version 2 with 32-byte programs when sharepool is active. The handler deserializes the five witness elements (descriptor, leaf_index, leaf_data, payout_branch, status_branch), reconstructs the payout root and old status root from the Merkle branches, computes the expected state hash, and verifies it matches the 32-byte program in the prevout script. If any check fails, return `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED`.

Add `SCRIPT_ERR_SHAREPOOL_VERIFY_FAILED` and related error codes to `src/script/script_error.{h,cpp}`.

The verification function itself should be in `src/consensus/sharepool.{h,cpp}` (extending the existing helpers) and called from the interpreter. This keeps consensus logic together and makes it testable independently.

### Unit B: ConnectBlock Commitment Enforcement

In `src/validation.cpp`, extend `ConnectBlock()` to enforce two rules when sharepool is active:

1. The coinbase must contain exactly one settlement output (witness v2, 32-byte program) whose value equals the full block reward.
2. For each transaction spending a settlement output (a claim), verify: payout output matches committed leaf, successor settlement output has correct new state hash and value, and value conservation holds.

Add claim-conservation validation to the transaction-level checks. This may be in `CheckTxInputs()` or a new `ValidateSharepoolClaim()` called from `ConnectBlock()`.

### Unit C: Multi-Leaf Reward-Window Commitment

In `src/node/miner.cpp`, replace the solo-settlement fallback with a full reward-window walk when the sharechain has shares:

1. Add a `SharechainStore*` or `std::shared_ptr<SharechainStore>` to `BlockAssembler`.
2. When sharepool is active, query the sharechain for the reward window (walk backward from best share tip, accumulate work up to 7200 target-spacing shares).
3. Aggregate shares by payout script.
4. Build payout leaves, sort them, compute the settlement state hash.
5. If no shares exist (cold start), fall back to the existing solo-settlement leaf.

Add a `GetRewardWindow()` method to `SharechainStore` that returns ordered shares for the window, given a block tip and work threshold.

## Implementation Units

### Unit A: Witness-v2 verification
- Goal: Script interpreter recognizes and verifies sharepool settlement programs
- Requirements advanced: R3, R5, R7
- Dependencies: POOL-07D (consensus helpers exist)
- Files to create or modify: `src/script/interpreter.cpp`, `src/script/script_error.{h,cpp}`, `src/consensus/sharepool.{h,cpp}`
- Tests to add or modify: `src/test/sharepool_claim_tests.cpp` (new)
- Approach: Add `VerifySharepoolSettlement()` to `consensus::sharepool`, call from interpreter when witness v2 + 32-byte program + sharepool active
- Specific test scenarios:
  - Valid claim witness reconstructs correct state hash -> passes
  - Wrong leaf index -> fails with SHAREPOOL_VERIFY_FAILED
  - Tampered leaf data -> payout root mismatch -> fails
  - Already-claimed leaf (status flag = 1) -> state hash mismatch -> fails
  - Pre-activation witness v2 -> passes as unknown (soft-fork compatible)
  - Wrong descriptor version -> fails

### Unit B: ConnectBlock enforcement
- Goal: Blocks must contain valid settlement commitments; claims must conserve value
- Requirements advanced: R2, R4, R6
- Dependencies: Unit A
- Files to create or modify: `src/validation.cpp`
- Tests to add or modify: `src/test/validation_sharepool_tests.cpp` (new)
- Approach: Add sharepool validation checks in ConnectBlock path, after coinbase validation
- Specific test scenarios:
  - Activated block with valid settlement output -> accepted
  - Activated block missing settlement output -> rejected
  - Activated block with wrong settlement value -> rejected
  - Claim tx with correct payout and successor -> accepted
  - Claim tx with wrong payout amount -> rejected
  - Claim tx with missing successor when claims remain -> rejected
  - Claim tx spending immature settlement -> rejected (standard maturity)
  - Pre-activation block without settlement -> accepted (unchanged behavior)

### Unit C: Multi-leaf commitment
- Goal: Block assembler builds real reward-window commitments from sharechain
- Requirements advanced: R1, R8
- Dependencies: Unit B, Plan 005 sharechain
- Files to create or modify: `src/node/miner.cpp`, `src/node/sharechain.{h,cpp}`
- Tests to add or modify: Extend `src/test/miner_tests.cpp` with multi-leaf scenarios
- Approach: Add GetRewardWindow() to SharechainStore, use in CreateNewBlock() when sharepool active
- Specific test scenarios:
  - Two payout scripts with 60/40 work split -> settlement leaves reflect split
  - Empty sharechain (cold start) -> solo fallback works
  - Reward window shorter than threshold (young chain) -> uses available shares
  - RegenerateCommitments() preserves settlement output

## Concrete Steps

Build and run existing tests to verify no regression:

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=sharepool_commitment_tests
    build/bin/test_bitcoin --run_test=miner_tests
    python3 contrib/sharepool/settlement_model.py --self-test

After Unit A:

    build/bin/test_bitcoin --run_test=sharepool_claim_tests

After Unit B:

    build/bin/test_bitcoin --run_test=validation_sharepool_tests

After Unit C:

    build/bin/test_bitcoin --run_test=miner_tests

Full validation after all units:

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=sharepool_commitment_tests
    build/bin/test_bitcoin --run_test=sharepool_claim_tests
    build/bin/test_bitcoin --run_test=validation_sharepool_tests
    build/bin/test_bitcoin --run_test=miner_tests
    python3 contrib/sharepool/settlement_model.py --self-test

## Validation and Acceptance

1. On an activated regtest chain with multiple payout scripts in the sharechain, `CreateNewBlock()` produces a coinbase with a multi-leaf settlement output whose state hash matches the expected commitment for the reward window.
2. A block without a valid settlement output is rejected by an activated peer.
3. A valid claim transaction (correct witness, correct payout output, correct successor) is accepted after 100-block maturity.
4. A duplicate claim (same leaf on a settlement already claimed) is rejected.
5. A claim that drains settlement value beyond the claimed leaf amount is rejected.
6. Pre-activation behavior is unchanged for all existing tests.

## Idempotence and Recovery

Each unit is additive. If Unit A is landed but Unit B regresses, Unit A's witness verification still functions correctly in isolation (it just is not enforcement-required until ConnectBlock checks are added). The activation gate means no mainnet behavior changes until BIP9 activates.

Partial completion can be reverted by removing the added code. Existing tests must continue passing at every intermediate state.

## Artifacts and Notes

- Settlement spec: `specs/sharepool-settlement.md`
- Reference vectors: `contrib/sharepool/reports/pool-07b-settlement-vectors.json`
- POOL-07C tracker entry: root `IMPLEMENTATION_PLAN.md`
- Settlement design details: `specs/sharepool-settlement.md`, `contrib/sharepool/settlement_model.py`, `contrib/sharepool/reports/pool-07b-settlement-vectors.json`
- Existing helpers: `src/consensus/sharepool.{h,cpp}` (to be extended)
- Existing miner wiring: `src/node/miner.cpp` lines 183-199 (to be extended)

## Interfaces and Dependencies

### New interfaces

In `src/consensus/sharepool.h`:

    bool VerifySharepoolSettlement(
        const CScript& settlement_script,
        const std::vector<std::vector<unsigned char>>& witness,
        ScriptError* serror);

In `src/node/sharechain.h`:

    struct RewardWindowResult {
        std::vector<ShareRecord> shares;
        uint256 share_tip;
    };

    RewardWindowResult SharechainStore::GetRewardWindow(
        const uint256& block_tip,
        int64_t work_threshold) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);

### Modified interfaces

- `BlockAssembler`: Add `SharechainStore*` member, use in `CreateNewBlock()`
- `ConnectBlock()`: Add sharepool commitment validation when active
- Script interpreter: Add witness-v2 settlement dispatch when active

### Dependencies

- `src/consensus/sharepool.{h,cpp}` (existing helpers)
- `src/node/sharechain.{h,cpp}` (existing store)
- `src/crypto/randomx_hash.h` (for share proof verification in window walk)
- Root `PLANS.md` (ExecPlan standard)
