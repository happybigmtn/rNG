# Specification: Wallet and RPC Surface

## Objective

Define the wallet capabilities and RPC interface exposed by `rngd` and `rng-cli`. Covers the Bitcoin-derived wallet, RNG-specific mining RPCs, transaction handling, and the planned extensions for pooled mining integration.

## Evidence Status

### Verified Facts (grounded in source code and documentation)

**Wallet implementation**:
- Bitcoin Core v29.0-derived wallet in `src/wallet/`
- SQLite-backed descriptor wallet (default for new wallets)
- Legacy (BDB) wallet support inherited from upstream
- SegWit and Taproot address types supported from genesis
- Default address type: Bech32 (`rng1...` on mainnet, `trng1...` on testnet)
- Wallet RPC: full Bitcoin Core wallet RPC surface (`getnewaddress`, `sendtoaddress`, `getbalance`, `listtransactions`, `getwalletinfo`, etc.)
- Wallet name configurable (default: empty, scripts use `miner`)

**RPC interface** (`src/rpc/`):
- All standard Bitcoin Core RPCs present (blockchain, mempool, mining, net, rawtransactions, wallet, util)
- RPC bind: `127.0.0.1` by default
- RPC port: `8432` (mainnet), `18432` (testnet)
- Authentication: `rpcuser`/`rpcpassword` in `rng.conf`

**RNG-specific RPCs**:
- `getinternalmininginfo`: Returns internal miner statistics (hashrate, blocks found, hash count, running status, template count, thread count)
  - Source: `src/rpc/mining.cpp`
  - Not present in upstream Bitcoin Core

**Standard mining RPCs** (inherited from Bitcoin Core):
- `getmininginfo`: Network hashrate, difficulty, chain info
- `getblocktemplate`: Block template for external miners
- `submitblock`: Submit a solved block
- `getnetworkhashps`: Estimated network hashrate
- `generatetoaddress`: Regtest block generation
- `generateblock`: Regtest block generation with specific transactions

**Blockchain RPCs** (key ones):
- `getblockchaininfo`: Chain state, soft fork status, IBD status, verification progress
- `getblock`, `getblockhash`, `getblockheader`: Block data access
- `getbestblockhash`: Current chain tip
- `getblockcount`: Current height

**Network RPCs**:
- `getnetworkinfo`: Protocol version, user agent, connections, local addresses
- `getpeerinfo`: Connected peer details
- `getconnectioncount`: Number of connections
- `addnode`: Manually connect to peer

**Transaction RPCs**:
- `sendtoaddress`, `sendmany`: Send transactions
- `gettransaction`: Transaction details
- `getrawtransaction`, `decoderawtransaction`: Raw transaction access
- `createrawtransaction`, `signrawtransactionwithwallet`: Manual transaction construction
- `getmempoolinfo`: Mempool state

**Configuration flags** (RNG-specific, in addition to Bitcoin Core flags):
- `-mine`: Enable internal miner
- `-mineaddress=<addr>`: Mining payout address
- `-minethreads=<n>`: Mining thread count
- `-minerandomx=fast|light`: RandomX mode
- `-minepriority=low|normal`: Mining CPU priority

**CLI tool**:
- Binary: `rng-cli`
- Usage: `rng-cli [options] <command> [params]`
- Wallet-specific: `rng-cli -rpcwallet=<name> <command>`
- Supports named parameters: `rng-cli -named sendtoaddress address="rng1..." amount=1.0`

### Recommendations (from planning corpus — plan 008)

**Planned RPC extensions for pooled mining**:
- `submitshare <hex>`: Accept hex-encoded share, validate PoW, store in sharechain, relay to peers
- `getsharechaininfo`: Return best tip, height, orphan count, reward window size
- `getrewardcommitment <blockhash>`: Return commitment root, leaves, and amounts for a specific block

**Planned `getmininginfo` extensions**:
- `sharepool_active`: Boolean indicating whether sharepool deployment is active
- `share_tip`: Best share tip hash
- `pending_pooled_reward`: Sum of immature pooled reward entries
- `accepted_shares`: Total accepted share count

**Planned `getbalances` extensions**:
- New `pooled` object: `{ "pending": <amount>, "claimable": <amount> }`
- `pending`: Sum of immature pooled reward entries (not yet spendable)
- `claimable`: Sum of mature pooled reward entries (ready for claim transaction)

**Planned wallet behavior**:
- Auto-scan coinbase for witness v2 payout commitment outputs
- Record `PooledRewardEntry` for matching payout scripts
- Auto-build and broadcast claim transactions on maturity
- Distinguish pending vs. claimable in all balance-reporting RPCs

### Hypotheses / Unresolved Questions

- Whether `getinternalmininginfo` will be merged into `getmininginfo` or kept separate
- Whether the wallet's auto-claim behavior should be opt-in or opt-out
- Whether `submitshare` should support batch submission for efficiency

## Acceptance Criteria

**Current system (verified)**:
- `rng-cli getblockchaininfo` returns correct chain name, block count, soft fork status
- `rng-cli getinternalmininginfo` returns hashrate, blocks found, running status when mining is active
- `rng-cli -rpcwallet=miner getnewaddress` returns a valid `rng1...` Bech32 address
- `rng-cli -rpcwallet=miner getbalance` returns correct confirmed balance
- `rng-cli getnetworkinfo` reports protocol version `70100` and user agent `/RNG:3.0.0/`
- `rng-cli getpeerinfo` shows connections on port `8433`
- `rng-cli validateaddress rng1...` validates Bech32 addresses with HRP `rng`
- RPC is accessible only on `127.0.0.1` by default

**Planned system (testable when code lands)**:
- `submitshare` validates PoW against share target and rejects invalid shares
- `getsharechaininfo` returns current sharechain state including tip hash and window size
- `getrewardcommitment` returns deterministic commitment data for any block with an active sharepool
- `getbalances` includes `pooled.pending` and `pooled.claimable` when sharepool is active
- `getmininginfo` includes sharepool fields when the deployment is active
- Wallet auto-builds claim transactions when pooled reward entries reach maturity

## Verification

```bash
# Verify RNG-specific RPC
rng-cli getinternalmininginfo
# Expected: JSON with hashrate, blocks_found, running, threads, etc.

# Verify wallet address generation
rng-cli createwallet test_wallet
rng-cli -rpcwallet=test_wallet getnewaddress "" bech32
# Expected: rng1...

# Verify balance reporting
rng-cli -rpcwallet=test_wallet getbalances
# Expected: JSON with mine.trusted, mine.untrusted_pending, mine.immature

# Verify blockchain info
rng-cli getblockchaininfo | jq '{chain, blocks, headers, bestblockhash}'
# Expected: chain "main", blocks and headers matching sync state

# Verify network info
rng-cli getnetworkinfo | jq '{version, subversion, protocolversion}'
# Expected: subversion "/RNG:3.0.0/", protocolversion 70100

# Verify mining RPC
rng-cli getmininginfo | jq '{blocks, difficulty, networkhashps}'

# Verify RPC authentication
rng-cli -rpcuser=agent -rpcpassword=<password> getblockcount
# Should succeed

# Verify RPC is localhost-only
curl http://0.0.0.0:8432 2>&1
# Should fail (not bound to all interfaces)
```

## Open Questions

1. Should `getinternalmininginfo` be documented in the public RPC help, or is it considered an internal/debug endpoint?
2. What is the error behavior when calling sharepool RPCs (`submitshare`, `getsharechaininfo`) before the sharepool deployment is active? Should they return an error or empty/inactive state?
3. Should the wallet support multiple payout addresses for pooled mining, or is single-address sufficient?
4. Is there a plan to add WebSocket or SSE subscription for mining events (new block, new share, claim ready)?
5. Should `getblocktemplate` be extended for external miners to participate in pooled mining, or is pooled mining internal-miner-only?
