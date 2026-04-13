# Specification: Sharechain Data Model, Storage, and P2P Relay

## Objective

Define the data model, persistent storage, and peer-to-peer relay protocol for
RNG share records. This layer enables nodes to receive, validate, persist, and
propagate shares so that upstream subsystems (reward windows, payout
commitments, claim verification) can operate on a consistent, replicated
sharechain.

## Evidence Status

### Verified Facts

All claims below are verified against source code and test artifacts.

**Data model** (`src/node/sharechain.h`, lines 31-55):

- `ShareRecord` fields: `version` (uint32_t, default 1), `parent_share`
  (uint256), `prev_block_hash` (uint256), `candidate_header` (CBlockHeader),
  `share_nBits` (uint32_t), `payout_script` (CScript).
- `GetHash()` produces the share ID via `HashWriter` serialization digest.
- Constants: `SHARE_RECORD_VERSION = 1`, `MAX_SHARE_ORPHANS = 64`,
  `MAX_SHARE_INV_SZ = 1000`, `MAX_SHARE_BATCH_SZ = 16`.

**Validation** (`src/node/sharechain.cpp`, lines 61-102):

- `ValidateShare()` checks: version == 1, candidate_header.hashPrevBlock ==
  prev_block_hash, block target derivable, share target derivable, share target
  >= block target (not harder than the block), share target <= share target
  limit (not easier than allowed), RandomX proof-of-work passes against
  share_nBits.
- Share target limit: `block_target * nPowTargetSpacing`, clamped to powLimit.
- Rejection reasons: `bad-share-version`, `share-prevblock-mismatch`,
  `bad-block-target`, `bad-share-target`, `share-target-too-hard`,
  `share-target-too-easy`, `share-pow-invalid`.

**Storage** (`src/node/sharechain.cpp`, lines 104-331):

- `SharechainStore` uses LevelDB (`CDBWrapper`) with key prefix `'s'`.
- In-memory maps: `m_shares` (accepted), `m_orphans`, `m_orphan_parent_index`,
  `m_orphan_order` (deque for FIFO eviction).
- Best-tip tracks highest cumulative work; ties broken by lexicographically
  smaller share ID.
- On startup, `LoadFromDisk()` iterates all DB entries and replays
  `AddShareInternal()` without re-validating consensus rules (shares were
  validated on first acceptance).

**P2P relay** (`src/net_processing.cpp`, lines 3965-4042):

- Three message types registered in `src/protocol.h`: `shareinv`, `getshare`,
  `share`.
- All handlers gated by `SharepoolRelayActive()` which checks
  `DeploymentActiveAt(*tip, m_chainman, Consensus::DEPLOYMENT_SHAREPOOL)`.
- `shareinv`: receives vector of share IDs (max 1000), responds with
  `getshare` for unknown IDs.
- `getshare`: receives vector of share IDs (max 1000), responds with `share`
  messages in batches of at most 16.
- `share`: receives vector of ShareRecords (max 16), adds each to store,
  triggers `Misbehaving()` on INVALID, requests missing parent on ORPHAN,
  relays accepted IDs via `RelayShareInv()` to all peers except originator.
- Oversized messages trigger `Misbehaving()` penalty.

**Activation** (`src/kernel/chainparams.cpp`):

- `DEPLOYMENT_SHAREPOOL` uses BIP9 bit 3.
- Mainnet: `NEVER_ACTIVE` (dormant until regtest/devnet gates pass).
- Regtest: `NEVER_ACTIVE` by default, overridable via `-vbparams`.
- Threshold: 95% mainnet (1916/2016), 75% regtest (108/144).

**Relay benchmark** (`contrib/sharepool/reports/pool-06-relay-viability.json`):

- Decision: GO at 10-second share interval.
- 4-node full-mesh, 180 shares over 1800s.
- p50 latency: 58.6ms, p99: 79.2ms, max: 88.8ms.
- Bandwidth per node: 0.063 KB/s.
- Orphan order events: 0, missing receipts: 0, propagation: 100%.

**Test coverage**:

- Unit: `src/test/sharechain_tests.cpp` (store, orphan, best-tip logic).
- Functional: `test/functional/feature_sharepool_relay.py` (P2P relay after
  activation).
- Benchmark: `test/functional/feature_sharepool_relay_benchmark.py` (latency
  and bandwidth measurement).
- Activation: `src/test/versionbits_tests.cpp` (BIP9 deployment lifecycle).

### Recommendations

1. **Expose orphan metrics via RPC.** The benchmark notes that share orphan
   counters are not yet available through RPC. Adding a `getsharechaininfo` RPC
   would let operators and monitoring observe orphan pressure in production.

2. **Measure 1-second cadence.** The relay benchmark only covers 10-second
   intervals. Extrapolation suggests 0.6 KB/s (under the 10 KB/s budget), but
   actual measurement is required before production activation at faster rates.

3. **Do not treat local best tip as consensus by itself.** `SharechainStore`
   is a relay/storage layer. Any later reward-window rule that affects block
   validity must define how all validators derive the same canonical share tip
   and leaf set; otherwise nodes with different relay histories could disagree
   on an otherwise identical block.

4. **Consider share pruning.** Currently all accepted shares are kept
   indefinitely in LevelDB and in memory. As the sharechain grows, a pruning
   policy (e.g., retaining only shares within the active reward window plus a
   safety margin) will be needed.

5. **DB corruption recovery.** `LoadFromDisk()` silently skips malformed
   entries. A diagnostic log or RPC reporting load errors would improve
   operability.

### Hypotheses / Unresolved Questions

- **Memory scaling.** The in-memory `m_shares` map holds all accepted shares.
  At sustained 1-second cadence, this grows by ~86,400 entries/day. The memory
  footprint per entry (ShareEntry with CBlockHeader, arith_uint256 x2, int) is
  estimated at ~300-400 bytes, implying ~25-35 MB/day. Without pruning, this
  becomes a concern over weeks/months.

- **Orphan parent resolution depth.** `ResolveOrphans()` uses a BFS that could
  cascade through the full orphan buffer (max 64). In practice the benchmark
  showed zero orphan events, but adversarial scenarios could trigger deep
  resolution chains.

- **No share expiry.** There is no TTL or height-based expiry on individual
  shares. The orphan buffer uses FIFO eviction, but accepted shares are
  permanent.

- **Consensus data availability.** The current P2P store is not an
  authoritative consensus input. If a future `ConnectBlock` rule validates a
  reward window, the block or consensus-persisted state must identify enough
  share context for deterministic replay.

## Data Model

### ShareRecord

Defined in `src/node/sharechain.h`, serialized via `SERIALIZE_METHODS`:

| Field              | Type          | Description                                      |
|--------------------|---------------|--------------------------------------------------|
| `version`          | `uint32_t`    | Protocol version; must be 1                      |
| `parent_share`     | `uint256`     | Hash of the parent share (null for genesis share)|
| `prev_block_hash`  | `uint256`     | Block hash the share builds on                   |
| `candidate_header` | `CBlockHeader`| Full block header proving PoW                    |
| `share_nBits`      | `uint32_t`    | Compact target for share difficulty               |
| `payout_script`    | `CScript`     | Miner's payout destination                       |

**Identity:** `share_id = SHA256d(serialized ShareRecord)` via `GetHash()`.

**Invariant:** `candidate_header.hashPrevBlock == prev_block_hash`.

### ShareEntry (internal)

Computed on acceptance and stored in memory alongside the record:

| Field              | Type            | Description                                |
|--------------------|-----------------|--------------------------------------------|
| `record`           | `ShareRecord`   | The original share                         |
| `share_work`       | `arith_uint256` | Work for this share alone (`GetBlockProof`)|
| `cumulative_work`  | `arith_uint256` | Sum of all work on chain to this share     |
| `height`           | `int`           | Distance from genesis share (0-based)      |

### ShareStoreResult

Returned by `AddShare()`:

| Field              | Type                      | Description                          |
|--------------------|---------------------------|--------------------------------------|
| `status`           | `ShareStoreStatus`        | ACCEPTED, ALREADY_PRESENT, ORPHAN, INVALID |
| `share_id`         | `uint256`                 | Hash of the submitted share          |
| `accepted_ids`     | `vector<uint256>`         | All shares accepted (including resolved orphans) |
| `missing_parent`   | `optional<uint256>`       | Set when status is ORPHAN            |
| `reject_reason`    | `string`                  | Set when status is INVALID           |

### Validation Rules

`ValidateShare()` applies these checks in order:

1. `version == SHARE_RECORD_VERSION` (currently 1)
2. `candidate_header.hashPrevBlock == prev_block_hash`
3. Block target derivable from `candidate_header.nBits`
4. Share target derivable from `share_nBits`
5. Share target >= block target (share cannot be harder than the block)
6. Share target <= `block_target * nPowTargetSpacing` (clamped to powLimit)
7. RandomX proof-of-work: `CheckProofOfWork(GetBlockPoWHash(header, seed), share_nBits, consensus)`

## Storage

### LevelDB Backend

- **Key format:** `('s', share_id)` where `share_id` is the uint256 hash.
- **Value format:** Serialized `ShareRecord`.
- Same `CDBWrapper` infrastructure used by Bitcoin Core's block index and UTXO
  set.

### In-Memory State

All state is mutex-protected (`Mutex m_mutex`):

| Structure               | Type                                  | Purpose                            |
|--------------------------|---------------------------------------|------------------------------------|
| `m_shares`              | `map<uint256, ShareEntry>`            | All accepted shares                |
| `m_orphans`             | `map<uint256, OrphanEntry>`           | Shares awaiting parents            |
| `m_orphan_parent_index` | `map<uint256, set<uint256>>`          | parent_id -> set of orphan IDs     |
| `m_orphan_order`        | `deque<uint256>`                      | FIFO insertion order for eviction  |
| `m_best_tip`            | `uint256`                             | Share ID with highest cumulative work |

### Orphan Handling

- Maximum 64 orphan shares (`MAX_SHARE_ORPHANS`).
- When the buffer exceeds capacity, the oldest orphan (front of
  `m_orphan_order`) is evicted along with its parent-index entry.
- When a parent share is accepted, `ResolveOrphans()` performs BFS: all
  orphans waiting on that parent are accepted, which may in turn resolve
  further orphans.

### Best-Tip Selection

- Highest `cumulative_work` wins.
- On tie: lexicographically smaller `share_id` wins (deterministic across all
  nodes).
- Updated on every `AcceptShare()` call.

### Startup Recovery

`LoadFromDisk()` iterates all LevelDB entries with prefix `'s'`, deserializes
each `ShareRecord`, and replays `AddShareInternal()` with `consensus=nullptr`
(skipping re-validation) and `write_to_disk=false` (already persisted). Shares
whose parents have not yet been loaded land in the orphan buffer and resolve as
their parents are iterated. LevelDB iteration order is lexicographic by key, so
parent-before-child ordering is not guaranteed; the orphan resolution mechanism
handles this.

## P2P Relay Protocol

### Message Types

All defined in `src/protocol.h` and handled in `src/net_processing.cpp`.

#### `shareinv`

- **Direction:** any peer -> any peer
- **Payload:** `vector<uint256>` of share IDs
- **Limit:** max 1000 IDs (`MAX_SHARE_INV_SZ`)
- **Behavior:** Receiver filters to unknown share IDs, sends `getshare` for
  those it wants.
- **Penalty:** `Misbehaving()` if count > 1000.

#### `getshare`

- **Direction:** any peer -> any peer
- **Payload:** `vector<uint256>` of share IDs
- **Limit:** max 1000 IDs per request
- **Behavior:** Sender looks up each ID in `SharechainStore`, responds with
  `share` messages in batches of at most 16 (`MAX_SHARE_BATCH_SZ`). Unknown
  IDs are silently skipped.
- **Penalty:** `Misbehaving()` if count > 1000.

#### `share`

- **Direction:** any peer -> any peer
- **Payload:** `vector<ShareRecord>`
- **Limit:** max 16 records per message (`MAX_SHARE_BATCH_SZ`)
- **Behavior:** Each record is passed to `AddShare()`. On INVALID:
  `Misbehaving()` and processing stops. On ORPHAN: `getshare` is sent for
  `missing_parent`. On ACCEPTED: share IDs (including resolved orphans) are
  collected for relay.
- **Relay:** `RelayShareInv()` broadcasts accepted IDs to all peers except the
  originating node.
- **Penalty:** `Misbehaving()` if count > 16; `Misbehaving()` if any share is
  INVALID.

### Activation Gate

All three message handlers early-return (silently drop the message) if
`SharepoolRelayActive()` returns false. This function checks:

```
DeploymentActiveAt(*tip, m_chainman, Consensus::DEPLOYMENT_SHAREPOOL)
```

This means share relay is only active after BIP9 activation of the sharepool
deployment (bit 3). Size-limit checks and `Misbehaving()` penalties fire
regardless of activation state -- oversized messages are always penalized.

### Relay Flow

```
Miner produces share
    |
    v
Node A: AddShare() -> ACCEPTED
    |
    v
Node A: RelayShareInv(A, [share_id]) -> sends `shareinv` to B, C, D
    |
    v
Node B: receives `shareinv`, filters unknown -> sends `getshare` to A
    |
    v
Node A: receives `getshare` -> sends `share` to B
    |
    v
Node B: receives `share`, AddShare() -> ACCEPTED
    |
    v
Node B: RelayShareInv(B, [share_id]) -> sends `shareinv` to C, D
    (A excluded as originator)
```

Orphan case: if Node B receives a share whose parent is unknown, `AddShare()`
returns ORPHAN with `missing_parent` set, and Node B sends `getshare` for the
missing parent back to the sending peer.

## Acceptance Criteria

- `ShareRecord` serialization round-trips correctly (serialize -> deserialize
   -> GetHash() matches).
- `ValidateShare()` rejects shares with each of the seven documented rejection
   reasons.
- `SharechainStore::AddShare()` returns ACCEPTED for valid shares, ORPHAN for
   shares with missing parents, ALREADY_PRESENT for duplicates, INVALID for
   invalid shares.
- Orphan buffer respects MAX_SHARE_ORPHANS=64 with FIFO eviction.
- Orphans resolve when their parent arrives, cascading through chains.
- Best-tip updates correctly by cumulative work with deterministic tie-break.
- Shares persist across daemon restart (LevelDB write + LoadFromDisk).
- P2P messages rejected before activation (SharepoolRelayActive check).
- P2P messages processed correctly after activation.
- Oversized messages trigger Misbehaving() regardless of activation state.
- Invalid shares from peers trigger Misbehaving().
- Accepted shares relay to all peers except originator.
- Any reward-window API added to `SharechainStore` documents whether it is
  mining-policy-only or consensus-authoritative, and consensus use is blocked
  until the block identifies enough share context for deterministic replay.

## Verification

### Unit Tests

```bash
build/bin/test_bitcoin --run_test=sharechain_tests
```

Source: `src/test/sharechain_tests.cpp`. Covers store operations, orphan
handling, best-tip selection, and edge cases.

### Functional Tests

```bash
python3 test/functional/feature_sharepool_relay.py \
    --configfile=build/test/config.ini
```

Tests P2P share relay between activated regtest nodes. Verifies shareinv,
getshare, share message flow. Network magic: `0xB07C0000` (regtest).

### Benchmark

```bash
python3 test/functional/feature_sharepool_relay_benchmark.py \
    --configfile=build/test/config.ini
```

Measures relay latency, bandwidth, and orphan rates. Report:
`contrib/sharepool/reports/pool-06-relay-viability.json`.

### Activation Tests

```bash
build/bin/test_bitcoin --run_test=versionbits_tests
```

Source: `src/test/versionbits_tests.cpp`. Verifies BIP9 lifecycle for
DEPLOYMENT_SHAREPOOL including bit assignment, threshold, and activation.

## Open Questions

1. **Share pruning policy.** When and how should old shares be removed from
   LevelDB and the in-memory map? The reward-window subsystem (not yet built)
   will define which shares are needed for payout computation. Pruning should
   retain at least those shares.

2. **Consensus data availability.** If the reward window is enforced in
   `ConnectBlock`, what share tip, share records, and proof data are part of
   the block-validity contract? The current P2P store alone is not enough to
   make local best-tip selection a consensus input.

3. **1-second cadence measurement.** The relay benchmark covers 10-second
   intervals. Plan 006 extrapolates to ~0.6 KB/s at 1-second cadence (under
   the 10 KB/s budget), but actual measurement is a prerequisite for production
   activation at that rate.

4. **RPC exposure.** There is no `submitshare` or `getsharechaininfo` RPC yet.
   The benchmark injects shares over P2P. Operator-facing RPCs are needed for
   monitoring and debugging in devnet/mainnet.

5. **Duplicate getshare suppression.** If multiple peers announce the same
   share via `shareinv` simultaneously, the node sends `getshare` to each of
   them. A deduplication layer (similar to block download tracking) could reduce
   redundant requests.

6. **Share eviction under memory pressure.** The in-memory map grows
   indefinitely. If the node runs low on memory, there is no mechanism to evict
   accepted shares (only orphans have a cap). This needs to be addressed before
   sustained production use.
