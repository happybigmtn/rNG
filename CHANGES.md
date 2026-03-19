# RNG: Changes From Bitcoin Core v29

This document describes the live RNG mainnet parameters as of March 19, 2026.

## Repository

- Base: Bitcoin Core v29.0
- Mainnet binary names: `rngd`, `rng-cli`
- Mainnet default address prefix: `rng1`
- Mainnet P2P port: `8433`

## Exact Delta From Upstream Bitcoin Core

### Consensus and Chain Identity

- Replaced SHA256d proof of work with RandomX
- Added RNG-specific genesis block and canonical coinbase message
- Changed mainnet network magic bytes, default ports, and Bech32 HRP
- Enabled BIP34, BIP65, BIP66, CSV, SegWit, and Taproot from genesis
- Added RNG-specific assumeutxo metadata for the live chain

### Mining Behavior

- Added internal RandomX mining RPC/status surface via `getinternalmininginfo`
- Added `-mine`, `-mineaddress`, `-minethreads`, `-minerandomx`, and `-minepriority` runtime handling for RNG's live miner workflow
- Default live mode is RandomX `fast`

### Repository and Distribution

- Renamed public binaries and datadir conventions from Bitcoin defaults to RNG defaults
- Added repo-bundled bootstrap assets for the live chain:
  - near-tip datadir archive
  - assumeutxo snapshot
- Added installer and operator helpers for mining/bootstrap/health checks
- Added CMake-time patching of upstream RandomX so fresh clones build the live chain without a dirty nested checkout

### Kept From Upstream

- UTXO transaction model
- Bitcoin script engine and transaction/block serialization
- Wallet/key model and most Bitcoin-derived RPC semantics
- secp256k1 cryptography, mempool policy structure, P2P protocol shape, and node architecture unless RNG-specific notes above say otherwise

## Consensus Changes

### 1. Proof Of Work

RNG replaces Bitcoin's SHA256d proof of work with RandomX.

- Algorithm: RandomX
- Target block interval: 120 seconds
- Difficulty retarget: every block
- Mainnet difficulty window: 720 blocks
- Mainnet timestamp cut: 60 timestamps from each side of the sorted window
- Live mainnet RandomX constants:
  - Genesis seed phrase: `RNG Genesis Seed`
  - ARGON salt: `RNGCHAIN01`
  - Seed policy: fixed genesis seed at all heights

### 2. Fast Bootstrap Snapshot

The repo now ships supported bootstrap assets for fast first sync:

- Near-tip datadir archive height: `15244`
- Near-tip datadir archive SHA256: `bf0bfad8054c73dc732391f2420d8b9f20f3c8276360745706783079a004c73d`

- Snapshot height: `15091`
- Snapshot base hash: `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb`
- Snapshot txoutset hash: `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393`

This lets a new node either unpack a near-tip chain bundle immediately or load a
verified UTXO set with `loadtxoutset` and continue syncing near the tip instead of
validating from height `0`.

The repo also includes two setup helpers for the live network:

- `rng-load-bootstrap`
- `rng-start-miner`

Excessive per-block LWMA sync logging was also removed so fresh nodes do not flood
`debug.log` during initial sync.

### 3. Genesis Block

- Genesis hash: `83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4`
- Coinbase message: `Life is a random number generator`
- Genesis output: `OP_RETURN` commitment, making the genesis reward unspendable
- Mainnet reset: February 26, 2026

### 4. Monetary Policy

- Subsidy halving interval: `2,100,000` blocks
- Tail emission: `0.6 RNG`
- Smallest unit: `1 RNG = 100,000,000 roshi`

### 5. Script And Deployment Policy

- BIP34, BIP65, BIP66, CSV, SegWit, and Taproot are active from genesis
- `generatetoaddress` is intentionally available on mainnet

## Branding And Operational Changes

- Client version string: `/RNG:3.0.0/`
- Public default datadir: `~/.rng`
- Public config file: `~/.rng/rng.conf`
- Public operator seed peers currently come from the live Contabo fleet
- The current live network is operator-seeded, so low peer counts and zero third-party miners are normal

## What Was Not Changed

- secp256k1 cryptography
- Transaction and block structure
- UTXO model
- Wallet model and key formats
- Bitcoin-derived RPC surface, except where RNG explicitly extends mining behavior

## Verification

```bash
git clone https://github.com/happybigmtn/rng.git
cd rng
./install.sh
rngd -daemon
sleep 10
rng-cli getblockhash 0
```

Expected genesis hash:

```text
83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4
```
