# Specification: Wallet Pooled Reward Tracking, Auto-Claim, and Sharepool RPCs

Plan 008, Units B (wallet integration) and C (sharepool RPCs).

## Objective

Extend the RNG wallet and RPC interface so that miners can:

1. See their pooled reward entitlements alongside normal balances.
2. Automatically claim matured settlement outputs without manual intervention.
3. Interact with the sharechain (submit shares, query state, inspect commitments)
   through standard JSON-RPC calls.

## Evidence Status

### Verified Facts

- **Wallet engine**: Bitcoin Core v30.2 descriptor-based SQLite wallet. The existing
  `getbalances` RPC returns `{ mine: { trusted, untrusted_pending, immature }, watchonly: {...} }`.
  No pooled reward concept exists today.
- **Address format**: Bech32 HRP is `rng`.
- **BIP9 activation gating**: `SharepoolRelayActive()` (P2P relay, `src/net_processing.cpp:2161`)
  and `SharepoolDeploymentActiveAfter()` (miner settlement, `src/node/miner.cpp:82`) both
  check `DeploymentActiveAt()` / `DeploymentActiveAfter()` for `DEPLOYMENT_SHAREPOOL`.
  All sharepool code paths are gated behind these checks.
- **Coinbase maturity**: 100 blocks (standard Bitcoin consensus rule, unchanged in RNG).
- **Regtest activation**: confirmed working via `-vbparams=sharepool:0:9999999999:0`
  (used in `src/test/miner_tests.cpp:77`).
- **Current settlement commitment shape**: `src/node/miner.cpp` builds a
  settlement output as `OP_2 <state_hash>` after activation. No implemented
  `getrewardcommitment` RPC, wallet settlement tracker, or consensus data
  contract currently exists for reconstructing the committed leaf set from a
  block alone.

### Recommendations

- Keep all new RPCs behind the same `DEPLOYMENT_SHAREPOOL` activation gate so they
  return clear errors (e.g., `RPC_MISC_ERROR`, "sharepool not active") before activation.
- Treat `getrewardcommitment`, `pooled.pending`, and wallet leaf discovery as
  blocked on the canonical reward-window data-availability decision in the
  settlement/protocol specs. Local `SharechainStore` state can inform mining and
  diagnostics, but it is not by itself sufficient for deterministic wallet/RPC
  leaf reconstruction after restart, pruning, or reorg.
- Implement auto-claim as an opt-out wallet feature (enabled by default) with a
  `-noautoclaim` startup flag, so miners who want manual control can disable it.
- Add a `claimreward` RPC for manual claiming, even with auto-claim enabled, to
  cover edge cases (fee bumping, wallet-locked scenarios).

### Hypotheses / Unresolved Questions

- **Fee economics**: If claim fees become a significant fraction of the claimed
  amount, small miners may find claims uneconomic. A threshold below which the
  wallet skips auto-claim (configurable via `-minclaimpayout`) may be needed. Not
  designed here; deferred to implementation.
- **Batched claims**: v1 enforces one claim per transaction per settlement output.
  Batching multiple settlement claims into a single transaction would reduce fees
  but adds complexity to coin selection and transaction construction. Deferred to v2.
- **Reorg handling**: If a settlement output is reorged away after the wallet has
  broadcast a claim, the claim becomes invalid. The wallet should detect this via
  block-disconnect callbacks and remove the pending claim from its tracking state.
  Exact mechanism TBD during implementation.
- **Commitment data availability**: The wallet/RPC surface needs a canonical
  source of settlement leaves for each block: an explicit block payload,
  consensus-persisted share context, or another deterministic mechanism. Until
  that is selected, multi-leaf wallet tracking and `getrewardcommitment` are
  interface recommendations, not implementable contracts.

## Wallet Balance Extensions

### getbalances Response Change

Current response (unchanged fields):
```json
{
  "mine": {
    "trusted": 0.0,
    "untrusted_pending": 0.0,
    "immature": 0.0
  }
}
```

Extended response:
```json
{
  "mine": {
    "trusted": 0.0,
    "untrusted_pending": 0.0,
    "immature": 0.0
  },
  "pooled": {
    "pending": 0.0,
    "claimable": 0.0
  }
}
```

Field definitions:

| Field | Meaning | Spendable? |
|-------|---------|------------|
| `pooled.pending` | Reward-window entitlement before the settlement matures. Derived from the miner's share weight when canonical commitment data is available; otherwise unavailable or diagnostic-only. | No |
| `pooled.claimable` | Matured settlement outputs where this miner has a committed leaf. The miner can construct a claim transaction to move these funds to `mine.trusted`. | Yes (via claim tx) |

### Distinction: immature vs. pooled.claimable

| Category | Source | Matures after | Claim needed? |
|----------|--------|---------------|---------------|
| `mine.immature` | Block-finder coinbase reward (miner's own block) | 100 blocks | No (auto-matures) |
| `pooled.claimable` | Settlement output (share contributor reward from others' blocks) | 100 blocks from settlement confirmation | Yes (explicit claim tx) |

Both require 100-block maturity. The difference is that `immature` coinbase outputs
become `trusted` automatically, while `pooled.claimable` outputs require the miner
to construct and broadcast a claim transaction.

## Auto-Claim Mechanism

### Trigger

The wallet monitors settlement outputs where the miner has a committed leaf.
When a settlement output reaches 100 confirmations (coinbase maturity), the
wallet automatically constructs and broadcasts a claim transaction. This requires
the canonical leaf data selected by the reward-window data-availability gate.

### Claim Transaction Structure

```
Inputs:
  [0] Settlement UTXO (the matured settlement output)
  [1..n] Fee-funding inputs (non-settlement UTXOs from the wallet)

Outputs:
  [0] Payout to leaf's payout_script (the miner's committed payout address)
  [1] Successor settlement output (continues the settlement chain)
```

Key constraints:

- **No inner signature needed**: The payout destination is consensus-locked in the
  commitment leaf. The claim transaction proves entitlement by satisfying the
  settlement script conditions, not by providing an additional signature.
- **Fee source**: Claim fees MUST come from non-settlement inputs (inputs 1..n).
  The settlement UTXO's full value flows to payout + successor settlement.
- **One claim per tx**: v1 allows exactly one settlement output claim per
  transaction. No batching across multiple settlement outputs.

### Wallet State Tracking

The wallet must maintain a mapping of:

```
settlement_outpoint -> {
    leaf_index: u32,
    payout_script: CScript,
    amount: CAmount,
    confirmation_height: int,
    claim_status: enum { PENDING_MATURITY, CLAIMABLE, CLAIM_BROADCAST, CLAIMED }
}
```

State transitions:

1. `PENDING_MATURITY`: Settlement output detected, fewer than 100 confirmations.
   Contributes to `pooled.pending`.
2. `CLAIMABLE`: 100+ confirmations. Contributes to `pooled.claimable`.
   Auto-claim constructs and broadcasts the claim tx.
3. `CLAIM_BROADCAST`: Claim tx broadcast, awaiting confirmation. Amount moves
   from `pooled.claimable` to `mine.untrusted_pending`.
4. `CLAIMED`: Claim tx confirmed. Amount in `mine.trusted`.

## Sharepool RPCs

### submitshare

```
submitshare "hexdata"
```

Validate, store, and relay a share to connected peers.

- **Parameters**: `hexdata` (string, required) -- serialized share in hex.
- **Returns**: `{ "accepted": true }` on success, or error with rejection reason.
- **Validation**: Full share validation currently means the consensus/P2P share
  checks implemented by `ValidateShare()` and `SharechainStore::AddShare()`:
  PoW target, version, parent linkage, payout-script policy if added, and
  activation gating. Reward-window membership is not a property of an individual
  submitted share until the later settlement data contract is selected.
- **Relay**: Accepted shares are announced to peers via share inventory messages.
- **Gate**: Returns `RPC_MISC_ERROR` if sharepool deployment is not active.

### getsharechaininfo

```
getsharechaininfo
```

Return summary information about the local sharechain state.

- **Parameters**: none.
- **Returns**:
  ```json
  {
    "tip": "hex-hash-of-best-share",
    "height": 12345,
    "orphan_count": 3,
    "reward_window_size": 256,
    "total_shares": 45678,
    "difficulty": 1.23456789
  }
  ```
- **Gate**: Returns `RPC_MISC_ERROR` if sharepool deployment is not active.

### getrewardcommitment

```
getrewardcommitment "blockhash"
```

Return the reward commitment details for an activated block.

- **Parameters**: `blockhash` (string, required) -- hash of a main-chain block.
- **Returns**:
  ```json
  {
    "blockhash": "...",
    "state_hash": "hex-settlement-state-hash",
    "leaves": [
      {
        "payout_script": "hex",
        "amount": 0.00012345,
        "share_count": 17
      }
    ],
    "total_committed": 0.00123456
  }
  ```
- **Gate**: Returns `RPC_MISC_ERROR` if sharepool deployment is not active.
  Returns `RPC_INVALID_PARAMETER` if the block has no reward commitment (pre-activation
  block or block without sharepool activity).
- **Data source**: For solo-settlement blocks, the existing commitment path can
  identify the single payout. For multi-leaf blocks, this RPC must use the
  canonical reward-window data contract chosen before implementation; it must
  not silently reconstruct leaves from the caller's local share relay history.

### Extended getmininginfo

Add sharepool-specific fields to the existing `getmininginfo` response when
sharepool is active:

```json
{
  "...existing fields...": "...",
  "sharepool": {
    "active": true,
    "sharechain_height": 12345,
    "reward_window_size": 256,
    "pending_shares": 42,
    "last_settlement_height": 100200
  }
}
```

When sharepool is not active, the `sharepool` object is omitted entirely (not
present with `"active": false`).

## Activation Gating for RPCs

All new RPCs and wallet extensions check `DeploymentActiveAt()` for
`DEPLOYMENT_SHAREPOOL` before executing. Behavior before activation:

| Component | Behavior before activation |
|-----------|---------------------------|
| `getbalances` `pooled` field | Omitted from response |
| `submitshare` | Returns `RPC_MISC_ERROR`: "sharepool not active" |
| `getsharechaininfo` | Returns `RPC_MISC_ERROR`: "sharepool not active" |
| `getrewardcommitment` | Returns `RPC_MISC_ERROR`: "sharepool not active" |
| `getmininginfo` `sharepool` field | Omitted from response |
| Auto-claim | Disabled (no settlement outputs exist) |

## Acceptance Criteria

- `getbalances` returns `pooled.pending` and `pooled.claimable` fields when
   sharepool is active and canonical leaf data is available; it omits the
   `pooled` object when inactive.
- `pooled.pending` accurately reflects the wallet's reward-window entitlement
   based on shares contributed by this wallet's mining keys when canonical
   commitment data is available; otherwise the field is omitted or marked
   unavailable rather than inferred from local-only relay state.
- `pooled.claimable` accurately reflects matured settlement outputs where this
   wallet holds a committed leaf.
- Auto-claim constructs valid claim transactions for matured settlement outputs,
   funded by non-settlement wallet inputs.
- `submitshare` accepts valid shares, stores them, and relays to peers.
- `submitshare` rejects invalid shares with specific, actionable error messages.
- `getsharechaininfo` returns accurate sharechain state.
- `getrewardcommitment` returns correct commitment data for blocks with
   sharepool activity using the canonical leaf data source selected by the
   reward-window data-availability gate.
- `getmininginfo` includes sharepool data when active.
- All new RPCs and balance fields return appropriate errors or are omitted
    when sharepool deployment is not active.

## Verification

- **Unit tests**: Each new RPC handler has unit tests covering valid input,
  invalid input, and pre-activation behavior.
- **Functional tests**: Python functional tests (in `test/functional/`) covering:
  - Regtest activation via `-vbparams=sharepool:0:9999999999:0`
  - Share submission, sharechain query, reward commitment query lifecycle
  - `getrewardcommitment` after restart/reindex, proving it does not depend on
    transient local relay state
  - Auto-claim: mine blocks with settlement, wait for maturity, verify claim tx
    is broadcast and `pooled.claimable` transitions to `mine.trusted`
  - Verify `getbalances` shows correct `pooled` values at each stage
- **Negative tests**: Verify RPCs reject cleanly before activation, with
  invalid parameters, and with non-existent block hashes.

## Open Questions

1. **Fee estimation for auto-claim**: Should auto-claim use the wallet's normal
   fee estimation, or a separate (possibly more conservative) estimator? Claiming
   is not time-sensitive, so lower fees may be acceptable.
2. **Wallet encryption interaction**: When the wallet is locked, auto-claim
   cannot sign fee-funding inputs. Should it queue claims and execute when
   unlocked, or require the user to call `claimreward` manually?
3. **Pruned node support**: Can a pruned node reconstruct enough settlement
   history to populate `pooled.pending`? If not, should `pooled.pending` return
   a "not available" indicator on pruned nodes?
4. **Notification**: Should the wallet emit a `zmq` notification or `-walletnotify`
   callback when a new settlement output becomes claimable or when a claim
   tx confirms?
5. **Descriptor wallet integration**: How do settlement tracking descriptors
   interact with the existing descriptor wallet? Do we need a new descriptor
   type, or can we use a watchonly import of the settlement script?
6. **Leaf data source**: Which data-availability mechanism lets
   `getrewardcommitment` and wallet tracking enumerate leaves deterministically
   after restart, reindex, pruning, and reorg?
