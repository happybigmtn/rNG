# RNG Minimum Viable Fork - Specification Index

## Overview

RNG is a Bitcoin fork with **RandomX proof-of-work** optimized for AI agents. This document indexes all specifications required to launch.

The live-network source of truth is the code in `src/`, plus the public
operator docs in `README.md` and `CHANGES.md`. The `specs/` directory is useful
reference material, but some files began as early planning docs and may lag the
live chain unless explicitly updated.

## Specification Status

| Spec | File | Status | Description |
|------|------|--------|-------------|
| Consensus | [consensus.md](consensus.md) | ✅ Complete | Block time, rewards, halving, difficulty |
| Network | [network.md](network.md) | ✅ Complete | Magic bytes, ports, protocol version |
| Addresses | [addresses.md](addresses.md) | ✅ Complete | All prefixes, version bytes, Bech32 |
| Genesis | [genesis.md](genesis.md) | ✅ Complete | Genesis block construction |
| RandomX | [randomx.md](randomx.md) | ✅ Complete | PoW algorithm, seed rotation |
| Blocks | [blocks.md](blocks.md) | ✅ Complete | Block structure, weight limits |
| Transactions | [transactions.md](transactions.md) | ✅ Complete | Tx format, fees, dust |
| Activation | [activation.md](activation.md) | ✅ Complete | Soft forks active from genesis |
| Testnets | [testnets.md](testnets.md) | ✅ Complete | Testnet/regtest parameters |
| Branding | [branding.md](branding.md) | ✅ Complete | User-facing identity |
| **Agent Integration** | [agent-integration.md](agent-integration.md) | ✅ Complete | MCP server, wallets, mining, autonomy |
| **Agent Ecosystem** | [agent-ecosystem.md](agent-ecosystem.md) | ✅ Complete | Engagement triggers, building on RNG, viral loops |

## Quick Reference

### Key Parameters

| Parameter | Value |
|-----------|-------|
| **Block time** | 120 seconds |
| **Block reward** | 50 RNG (halves every 2.1M blocks, floor 0.6 RNG) |
| **Supply model** | Halving schedule plus perpetual 0.6 RNG tail emission |
| **PoW algorithm** | RandomX |
| **Dataset memory** | 2080 MiB (mining) / 256 MiB (validation) |
| **Seed rotation** | Every 2048 blocks + 64 block lag |
| **Difficulty retarget** | Every block (LWMA-style windowing) |
| **Max block weight** | 4,000,000 WU |
| **Coinbase maturity** | 100 blocks |
| **Minimum fee** | 1 roshi/vbyte |

### Address Prefixes (Mainnet)

| Type | Prefix |
|------|--------|
| P2PKH | B |
| P2SH | A |
| Bech32 | rng1 |
| WIF | 5/K/L |
| xpub | bpub |
| xprv | bprv |

### Network Ports (Mainnet)

| Service | Port |
|---------|------|
| P2P | 8433 |
| RPC | 8432 |

### Active from Genesis

- SegWit (BIP141/143/147)
- Taproot (BIP340/341/342)
- CSV (BIP68/112/113)
- CLTV (BIP65)
- Strict DER (BIP66)
- Height in coinbase (BIP34)

## Implementation Checklist

### Phase 1: Core Changes
- [ ] Fork Bitcoin Core repository
- [ ] Modify `chainparams.cpp` with all RNG parameters
- [ ] Integrate RandomX library
- [ ] Replace SHA256d with RandomX in `pow.cpp`
- [ ] Update address prefixes in `base58.cpp`
- [ ] Update bech32 HRP
- [ ] Set all soft fork heights to 0
- [ ] Create genesis block

### Phase 2: Build & Test
- [ ] Compile successfully
- [ ] Regtest: mine blocks with RandomX
- [ ] Regtest: send transactions
- [ ] Regtest: verify all address types
- [ ] Run full test suite
- [ ] Testnet: multi-node sync test

### Phase 3: Launch Preparation
- [ ] Generate mainnet genesis block
- [ ] Set up DNS seeds
- [ ] Prepare release binaries
- [ ] Write user documentation
- [ ] Set up block explorer
- [ ] Create faucet (testnet)

## Files to Modify in Bitcoin Core

```
src/
├── chainparams.cpp          # ALL chain parameters
├── chainparams.h            # Parameter declarations
├── consensus/params.h       # Consensus parameter struct
├── pow.cpp                  # RandomX instead of SHA256
├── pow.h                    # PoW function declarations
├── validation.cpp           # Block validation
├── miner.cpp                # Mining with RandomX
├── rpc/mining.cpp           # Mining RPCs
├── base58.cpp               # Address prefixes
├── bech32.cpp               # Bech32 HRP
├── policy/policy.h          # Fee policy
├── crypto/                  # Add RandomX
└── Makefile.am              # Build system
```

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| RandomX | master | PoW algorithm |
| Bitcoin Core | 27.x | Base codebase |
| Boost | 1.81+ | Core dependency |
| libevent | 2.1+ | Networking |
| OpenSSL | 3.0+ | Cryptography |

## Launch Readiness Checklist

- [ ] All specs complete and reviewed
- [ ] Code compiles on Linux/macOS/Windows
- [ ] All tests pass
- [ ] Testnet running with multiple nodes
- [ ] Genesis block mined
- [ ] DNS seeds operational
- [ ] Documentation complete
- [ ] Security review complete
- [ ] Binaries signed and released

---

*Last updated: 2026-03-19*
| **P2P Atomic Swaps** | [swaps.md](swaps.md) | ✅ Complete | BTC↔BTC trustless exchange |
