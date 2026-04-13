# Regtest End-to-End Proof

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan produces the definitive regtest proof that the entire sharepool lifecycle works: BIP9 activation, share production, share relay, payout commitment, block mining with settlement outputs, claim after maturity, and proportional reward distribution. After this plan, anyone can reproduce the full lifecycle on a local regtest network.

This is the proof that validates Plans 002-008 before any devnet or mainnet deployment begins.

## Requirements Trace

`R1`. A 4-node regtest network activates sharepool via BIP9.

`R2`. All nodes produce and relay shares at approximately the expected rate.

`R3`. Blocks contain valid settlement commitments with multi-leaf payout trees when multiple miners are active.

`R4`. Claims succeed after 100-block maturity.

`R5`. A 10% hashrate miner receives approximately 10% of total rewards over 50 blocks (within ±5%).

`R6`. The proof script is reproducible: running it again produces the same pass/fail result.

## Scope Boundaries

This plan is regtest-only. It does not deploy to devnet (Plan 011) or mainnet (Plan 012). It does not test adversarial scenarios (Plan 011). It exercises the happy path of the full lifecycle.

## Progress

- [ ] Write `test/functional/feature_sharepool_e2e.py`
- [ ] Run and validate the e2e test
- [ ] Commit the test and results

## Surprises & Discoveries

None yet.

## Decision Log

- Decision: Use 4 nodes to match the decision gate criteria from `IMPLEMENTATION_PLAN.md`.
  Rationale: 4 nodes provides enough diversity for relay testing while remaining fast on a single machine.
  Date/Author: 2026-04-13 / this plan

- Decision: Use 50 blocks (not 100) for the proportional reward measurement.
  Rationale: 100 blocks at 1-second share spacing would take ~200 minutes of simulated time. 50 blocks is sufficient for a statistical test with ±5% tolerance.
  Date/Author: 2026-04-13 / this plan

## Outcomes & Retrospective

Not yet started.

## Context and Orientation

The functional test framework lives in `test/functional/`. Tests are Python scripts that inherit from `BitcoinTestFramework` (located in `test/functional/test_framework/`). Each test starts regtest nodes, connects them, and exercises behavior through RPC calls. Existing examples: `feature_sharepool_relay.py`, `feature_qsb_mining.py`.

The test needs to:
1. Start 4 regtest nodes with `-vbparams=sharepool:0:9999999999:0`
2. Mine enough blocks to activate the sharepool deployment
3. Set different mining addresses on different nodes
4. Mine 50+ blocks with all nodes mining
5. Verify shares are produced and relayed
6. Verify blocks contain valid settlement commitments
7. Mine 100 more blocks for maturity
8. Verify claims succeed
9. Verify proportional reward distribution

## Plan of Work

Write `test/functional/feature_sharepool_e2e.py` that exercises the full lifecycle:

1. **Setup**: Start 4 nodes with sharepool activation parameters and different mining addresses.
2. **Activation**: Mine enough blocks to pass the BIP9 signaling threshold.
3. **Share production**: Enable mining on all 4 nodes. Wait for shares to appear in `getsharechaininfo`.
4. **Block mining**: Mine 50 blocks. For each block, verify `getrewardcommitment` returns a multi-leaf commitment.
5. **Maturity**: Mine 100 more blocks to mature the earliest settlements.
6. **Claims**: Verify `getbalances` shows `pooled.claimable > 0` on at least one node. Trigger or verify auto-claim. Check that claimed balance appears in confirmed wallet balance.
7. **Proportional check**: Compute total rewards across all 4 miners. Verify each miner's share is within ±5% of their hashrate proportion.

## Implementation Units

### Unit 1: End-to-end functional test
- Goal: Reproducible regtest proof of full sharepool lifecycle
- Requirements advanced: R1, R2, R3, R4, R5, R6
- Dependencies: Plans 007 and 008 (all sharepool code must be complete)
- Files to create or modify: `test/functional/feature_sharepool_e2e.py`
- Tests to add or modify: The e2e script IS the test
- Approach: Python functional test using BitcoinTestFramework
- Specific test scenarios:
  - 4-node activation via BIP9 signaling
  - Share production verified via `getsharechaininfo`
  - Multi-leaf commitment verified via `getrewardcommitment`
  - Claim after maturity verified via `getbalances` and transaction inspection
  - Proportional reward within ±5% of expected

## Concrete Steps

    python3 test/functional/feature_sharepool_e2e.py --configfile=build/test/config.ini

Expected: Test passes. All assertions hold. Proportional reward distribution verified.

## Validation and Acceptance

The test script passes on a clean build. Running it twice produces the same result (deterministic seeds for share generation where possible, statistical tolerance for proportional checks). The test output includes the measured reward distribution percentages.

## Idempotence and Recovery

The test starts fresh regtest nodes each run. No persistent state between runs. Can be interrupted and restarted safely.

## Artifacts and Notes

To be filled with test output when the test is run.

## Interfaces and Dependencies

- All Plans 002-008 must be complete
- `test/functional/test_framework/` (BitcoinTestFramework)
- Python 3 with standard test dependencies
- All sharepool RPCs: `getsharechaininfo`, `getrewardcommitment`, `getbalances` (with pooled), `getinternalmininginfo` (with shares_found)
