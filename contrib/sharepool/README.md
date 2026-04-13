# Sharepool Simulator

This directory contains the POOL-02 offline economic simulator for the planned
RNG sharepool protocol. It implements the reward-window, payout-leaf, remainder
allocation, and commitment-root formulas from `specs/sharepool.md`.

Run the required proportional split smoke test:

```bash
cd contrib/sharepool
python3 simulate.py --trace examples/two_miners_90_10.json --verbose
```

Run the unit tests:

```bash
python3 -m pytest contrib/sharepool/test_simulate.py
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
