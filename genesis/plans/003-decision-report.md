# Decision Gate 003: Evaluation Report

Date: 2026-04-13
Evaluator: Codex
Overall recommendation: NO-GO

## Input Artifacts

Plan 002 is complete enough to run the active queue's POOL-03 gate:

- `specs/sharepool.md` exists and defines the current sharepool candidate.
- `contrib/sharepool/simulate.py` exists and runs.
- `contrib/sharepool/test_simulate.py` passes.
- `contrib/sharepool/examples/two_miners_90_10.json` produces the expected 90/10 split.

Historical corpus instructions mention `contrib/sharepool/constants.json`,
`withholding-attack.json`, and `variance-100-blocks.json`. Those files do not
exist in the live tree. The live simulator keeps the candidate defaults in code
and tests the withholding and variance scenarios through unit-test traces and
callable simulator functions. This report therefore records the live commands
actually available in this repository.

## Question 1: Economic Soundness

Threshold: target share spacing and reward-window work are achievable, smooth
small-miner rewards, and do not create perverse incentives.

Observed:

- Current candidate: 10-second target share spacing, about 12 shares per
  120-second block, and reward-window work equivalent to about 720
  target-spacing shares.
- The basic proportional split is correct: the two-miner example pays 90.00% /
  10.00%.
- The same constants fail the small-miner variance gate in Question 3.

Verdict: FAIL.

The reward formula is deterministic and proportional, but the current constants
do not satisfy the pooled-mining purpose of smoothing rewards for a 10% miner.

## Question 2: Share Withholding

Threshold: withholder advantage must be less than 5%.

Observed:

The checked withholding trace used by `contrib/sharepool/test_simulate.py`
reports:

```text
{'miner': 'withholder', 'withheld_share_count': 2, 'withheld_work': 2,
 'honest_reward_roshi': 400000000, 'published_reward_roshi': 222222223,
 'delta_roshi': -177777777, 'advantage_percent': 0.0}
```

Verdict: PASS.

The current no-finder-bonus model gives no measurable share-withholding
advantage in the checked simulator trace. Withholding removes visible work and
reduces the withholder's reward in that trace.

## Question 3: Reward Variance

Threshold: coefficient of variation for a 10% miner over 100 blocks must be
less than 10%.

Observed:

```text
{'miner': 'miner_10pct', 'miner_fraction': 0.1, 'blocks': 100,
 'shares_per_block': 12, 'reward_window_work': 720, 'seed': 42,
 'mean_reward_roshi': 519754649.93, 'mean_reward_share': 0.10395093,
 'coefficient_of_variation_percent': 25.10297804}
```

The human-readable simulator output rounds this to:

```text
variance_10pct_100_blocks: mean_share=10.40% cv=25.10% seed=42
```

Verdict: FAIL.

The measured 25.10% CV is well above the 10% decision threshold. This alone
blocks consensus implementation on the current constants.

## Question 4: Commitment Size

Threshold: total coinbase commitment encoding must be under 100 bytes.

Observed from `specs/sharepool.md`:

- Current spendable candidate: `OP_2 <32-byte root>`.
- Optional discovery marker: `OP_RETURN <"RNGS"> <root>`.

Byte counts:

```text
witness_v2_script_bytes=34
witness_v2_txout_bytes=43
optional_opreturn_marker_script_bytes=38
optional_opreturn_marker_txout_bytes=47
combined_txout_bytes=90
```

Verdict: PASS.

The spendable witness-v2 output alone is 43 serialized txout bytes. Even if the
optional OP_RETURN marker is kept, the combined output footprint is 90 bytes,
below the 100-byte gate.

## Question 5: Claim Witness Program Feasibility

Threshold: the claim can be expressed as a witness-program interpreter without
new opcodes, and non-upgraded nodes treat the program as an unknown witness
version.

Observed:

- Live code recognizes witness v0 and v1 only in `src/script/solver.cpp`.
- Live tests explicitly treat `OP_2 <32-byte program>` as `WitnessUnknown`.
- The active spec selects witness version 2, the next unassigned version after
  Taproot.
- The active spec also records a separate POOL-07 design risk: one shared reward
  UTXO can normally be spent only once, so the final claim accounting model must
  still be proven before consensus code lands.

Verdict: PASS for witness-version allocation, with a recorded later design risk.

Witness version 2 is the correct candidate version to carry forward. It is not
promoted to a confirmed consensus parameter in `specs/sharepool.md` because the
overall gate is no-go and the constants revision loop must run first.

## Question 6: Activation Path

Threshold: the design remains deployable as a soft fork through version bits.

Observed:

- `src/consensus/params.h` currently defines only `DEPLOYMENT_TESTDUMMY` and
  `DEPLOYMENT_TAPROOT`.
- The spendable output form is an unknown witness-v2 program before activation.
- The optional OP_RETURN marker is consensus-valid metadata and not a funding
  source.
- The historical dated spec phrase "witness v2 OP_RETURN" is internally
  contradictory. The active `specs/sharepool.md` corrects this by using
  `OP_2 <32-byte root>` as the funding/claim output and making OP_RETURN
  metadata optional.

Verdict: PASS for the staged activation shape.

Version-bits activation remains realistic if the later implementation keeps
mainnet `NEVER_ACTIVE` until simulator, regtest, and devnet gates pass.

## Validation Commands

```bash
python3 -m pytest contrib/sharepool/test_simulate.py
cd contrib/sharepool && python3 simulate.py --scenario baseline --verbose
cd contrib/sharepool && python3 simulate.py --trace examples/two_miners_90_10.json --verbose
cd contrib/sharepool && python3 simulate.py --scenario baseline > /tmp/rng-pool03-run1.json
cd contrib/sharepool && python3 simulate.py --scenario baseline > /tmp/rng-pool03-run2.json
diff -u /tmp/rng-pool03-run1.json /tmp/rng-pool03-run2.json
```

## Required Revisions Before Re-Evaluation

POOL-01 and POOL-02 must be re-entered before any consensus code is written.

Specific revision targets:

1. Update `specs/sharepool.md` so the failed 10-second / 720-share candidate is
   not treated as the likely consensus constant set.
2. Extend the simulator with an explicit parameter sweep for share spacing,
   reward-window work, cold-start handling, and multi-seed variance.
3. Evaluate shorter share spacing as the next candidate family. A non-authority
   exploratory sweep shows that one-hour windows with higher share rates improve
   the current metric:

```text
spb=60 window=3600 seed42 cv=7.17%
spb=60 window=3600 seeds1-20 cv_min=4.22 cv_max=10.33 cv_mean=7.16
spb=120 window=7200 seed42 cv=3.33%
spb=120 window=7200 seeds1-20 cv_min=2.32 cv_max=8.06 cv_mean=5.02
```

4. Treat the relay cost of any shorter share spacing as an explicit input to
   the later POOL-06 relay viability gate. Do not silently pick a high share
   rate without measuring bandwidth and orphan behavior.
5. Re-run this POOL-03 decision gate after the spec and simulator revision.

## Final Decision

NO-GO.

Do not promote any `[PROPOSED - PENDING SIMULATOR VALIDATION]` constant in
`specs/sharepool.md` to confirmed status. Do not start `DEPLOYMENT_SHAREPOOL`,
sharechain, payout commitment, or wallet/miner implementation until the revised
constants pass a fresh decision report.
