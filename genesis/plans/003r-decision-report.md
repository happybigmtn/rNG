# Decision Gate 003R: Revised Simulator Results Review

Date: 2026-04-13
Evaluator: Codex
Overall recommendation: GO
Supersedes: `genesis/plans/003-decision-report.md`

## Input Artifacts

POOL-03R reviewed the revised spec and simulator evidence produced by POOL-01R
and POOL-02R:

- `specs/sharepool.md`
- `contrib/sharepool/simulate.py`
- `contrib/sharepool/test_simulate.py`
- `contrib/sharepool/reports/pool-02r-revised-sweep.json`
- `genesis/plans/003-decision-report.md`

The dated `specs/120426-sharepool-protocol.md` remains historical context where
it still names the rejected 10-second / 720-share candidate, the reversed
`block_target / 12` target arithmetic, the contradictory "witness v2 OP_RETURN"
phrase, and the incorrect `1815 (95%)` threshold. The authoritative current
surface is `specs/sharepool.md`.

Live-code check: no sharepool consensus, relay, miner, wallet, or RPC code exists
yet. `src/consensus/params.h` still defines only `DEPLOYMENT_TESTDUMMY` and
`DEPLOYMENT_TAPROOT`; searches for `DEPLOYMENT_SHAREPOOL`, `submitshare`,
`getsharechaininfo`, `getrewardcommitment`, `shareinv`, and `getshare` in the
owned code surfaces returned no matches.

## Question 1: Economic Soundness

Threshold: target share spacing and reward-window work are achievable, smooth
small-miner rewards, and do not create perverse incentives.

Observed from `pool-02r-revised-sweep.json`:

| Candidate | Shares per block | Reward-window work | Seed 42 CV | Stress max CV | Status |
|-----------|------------------|--------------------|------------|---------------|--------|
| 10-second rejected baseline | 12 | 720 | 25.10297804% | 31.33413387% | FAIL |
| 2-second secondary candidate | 60 | 3600 | 7.17360085% | 10.33353533% | FAIL |
| 1-second primary candidate | 120 | 7200 | 3.33048048% | 8.06160552% | PASS |

Verdict: PASS for the 1-second / 7200 target-spacing-share candidate. The
2-second candidate and original 10-second baseline remain rejected.

The confirmed mainnet constants are target share spacing of 1 second and
reward-window work of 7200 target-spacing shares. With 120-second mainnet block
spacing, this implies 120 target-spacing shares per block and a roughly 60-block
window horizon. Relay bandwidth and orphan behavior are not decided by this
simulator gate and remain POOL-06-GATE requirements.

## Question 2: Share Withholding

Threshold: withholder advantage must be less than 5%.

Observed from the committed revised sweep:

```text
advantage_percent=0.0
honest_reward_roshi=400000000
published_reward_roshi=222222223
delta_roshi=-177777777
withheld_share_count=2
withheld_work=2
```

Verdict: PASS.

The no-finder-bonus model still gives no measurable advantage to withholding in
the checked simulator trace. The withholder loses visible work and receives less
reward in this trace.

## Question 3: Reward Variance

Threshold: coefficient of variation for a 10% miner over 100 blocks must be
less than 10% for seed `42` and for every stress seed `1..20`.

Observed:

- 1-second primary candidate: seed `42` CV `3.33048048%`; seeds `1..20` max CV
  `8.06160552%`; no failing seeds.
- 2-second secondary candidate: seed `42` CV `7.17360085%`; seeds `9` and `18`
  fail with CV above 10%; max CV `10.33353533%`.
- 10-second baseline: seed `42` CV `25.10297804%`; stress max CV
  `31.33413387%`.

Verdict: PASS for the 1-second candidate only.

## Question 4: Commitment Size

Threshold: total coinbase commitment encoding must be under 100 bytes.

Observed from the corrected current spec and prior byte-count analysis:

```text
witness_v2_script_bytes=34
witness_v2_txout_bytes=43
optional_opreturn_marker_script_bytes=38
optional_opreturn_marker_txout_bytes=47
combined_txout_bytes=90
```

Verdict: PASS.

The spendable `OP_2 <32-byte root>` output is 43 serialized txout bytes. Keeping
the optional zero-value `OP_RETURN <"RNGS"> <root>` discovery marker would bring
the combined footprint to 90 bytes, below the 100-byte gate.

## Question 5: Claim Witness Program Feasibility

Threshold: the claim can be expressed as a witness-program interpreter without
new opcodes, and non-upgraded nodes treat the program as an unknown witness
version before activation.

Observed:

- Live solver code recognizes witness v0 and witness v1 Taproot.
- Live tests treat `OP_2 <32-byte program>` as `WitnessUnknown`.
- Witness version 2 is the next unassigned witness version after Taproot.
- The current spec keeps the POOL-07 UTXO-accounting risk explicit: one shared
  reward UTXO can normally be spent only once, so POOL-07 must prove the final
  claim accounting model before consensus code lands.

Verdict: PASS for witness-version allocation and soft-fork shape, with the
POOL-07 claim-accounting risk still open.

The claim witness version is confirmed as witness version 2 for the next
implementation gates. This does not waive the later requirement to prove the
claim accounting model in code and tests.

## Question 6: Activation Path

Threshold: the design remains deployable as a soft fork through version bits.

Observed:

- `src/consensus/params.h` has no `DEPLOYMENT_SHAREPOOL` yet, so there is no
  deployment-slot conflict in live code.
- The current spec uses a spendable `OP_2 <32-byte root>` output for the funding
  and claim surface, not an impossible "witness v2 OP_RETURN" output.
- The optional OP_RETURN marker is metadata only and not a funding source.
- Current BIP9 deployment machinery exists for `DEPLOYMENT_TESTDUMMY` and
  `DEPLOYMENT_TAPROOT`.
- The mathematically correct 95% threshold over a 2016-block period is 1916, not
  1815.

Verdict: PASS.

Version-bits activation remains realistic if future implementation keeps
mainnet dormant until the later regtest and devnet gates pass.

## Confirmed Constants

POOL-03R promotes these constants in `specs/sharepool.md`:

- Target share spacing: 1 second.
- Mainnet share target ratio: 120 at 120-second block spacing, using
  `share_target = min(powLimit, block_target * (block_spacing / share_spacing))`.
- Reward-window work: 7200 target-spacing shares.
- Max orphan shares: 64.
- Claim witness version: 2.
- Optional commitment tag: `RNGS`.
- BIP9 period: 2016 blocks.
- BIP9 threshold for 95%: 1916 of 2016.

Rejected constants:

- 2-second / 3600 target-spacing-share candidate: rejected because stress seeds
  exceed the 10% variance threshold.
- 10-second / 720 target-spacing-share candidate: rejected because seed `42` and
  the stress sweep exceed the 10% variance threshold.

## Required Follow-Up

Do not re-enter POOL-01R or POOL-02R. Proceed to `CHKPT-02`.

`CHKPT-02` must still verify the deployment slot, witness version availability,
internal miner extension path, and QSB interaction before `POOL-04` starts.
`POOL-06-GATE` must later measure relay bandwidth, latency, orphan rate, and
share propagation completeness before payout and claim implementation proceeds.

## Validation Commands

```bash
python3 -m pytest contrib/sharepool/test_simulate.py
cd contrib/sharepool && python3 simulate.py --sweep revised-candidates > /tmp/pool-03r-revised-sweep.json && diff -u reports/pool-02r-revised-sweep.json /tmp/pool-03r-revised-sweep.json
cd contrib/sharepool && python3 simulate.py --scenario baseline --verbose
cd contrib/sharepool && python3 simulate.py --trace examples/two_miners_90_10.json --verbose
if rg -n "DEPLOYMENT_SHAREPOOL|submitshare|getsharechaininfo|getrewardcommitment|shareinv|getshare" src/consensus/params.h src/rpc src/protocol.h src/net_processing.cpp src/node/internal_miner.h src/node/internal_miner.cpp; then exit 1; else echo "no sharepool code yet"; fi
jq '.candidates[] | {id, status, shares_per_block, reward_window_work, seed_42_cv: .seed_42.coefficient_of_variation_percent, stress_max_cv: .stress_summary.max_cv_percent, failing_seeds: .stress_summary.failing_seeds}' contrib/sharepool/reports/pool-02r-revised-sweep.json
grep "CONFIRMED" specs/sharepool.md | wc -l
```

The sharepool-code search is expected to print `no sharepool code yet` until
`POOL-04` or later adds sharepool code.
