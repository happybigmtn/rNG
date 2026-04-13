# Specification: Miner Share Production -- Dual-Target Internal Miner

Plan 008 Unit A. Date: 2026-04-13.

## Objective

Extend the existing internal miner so that each RandomX hash is checked against
both the block difficulty target and a lower share difficulty target. When a hash
meets the share target, the miner constructs a `ShareRecord`, stores it in the
local `SharechainStore`, and relays it to the P2P network. When a hash meets the
block target (which always also meets the share target), the miner submits the
block and constructs a share. Before sharepool activation, the miner works
exactly as it does today.

## Evidence Status

### Verified Facts

These facts are confirmed against the current codebase.

**InternalMiner class** (`src/node/internal_miner.h`, ~240 lines header;
`src/node/internal_miner.cpp`, ~770 lines implementation):

- Inherits `CValidationInterface` for event-driven block notifications.
- Constructor: `InternalMiner(ChainstateManager&, interfaces::Mining&, CConnman*)`.
- `Start(num_threads, coinbase_script, fast_mode, low_priority)` / `Stop()`.
- Architecture: 1 coordinator thread (`CoordinatorThread`) + N worker threads
  (`WorkerThread(int thread_id)`), stride-based nonce grinding, lock-free hot
  path via atomic `shared_ptr<MiningContext>`.
- Atomic counters: `m_hash_count`, `m_blocks_found`, `m_stale_blocks`,
  `m_template_count`.
- Public accessors: `GetHashRate()`, `GetThreadCount()`, `GetTemplateCount()`.
- CLI flags: `-mine`, `-mineaddress`, `-minethreads`, `-minerandomx`,
  `-minepriority`.
- Currently block-only: each RandomX hash is checked against `MiningContext::nBits`
  (block difficulty target) only.

**MiningContext struct** (private to `InternalMiner`, `src/node/internal_miner.h`
lines 143-151):

    struct MiningContext {
        CBlock block;
        uint256 seed_hash;
        unsigned int nBits;
        uint64_t job_id;
        int height;
    };

Immutable once published; workers read only.

**ShareRecord struct** (`src/node/sharechain.h` lines 36-55):

    struct ShareRecord {
        uint32_t version{SHARE_RECORD_VERSION};  // currently 1
        uint256 parent_share;
        uint256 prev_block_hash;
        CBlockHeader candidate_header;
        uint32_t share_nBits;
        CScript payout_script;
    };

Has `GetHash()` and full serialization support.

**SharechainStore** (`src/node/sharechain.h` lines 75-128):

- `AddShare(share, consensus)` -> `ShareStoreResult` with status, share_id,
  accepted_ids, missing_parent, reject_reason.
- `BestTip()` -> `uint256` (the current best share tip by cumulative work).
- `Contains(share_id)`, `GetShare(share_id)`, `Height(share_id)`,
  `ShareCount()`, `OrphanCount()`.
- LevelDB-backed with in-memory orphan buffer (max 64).

**P2P relay** (`src/net_processing.cpp`): `shareinv`/`getshare`/`share` message
types are wired. `RelayShareInv()` broadcasts a share inventory to peers.

**Share target formula** (from SPEC.md): `share_target = min(powLimit, block_target * (block_spacing / share_spacing))`.
For the current 1-second share cadence, the multiplier is
`consensus.nPowTargetSpacing`: 120 on mainnet and 60 on 60-second test
networks. This produces approximately one share per second network-wide on
each network.

### Recommendations

1. Add `share_target` and `share_nBits` to `MiningContext` so the coordinator
   computes them once per template refresh instead of each worker recomputing
   per hash.
2. Add `m_shares_found` as an `atomic<uint64_t>` counter to `InternalMiner`,
   with a public `GetSharesFound()` accessor.
3. The coordinator thread needs a reference (or pointer) to `SharechainStore`
   to fetch `BestTip()` when building `MiningContext`, and workers need a way
   to submit shares. Pass a `SharechainStore*` to `InternalMiner` at
   construction time.
4. Share construction and relay are infrequent (~1/second network-wide), so they
   can use the existing mutex-guarded paths without affecting the lock-free hot
   loop.
5. Activation gating should use `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)` at
   template-build time (coordinator), not per-hash (worker). When inactive, the
   coordinator simply does not populate the share target fields, and workers
   skip the share check.

### Hypotheses / Unresolved Questions

1. **Worker-to-sharechain submission path.** Workers currently call `SubmitBlock`
   (mutex-guarded) when they find a block. Should share submission follow the
   same pattern (worker calls a `SubmitShare` method that locks, constructs the
   record, calls `AddShare`, and triggers relay)? Or should workers enqueue
   shares on a lock-free queue for the coordinator to drain? Given the expected
   frequency (~1/second across all threads combined), the mutex path is almost
   certainly fine, but this should be confirmed under load.

2. **Parent share staleness.** The `parent_share` in `MiningContext` reflects
   the best tip at template-build time. Between template refreshes (up to 30
   seconds), new shares from other miners may arrive and change the best tip.
   Shares built against a stale parent are still valid (they form a fork that
   will be resolved by cumulative work), but producing many stale-parent shares
   wastes network relay bandwidth. Should the coordinator update `parent_share`
   more frequently than the full template refresh? This is an optimization
   question, not a correctness one.

3. **Relay integration point.** `RelayShareInv()` is currently a
   `PeerManagerImpl` method in `src/net_processing.cpp`, not a public method on
   `CConnman`. The miner already holds a nullable `CConnman*`, but the
   implementation still needs an explicit relay hook or node-level share
   submission service rather than assuming `CConnman` can call `RelayShareInv`
   directly.

4. **Activation transition logging.** When sharepool activates mid-session (the
   miner is already running), the coordinator's next template refresh should
   detect activation and log the transition. Should this produce a one-time
   banner log similar to the startup banner?

## Current Miner Architecture

The internal miner uses a coordinator-worker architecture:

1. **Coordinator thread** (`CoordinatorThread`):
   - Calls `CreateTemplate()` to build a new `MiningContext` from the current
     chain tip via `interfaces::Mining`.
   - Publishes the context to workers via `m_current_context` (atomic
     `shared_ptr` swap under `m_context_mutex`).
   - Sleeps on `m_new_block_cv`, waking on `UpdatedBlockTip` signals or on a
     30-second template refresh timer (`TEMPLATE_REFRESH_INTERVAL_SECS`).
   - Checks `ShouldMine()` for backoff conditions (IBD, no peers, etc.).

2. **Worker threads** (`WorkerThread(thread_id)`):
   - Each thread initializes its own RandomX VM (fast or light mode).
   - Reads the current `MiningContext` and grinds nonces using stride:
     `nonce = thread_id, thread_id + N, thread_id + 2N, ...`
   - For each hash: compares against `nBits` target. If it passes, calls
     `SubmitBlock()`.
   - Periodically checks `m_job_id` for staleness (every
     `STALENESS_CHECK_INTERVAL` = 1000 hashes).
   - Updates `m_hash_count` atomically.

3. **Block submission** (`SubmitBlock`):
   - Called by any worker that finds a valid block.
   - Thread-safe (mutex-guarded internally by `interfaces::Mining`).
   - Increments `m_blocks_found` or `m_stale_blocks` based on result.

The RandomX hash computation dominates cost at ~0.5-1ms per hash. All other
operations in the hot loop (nonce increment, target comparison, staleness check)
are sub-microsecond.

## Dual-Target Extension

### MiningContext Changes

Add two fields to `MiningContext`:

    uint256 share_target;      // Target for share difficulty (higher than block target = easier)
    unsigned int share_nBits;  // Compact encoding of share target

These are populated by the coordinator in `CreateTemplate()`:

    // After building the block template and extracting block nBits:
    if (sharepool_active) {
        arith_uint256 block_target;
        bool negative, overflow;
        block_target.SetCompact(context->nBits, &negative, &overflow);

        arith_uint256 ratio = arith_uint256{static_cast<uint64_t>(consensus.nPowTargetSpacing)};
        arith_uint256 raw_share_target = block_target * ratio;
        arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
        arith_uint256 share_target = std::min(raw_share_target, pow_limit);

        context->share_nBits = share_target.GetCompact();
        context->share_target = ArithToUint256(share_target);
    }

When sharepool is not active, `share_nBits` remains 0, signaling workers to
skip the share check entirely.

### Worker Loop Changes

In the worker hot loop, after computing the RandomX hash and before the
staleness check:

    // Existing: check block target
    if (hash_meets_target(rx_hash, block_target)) {
        SubmitBlock(block);
        // A block-meeting hash always meets the share target too.
        if (ctx->share_nBits != 0) {
            SubmitShare(ctx, block.GetBlockHeader());
        }
    }
    // New: check share target (only when sharepool is active)
    else if (ctx->share_nBits != 0 && hash_meets_target(rx_hash, ctx->share_target)) {
        SubmitShare(ctx, block.GetBlockHeader());
    }

The second comparison is a 256-bit integer compare, cost ~1ns. The branch is
taken approximately once per `(block_difficulty / share_difficulty)` hashes,
which for a solo miner on mainnet is extremely rare (the network-wide rate is
~1/second, so a single miner with a small fraction of hashrate may go minutes
or hours between shares).

### SubmitShare Method

New private method on `InternalMiner`:

    void SubmitShare(const std::shared_ptr<MiningContext>& ctx,
                     const CBlockHeader& header);

Implementation:

1. Construct `ShareRecord`:
   - `version = SHARE_RECORD_VERSION` (currently 1).
   - `parent_share = ctx->parent_share` (best share tip at template time).
   - `prev_block_hash = ctx->block.hashPrevBlock`.
   - `candidate_header = header` (includes the nonce that met the share target).
   - `share_nBits = ctx->share_nBits`.
   - `payout_script = m_coinbase_script` (from `-mineaddress`).
2. Call `m_sharechain->AddShare(share, consensus)`.
3. If accepted: increment `m_shares_found`, then relay through the explicit
   peer-manager or node-context hook added for share announcements.
4. If already present or invalid: log at debug level, do not relay.

### Constructor Change

Add `SharechainStore*` parameter:

    InternalMiner(ChainstateManager& chainman,
                  interfaces::Mining& mining,
                  CConnman* connman = nullptr,
                  SharechainStore* sharechain = nullptr);

Store as `m_sharechain`. When null (pre-activation or tests), share production
is disabled regardless of activation state.

### New Counter

    std::atomic<uint64_t> m_shares_found{0};

Public accessor:

    uint64_t GetSharesFound() const {
        return m_shares_found.load(std::memory_order_relaxed);
    }

### Parent Share in MiningContext

Add to `MiningContext`:

    uint256 parent_share;  // Best share tip at template build time

Populated by the coordinator from `m_sharechain->BestTip()`. When the sharechain
is empty (first share on a fresh chain), this is `uint256::ZERO`, which is the
sentinel for "genesis share" (no parent).

## Share Construction

A `ShareRecord` constructed by the miner contains:

| Field              | Source                                      |
|--------------------|---------------------------------------------|
| `version`          | `SHARE_RECORD_VERSION` (constant, currently 1) |
| `parent_share`     | `MiningContext::parent_share` (from `SharechainStore::BestTip()`) |
| `prev_block_hash`  | `MiningContext::block.hashPrevBlock` (current chain tip) |
| `candidate_header` | The 80-byte block header with the nonce that met the share target |
| `share_nBits`      | `MiningContext::share_nBits` (derived from block target * 120) |
| `payout_script`    | `m_coinbase_script` (from `-mineaddress` CLI flag) |

The share's ID is `ShareRecord::GetHash()` (double-SHA256 of the serialized
record).

Validation (performed by `ValidateShare()` in `src/node/sharechain.h`):

- Version must equal `SHARE_RECORD_VERSION`.
- `candidate_header` RandomX hash must meet the `share_nBits` target.
- `share_nBits` must be consistent with the `prev_block_hash`'s block target
  (i.e., `share_nBits` target must equal
  `block_target * consensus.nPowTargetSpacing` clamped to `powLimit`).
- `payout_script` validation is a future miner/RPC policy requirement. The
  current `ValidateShare()` implementation does not reject empty or non-standard
  payout scripts.

## Activation Behavior

Sharepool activation is gated by `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`,
checked at template-build time by the coordinator thread.

**Before activation:**

- `MiningContext::share_nBits` = 0, `share_target` is unset.
- Workers check only the block target (existing behavior, zero overhead).
- `m_shares_found` stays at 0.
- `GetSharesFound()` returns 0.
- The miner does not interact with `SharechainStore`.

**At activation (first template refresh after the activation block):**

- The coordinator detects `DeploymentActiveAt` returning true.
- Logs a transition message:
  `InternalMiner: Sharepool activated at height %d -- dual-target mining enabled`
- Populates `share_target`, `share_nBits`, and `parent_share` in `MiningContext`.
- Workers begin checking the share target on their next context pickup.

**After activation:**

- Every template refresh includes share target fields.
- Workers produce shares whenever a hash meets the share target.
- No configuration change is needed from the operator.

**Deactivation:** Not currently specified. If the deployment is buried (as is
standard for Bitcoin Core soft forks), deactivation does not occur. If it were
to occur, the coordinator would stop populating share fields, and workers would
revert to block-only mode on the next context pickup.

## Acceptance Criteria

- On an activated 2-node regtest network with both nodes mining, shares appear
   in both nodes' `SharechainStore` via P2P relay.
- The share production rate approximately matches the expected cadence within
   statistical bounds (`share_target` uses the active network's
   `consensus.nPowTargetSpacing` multiplier, so shares target roughly one-second
   cadence).
- A hash that meets the block target also produces a share (block-finding
   shares are counted in both `m_blocks_found` and `m_shares_found`).
- Before sharepool activation, the miner produces zero shares and does not
   interact with `SharechainStore`.
- `getinternalmininginfo` reports `shares_found > 0` after mining on an
   activated network.
- The dual-target check adds less than 1% overhead to per-hash cycle time
   (measured by comparing hashrate before and after the change with sharepool
   active).
- The miner transitions seamlessly from block-only to dual-target mode when
   sharepool activates, with a clear log message and no restart required.
- Shares constructed by the miner pass `ValidateShare()` on the receiving peer.

## Verification

**Unit tests** (`src/test/miner_share_tests.cpp`, new):

- Construct a `MiningContext` with known `nBits`, verify `share_nBits` and
  `share_target` are computed correctly (`block_target *
  consensus.nPowTargetSpacing` for one-second share spacing, clamped to
  powLimit).
- Construct a `ShareRecord` from a mock mining context and verify it passes
  `ValidateShare()`.
- Verify `share_nBits = 0` when sharepool is not active.
- Verify the share target clamp: when `block_target *
  consensus.nPowTargetSpacing > powLimit`, the share target equals `powLimit`.

**Functional tests** (`test/functional/feature_sharepool_miner.py`, new):

- Start 2-node activated regtest. Mine with internal miner on both. After N
  blocks, assert `SharechainStore::ShareCount() > 0` on both nodes.
- Assert `getinternalmininginfo` shows `sharepool_active: true` and
  `shares_found > 0`.
- Pre-activation mining (mine blocks before activation height): assert
  `shares_found == 0`.
- Single-node mining, then connect second node: shares relay to the second
  node's sharechain.

**Performance test** (manual or CI, optional):

- Run internal miner for 60 seconds with sharepool inactive, record hashrate.
- Run internal miner for 60 seconds with sharepool active, record hashrate.
- Assert difference is less than 1%.

## Open Questions

1. Should the coordinator update `parent_share` independently of full template
   refreshes? A lightweight "share tip update" every few seconds would reduce
   stale-parent shares without the cost of rebuilding the entire block template.
   This is an optimization for a later iteration.

2. Should `SubmitShare` be synchronous (blocking the worker until `AddShare`
   completes) or asynchronous (enqueue and return)? Given expected frequency,
   synchronous is almost certainly fine, but profiling under adversarial share
   rates (e.g., very low share difficulty on regtest) should confirm this.

3. The `parent_share` field creates a dependency on the sharechain state. If the
   sharechain is temporarily unavailable (e.g., database corruption), should the
   miner fall back to block-only mode or use `uint256::ZERO` as the parent?
   Block-only fallback is safer and simpler.

4. RPC extensions (`getinternalmininginfo` adding `sharepool_active`,
   `shares_found`, `share_tip`, `pending_pooled_reward`) are specified here for
   context but will be implemented in Plan 008 Unit B. The miner's only
   responsibility is exposing the counters and state via public accessors.
