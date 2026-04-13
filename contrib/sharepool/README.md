# Sharepool Simulator

This directory contains the POOL-02 offline economic simulator for the planned
RNG sharepool protocol. It implements the reward-window, payout-leaf, remainder
allocation, and commitment-root formulas from `specs/sharepool.md`. It also
contains the POOL-07B settlement-state reference model for many-claim pooled
reward accounting from `specs/sharepool-settlement.md`.

Run the required proportional split smoke test:

```bash
cd contrib/sharepool
python3 simulate.py --trace examples/two_miners_90_10.json --verbose
```

Run the unit tests:

```bash
python3 -m pytest contrib/sharepool/test_simulate.py contrib/sharepool/test_settlement_model.py
```

Run the POOL-02R revised candidate sweep and compare it with the committed
artifact:

```bash
cd contrib/sharepool
python3 simulate.py --sweep revised-candidates > /tmp/pool-02r-revised-sweep.json
diff -u reports/pool-02r-revised-sweep.json /tmp/pool-02r-revised-sweep.json
```

Run the settlement-state self-test and compare it with the committed vectors:

```bash
cd contrib/sharepool
python3 settlement_model.py --self-test
```

Rewrite the committed settlement vectors:

```bash
cd contrib/sharepool
python3 settlement_model.py --write-report reports/pool-07b-settlement-vectors.json
```

Trace files can be JSON or CSV. JSON traces may provide explicit `shares` or
compact `share_runs`. Each share has a miner, positive work, optional parent,
optional payout script, and optional `withheld` marker.

The simulator reports:

- reward leaves by miner and deterministic payout commitment roots;
- pending entitlement at the chosen pending tip before any block is found;
- share-withholding advantage for shares marked `withheld`;
- deterministic 10% miner reward variance over 100 blocks.

The baseline no-finder-bonus withholding metric is non-positive in the checked
trace model: withholding own shares removes visible work and does not increase
the withholder's proportional payout. Future protocol changes such as finder
bonuses must be re-simulated here before constants are confirmed.

With the default deterministic variance seed (`42`), the current candidate
constants report a 10.40% mean reward share for the 10% miner and a 25.10%
per-block coefficient of variation across 100 blocks. POOL-03 must decide
whether that measured variance is acceptable or requires revised constants.

POOL-03 result (2026-04-13): no-go. The 25.10% CV exceeds the 10% decision
threshold, so the sharepool spec and simulator must be revised before consensus
implementation starts.

POOL-02R revised sweep result (2026-04-13):

| Candidate | `shares_per_block` | `reward_window_work` | seed `42` CV | seeds `1..20` max CV | Status |
|-----------|--------------------|----------------------|--------------|----------------------|--------|
| 10-second rejected baseline | 12 | 720 | 25.10% | 31.33% | comparison fail |
| 2-second secondary candidate | 60 | 3600 | 7.17% | 10.33% | fail |
| 1-second primary candidate | 120 | 7200 | 3.33% | 8.06% | pass |

The checked withholding metric remains `0.00%` advantage, below the `5%`
threshold. The 1-second primary candidate is the only revised candidate that
passes every POOL-02R variance threshold. Constants remain unconfirmed until
POOL-03R reviews the committed sweep evidence.
