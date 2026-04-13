# Miner, Wallet, and RPC Integration

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan delivers the user-facing integration layer for sharepool. After this plan, a CPU miner running `rngd -mine -mineaddress=<addr> -minethreads=4` on an activated network will: (a) produce shares alongside block attempts, (b) see accepted shares counted in `getinternalmininginfo`, (c) see pending and claimable pooled reward in `getbalances`, (d) have the wallet auto-construct claim transactions when settlements mature, and (e) use `submitshare`, `getsharechaininfo`, and `getrewardcommitment` RPCs for diagnostics.

This is where the "default mining mode" and "small miner accrual" goals become tangible for users.

## Requirements Trace

`R1`. The internal miner must check each RandomX hash against both the block target and the share target. When a hash meets the share target, the miner constructs and relays a `ShareRecord`.

`R2`. `getinternalmininginfo` must add `sharepool_active`, `shares_found`, `share_tip`, and `pending_pooled_reward` fields when sharepool is active.

`R3`. `getbalances` must add `pooled: { pending, claimable }` when sharepool is active.

`R4`. The wallet must auto-construct claim transactions for matured settlements where the local wallet owns the committed payout script.

`R5`. `submitshare` validates, stores, and relays one serialized share.

`R6`. `getsharechaininfo` returns best share tip, share height, orphan count, and reward window size.

`R7`. `getrewardcommitment` returns the commitment root, leaves, and amounts for a given block hash.

`R8`. All new features gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`.

## Scope Boundaries

This plan does not change consensus rules (those are Plan 007). It does not change the P2P relay protocol (Plan 005). It does not run the end-to-end regtest proof (Plan 009). It extends the miner, wallet, and RPC layers to use the consensus and sharechain infrastructure built by earlier plans.

## Progress

- [ ] Unit A: Dual-target share-producing miner
- [ ] Unit B: Sharepool RPCs
- [ ] Unit C: Wallet pooled reward tracking and auto-claim

## Surprises & Discoveries

None yet.

## Decision Log

- Decision: The share-producing miner should be an extension of the existing internal miner, not a separate binary or service.
  Rationale: The internal miner already has the coordinator/worker architecture, RandomX VM management, and template refresh. Adding a second target check is a natural extension. Running a separate service would fragment the mining experience and contradict the "default mining mode" goal.
  Date/Author: 2026-04-13 / this plan

- Decision: Auto-claim should be wallet-initiated, not miner-initiated.
  Rationale: The wallet owns the payout script and can scan coinbase outputs for settlements. The miner does not need to know about claims. This separation allows third-party claim helpers to work alongside wallet auto-claim.
  Date/Author: 2026-04-13 / this plan

- Decision: `pending_pooled_reward` in `getinternalmininginfo` should be an approximate value based on the current reward window, not a consensus-enforced number.
  Rationale: The exact commitment is built at block assembly time. Between blocks, the reward window shifts as new shares arrive. An approximate value gives the user a useful signal without requiring block-assembly-level computation on every RPC call.
  Date/Author: 2026-04-13 / this plan

## Outcomes & Retrospective

Not yet started.

## Context and Orientation

The internal miner lives in `src/node/internal_miner.{h,cpp}` (770 lines). The coordinator thread creates block templates and publishes them to worker threads via an atomic shared pointer. Workers grind nonces in lock-free loops. Currently, workers check only the block difficulty target. After this plan, workers also check a share difficulty target derived from the confirmed 120x ratio.

The wallet is the standard Bitcoin Core v30.2 descriptor-based wallet in `src/wallet/`. Balance reporting is in `src/wallet/rpc/coins.cpp` (`getbalances` RPC). The wallet scans blocks for transactions relevant to its descriptors. After this plan, it also scans for settlement outputs where the committed payout script matches a wallet descriptor.

RPCs are in `src/rpc/mining.cpp` (`getinternalmininginfo`, `getmininginfo`). New sharepool RPCs will be added to the same file or a new `src/rpc/sharepool.cpp`.

## Plan of Work

### Unit A: Dual-target share-producing miner

Modify the worker thread in `src/node/internal_miner.cpp` to compute the share target from the block target using the confirmed 120x ratio. After each RandomX hash computation, check against both targets:

1. If hash meets block target: submit block (existing behavior).
2. If hash meets share target but not block target: construct a `ShareRecord` from the current block template, store it in the local `SharechainStore`, relay it via `shareinv`, and increment `m_shares_found`.
3. If hash meets neither: continue grinding (existing behavior).

Add `m_shares_found` as an atomic counter to `InternalMiner`. The coordinator thread should set the share target in the `MiningContext` so workers do not need to recompute it.

Share construction requires: the 80-byte candidate header (already available from the block template), the share nBits (derived from block nBits * ratio), the miner's payout script (from `-mineaddress`), and the parent share ID (from the sharechain best tip).

### Unit B: Sharepool RPCs

Add three new RPCs:

1. `submitshare`: Accept hex-encoded serialized `ShareRecord`, validate via `SharechainStore::AddShare()`, relay if accepted, return `{ accepted: bool, share_id: hex, error: string }`.

2. `getsharechaininfo`: Query `SharechainStore` for best tip, height, share count, orphan count. If sharepool active, also compute reward window size from the best tip.

3. `getrewardcommitment`: Given a block hash, look up the coinbase, extract the settlement output, decode the state hash, and return the payout leaves and amounts. This requires either storing the leaves alongside the block or recomputing them from the sharechain at the block's share tip.

Extend `getinternalmininginfo` with `sharepool_active`, `shares_found`, `share_tip`, and `pending_pooled_reward`.

Extend `getmininginfo` with `sharepool_active` and `share_tip`.

### Unit C: Wallet pooled reward tracking and auto-claim

Extend the wallet to recognize settlement outputs in coinbase transactions:

1. When a block is connected and sharepool is active, scan the coinbase for the settlement output (witness v2, 32-byte program).
2. Decode the settlement descriptor from the known payout leaves (stored or recomputed).
3. If any payout leaf's `payout_script` matches a wallet descriptor, record the pending pooled reward entry.
4. Track maturity: after 100 blocks, the entry becomes claimable.
5. Auto-claim: construct the claim transaction with the settlement input, payout output, successor settlement output, and a fee-paying input from the wallet.

Add `pooled: { pending, claimable }` to `getbalances` output.

## Implementation Units

### Unit A: Dual-target miner
- Goal: Internal miner produces shares alongside block attempts
- Requirements advanced: R1, R8
- Dependencies: Plan 007 (consensus rules), Plan 005 (sharechain store)
- Files to create or modify: `src/node/internal_miner.{h,cpp}`
- Tests to add or modify: `test/functional/feature_sharepool_miner.py` (new)
- Approach: Extend MiningContext with share_target and share_nBits; extend worker loop with second target check; add ShareRecord construction
- Specific test scenarios:
  - Activated 2-node regtest, both mining: shares appear in sharechain
  - Share rate approximately matches expected cadence (within statistical bounds)
  - Block-finding shares also count as shares
  - Pre-activation mining produces no shares
  - `getinternalmininginfo` reports shares_found > 0 after mining

### Unit B: Sharepool RPCs
- Goal: Diagnostic and submission RPCs for sharepool
- Requirements advanced: R2, R5, R6, R7, R8
- Dependencies: Unit A (shares must exist to query)
- Files to create or modify: `src/rpc/mining.cpp` or `src/rpc/sharepool.cpp` (new), `src/rpc/register.h`
- Tests to add or modify: `test/functional/feature_sharepool_rpc.py` (new)
- Approach: Register new RPCs, implement using SharechainStore queries
- Specific test scenarios:
  - `getsharechaininfo` returns non-zero height after mining
  - `submitshare` with valid share -> accepted
  - `submitshare` with invalid share -> rejected with error
  - `getrewardcommitment` for an activated block -> returns leaves and amounts
  - RPCs return error before activation

### Unit C: Wallet auto-claim
- Goal: Wallet tracks pooled rewards and auto-claims
- Requirements advanced: R3, R4, R8
- Dependencies: Plan 007 (claims must be consensus-valid), Unit A (shares must exist)
- Files to create or modify: `src/wallet/wallet.{h,cpp}`, `src/wallet/receive.cpp`, `src/wallet/spend.cpp`, `src/wallet/rpc/coins.cpp`
- Tests to add or modify: `test/functional/feature_sharepool_wallet.py` (new)
- Approach: Scan coinbase for settlement outputs on BlockConnected, match payout scripts to descriptors, track maturity, auto-build claim transactions
- Specific test scenarios:
  - After mining 100+ blocks with activation, `getbalances` shows pooled.pending > 0
  - After 100-block maturity, pooled.claimable > 0
  - Auto-claim produces valid claim transaction
  - Claimed amount appears as confirmed wallet balance
  - Non-matching payout scripts are not tracked

## Concrete Steps

After implementing Unit A:

    cmake --build build -j$(nproc)
    python3 test/functional/feature_sharepool_miner.py --configfile=build/test/config.ini

After implementing Unit B:

    python3 test/functional/feature_sharepool_rpc.py --configfile=build/test/config.ini

After implementing Unit C:

    python3 test/functional/feature_sharepool_wallet.py --configfile=build/test/config.ini

Full validation:

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=sharepool_commitment_tests
    build/bin/test_bitcoin --run_test=sharepool_claim_tests
    build/bin/test_bitcoin --run_test=miner_tests
    python3 test/functional/feature_sharepool_miner.py --configfile=build/test/config.ini
    python3 test/functional/feature_sharepool_rpc.py --configfile=build/test/config.ini
    python3 test/functional/feature_sharepool_wallet.py --configfile=build/test/config.ini

## Validation and Acceptance

1. A 2-node activated regtest network with both nodes mining produces shares at approximately the expected rate.
2. `getinternalmininginfo` shows `shares_found > 0` and `sharepool_active: true`.
3. `getsharechaininfo` shows a non-zero share height and share count.
4. `getrewardcommitment` returns the expected payout leaves for an activated block.
5. `getbalances` shows `pooled.pending > 0` after shares are produced.
6. After 100-block maturity, `pooled.claimable > 0`.
7. The wallet auto-claims, and the claimed amount appears as confirmed balance.
8. A 2-node scenario with different mining addresses shows proportional reward accrual.

## Idempotence and Recovery

The miner extension is stateless (shares are written to the sharechain store which is persistent). The wallet tracks pooled rewards per settlement output; rescanning the chain reconstructs the tracking state. RPCs are stateless queries.

If the wallet state becomes inconsistent, `rescanblockchain` rebuilds pooled reward tracking from block data.

## Artifacts and Notes

To be filled during implementation.

## Interfaces and Dependencies

### New interfaces

In `src/node/internal_miner.h`:

    uint64_t GetSharesFound() const;

In `MiningContext`:

    uint256 share_target;
    unsigned int share_nBits;

In `src/rpc/` (new RPCs):

    submitshare, getsharechaininfo, getrewardcommitment

In `src/wallet/wallet.h`:

    struct PooledRewardEntry {
        uint256 block_hash;
        int height;
        CScript payout_script;
        CAmount amount;
        bool claimed;
    };

### Modified interfaces

- `InternalMiner::WorkerLoop()`: Add second target check
- `getbalances` RPC: Add `pooled` object
- `getinternalmininginfo` RPC: Add sharepool fields
- `getmininginfo` RPC: Add `sharepool_active` field

### Dependencies

- Plan 007 (consensus enforcement must be complete)
- Plan 005 (sharechain store)
- `src/consensus/sharepool.{h,cpp}` (settlement helpers)
- `src/node/sharechain.{h,cpp}` (share storage)
- `src/crypto/randomx_hash.h` (RandomX for share proof)
