# Specification: Economic Model and Simulation Validation

## Objective

Define the simulator-proven constants, variance bounds, and withholding resistance
that underpin the sharepool protocol. Every consensus constant adopted by the
POOL-03R decision gate traces back to deterministic evidence produced by
`contrib/sharepool/simulate.py` and `contrib/sharepool/settlement_model.py`.

This specification records what was measured, what passed, what failed, and what
remains unmodeled.

## Evidence Status

### Verified Facts

- **Simulator exists and is deterministic.** `contrib/sharepool/simulate.py` (789 lines) replays share traces offline using a seeded PRNG (`random.Random(seed)`). Identical seed and config produce identical commitment roots and reward splits.
- **Settlement reference model exists.** `contrib/sharepool/settlement_model.py` (660 lines) implements the many-claim state machine for settlement outputs. It produces deterministic test vectors consumed by C++ parity tests.
- **1-second candidate passes all variance gates.** The `primary_1s` candidate in `contrib/sharepool/reports/pool-02r-revised-sweep.json` shows max CV 8.06% across seeds 1-20 and CV 3.33% for seed 42. Zero failing seeds.
- **10-second baseline rejected.** CV 25.10% for seed 42; max CV 31.33% across stress seeds. Formally NO-GO at POOL-03.
- **2-second candidate rejected.** Passes seed 42 (CV 7.17%) but fails seeds 9 (CV 10.33%) and 18 (CV 10.33%). Two failing stress seeds disqualify it.
- **Withholding advantage is 0%.** The built-in withholding trace (withholder holds back 2 of 4 shares) shows `advantage_percent: 0.0` and `delta_roshi: -177777777` (withholder loses money). Status: pass.
- **Settlement vectors have C++ parity.** `src/test/sharepool_commitment_tests.cpp` reads `contrib/sharepool/reports/pool-07b-settlement-vectors.json` and reproduces all hashes in C++.
- **Test suite covers simulator.** `contrib/sharepool/test_simulate.py` tests proportional reward splits, deterministic replay, reorg behavior, and more.

### Recommendations

- Adopt the 1-second / 7200-share constants as confirmed consensus parameters. These are the only tested configuration that passes every required variance seed.
- Treat settlement vector parity (Python-to-C++) as a regression gate. Any change to leaf serialization or Merkle construction must update both `settlement_model.py` vectors and `sharepool_commitment_tests.cpp`.
- Do not weaken the 10% CV threshold without re-running the full stress sweep and documenting the rationale.

### Hypotheses / Unresolved Questions

- Claim fee market behavior is unmodeled (see Unmodeled Economic Surfaces).
- 1-second relay bandwidth is extrapolated, not measured end-to-end.
- Adversarial scenarios beyond simple withholding are deferred to Plan 011 devnet testing.

## Simulator Design

**File:** `contrib/sharepool/simulate.py` (789 lines)

**Architecture:** Deterministic offline economic simulator. No node state, RPC, or consensus code dependencies.

**Core classes:**

| Class | Role |
|-------|------|
| `Share` | Frozen dataclass: `share_id`, `miner`, `payout_script`, `work`, `parent_share`, `withheld` flag |
| `RewardLeaf` | Frozen dataclass: `miner`, `payout_script`, `amount_roshi`, share range, work |
| `PayoutResult` | Block-level payout: commitment root, leaves, window stats |
| `SimulationResult` | Full run output: block results, pending payout, withholding, variance |
| `SharepoolSimulator` | Stateful simulator: builds reward windows, computes payouts, Merkle roots |

**Configurable parameters:**

| Parameter | Default | Source |
|-----------|---------|--------|
| `block_reward_roshi` | `50 * COIN` (5,000,000,000 roshi) | `DEFAULT_BLOCK_REWARD_ROSHI` |
| `block_spacing_seconds` | 120 | `DEFAULT_BLOCK_SPACING_SECONDS` |
| `share_spacing_seconds` | 10 (overridden to 1 for accepted candidate) | `DEFAULT_SHARE_SPACING_SECONDS` |
| `reward_window_work` | 720 (overridden to 7200 for accepted candidate) | `DEFAULT_REWARD_WINDOW_SHARES` |
| `variance_miner_fraction` | 0.10 | Trace config or `POOL02R_VARIANCE_MINER_FRACTION` |
| `variance_blocks` | 100 | `POOL02R_VARIANCE_BLOCKS` |
| `variance_seed` | 42 (continuity) / 1-20 (stress) | `POOL02R_CONTINUITY_SEED`, `POOL02R_STRESS_SEEDS` |

**Variance measurement** (`measure_reward_variance`):
1. Generate `blocks * shares_per_block` shares using seeded PRNG.
2. Each share is assigned to `miner_10pct` with probability `miner_fraction` (0.10), else `miner_rest`.
3. For each block, compute reward window payout and record `miner_10pct` reward.
4. Compute population standard deviation and coefficient of variation (CV = 100 * stddev / mean).

**Withholding measurement** (`measure_withholding`):
1. Run honest trace to get honest rewards.
2. Remove withheld shares; recompute rewards using nearest published ancestor as tip.
3. Advantage = `max(0, (published_reward - honest_reward) / honest_reward * 100)`.

**Entry points:**

| CLI flag | Behavior |
|----------|----------|
| `--trace <path>` | Replay a JSON or CSV share trace |
| `--scenario baseline` | Run built-in 90/10 split baseline |
| `--sweep revised-candidates` | Run full POOL-02R parameter sweep, output JSON report |

**Test suite:** `contrib/sharepool/test_simulate.py`

| Test | What it verifies |
|------|------------------|
| `test_90_10_work_split_produces_proportional_reward_leaves` | 90/10 work split produces ~90%/~10% reward |
| `test_deterministic_replay_produces_identical_commitment_roots` | Same trace always yields same commitment root |
| `test_reorged_share_suffix_changes_only_affected_window_outputs` | Reorg isolation: only affected windows change |

## Confirmed Constants

All constants below are `[CONFIRMED]` in `specs/sharepool.md` by the POOL-03R decision gate.

| Constant | Value | Derivation |
|----------|-------|------------|
| Target share spacing | 1 second | Only candidate passing all 21 variance seeds |
| Block spacing (mainnet) | 120 seconds | Network parameter |
| Shares per block | 120 | `block_spacing / share_spacing` |
| Share target ratio | Mainnet: `share_target = min(powLimit, block_target * 120)` | Generic formula: `block_target * (block_spacing / share_spacing)`; test networks use their own `consensus.nPowTargetSpacing` |
| Reward window work | 7200 target-spacing shares | 120 shares/block * 60 blocks = 2-hour smoothing horizon |
| Window horizon | 60 blocks (~2 hours at 120s spacing) | `reward_window_work / shares_per_block` |
| Cold start | 60 blocks before full window | First 60 blocks use partial windows |
| Max orphan shares | 64 | In-memory relay buffer |
| Claim witness version | 2 | Next unassigned after Taproot |
| Commitment tag | `RNGS` (optional) | Discovery marker only, not a funding source |
| BIP9 period | 2016 blocks | Standard version-bits shape |
| BIP9 threshold | 1916 of 2016 (95%) | Corrected from older erroneous 1815 figure |

## Decision History

### POOL-03: Initial Baseline (NO-GO)

- **Candidate:** 10-second share spacing, 720-share reward window
- **Shares per block:** 12
- **Seed 42 CV:** 25.10% -- exceeds 10% threshold
- **Stress seed max CV:** 31.33% (seed 7)
- **Stress seed mean CV:** 15.90%
- **Decision:** NO-GO
- **Date:** 2026-04-13
- **Consequence:** Triggered revision loop back to Plan 002

### POOL-03R: Revised Candidates (GO)

Three candidates were swept. Results for the 10% miner across 100 blocks:

| Candidate | Spacing | Window | Seed 42 CV | Stress max CV | Stress mean CV | Failing seeds | Status |
|-----------|---------|--------|------------|---------------|----------------|---------------|--------|
| `rejected_10s` | 10s | 720 | 25.10% | 31.33% | 15.90% | 16 of 20 | comparison_fail |
| `secondary_2s` | 2s | 3600 | 7.17% | 10.33% | 7.16% | 2 of 20 | fail |
| `primary_1s` | 1s | 7200 | 3.33% | 8.06% | 5.02% | 0 of 20 | **pass** |

- **Withholding advantage:** 0.00% (withholder loses 177,777,777 roshi relative to honest play)
- **Decision:** GO on `primary_1s`
- **Date:** 2026-04-13
- **Evidence:** `contrib/sharepool/reports/pool-02r-revised-sweep.json`

### Acceptance Thresholds (from Plan 003)

| ID | Criterion | Threshold | Measured | Result |
|----|-----------|-----------|----------|--------|
| R1 | 10% miner CV for seed 42 | < 10% | 3.33% | PASS |
| R1 | 10% miner CV for seeds 1-20 (max) | < 10% | 8.06% | PASS |
| R2 | Withholding advantage | < 5% | 0.00% | PASS |
| R3 | Decision committed with evidence | committed | `pool-02r-revised-sweep.json` | PASS |

## Settlement Reference Model

**File:** `contrib/sharepool/settlement_model.py` (660 lines)

**Purpose:** Proves the many-claim accounting state machine for a single compact
pooled-reward settlement output. Sits one layer below `simulate.py`: where
`simulate.py` proves reward-window math and payout-root determinism,
`settlement_model.py` proves claim accounting correctness.

**Key types:**

| Type | Role |
|------|------|
| `SettlementLeaf` | Leaf in the payout Merkle tree: script, amount, share range |
| `SettlementDescriptor` | Version + payout root + leaf count; hashed with `"RNGSharepoolDescriptor"` tag |
| Merkle functions | `merkle_levels`, `merkle_branch`, `verify_merkle_branch` |

**Test vectors:** `contrib/sharepool/reports/pool-07b-settlement-vectors.json`

| Scenario | What it proves |
|----------|----------------|
| `initial_state` | 3-leaf settlement with 50 RNG total, all unclaimed. Descriptor hash, payout root, claim status root, state hash. |
| `one_valid_claim_transition` | Leaf 1 (10 RNG) claimed. Status bit flips, state hash updates, successor output = 40 RNG. |
| `final_claim_transition` | Last remaining leaf (25 RNG) claimed. No successor output (settlement fully drained). |
| `duplicate_claim_rejection` | Re-claiming leaf 1 produces error `"leaf 1 is already claimed"`. |
| `non_settlement_fee_funding` | Leaf 0 (15 RNG) claimed with 50,000 roshi extra input and 10,000 roshi fee. Proves fee funding from non-settlement inputs. |

**C++ parity:** `src/test/sharepool_commitment_tests.cpp` reads the same JSON
vectors and reproduces every hash using the C++ consensus implementation
(`consensus::sharepool::*` functions). This is the cross-language regression
gate.

## Unmodeled Economic Surfaces

The following economic behaviors are not captured by the current simulator or
settlement model. Each is a hypothesis that needs separate proof before mainnet
activation.

| Surface | What is unknown | Where it gets addressed |
|---------|----------------|------------------------|
| **Claim fee market** | Fees come from non-settlement inputs. The cost to a small miner of funding a claim transaction is unknown. The `non_settlement_fee_funding` vector proves the mechanism works but does not model fee pressure under load. | Plan 011 (devnet adversarial testing) |
| **Claim throughput** | One claim per block per settlement output. With many miners, a backlog could form. The queue depth and worst-case claim latency are not measured. | Plan 011 |
| **Sharechain storage cost** | At 1-second cadence, the sharechain grows at ~120 shares per block. Storage requirements for long-running nodes are not measured. | Plan 011 |
| **1-second relay bandwidth** | POOL-06-GATE measured relay at 10-second intervals. The 1-second extrapolation (~0.6 KB/s per peer) is linear scaling, not an end-to-end measurement. | Plan 009 (regtest proof) or Plan 011 |
| **Adversarial withholding** | The simulator models a single withholder hiding 2 of 4 shares. Eclipse attacks, selfish-mining variants, and coordinated withholding are not tested. | Plan 011 |
| **Spam shares** | Share validation cost and DoS resistance at 1-second cadence are not profiled. | Plan 011 |
| **Difficulty adjustment interaction** | The simulator uses fixed difficulty. Interaction between share difficulty retargeting and block difficulty retargeting is not modeled. | Plan 011 |

## Acceptance Criteria

- The 1-second / 7200-share candidate must show CV < 10% for seed 42 and all seeds 1-20 in `contrib/sharepool/reports/pool-02r-revised-sweep.json`. **Status: met.**
- Withholding advantage must be < 5% in the same report. **Status: met (0.00%).**
- Settlement test vectors in `contrib/sharepool/reports/pool-07b-settlement-vectors.json` must cover: initial state, single claim, final claim (drain), duplicate rejection, and fee funding. **Status: met (5 scenarios).**
- C++ parity tests in `src/test/sharepool_commitment_tests.cpp` must reproduce all settlement vector hashes. **Status: met.**
- The decision must be committed with evidence tracing to the report files. **Status: met (POOL-03R in Plan 003).**

## Verification

```bash
# Reproduce the variance sweep from scratch
cd contrib/sharepool && python3 simulate.py --sweep revised-candidates

# Verify the 1-second candidate passes (jq extracts max CV)
python3 simulate.py --sweep revised-candidates \
  | jq '.candidates[] | select(.id == "primary_1s") | .stress_summary.max_cv_percent'
# Expected: 8.06160552

# Verify withholding advantage
python3 simulate.py --sweep revised-candidates \
  | jq '.withholding.advantage_percent'
# Expected: 0.0

# Run simulator unit tests
cd contrib/sharepool && python3 -m pytest test_simulate.py -q

# Regenerate settlement vectors
cd contrib/sharepool && python3 settlement_model.py

# Run C++ settlement parity tests
cd <build-dir> && ctest -R sharepool_commitment_tests

# Verify confirmed constants in spec
grep "CONFIRMED" specs/sharepool.md | wc -l
# Expected: >= 8
```

## Open Questions

1. **Should the CV threshold be tightened?** The 1-second candidate has mean CV 5.02% with headroom to 10%. A tighter threshold (e.g., 7%) would still pass but would narrow the margin for future parameter changes.
2. **What is the claim backlog worst case?** With N miners, N-1 must wait for block inclusion. If N exceeds ~100, the queue could span hours. This needs measurement on devnet.
3. **Does 1-second relay scale to real network topology?** The 0.6 KB/s extrapolation assumes the same message size as 10-second shares. Actual 1-second shares on a multi-hop network may show different latency characteristics.
4. **Should the withholding model test more configurations?** The current test uses 4 shares from one miner with 2 withheld. A sweep across different withholding fractions and miner sizes would strengthen confidence.
