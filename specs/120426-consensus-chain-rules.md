# Specification: Consensus and Chain Rules

## Objective

Define the consensus rules governing RNG's blockchain: block timing, subsidy schedule, difficulty adjustment, coinbase maturity, weight limits, and soft-fork activation baseline. This spec captures the live mainnet behavior as implemented in Bitcoin Core v29.0-derived code with RNG-specific modifications.

## Evidence Status

### Verified Facts (grounded in source code)

- **Mainnet block target spacing**: 120 seconds (`nPowTargetSpacing = 120` in `src/kernel/chainparams.cpp`)
- **Non-mainnet block target spacing**: 60 seconds on testnet, testnet4, signet, and regtest (`nPowTargetSpacing = 60` in `src/kernel/chainparams.cpp`)
- **Initial block subsidy**: 50 RNG (`GetBlockSubsidy()` in `src/validation.cpp`, halving interval 2,100,000 blocks)
- **Halving interval**: 2,100,000 blocks (`nSubsidyHalvingInterval = 2100000` in `src/kernel/chainparams.cpp`)
- **Tail emission floor**: 0.6 RNG per block (custom modification to `GetBlockSubsidy()`)
- **Coinbase maturity**: 100 confirmations (`COINBASE_MATURITY = 100` in `src/consensus/tx_verify.cpp`)
- **Max block weight**: 4,000,000 weight units (`MAX_BLOCK_WEIGHT = 4000000` in `src/consensus/consensus.h`)
- **Block header size**: 80 bytes (unchanged from Bitcoin)
- **Block version**: `0x20000000` (BIP9-capable from genesis)
- **Difficulty algorithm**: Per-block LWMA retarget (Monero-style), replacing Bitcoin's 2016-block epoch retarget
- **Difficulty window**: 720 blocks (`nDifficultyWindow = 720` in `src/kernel/chainparams.cpp`)
- **Difficulty timestamp cut**: 60 (`nDifficultyCut = 60` — sorted window trims 60 from each end)
- **Minimum-difficulty flag on mainnet**: `fPowAllowMinDifficultyBlocks = true`
- **Retargeting enabled**: `fPowNoRetargeting = false` (mainnet)
- **Proof-of-work function**: RandomX (replaces SHA256d; see `src/crypto/randomx_hash.cpp`)
- **Smallest unit**: 1 roshi = 0.00000001 RNG (same precision as satoshi)
- **Genesis block hash**: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- **Genesis timestamp**: 1738195200 (2025-01-30 00:00:00 UTC)
- **Genesis coinbase message**: `Life is a random number generator`
- **Genesis output**: OP_RETURN (provably unspendable)
- **Genesis nBits**: `0x207fffff` (easiest safe difficulty)
- **Genesis merkle root**: `b713a92ad8104e5a1650d02f96df9cb18bd6a39a222829ba4e4b5e79e4de7232`
- **Mainnet restart**: February 26, 2026 (network restarted from genesis)
- **All soft forks active from height 0**: BIP34, BIP65, BIP66, BIP68, BIP112, BIP113, BIP141, BIP143, BIP147, Taproot (BIP340/341/342)
- **BIP9 activation threshold**: 1815 out of 2016 blocks (90%)
- **Current deployments**: Only `DEPLOYMENT_TESTDUMMY` and `DEPLOYMENT_TAPROOT` defined in `src/consensus/params.h`
- **Minimum chain work**: `0x5e9a730b` (in `src/kernel/chainparams.cpp`)
- **Default assume-valid block**: `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb` (height 15091)
- **BIP94 enforcement**: disabled (`enforce_BIP94 = false`)
- **Prune-after height**: 100,000 blocks

### Network Timing And Difficulty Parameters

| Network | Target spacing | Target timespan | Difficulty interval helper | LWMA window / cut | `fPowAllowMinDifficultyBlocks` | `fPowNoRetargeting` | `enforce_BIP94` |
|---------|----------------|-----------------|----------------------------|-------------------|-------------------------------|---------------------|-----------------|
| Mainnet | 120 seconds | 120 seconds | 1 block | 720 / 60 | `true` | `false` | `false` |
| Testnet | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `true` | `false` | `false` |
| Testnet4 | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `true` | `false` | `true` |
| Signet | 60 seconds | 14 days | 20,160 blocks | 720 / 60 | `false` | `false` | `false` |
| Regtest | 60 seconds | 1 day | 1,440 blocks | 720 / 60 | `true` | `true` | configurable |

`nDifficultyWindow` and `nDifficultyCut` default to 720 and 60 in
`src/consensus/params.h`; mainnet sets the same values explicitly. Regtest keeps
those values in params, but `fPowNoRetargeting = true` makes
`GetNextWorkRequired()` return the previous block's difficulty.

### Recommendations (intended system, not yet in code)

- Protocol-native pooled mining would add `DEPLOYMENT_SHAREPOOL` to the BIP9 deployment list
- Sharepool activation would use the existing BIP9 machinery with 95% threshold (1815/2016 adjusted to 1815 if period remains 2016)
- Witness version 2 reserved for claim programs (no code exists for this yet)

### Hypotheses / Unresolved Questions

- Whether `fPowAllowMinDifficultyBlocks = true` on mainnet is intentional long-term policy, launch-era configuration, or stale configuration. The current LWMA `GetNextWorkRequired()` path in `src/pow.cpp` does not branch on this flag, so its practical effect is unresolved.
- Exact supply cap: tail emission at 0.6 RNG makes total supply theoretically unbounded; the documented "1,000,000,000 RNG" figure is not a hard consensus cap

## Acceptance Criteria

- Block header is exactly 80 bytes and contains version, prev_hash, merkle_root, timestamp, nBits, nNonce
- Blocks with weight exceeding 4,000,000 WU are rejected by validation
- Coinbase outputs are unspendable until 100 confirmations have elapsed
- Block subsidy at height 0 is 50 RNG (5,000,000,000 roshi)
- Block subsidy halves at heights 2,100,000, 4,200,000, etc.
- After final halving, subsidy floors at 0.6 RNG (60,000,000 roshi) per block
- Difficulty retargets every block using LWMA over a 720-block window
- Difficulty window sorts timestamps and trims 60 from each end before computing
- Genesis block hash matches `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- All script validation flags for SegWit, Taproot, and pre-SegWit soft forks are active from block 0
- Blocks with version < `0x20000000` are not produced by the internal miner
- `GetBlockSubsidy()` returns the tail emission floor rather than zero after the last halving

## Verification

```bash
# Verify genesis hash
rng-cli getblockhash 0
# Expected: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4

# Verify block subsidy at genesis
rng-cli getblock $(rng-cli getblockhash 0) 2 | jq '.tx[0].vout[0].value'
# Expected: 50.00000000

# Verify difficulty retargets every block (compare consecutive blocks)
rng-cli getblockheader $(rng-cli getblockhash 100) | jq '.difficulty'
rng-cli getblockheader $(rng-cli getblockhash 101) | jq '.difficulty'
# Values should differ (per-block LWMA retarget)

# Verify coinbase maturity
rng-cli getblockchaininfo | jq '.blocks'
# Coinbase at height N is spendable only at height N+100

# Verify SegWit active from genesis
rng-cli getblockchaininfo | jq '.softforks.segwit'
# status: "active", height: 0

# Verify Taproot active from genesis
rng-cli getblockchaininfo | jq '.softforks.taproot'
# status: "active"

# Verify documented per-network timing and difficulty params against source
rg -n "nPowTargetSpacing|fPowAllowMinDifficultyBlocks|fPowNoRetargeting|enforce_BIP94" src/kernel/chainparams.cpp
rg -n "Mainnet \\|.*120 seconds|Testnet \\|.*60 seconds|Regtest \\|.*60 seconds" specs/consensus.md specs/120426-consensus-chain-rules.md
```

## Open Questions

1. Is `fPowAllowMinDifficultyBlocks = true` intended as permanent mainnet policy? On Bitcoin, this flag is testnet-only. On RNG it is currently configured on mainnet, but the LWMA difficulty implementation does not branch on it.
2. Should the documented "1 billion RNG" supply figure be reconciled with the unbounded tail emission? The tail emission means supply grows perpetually at 0.6 RNG/block after all halvings complete.
3. Does the 720-block difficulty window need tuning as network hashrate grows? The current window is calibrated for a small operator-seeded network.
4. Should `enforce_BIP94` be enabled in a future release? It is currently `false` on mainnet.
