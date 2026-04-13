# Specification: Sharepool Protocol -- Protocol-Native Trustless Pooled Mining

## Objective

Replace the legacy "block finder receives the entire block reward" contract with
a deterministic, protocol-native pooled mining system. After BIP9 activation,
every block commits to a reward split derived from publicly relayed shares,
miners claim committed amounts after coinbase maturity, and no trusted pool
operator exists at any point in the pipeline. Solo mining is the degenerate case
where one miner fills the entire reward window.

## Evidence Status

### Verified Facts

**Chain parameters (live)**:
- RNG is a Bitcoin Core v30.2 fork with RandomX PoW, chain live at ~32,122 blocks as of 2026-04-13
- Mainnet block spacing: 120 seconds
- Subsidy: 50 RNG, halves every 2,100,000 blocks; 1 RNG = 100,000,000 roshi
- Coinbase maturity: 100 blocks (unchanged from Bitcoin Core)

**BIP9 deployment (`src/consensus/params.h` line 36, `src/kernel/chainparams.cpp` lines 133-139)**:
- `DEPLOYMENT_SHAREPOOL` defined as `DeploymentPos` enum value
- Mainnet: bit 3, period 2016, threshold 1916 (95%), `NEVER_ACTIVE`
- Testnet/testnet4/signet: bit 3, period 2016, threshold 1916 (95%), `NEVER_ACTIVE`
- Regtest: bit 3, period 144, threshold 108 (75%), `NEVER_ACTIVE` (activatable via `-vbparams=sharepool:0:9999999999:0`)

**Confirmed constants (POOL-03R decision, `specs/sharepool.md` lines 88-97)**:
- Target share spacing: 1 second
- Share target ratio: `share_target = min(powLimit, block_target * (block_spacing / share_spacing))` -- mainnet ratio 120
- Reward window work: 7200 target-spacing shares at 1-second spacing
- Max orphan shares: 64
- Claim witness version: 2
- Commitment tag: `RNGS` (confirmed optional)
- BIP9 period: 2016, threshold: 1916/2016 (95%)

**ShareRecord struct (`src/node/sharechain.h` lines 36-55)**:
- `version` (uint32_t, initially `SHARE_RECORD_VERSION = 1`)
- `parent_share` (uint256)
- `prev_block_hash` (uint256)
- `candidate_header` (CBlockHeader, 80 bytes serialized)
- `share_nBits` (uint32_t)
- `payout_script` (CScript)
- Derived: `candidate_header_hash = Hash(candidate_header)`, `share_id = Hash(serialized ShareRecord)`, `share_work = GetBlockProof(share_nBits)`
- Serialization order matches field declaration order (`READWRITE` macro, line 44)

**Sharechain storage (`src/node/sharechain.h` lines 75-128, `src/node/sharechain.cpp`)**:
- `SharechainStore` with LevelDB-backed persistence
- `AddShare()` validates and stores, returning `ShareStoreResult` with status enum: `ACCEPTED`, `ALREADY_PRESENT`, `ORPHAN`, `INVALID`
- Best-tip selection by highest cumulative `share_work`, with lower `share_id` as tiebreaker
- Orphan buffer: 64-entry cap (`MAX_SHARE_ORPHANS`), FIFO eviction by `m_orphan_order` deque
- Orphan resolution cascades via `ResolveOrphans()` when missing parent arrives
- Batch limits: `MAX_SHARE_INV_SZ = 1000`, `MAX_SHARE_BATCH_SZ = 16`

**P2P relay (`src/protocol.h` lines 268-320, `src/net_processing.cpp` lines 3967-3995)**:
- Message types: `shareinv`, `getshare`, `share`
- Relay gated on `DeploymentActiveAt(*tip, ..., DEPLOYMENT_SHAREPOOL)`
- `shareinv` and `getshare` payload bounded to `MAX_SHARE_INV_SZ` (1000)
- Misbehavior scoring on oversized payloads

**Settlement consensus helpers (`src/consensus/sharepool.h` lines 18-72, `src/consensus/sharepool.cpp`)**:
- `SettlementLeaf`: `payout_script`, `amount_roshi` (int64), `first_share_id`, `last_share_id`
- `SettlementDescriptor`: `version` (CompactSize, initially 1), `payout_root` (uint256), `leaf_count` (CompactSize uint32)
- Tagged hash functions with string prefixes:
  - `LEAF_TAG = "RNGSharepoolLeaf"`
  - `DESCRIPTOR_TAG = "RNGSharepoolDescriptor"`
  - `CLAIM_FLAG_TAG = "RNGSharepoolClaimFlag"`
  - `STATE_TAG = "RNGSharepoolState"`
  - `SOLO_LEAF_TAG = "RNGSharepoolSoloLeaf"`
- `state_hash = SHA256(SHA256("RNGSharepoolState" || serialized_descriptor || claim_status_root))`
- Settlement script: `OP_2 <32-byte state_hash>` via `BuildSettlementScriptPubKey()`
- Leaf ordering: primary key `Hash(payout_script)`, tiebreak raw `payout_script` bytes (`SettlementLeafLess()`)
- Merkle tree: binary, duplicate-last-hash for odd levels (`ComputeSettlementMerkleRoot()`)
- Claim-status tree: power-of-two sized, padding leaves permanently `claimed=1`
- `ComputeRemainingSettlementValue()` sums unclaimed leaf amounts

**Solo settlement coinbase (`src/node/miner.cpp` lines 185-198)**:
- When `sharepool_active`, coinbase output 0 is a zero-value legacy output
- Coinbase output 1 is the settlement output with `nValue = block_reward` and script `OP_2 <state_hash>`
- Solo case uses `MakeSoloSettlementLeaf()` which derives a synthetic share_id from `SHA256(SHA256("RNGSharepoolSoloLeaf" || prev_block_hash || height || payout_script || amount_roshi))`
- Multi-leaf payout commitment is not yet implemented; only the solo path exists

**Simulator (`contrib/sharepool/simulate.py`)**:
- Offline deterministic model of reward-window, payout-leaf, commitment-root, and withholding metrics
- POOL-02R variance sweep: 1-second candidate max CV 8.06% across seeds 1-20 (passes <10% threshold)
- 2-second candidate fails (max CV 10.33%), 10-second baseline remains rejected (CV 25.10%)
- Withholding advantage: 0.00%, below 5% threshold

**Settlement reference model (`contrib/sharepool/settlement_model.py`)**:
- Python reference for the many-claim accounting state machine
- Produces deterministic test vectors for claim transitions

### Recommendations

The following behaviors are specified in `specs/sharepool.md` and `specs/sharepool-settlement.md` but not yet enforced in C++ consensus code:

1. **Witness-v2 claim verification** (`src/script/interpreter.cpp`): The five-element witness stack (descriptor, leaf_index, leaf_data, payout_branch, status_branch) is specified but no verifier exists. Witness v2 programs currently pass as anyone-can-spend under pre-activation rules.

2. **ConnectBlock enforcement** (`src/validation.cpp`): Post-activation blocks must contain a valid payout commitment matching the active reward window. No validation rule currently rejects blocks with missing or mismatched commitments.

3. **Multi-leaf payout commitment** (`src/node/miner.cpp`): The block assembler only produces solo settlement leaves. Multi-miner reward windows with work-proportional splits are specified but not built.

4. **Canonical reward-window data availability**: The current code stores shares in `SharechainStore` and relays them over P2P, but the block only carries an `OP_2 <state_hash>` settlement output. A consensus rule cannot derive the multi-leaf reward window from each validator's local share relay state unless the block or a consensus-persisted index commits to the same share tip, leaf set, and proof material for every validator. Add this as a required decision gate before multi-leaf enforcement.

5. **Dual-target mining** (`src/node/internal_miner.cpp`): The internal miner produces block-difficulty work only. It does not produce share-difficulty work for the sharepool.

6. **Reward RPCs** (`src/rpc/`): `submitshare`, `getsharechaininfo`, and `getrewardcommitment` are specified but not implemented.

7. **Wallet integration**: No `pooled.pending` or `pooled.claimable` balance tracking. No auto-claim construction.

8. **Remainder distribution**: Dust roshi from `floor()` rounding must be distributed deterministically by ascending `Hash(payout_script)`. Not yet implemented.

### Hypotheses / Unresolved Questions

1. **Relay bandwidth at 1-second share spacing**: POOL-06-GATE measurement is required before payout/claim code proceeds. The initial relay target is <10 KB/s per node. No live multi-node measurement exists yet.

2. **Orphan rate under real network conditions**: The 64-entry orphan buffer is a confirmed constant, but live orphan rates at 1-second share intervals have not been measured.

3. **Script support scope for claim verifier**: Which payout_script forms does the v1 claim verifier accept? SegWit v0 keyhash and Taproot v1 are proposed; broader support depends on POOL-07.

4. **Finder bonus or publication incentive**: Should the block-finding share receive a bonus? Not decided.

5. **Test-network share spacing**: Should 60-second test networks keep 1-second share spacing or scale the ratio?

6. **QSB policy interaction**: How does merged QSB policy interact with witness-v2 claim standardness?

7. **Pre-activation relay on regtest**: Whether share relay should be available before activation for testing purposes.

8. **Canonical reward-window input**: Which data becomes consensus-authoritative for the payout leaf set: a committed share tip plus required share availability, an explicit commitment/descriptor payload in the block, or another mechanism? Until this is decided, multi-leaf reward fairness and `getrewardcommitment` leaf enumeration are recommendations, not verified consensus behavior.

## Protocol Design

### Overview

The sharepool protocol has five layers:

1. **Share production**: Miners produce lower-difficulty RandomX proofs (shares) against candidate block headers.
2. **Sharechain**: Shares form an ordered DAG linked by `parent_share`, stored and relayed peer-to-peer.
3. **Reward window**: A trailing work-weighted window of eligible shares determines the payout split.
4. **Payout commitment**: Each block commits to a Merkle root over sorted reward leaves in a witness-v2 settlement output.
5. **Claim program**: A stateful UTXO covenant allows each leaf to claim its exact committed amount.

### Layer 1: Share Production

A share is a RandomX proof where `RandomXHash(candidate_header) <= share_target` but not necessarily `<= block_target`. When a share also meets the block target, it represents a block-finding event.

**Share target formula**:
```
share_target = min(powLimit, block_target * (block_spacing / share_spacing))
```

At mainnet's 120-second blocks and 1-second target share spacing, the ratio is 120: the share target is 120x easier than the block target. This produces ~120 shares per block interval on expectation.

**ShareRecord fields** (serialized in declaration order per `src/node/sharechain.h`):

| Field | Type | Description |
|-------|------|-------------|
| `version` | uint32_t | Share format version (initially 1) |
| `parent_share` | uint256 | Previous accepted share id, or null for segment start |
| `prev_block_hash` | uint256 | Block tip the miner built on |
| `candidate_header` | CBlockHeader | Serialized 80-byte block header candidate |
| `share_nBits` | uint32_t | Compact target for the share proof |
| `payout_script` | CScript | Miner's payout destination scriptPubKey |

**Validity rules**:
- `candidate_header.hashPrevBlock == prev_block_hash`
- `share_nBits` decodes to a positive target no easier than `powLimit`
- Share target is easier than or equal to the current block target and satisfies the share ratio
- `RandomXHash(candidate_header) <= share_target`
- `payout_script` must be a supported claim destination (v1: SegWit v0 keyhash, Taproot v1)

### Layer 2: Sharechain

**Storage**: LevelDB-backed `SharechainStore` with in-memory index (`src/node/sharechain.h`).

**Tip selection**: Best share tip is the share with highest cumulative `share_work` along its parent chain. Ties break by lower `share_id` bytewise.

**Orphan handling**: Shares whose parent is unknown are buffered (max 64). FIFO eviction when full. Receiving a missing parent triggers cascading orphan resolution via `ResolveOrphans()`.

**Reorg handling**: The share store is not deleted on block reorg. Reward-window reconstruction filters shares by the active block chain. A block reorg changes which shares are eligible for a given block's commitment.

**P2P relay** (gated on `DEPLOYMENT_SHAREPOOL` activation):

| Message | Payload | Limit |
|---------|---------|-------|
| `shareinv` | Vector of share ids | MAX_SHARE_INV_SZ (1000) |
| `getshare` | Vector of share ids | MAX_SHARE_INV_SZ (1000) |
| `share` | Vector of serialized ShareRecords | MAX_SHARE_BATCH_SZ (16) |

Nodes validate share PoW before accepting or forwarding. Invalid shares are rejected with misbehavior scoring.

### Layer 3: Reward Window

For a block at height H, the reward window is the trailing set of eligible shares from the best share tip, walking backward through `parent_share` and accumulating `share_work` until the threshold is reached or the chain is exhausted.

**Window threshold**: Enough cumulative work for 7200 target-spacing shares at 1-second spacing. At mainnet's 120-second blocks, this is ~60 blocks of smoothing (~2 hours).

**Consensus determinism requirement**: Reward-window construction must not depend on a validator's unstated local relay view. Before a post-activation block can be rejected for a "wrong" multi-leaf payout split, the implementation must define the canonical share tip, data availability, and replay rules that let every fully validating node derive the same ordered `SettlementLeaf` set after restart and across reorgs. A local `SharechainStore::BestTip()` query is sufficient for mining policy, but not by itself a consensus input.

**Reward calculation**:
```
total_reward = block_subsidy(height) + transaction_fees

For each payout_script in window:
  script_work = sum of share_work for all shares with that payout_script
  amount_roshi = floor(total_reward * script_work / window_work)

Remainder roshi distributed by ascending Hash(payout_script) until sum == total_reward
```

**Solo mining**: When the window contains only one payout script, that script receives the full reward. The solo path uses `MakeSoloSettlementLeaf()` which derives a synthetic share_id deterministically.

### Layer 4: Payout Commitment

**Reward leaf fields** (`SettlementLeaf` in `src/consensus/sharepool.h`):
- `payout_script`: serialized scriptPubKey
- `amount_roshi`: int64 amount
- `first_share_id`: oldest share id for this script in the window
- `last_share_id`: newest share id for this script in the window

**Leaf hash**: `SHA256(SHA256("RNGSharepoolLeaf" || serialized_leaf))`

**Leaf ordering**: Sort by `Hash(payout_script)` ascending, tiebreak by raw `payout_script` bytes.

**Merkle tree**: Binary, duplicate-last-hash for odd levels (Bitcoin-style). Root is the `payout_root`.

**Coinbase encoding** (post-activation):
- Output 0: zero-value legacy compatibility output to `coinbase_output_script`
- Output 1: settlement output with `nValue = total_reward` and script `OP_2 <state_hash>`
- Existing SegWit witness commitment OP_RETURN remains separate and unchanged
- Optional `OP_RETURN <"RNGS"> <root>` discovery marker (metadata only, not a funding source)

### Layer 5: Claim Program

**Settlement output script**: `OP_2 <32-byte state_hash>`

**State hash construction**:
```
descriptor = { version: 1, payout_root: uint256, leaf_count: uint32 }
state_hash = SHA256(SHA256("RNGSharepoolState" || serialized_descriptor || claim_status_root))
```

**Claim-status tree**: Binary Merkle tree, power-of-two sized. Each real leaf: `SHA256(SHA256("RNGSharepoolClaimFlag" || CompactSize(index) || byte(claimed_flag)))`. Padding leaves are permanently `claimed=1`.

**Initial state**: All real leaves `claimed=0`, all padding leaves `claimed=1`.

**Claim transaction shape**:
- Input 0: current settlement output (must be matured coinbase)
- Output 0: payout to committed `leaf.payout_script` for exact `leaf.amount_roshi`
- Output 1: successor settlement output with updated state_hash (omitted if final claim)
- Additional non-settlement inputs for fees permitted after input 0
- Change outputs permitted after mandatory outputs

**Witness stack** (five elements):
1. `settlement_descriptor`: serialized immutable descriptor
2. `leaf_index`: CScriptNum-encoded non-negative index
3. `leaf_data`: serialized payout leaf
4. `payout_branch`: concatenated 32-byte Merkle siblings proving leaf under `payout_root`
5. `status_branch`: concatenated 32-byte Merkle siblings proving status under `claim_status_root`

No inner signature required in v1 -- the payout destination is consensus-constrained.

**10 consensus invariants**:

| # | Invariant |
|---|-----------|
| 1 | One block creates at most one settlement output |
| 2 | Settlement value = full block reward to sharepool |
| 3 | Descriptor never changes across successors |
| 4 | Claim-status root changes only by flipping one leaf 0 to 1 |
| 5 | Exact committed amount released per claim |
| 6 | Value conservation: `old_value = claimed_amount + new_value` |
| 7 | No double claims (leaf already 1) |
| 8 | If `old_value == claimed_amount`, no successor output |
| 9 | If `old_value > claimed_amount`, successor required |
| 10 | Fees from non-settlement inputs only |

### Activation

- Pre-activation: classical coinbase semantics, witness v2 is unknown witness program
- Post-activation: blocks must contain valid payout commitment; missing/mismatched commitments are invalid
- Mainnet stays `NEVER_ACTIVE` until simulator, regtest, and devnet gates pass
- Regtest activatable with `-vbparams=sharepool:0:9999999999:0`

## Acceptance Criteria

- `ShareRecord` serializes in the field order defined in `src/node/sharechain.h` (version, parent_share, prev_block_hash, candidate_header, share_nBits, payout_script)
- Share validation rejects shares where `RandomXHash(candidate_header) > share_target`
- Share validation rejects shares where `candidate_header.hashPrevBlock != prev_block_hash`
- Share validation rejects shares where `share_nBits` decodes to a target easier than `powLimit`
- `SharechainStore::AddShare()` returns `ORPHAN` when `parent_share` is unknown, buffers at most 64 orphans, and evicts oldest-first
- `SharechainStore::BestTip()` returns the share with highest cumulative work, with lower `share_id` as deterministic tiebreak
- P2P relay is silent when `DEPLOYMENT_SHAREPOOL` is not active at the chain tip
- `shareinv` and `getshare` messages exceeding 1000 ids trigger misbehavior scoring
- Reward window walks backward from share tip accumulating `share_work` and stops at 7200 target-spacing shares of cumulative work
- Multi-leaf reward-window validation has an explicit consensus data contract: the block or consensus-persisted state identifies the share tip, leaf set, and proof material needed for all validators to reproduce the same reward window
- Reward leaves are sorted by `Hash(payout_script)` ascending with raw-bytes tiebreak
- `amount_roshi = floor(total_reward * script_work / window_work)` for each payout script
- Remainder roshi after floor division are distributed by ascending `Hash(payout_script)`
- `leaf_hash = SHA256(SHA256("RNGSharepoolLeaf" || serialized_leaf))` matches `HashSettlementLeaf()` output
- Settlement descriptor version is 1 and serializes as CompactSize
- `state_hash` matches `ComputeSettlementStateHash()` for given descriptor and claim_status_root
- Settlement script is exactly `OP_2 <32-byte state_hash>` via `BuildSettlementScriptPubKey()`
- Claim-status tree uses `next_power_of_two(leaf_count)` leaves, padding permanently `claimed=1`
- Initial claim_status_root has all real leaves unclaimed and matches `ComputeInitialSettlementClaimStatusRoot()`
- Post-activation coinbase contains exactly one settlement output with `nValue == block_reward`
- Solo mining produces a valid settlement output using `MakeSoloSettlementLeaf()` with synthetic share_id
- Claim transaction with witness stack (descriptor, leaf_index, leaf_data, payout_branch, status_branch) passes verification when all proofs are correct
- Claim transaction fails if leaf is already claimed (status bit is 1)
- Claim transaction fails if payout output does not exactly match committed script and amount
- Successor settlement output has updated state_hash reflecting the single flipped claim bit
- No successor settlement output exists when the final leaf is claimed
- `old_settlement_value == claimed_leaf_amount + new_settlement_value` holds exactly
- Non-settlement inputs cover all fees; settlement value is never used as fee source
- POOL-02R variance metric: 10% miner over 100 blocks at 1-second/7200-work has CV < 10% for seed 42 and seeds 1-20
- Share withholding advantage remains below 5%

## Verification

**Existing code verification**:

```bash
# Confirm BIP9 deployment exists
grep -c "DEPLOYMENT_SHAREPOOL" src/consensus/params.h
# Expected: >= 1

# Confirm sharechain storage and relay
test -f src/node/sharechain.h && test -f src/node/sharechain.cpp
test -f src/net_processing.cpp && grep -q "shareinv" src/net_processing.cpp

# Confirm consensus helpers
test -f src/consensus/sharepool.h && test -f src/consensus/sharepool.cpp

# Confirm tagged hash constants
grep -c "RNGSharepool" src/consensus/sharepool.cpp
# Expected: 5 (Leaf, Descriptor, ClaimFlag, State, SoloLeaf)

# Confirm solo settlement coinbase
grep -q "MakeSoloSettlementLeaf" src/node/miner.cpp

# Confirm settlement script construction
grep -q "BuildSettlementScriptPubKey" src/consensus/sharepool.cpp

# Confirm simulator and settlement model
test -f contrib/sharepool/simulate.py
test -f contrib/sharepool/settlement_model.py

# Run simulator variance check (1-second candidate, seed 42)
python3 contrib/sharepool/simulate.py variance \
  --target-share-spacing 1 \
  --reward-window-work 7200 \
  --block-spacing 120 \
  --seed 42 \
  --miner-fraction 0.10 \
  --blocks 100

# Run C++ consensus unit tests (when built)
# ctest --test-dir build -R sharepool
```

**Not-yet-built verification** (must pass before mainnet activation):

- Witness-v2 claim verifier unit tests covering: initial claim, intermediate claim, final claim, double-claim rejection, value conservation with extra inputs
- ConnectBlock rejection of post-activation blocks with missing or mismatched payout commitment
- Multi-node regtest share relay bandwidth measurement (POOL-06-GATE, target <10 KB/s per node)
- Multi-leaf payout commitment producing byte-identical roots across independent nodes
- Wallet auto-claim construction and broadcast
- Dual-target mining producing shares at expected rate

## Open Questions

- Should the first version include a finder bonus or publication incentive for the block-producing share?
- Should 60-second test networks keep 1-second target share spacing or maintain the same target ratio (60:1) as mainnet?
- Which payout_script forms are supported by the v1 claim verifier beyond SegWit v0 keyhash and Taproot v1?
- Should share relay be available pre-activation on regtest for testing?
- What consensus data-availability mechanism makes multi-leaf reward-window validation deterministic across nodes with different local share relay histories?
- How does merged QSB policy interact with witness-v2 claim standardness?
- Should v1 permit batched multi-leaf claims, or keep one-leaf-per-claim for simpler consensus code?
- Should the optional RNGS OP_RETURN marker be removed once RPC and wallet discovery are mature?
- Should claim-helper incentives be added later, or is wallet self-claim sufficient?
- What is the measured live orphan rate at 1-second share intervals on a multi-node regtest network?
