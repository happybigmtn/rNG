# Consensus Parameters Specification

## Topic
The consensus rules that govern block creation, rewards, and difficulty adjustment.

## Source of truth

For the live network, the canonical parameters are the code in
`src/kernel/chainparams.cpp`, `src/pow.cpp`, and the public install docs in
`README.md`. This file is a human-readable summary of those live parameters.

## Behavioral Requirements

### Block Timing
- Target block time: **120 seconds**
- Rationale: faster confirmations than Bitcoin while leaving more time for a
  small CPU-mined network to propagate blocks cleanly

### Block Rewards
- Initial block reward: **50 RNG**
- Halving interval: **2,100,000 blocks**
- Tail emission floor: **0.6 RNG** per block once halvings would drop below that
- Smallest unit: **1 roshi** = `0.00000001 RNG`
- Consensus sanity cap: `MAX_MONEY = 1,000,000,000 RNG` (not a fixed-supply promise)

### Difficulty Adjustment
- Retarget interval: **every block**
- Target spacing: **120 seconds**
- Window: **720 blocks**
- Timestamp cut: **60 timestamps** trimmed from each side of the sorted window
- Algorithm: Monero-style difficulty calculation over recent timestamps/work,
  rather than Bitcoin's 2016-block epoch retarget

### Coinbase Maturity
- Coinbase outputs spendable after: **100 confirmations** (~200 minutes at target)

## Acceptance Criteria

1. [ ] Block at height 1 has reward of 50 RNG
2. [ ] Block at height 2,100,000 has reward of 25 RNG
3. [ ] Block at height 4,200,000 has reward of 12.5 RNG
4. [ ] Difficulty adjusts correctly when blocks are too fast (increases)
5. [ ] Difficulty adjusts correctly when blocks are too slow (decreases)
6. [ ] Difficulty recalculates on every block using the 720-block window
7. [ ] Subsidy never drops below the 0.6 RNG tail-emission floor
8. [ ] Coinbase transaction is unspendable until 100 confirmations
9. [ ] Block timestamp validation allows reasonable clock drift

## Test Scenarios

- Generate a long run of blocks at varying speeds, verify difficulty adjusts correctly
- Mine past halving height, verify reward reduction
- Mine past the final halvings, verify the 0.6 RNG tail emission floor
- Attempt to spend immature coinbase, verify rejection
- Confirm target spacing is treated as 120 seconds by sync/progress code
