# Specification: Network Identity and P2P Protocol

## Objective

Define RNG's network-layer identity: magic bytes, ports, address encoding, protocol version, peer discovery, and user agent. This spec ensures nodes correctly identify RNG peers and reject cross-chain connections.

## Evidence Status

### Verified Facts (grounded in source code)

- **Network magic bytes** (from `src/kernel/chainparams.cpp`):
  - Mainnet: `0xB07C010E`
  - Testnet: `0xB07C7E57`
  - Testnet4: `0xB07C7434`
  - Regtest: `0xB07C0000`
- **Default ports**:
  - Mainnet P2P: `8433`
  - Mainnet RPC: `8432`
  - Testnet P2P: `18433`
  - Testnet RPC: `18432`
  - Testnet4 P2P: `48433`
  - Regtest P2P: `18544`
  - Regtest RPC: `18543`
- **Protocol version**: `70016` (`PROTOCOL_VERSION` and `WTXID_RELAY_VERSION` in `src/node/protocol_version.h`)
- **User agent**: `/RNG:3.0.0/` (from `CMakeLists.txt` version 3.0.0)
- **Address prefixes (mainnet)**:
  - P2PKH: byte `25` → base58 prefix `B`
  - P2SH: byte `5` → base58 prefix `A`
  - Bech32 HRP: `rng` (addresses: `rng1...`)
  - WIF: byte `128` → base58 prefix `5/K/L`
  - Extended public key: `bpub`
  - Extended private key: `bprv`
- **Address prefixes (testnet)**:
  - P2PKH: byte `111` → base58 prefix `t`
  - P2SH: byte `196` → base58 prefix `s`
  - Bech32 HRP: `trng` (addresses: `trng1...`)
  - WIF: byte `239` → base58 prefix `c`
  - Extended public key: `tpub`
  - Extended private key: `tprv`
- **Mainnet seed peers** (from `src/kernel/chainparams.cpp`):
  - `95.111.239.142:8433`
  - `161.97.114.192:8433`
  - `185.218.126.23:8433`
  - `185.239.209.227:8433`
- **Historical DNS seed names**:
  - `seed1.rng.network`, `seed2.rng.network`, and `seed3.rng.network` are not present in mainnet `chainparams` in the current checkout
  - `dig +short seed1.rng.network seed2.rng.network seed3.rng.network` returned no A records on 2026-04-13
- **P2P message format**: Bitcoin-compatible (same serialization, same message types)
- **Data directory**: `~/.rng` (default, changed from Bitcoin's `~/.bitcoin`)
- **Config file**: `rng.conf` (in data directory)
- **Default RPC credentials** (from install scripts): user `agent`, password randomly generated (32-char hex)
- **RPC bind**: `127.0.0.1` by default (localhost only)

### Recommendations (intended system)

- Plan 005 proposes adding three new P2P message types for share relay: `shareinv`, `getshare`, `share` — these do not exist in the current codebase
- DNS seed domains should resolve to operational nodes before being reintroduced to mainnet `chainparams`; until then, mainnet discovery should continue to rely on the operator-run IPv4 seeds already in source

### Hypotheses / Unresolved Questions

- Whether DNS seed domains should be provisioned later as the network grows beyond the current operator-run IPv4 seed set
- Whether protocol version `70016` will need bumping when/if sharepool P2P messages are added

## Acceptance Criteria

- Nodes on mainnet use magic bytes `0xB07C010E` for all P2P messages
- Nodes on mainnet listen on port `8433` by default (P2P) and `8432` (RPC)
- Bech32 addresses on mainnet use HRP `rng` (e.g., `rng1qw508d6qejxtdg4y5r3zarvary0c5xw7k...`)
- Bech32 addresses on testnet use HRP `trng`
- P2PKH addresses on mainnet start with `B`
- The user agent string includes `/RNG:` followed by the version
- RPC is bound to `127.0.0.1` by default — not exposed to public network
- Nodes reject connections with mismatched magic bytes
- Data directory defaults to `~/.rng`, not `~/.bitcoin`
- Config file is named `rng.conf`, not `bitcoin.conf`

## Verification

```bash
# Verify network identity from a running node
rng-cli getnetworkinfo | jq '{version, subversion, protocolversion, localaddresses}'
# Expected: subversion "/RNG:3.0.0/", protocolversion 70016

# Verify mainnet genesis (confirms correct chain)
rng-cli getblockhash 0
# Expected: 83a6a482f85dc88c07387980067e9b61e5d8f61818aae9106b6bbc496d36ace4

# Verify peer connections use correct port
rng-cli getpeerinfo | jq '.[].addr'
# Addresses should show port 8433

# Verify bech32 address generation
rng-cli -rpcwallet=miner getnewaddress "" bech32
# Should return rng1... address

# Verify data directory
ls ~/.rng/
# Should contain blocks/, chainstate/, rng.conf, etc.

# Verify RPC is localhost-only
rng-cli getnetworkinfo | jq '.localaddresses'
# RPC should not be publicly exposed
```

## Open Questions

1. Should DNS seed domains be provisioned and reintroduced once there are enough independently operated peers to seed?
2. Should the hardcoded seed peer list be expanded as the network grows? The current 4 IPs are all operator-run Contabo validators.
3. Is protocol version `70016` documented anywhere as a version policy? E.g., when should it be bumped?
4. Should RNG support Tor/I2P/CJDNS transports like upstream Bitcoin Core v29, or are those deprioritized?
