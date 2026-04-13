# Specification: End-to-End Testing and Regtest Proof

## Objective

Validate that the complete sharepool lifecycle works end-to-end on a regtest network, pass a formal decision gate, and then stress-test under adversarial conditions on a multi-host devnet. This spec covers Plans 009 (Regtest End-to-End Proof), 010 (Decision Gate: Regtest Proof Review), and 011 (Devnet Deployment and Adversarial Testing) from the genesis corpus.

The testing sequence is: build the regtest e2e proof, evaluate it against decision criteria, then deploy to a longer-running devnet for adversarial scenarios. Each phase gates the next. Nothing in this spec is actionable until Plans 007 (consensus enforcement) and 008 (miner/wallet integration) are complete.

## Evidence Status

### Verified Facts

- `test/functional/feature_sharepool_relay.py` (87 lines) tests P2P share relay after activation. It creates `CShareRecord` from block headers, validates `shareinv`/`getshare`/`share` message flow, and verifies serialization roundtrips. It tests relay only -- no commitment, claim, or lifecycle coverage.
- `test/functional/feature_sharepool_relay_benchmark.py` (464 lines) benchmarks relay latency and bandwidth at configurable intervals. The 10-second interval was measured; the confirmed 1-second cadence has not been benchmarked separately.
- `src/test/sharepool_commitment_tests.cpp` (262 lines) tests settlement state hash computation against reference vectors from `pool-07b-settlement-vectors.json`. Covers all 38 functions in the `consensus::sharepool` namespace.
- `src/test/miner_tests.cpp` contains pre/post-activation coinbase construction tests for the solo settlement output.
- `contrib/sharepool/test_simulate.py` provides deterministic test coverage for the economic simulator.
- `contrib/sharepool/settlement_model.py --self-test` runs 5 transition scenarios against the reference settlement model.
- Regtest activation uses `-vbparams=sharepool:0:9999999999:0` with period 144 and threshold 108 (75%).
- The functional test framework lives in `test/functional/` with tests inheriting from `BitcoinTestFramework` in `test/functional/test_framework/`.

### Recommendations

1. The regtest e2e proof (Plan 009) should be the first test written after Plans 007 and 008 are complete. It validates the entire dependency chain.
2. The decision gate (Plan 010) should be a hard stop: no devnet deployment without a reproducible regtest pass and clean test suite.
3. The devnet adversarial plan (Plan 011) should remain research-shaped until the regtest proof establishes the concrete attack surface. Adversarial scenarios listed here are the expected minimum set.

### Hypotheses / Unresolved Questions

- Whether 50 blocks provides sufficient statistical power for the proportional reward test at +/-5% tolerance. The Plan 009 decision log chose 50 over 100 for time reasons.
- Whether claim throughput (one claim per block per settlement output) is sufficient under realistic multi-miner conditions. This is testable on regtest but more meaningful on devnet.
- Whether the 1-second share cadence relay bandwidth is acceptable. Only the 10-second cadence was benchmarked (POOL-06-GATE).
- Whether the canonical reward-window data-availability contract is explicit
  enough for `getrewardcommitment`, wallet tracking, and consensus validation to
  reproduce the same leaves after restart/reindex/reorg.
- Whether devnet infrastructure should reuse existing Contabo fleet slots or provision separate nodes.
- The exact set of adversarial scenarios depends on what the regtest proof reveals. Additional scenarios may be added after Plan 010 review.

## Existing Test Coverage

| Test | Location | Scope | Lines |
|------|----------|-------|-------|
| P2P share relay | `test/functional/feature_sharepool_relay.py` | Relay message flow, serialization roundtrip | 87 |
| Relay benchmark | `test/functional/feature_sharepool_relay_benchmark.py` | Latency/bandwidth at configurable intervals | 464 |
| Settlement consensus | `src/test/sharepool_commitment_tests.cpp` | State hash computation, 38 consensus functions | 262 |
| Coinbase construction | `src/test/miner_tests.cpp` | Pre/post-activation solo settlement output | -- |
| Simulator | `contrib/sharepool/test_simulate.py` | Deterministic simulator coverage | -- |
| Reference model | `contrib/sharepool/settlement_model.py --self-test` | 5 settlement transition scenarios | -- |

This review did not run the existing suites. The repository contains the tests
listed above, and none exercise the full activated lifecycle beyond relay.

## Test Gaps

These gaps are drawn from ASSESSMENT.md and verified against the codebase:

1. **No functional test for activated multi-miner sharepool lifecycle.** `feature_sharepool_relay.py` tests relay only. No test exercises: activate sharepool, produce shares from multiple miners, build multi-leaf commitment, mine block with settlement output, claim after maturity, verify proportional reward distribution.

2. **No data-availability/restart proof for reward commitments.** No test proves
   a node can enumerate multi-leaf settlement data for an already-mined block
   after restart, reindex, pruning, or reorg. This blocks trustworthy
   `getrewardcommitment` and wallet tracking.

3. **No adversarial share tests.** Share withholding, invalid share injection, orphan flooding, and relay spam are untested.

4. **No witness-v2 settlement claim test.** No test creates a claim transaction with settlement input, payout output, and successor settlement output. Blocked on Plan 007 (witness-v2 verifier).

5. **No wallet pooled-reward test.** No test verifies `getbalances` with `pooled.pending`/`pooled.claimable` fields. Blocked on Plan 008 (wallet integration).

6. **1-second relay benchmark not run.** POOL-06-GATE measured at 10-second intervals. The confirmed 1-second cadence needs separate measurement.

7. **No cross-platform CI for RNG-specific tests.** QSB and sharepool tests run on Linux CI. macOS and Windows coverage is implicit via build but not explicitly verified for sharepool-specific tests.

## Regtest End-to-End Proof

**Plan 009 -- the core deliverable.**

### Test: `test/functional/feature_sharepool_e2e.py`

A single Python functional test that exercises the full sharepool lifecycle on a 4-node regtest network.

### Setup

- 4 regtest nodes started with `-vbparams=sharepool:0:9999999999:0`
- Each node configured with a distinct mining address
- Nodes connected in a mesh topology

### Lifecycle Steps

1. **BIP9 Activation.** Mine enough blocks to pass the BIP9 signaling threshold (108 of 144 blocks in the activation period). Verify sharepool transitions to ACTIVE via `getdeploymentinfo`.

2. **Share Production.** Enable mining on all 4 nodes. Wait for shares to appear in `getsharechaininfo`. Verify all nodes are producing and relaying shares at approximately the expected rate.

3. **Block Mining with Commitments.** Mine 50 blocks with all nodes mining. For each block, verify `getrewardcommitment` returns a multi-leaf commitment when multiple miners have shares in the reward window, using the canonical data source selected before multi-leaf enforcement.

4. **Maturity Wait.** Mine 100 additional blocks to mature the earliest settlement outputs past coinbase maturity.

5. **Claim Verification.** Verify `getbalances` shows `pooled.claimable > 0` on at least one node. Trigger or verify auto-claim. Confirm claimed balance appears in the wallet's confirmed balance.

6. **Proportional Reward Check.** Compute total rewards across all 4 miners. Verify each miner's reward share is within +/-5% of their hashrate proportion over the 50-block measurement window.

### Requirements Traced

| Requirement | Description |
|-------------|-------------|
| R1 | 4-node regtest network activates sharepool via BIP9 |
| R2 | All nodes produce and relay shares at expected rate |
| R3 | Blocks contain valid multi-leaf settlement commitments |
| R4 | Claims succeed after 100-block maturity |
| R5 | 10% hashrate miner receives ~10% of rewards (+/-5%) |
| R6 | Test is reproducible: reruns produce the same pass/fail |
| R7 | Multi-leaf commitment leaves remain reconstructable after restart/reindex |

### Reproducibility

The test must produce deterministic pass/fail results on reruns. Where randomness is inherent (share production timing, block assignment), statistical tolerances (+/-5%) absorb variance. The test output must include measured reward distribution percentages for human review.

### Dependencies

All of Plans 002-008 must be complete. Specifically:
- Plan 007: Witness-v2 claim verification, `ConnectBlock` commitment enforcement, multi-leaf reward-window commitment
- Plan 008: Share-producing miner, wallet auto-claim, sharepool RPCs (`getsharechaininfo`, `getrewardcommitment`, `submitshare`)
- Canonical reward-window data-availability decision from the settlement/protocol specs

### Execution

```
python3 test/functional/feature_sharepool_e2e.py --configfile=build/test/config.ini
```

## Decision Gate Criteria

**Plan 010 -- a review checkpoint, not a code deliverable.**

The decision gate sits between the regtest proof (Plan 009) and devnet deployment (Plan 011). It is a GO/NO-GO decision with evidence.

### GO Criteria (all must hold)

| Criterion | Verification |
|-----------|-------------|
| Regtest e2e test passes reproducibly | Run `feature_sharepool_e2e.py` twice, both pass |
| All sharepool unit tests pass | `build/bin/test_bitcoin` exits 0 |
| All pre-existing tests pass without regression | `test/functional/test_runner.py` exits 0 |
| Reward distribution within +/-5% | Measured percentages in e2e test output |
| No consensus-level bugs requiring protocol changes | Review of test results and any failures |

### NO-GO Triggers

- E2e test fails or produces non-reproducible results
- Unit test regressions in sharepool or inherited Bitcoin Core tests
- Reward distribution outside +/-5% tolerance
- Discovery of a consensus bug requiring changes to settlement spec or witness-v2 program

### NO-GO Recovery

A NO-GO decision triggers fixes that loop back to Plans 007-009. The gate is re-evaluated after fixes are applied and the full test suite passes again.

### Execution

```
cmake --build build -j$(nproc)
build/bin/test_bitcoin
python3 test/functional/test_runner.py --configfile=build/test/config.ini
python3 test/functional/feature_sharepool_e2e.py --configfile=build/test/config.ini
python3 contrib/sharepool/settlement_model.py --self-test
python3 -m pytest contrib/sharepool/test_simulate.py
```

### Artifact

A decision report committed to the repository with test output evidence and the recorded GO or NO-GO verdict.

## Devnet Adversarial Testing

**Plan 011 -- research-shaped, pending regtest results.**

This plan is intentionally underspecified. The exact adversarial scenarios depend on the attack surface revealed by the regtest proof and the Plan 010 review. What follows is the expected minimum set.

### Infrastructure

- 4+ nodes across at least 2 independent hosts
- Separate devnet genesis with short BIP9 activation window
- 48+ hours continuous operation (24h baseline + adversarial phase)

### Phase 1: Stability Baseline (24 hours)

All nodes honest, mining continuously. Monitor:
- Block production rate
- Share production rate
- Relay latency
- Orphan rate
- Settlement commitment correctness
- Memory and storage growth

### Phase 2: Adversarial Scenarios

Each scenario documented with: hypothesis, test setup, expected behavior, actual behavior, verdict.

| Scenario | Description | Target |
|----------|-------------|--------|
| Share withholding | 25% miner withholds shares; measure advantage | Must not exceed 5% threshold from simulator |
| Eclipse attack | Isolate node, feed attacker shares; verify recovery after reconnection | Reward window must not diverge permanently |
| Relay spam | Flood node with invalid shares (wrong RandomX proof, wrong target, garbage) | Misbehavior scoring disconnects attacker; no performance degradation |
| Orphan flooding | Send shares with nonexistent parents to fill 64-entry orphan buffer | FIFO eviction works; no memory growth |
| Claim front-running | Two parties race to claim same settlement leaf | Exactly one succeeds (UTXO single-spend) |
| Settlement draining | Construct claim extracting more than committed leaf amount | Consensus rejection |
| Reorg with shares | Mine 2-3 block fork while shares produced on main chain | Reward window adjusts; orphaned-fork shares excluded |

### Phase 3: Results

All findings documented. If any scenario reveals a bug or exploitable vector, fix it, re-run the affected scenario, update the report. Stability report committed to the repository.

### Dependencies

- Plan 010 decision gate must be GO
- Devnet infrastructure provisioned (does not yet exist)

## Acceptance Criteria

### Plan 009 (Regtest E2E Proof)

- `feature_sharepool_e2e.py` exists in `test/functional/`
- Test passes on a clean build
- Running the test twice produces the same pass/fail result
- Restart/reindex proof shows multi-leaf commitment leaves can be reconstructed
  without relying on transient local relay history
- Test output includes measured reward distribution percentages
- All 7 requirements (R1-R7) are exercised and pass

### Plan 010 (Decision Gate)

- Full test suite executed (C++ unit tests, Python functional tests, model self-tests, simulator tests)
- All tests pass without regression
- GO/NO-GO decision recorded with evidence
- Decision report committed

### Plan 011 (Devnet Adversarial)

- 4+ nodes running across 2+ hosts for 48+ hours
- All adversarial scenarios from Phase 2 executed and documented
- No unresolved consensus bugs or exploitable attack vectors
- Stability report committed with measurement data

## Verification

### How to verify Plan 009 is complete

```
# Build
cmake -B build && cmake --build build -j$(nproc)

# Run the e2e proof
python3 test/functional/feature_sharepool_e2e.py --configfile=build/test/config.ini

# Verify output includes reward distribution percentages and all assertions pass
```

### How to verify Plan 010 is complete

A committed decision report exists with:
- Full test suite output (all green)
- E2e test run twice with matching results
- Measured reward distribution within +/-5%
- Explicit GO or NO-GO verdict

### How to verify Plan 011 is complete

A committed stability report exists with:
- Infrastructure description (node count, host distribution, uptime)
- Baseline metrics (24h nominal operation)
- Adversarial scenario results (hypothesis, setup, expected, actual, verdict for each)
- Final verdict on readiness for mainnet activation

## Open Questions

1. **50-block sample size.** Is 50 blocks sufficient for the proportional reward test at +/-5% tolerance? A power analysis against the simulator's CV predictions would resolve this.

2. **Claim throughput under load.** One claim per block per settlement output may create backlogs with many miners. The regtest proof exercises the happy path; the devnet should measure claim queue depth under sustained operation.

3. **1-second relay benchmark.** The relay benchmark (`feature_sharepool_relay_benchmark.py`) measured at 10-second intervals. The confirmed 1-second cadence needs separate measurement before devnet deployment to validate bandwidth assumptions.

4. **Devnet infrastructure source.** Whether to provision new hosts or temporarily reuse non-mining Contabo slots is undecided. The existing fleet runs mainnet; the devnet must be isolated.

5. **Adversarial scenario completeness.** The 7 scenarios listed are the expected minimum. The Plan 010 review may identify additional attack vectors from the regtest proof that should be tested on devnet.

6. **Cross-platform CI.** RNG-specific sharepool tests (relay, commitment, e2e) run only on Linux. macOS and Windows CI coverage for these tests is implicit but not verified. This is tracked as a gap but not blocking for regtest proof.

7. **Reward-window data availability.** Which persisted or block-carried data
   lets a node reconstruct multi-leaf reward commitments after restart, reindex,
   pruning, and reorg?
