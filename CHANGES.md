# RNG: Changes From Bitcoin Core v29

This document describes the live RNG mainnet parameters as of March 19, 2026.

## Repository

- Base: Bitcoin Core v29.0
- Mainnet binary names: `rngd`, `rng-cli`
- Mainnet default address prefix: `rng1`
- Mainnet P2P port: `8433`

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

The repo now ships a supported assumeutxo snapshot for fast first sync:

- Snapshot height: `15091`
- Snapshot base hash: `2c97b53893d5d4af36f2c500419a1602d8217b93efd50fac45f0c8ad187466eb`
- Snapshot txoutset hash: `9ca1b551b9837c0b0e9158436bac5051e4984d39f691e1374c4786a6c0ed5393`

This lets a new node load a verified UTXO set with `loadtxoutset` and continue
syncing near the tip instead of validating from height `0`.

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
