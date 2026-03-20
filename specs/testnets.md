# Test Networks Specification

## Topic
The parameters for RNG's test and development networks.

Mainnet currently targets 120-second blocks. Testnet and regtest intentionally
use faster 60-second spacing so public testing and local development stay more responsive.

## Behavioral Requirements

### Network Overview

| Network | Purpose | Coins Have Value? |
|---------|---------|-------------------|
| **Mainnet** | Production | Yes |
| **Testnet** | Public testing | No (free from faucet) |
| **Regtest** | Local development | No (instant mining) |

### Testnet Parameters

#### Network Identification

| Parameter | Value |
|-----------|-------|
| Network magic | `0xB07C7E57` |
| P2P port | 18433 |
| RPC port | 18432 |
| Protocol version | 70100 |

#### Address Prefixes

| Type | Prefix | Version Byte |
|------|--------|--------------|
| P2PKH | **t** | 111 (0x6F) |
| P2SH | **s** | 196 (0xC4) |
| Bech32 HRP | **trng** | N/A |
| WIF | **c** | 239 (0xEF) |
| xpub | `tpub` | 0x043587CF |
| xprv | `tprv` | 0x04358394 |

#### Consensus

| Parameter | Value | Notes |
|-----------|-------|-------|
| Genesis hash | TBD | Different from mainnet |
| Genesis time | Pre-launch | Earlier than mainnet |
| Initial difficulty | Very low | ~1 block/minute on laptop |
| Min difficulty | Yes | Allows fast testing |
| Retarget interval | 2016 blocks | Same as mainnet |
| Block reward | 50 TBTC | Test coins |

#### Special Rules

- **Minimum difficulty blocks**: After 120 seconds without a block, next block may use minimum difficulty
- **Reset on long gaps**: Allows testing to continue even with low hashrate
- **No value**: Testnet coins explicitly have no monetary value

#### DNS Seeds

```
testnet-seed1.rng.network
testnet-seed2.rng.network
```

### Regtest Parameters

Regression test mode for local development and automated testing.

#### Network Identification

| Parameter | Value |
|-----------|-------|
| Network magic | `0xB07C0000` |
| P2P port | 18544 |
| RPC port | 18543 |
| Protocol version | 70100 |

#### Address Prefixes

Same as testnet (reuse for simplicity):

| Type | Prefix | Version Byte |
|------|--------|--------------|
| P2PKH | **t** | 111 (0x6F) |
| P2SH | **s** | 196 (0xC4) |
| Bech32 HRP | **trng** | N/A |

#### Special Rules

- **Instant mining**: `generatetoaddress` RPC mines blocks instantly
- **No minimum difficulty delay**: Always accepts minimum difficulty
- **No peer discovery**: Connect manually or via `-connect`
- **Independent chains**: Each regtest instance is isolated

#### Typical Usage

```bash
# Start regtest node
rngd -regtest -daemon

# Generate 101 blocks (mature coinbase)
rng-cli -regtest generatetoaddress 101 <address>

# Run tests
./test_framework.py --regtest
```

### Signet (Optional Future)

Signed testnet for more controlled testing:

| Parameter | Value |
|-----------|-------|
| Network magic | `0xB07C516E` |
| P2P port | 38433 |
| Challenge script | Multisig of trusted signers |

**Not required for MVP** - implement if community requests.

### Chainparams Summary

```cpp
// Mainnet
class CMainParams : public CChainParams {
    pchMessageStart[0] = 0xb0; // B
    pchMessageStart[1] = 0x7c; // O
    pchMessageStart[2] = 0x01; // T
    pchMessageStart[3] = 0x0e; // checksum
    nDefaultPort = 8433;
    // ...
};

// Testnet
class CTestNetParams : public CChainParams {
    pchMessageStart[0] = 0xb0;
    pchMessageStart[1] = 0x7c;
    pchMessageStart[2] = 0x7e; // TE
    pchMessageStart[3] = 0x57; // ST
    nDefaultPort = 18433;
    fAllowMinDifficultyBlocks = true;
    // ...
};

// Regtest
class CRegTestParams : public CChainParams {
    pchMessageStart[0] = 0xb0;
    pchMessageStart[1] = 0x7c;
    pchMessageStart[2] = 0x00;
    pchMessageStart[3] = 0x00;
    nDefaultPort = 18544;
    fAllowMinDifficultyBlocks = true;
    fMineBlocksOnDemand = true;
    // ...
};
```

## Acceptance Criteria

1. [ ] Testnet uses different network magic than mainnet
2. [ ] Testnet uses different ports than mainnet
3. [ ] Testnet addresses start with 't' (P2PKH)
4. [ ] Testnet bech32 uses 'trng' HRP
5. [ ] Testnet allows minimum difficulty blocks
6. [ ] Regtest can mine blocks instantly via RPC
7. [ ] Regtest doesn't try peer discovery
8. [ ] Mainnet rejects testnet transactions
9. [ ] Testnet rejects mainnet transactions
10. [ ] All three networks have different genesis blocks

## Test Scenarios

- Start testnet node, verify correct ports
- Generate testnet address, verify 't' prefix
- Mine testnet block after 2 minute gap, verify min difficulty
- Start regtest, generate 100 blocks in <1 second
- Attempt to broadcast testnet tx on mainnet, verify rejection
- Connect two testnet nodes, verify sync
- Run full test suite on regtest
