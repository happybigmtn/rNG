# Consensus Parameters

## Source Of Truth

Live consensus parameters are defined in `src/kernel/chainparams.cpp`, with
defaults in `src/consensus/params.h` and difficulty calculation in `src/pow.cpp`.
This file is a human-readable summary of those parameters.

## Current Behavior

RNG uses RandomX proof-of-work with a Monero-style per-block LWMA difficulty
calculation. Mainnet targets 120-second blocks. Testnet, testnet4, signet, and
regtest target 60-second blocks.

### Block Timing And Difficulty

| Network | Chainparams class | Target spacing | Target timespan | Difficulty interval helper | LWMA window / cut | Min-difficulty flag | Retargeting disabled | BIP94 enforcement |
|---------|-------------------|----------------|-----------------|----------------------------|-------------------|---------------------|----------------------|-------------------|
| Mainnet | `CMainParams` | 120 seconds | 120 seconds | 1 block | 720 / 60 | `true` | `false` | `false` |
| Testnet | `CTestNetParams` | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `true` | `false` | `false` |
| Testnet4 | `CTestNet4Params` | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `true` | `false` | `true` |
| Signet | `SigNetParams` | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `false` | `false` | `false` |
| Regtest | `CRegTestParams` | 60 seconds | 1 day | 1,440 blocks | 720 / 60 | `true` | `true` | configurable |

`nDifficultyWindow` and `nDifficultyCut` default to 720 and 60 in
`src/consensus/params.h`; mainnet sets the same values explicitly. Regtest keeps
those values in params, but `fPowNoRetargeting = true` makes the LWMA retarget
path return the previous block's difficulty.

### Mainnet Minimum-Difficulty Flag

`src/kernel/chainparams.cpp` sets `fPowAllowMinDifficultyBlocks = true` on
mainnet. That is unusual relative to Bitcoin, where this flag is normally used
on test networks.

The policy rationale is not documented in code. The practical effect is also an
open implementation question: the current RNG `GetNextWorkRequired()` path in
`src/pow.cpp` uses LWMA retargeting and does not branch on
`fPowAllowMinDifficultyBlocks`.

## Block Rewards

- Initial subsidy: 50 RNG.
- Halving interval: 2,100,000 blocks on mainnet, testnet, testnet4, and signet.
- Regtest halving interval: 150 blocks.
- Tail emission floor: none in live consensus. `GetBlockSubsidy()` keeps the
  Bitcoin-derived halving schedule and returns `0` once `halvings >= 64`.
- Smallest unit: 1 roshi = 0.00000001 RNG.
- `MAX_MONEY = 21,000,000 RNG` remains a consensus sanity cap.

## Other Consensus Rules

- Coinbase maturity: 100 confirmations.
- Max block weight: 4,000,000 weight units.
- Block header size: 80 bytes.
- Proof-of-work hash: RandomX over the serialized 80-byte block header.
- RandomX seed policy: fixed genesis seed for every block height.
- Mainnet genesis hash:
  `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`.
- All buried soft forks are active from height 0.
- Current BIP9 deployments are `DEPLOYMENT_TESTDUMMY`,
  `DEPLOYMENT_TAPROOT`, and dormant `DEPLOYMENT_SHAREPOOL`.

## Open Questions

1. Is `fPowAllowMinDifficultyBlocks = true` intended to remain a mainnet policy,
   or is it stale configuration left over from launch and testnet defaults?
2. Should the LWMA difficulty code deliberately use or ignore the
   min-difficulty flag? The current code ignores it.
3. Should the historical 0.6 RNG tail-emission goal be implemented in
   consensus, or should all supply docs preserve the current no-tail behavior?

## Verification

```bash
rg -n "nPowTargetSpacing|fPowAllowMinDifficultyBlocks|fPowNoRetargeting|enforce_BIP94" src/kernel/chainparams.cpp
rg -n "Mainnet \\|.*120 seconds|Testnet \\|.*60 seconds|Regtest \\|.*60 seconds|fPowAllowMinDifficultyBlocks" specs/consensus.md
```
