# Internal Miner, External Miner, and Wallet Claim Integration

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root, which defines the ExecPlan standard for all plans in this corpus.


## Purpose / Big Picture

After this change, pooled mining is usable end-to-end by ordinary RNG miners. Before this change, the payout commitment exists on-chain (Plan 007) and shares propagate peer-to-peer (Plan 005), but the internal miner still searches only for full blocks, the wallet does not recognize payout commitment outputs, and mining RPCs do not report pooled reward status. After this change, a miner starts RNG with `-mine -mineaddress=<addr> -minethreads=2` on an activated network, and within minutes sees pending pooled rewards accumulating. After 100 confirmations, the wallet automatically builds claim transactions that transfer the miner's proportional reward to a spendable output. No external pool URL, no operator coordination, no manual claim construction.

The observable result: two regtest miners with unequal thread counts (for example, 1 thread and 4 threads) both start mining on an activated network. Within the first few blocks, `rng-cli getmininginfo` on both nodes shows `sharepool_active: true`, a non-zero `accepted_shares` count, and a `pending_pooled_reward` amount proportional to their contribution. After 100 blocks of maturity, `rng-cli getbalances` shows the pending reward transitioning to claimable, and the wallet automatically broadcasts claim transactions that confirm and appear as spendable balance.

This plan depends on Plan 005 (sharechain data model and P2P relay), Plan 007 (payout commitment and claim program), and Plan 004 (version-bits deployment skeleton). It does not depend on Plan 006 (the relay viability decision gate) or Plan 009 (the end-to-end proof, which exercises this plan's output).

Terminology used throughout this plan:

The "internal miner" is the built-in multi-threaded mining engine in `src/node/internal_miner.cpp`. It runs inside the `rngd` process and is controlled by the `-mine`, `-minethreads`, `-mineaddress`, and `-minerandomx` flags. It currently searches for block-difficulty hashes only. After this plan, it also produces share-difficulty hashes.

A "share" is a candidate block header whose RandomX hash meets the share difficulty target but may not meet the full block difficulty target. Shares prove that the miner performed work. When a share also meets the full block target, it is simultaneously a valid block.

"Pending pooled reward" means the deterministic amount a miner has earned based on accepted shares in the reward window, for blocks whose coinbase has not yet matured. The miner can see this amount but cannot spend it.

"Claimable pooled reward" means the same amount after the relevant coinbase has reached 100 confirmations. The wallet can now build a claim transaction to spend it.

A "claim transaction" is a regular RNG transaction that spends a payout commitment output (witness v2) using the claim witness format defined in Plan 007: Merkle branch, leaf index, leaf data, and signature. The output of the claim transaction sends the funds to the miner's wallet address.

The `-mineaddress` flag specifies the miner's payout destination. Before activation, this is the scriptPubKey that receives the full coinbase reward. After activation, this same script becomes the payout script encoded in shares, which determines the miner's leaf in payout commitments. The flag name does not change.


## Requirements Trace

`R3` (immediate accrual). Miners see pending pooled reward accumulating after each block, proportional to their share contribution, even though the funds are not spendable until maturity.

`R5` (miner-built templates). The internal miner continues to build its own block templates. No external service constructs templates on the miner's behalf. The miner calls `CreateNewBlock()` (which now includes the payout commitment from Plan 007) and searches for both share-difficulty and block-difficulty solutions.

`R8` (pre-activation preservation). Before activation, the internal miner behaves exactly as it does today: searching for full blocks, paying the winner-takes-all coinbase. All existing mining RPCs return their current responses. The wallet does not attempt to recognize payout commitment outputs.

`R10` (solo as special case). When only one miner is active, that miner contributes all shares and receives the full reward through the payout commitment. The experience is functionally identical to classical solo mining, but the accounting flows through the sharechain and commitment machinery.

`R11` (truthfulness). RPCs and wallet output distinguish between "pending" (accrued but immature) and "claimable" (mature, claim transaction can be built). Documentation and help text reflect this distinction. The wallet never reports funds as spendable before they actually are.


## Scope Boundaries

This plan does not implement the sharechain data model or P2P relay (Plan 005). It does not implement the payout commitment or claim-spend verification (Plan 007). It assumes both are complete and tested.

This plan does not implement `getblocktemplate` extensions for external miners. The initial integration focuses on the internal miner. External miner support (extending `getblocktemplate` with a `sharepool` section, adding `submitshare` RPC) is noted as a follow-up and partially described here for orientation, but the core deliverable is internal miner and wallet integration.

This plan does not reduce coinbase maturity. The 100-block maturity rule (approximately 200 minutes at the 120-second block target) remains a consensus constraint. Miners must wait before claiming.

This plan does not implement adversarial testing or devnet deployment (Plan 011). It does not implement the end-to-end regtest proof (Plan 009), though it produces the working system that Plan 009 exercises.

This plan does not depend on any parallel QSB rollout work described in the local root `EXECPLAN.md`. The inspected checkout did not contain the corresponding QSB source files, so this integration plan should stay grounded in verified miner, wallet, and RPC seams only.


## Progress

- [ ] Modify the internal miner's worker loop to produce shares continuously after activation.
- [ ] Integrate share submission into the coordinator thread's block-found flow.
- [ ] Extend `getmininginfo` with sharepool fields.
- [ ] Add `getrewardcommitment` RPC user-facing version (extending the minimal version from Plan 007).
- [ ] Add wallet recognition of payout commitment outputs addressed to local scripts.
- [ ] Add wallet tracking of pending vs claimable pooled rewards.
- [ ] Implement automatic claim transaction construction in the wallet.
- [ ] Extend `getbalances` to include pooled reward fields.
- [ ] Reinterpret `-mineaddress` as share payout destination after activation.
- [ ] Write unit tests for wallet claim tracking.
- [ ] Write `test/functional/feature_sharepool_wallet.py`.
- [ ] Write `test/functional/feature_sharepool_mining.py`.
- [ ] Update documentation.


## Surprises & Discoveries

(No entries yet. This section will be updated as implementation proceeds.)

- Observation: ...
  Evidence: ...


## Decision Log

- Decision: Produce shares in the same worker loop that searches for blocks, using a dual-target comparison.
  Rationale: The internal miner's worker threads in `src/node/internal_miner.cpp` already grind RandomX nonces in a tight loop (the stride pattern starting around line 300 in the worker function). Each hash is currently compared only against the block difficulty target. After activation, each hash is compared against two targets: the share target (lower difficulty, more solutions) and the block target (higher difficulty, fewer solutions). If the hash meets the share target but not the block target, the worker reports a share to the coordinator. If the hash meets both targets, the worker reports a block (which is also a share). This avoids duplicating the hashing loop and preserves the existing coordinator/worker architecture.
  Date/Author: 2026-04-12 / Plan authored

- Decision: The wallet scans coinbase transactions for payout commitment outputs whose leaves match local scripts, rather than watching for claim transactions.
  Rationale: The wallet needs to know about pending rewards before any claim transaction exists. When a new block confirms, the wallet must check whether any reward leaf in that block's payout commitment has a payout script that the wallet owns (via `IsMine()`). If so, the wallet records a pending pooled reward entry. This is similar to how the wallet currently tracks immature coinbase rewards, but instead of tracking the coinbase output directly, it tracks the leaf data from the commitment. The wallet stores these pending entries in its database alongside regular transaction records.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Automatic claim transaction construction, not manual.
  Rationale: Requiring miners to manually construct claim transactions would be a poor user experience and would violate the "immediate accrual" spirit of R3. The wallet should monitor maturing payout commitments and automatically build, sign, and broadcast claim transactions once the coinbase matures. This is analogous to how some wallets automatically consolidate UTXOs. The miner does not need to take any action beyond running the node with `-mine`.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Keep the `-mineaddress` flag name unchanged after activation.
  Rationale: The flag already means "where my mining rewards go." After activation, it means the same thing but the mechanism changes from direct coinbase payment to share payout script. Renaming the flag would break existing configurations and scripts. The documentation should explain that after activation, this address is used as the payout destination in shares.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Surface pooled reward in `getbalances` under a new `pooled` object, not by overloading existing balance fields.
  Rationale: Mixing pooled rewards with confirmed or immature balances would violate R11 (truthfulness). The `getbalances` response already has a `mine` object with fields like `trusted`, `untrusted_pending`, and `immature`. Adding a `pooled` object with `pending` and `claimable` sub-fields keeps the distinction clear and avoids confusing existing wallet integrations.
  Date/Author: 2026-04-12 / Plan authored


## Outcomes & Retrospective

(No entries yet. This section will be updated at major milestones and at completion.)


## Context and Orientation

The reader needs to understand four code areas to implement this plan.

First, the internal miner in `src/node/internal_miner.cpp`. This is approximately 600 lines of RNG-specific code. The class `InternalMiner` (declared in `src/node/internal_miner.h`) manages a coordinator thread and N worker threads. The `Start()` method (line 38) initializes the miner with a coinbase script, thread count, and RandomX mode. Worker threads run a tight loop that: (a) gets the current block template from the coordinator, (b) grinds RandomX nonces using a stride pattern (worker 0 tries nonces 0, N, 2N, ...; worker 1 tries 1, N+1, 2N+1, ...), and (c) reports to the coordinator when a hash meets the block target. The coordinator thread builds new block templates when the chain tip changes and submits completed blocks. Statistics are tracked via atomic counters: `m_hash_count`, `m_blocks_found`, `m_stale_blocks`, `m_template_count`.

Second, the mining RPCs in `src/rpc/mining.cpp`. The `getmininginfo` RPC (line 435) returns mining status including difficulty, network hashrate, and chain information. The `getinternalmininginfo` RPC (RNG-specific) returns detailed internal miner statistics. After this plan, `getmininginfo` gains additional fields: `sharepool_active` (boolean), `share_tip` (hex hash of the best known share), `pending_pooled_reward` (amount in RNG), and `accepted_shares` (count of shares this node has submitted and had accepted).

Third, the wallet in `src/wallet/wallet.cpp`. The `CWallet` class tracks transactions via `AddToWalletIfInvolvingMe()` (line 1181), which is called for every transaction the wallet sees (in blocks and in the mempool). The `IsMine()` method (line 1632) checks whether a script belongs to the wallet's keypool. The `SyncTransaction()` method (line 1393) is the entry point for block processing. The wallet currently tracks immature coinbase rewards and makes them spendable after 100 confirmations. This plan adds parallel tracking for pooled reward claims.

Fourth, the wallet balance RPCs in `src/wallet/rpc/coins.cpp`. The `getbalances` RPC (line 401) returns a structured JSON object with balance categories under `mine`. This plan adds a `pooled` category at the same level as `mine` with `pending` and `claimable` sub-fields.

The `-mineaddress` flag is parsed in `src/init.cpp` and converted to a `CScript` that becomes `m_options.coinbase_output_script` in `BlockAssembler`. After activation, this same script is embedded in every share the miner produces, linking the miner's work to a specific payout destination in the sharechain.


## Plan of Work

The work proceeds in three stages: internal miner share production, mining RPC extensions, and wallet claim integration.

Stage one modifies the internal miner. In the worker thread's hash comparison loop, add a second target check. The share target is provided by the sharechain parameters from Plan 004 (the `SharePoolParams.target_share_spacing` field in `src/consensus/params.h` determines the share difficulty, which is lower than block difficulty). When a worker finds a hash that meets the share target, it constructs a `ShareRecord` (from Plan 005's `src/sharechain/share.h`) and passes it to the coordinator thread via a thread-safe queue. The coordinator submits the share to the local sharechain store and relays it to peers. If the hash also meets the block target, the coordinator handles it as a found block (existing behavior) and also records it as a share. Add a new atomic counter `m_shares_found` to the internal miner statistics.

Stage two extends the mining RPCs. In `src/rpc/mining.cpp`, modify `getmininginfo` to include four new fields when sharepool is active: `sharepool_active` (true when DEPLOYMENT_SHAREPOOL is active at the current chain tip), `share_tip` (the hash of the best known share from the local sharechain store), `pending_pooled_reward` (the sum of all pending reward leaves addressed to the node's `-mineaddress` script across all immature activated blocks), and `accepted_shares` (the total number of shares this node has submitted that were accepted into the sharechain). When sharepool is not active, these fields are omitted. Extend the existing `getrewardcommitment` RPC from Plan 007 with human-readable formatting: amounts in RNG (not just roshi), payout addresses in bech32 where possible.

Stage three integrates the wallet. In `src/wallet/wallet.cpp`, extend `blockConnected()` (the callback when a new block is connected) to scan the coinbase for payout commitment outputs (witness v2, 32-byte program). When found, call `ComputeRewardCommitment()` to reconstruct the leaves. For each leaf whose payout script passes `IsMine()`, record a pending pooled reward entry in the wallet database with the block hash, leaf index, amount, and maturity height (block height + COINBASE_MATURITY). Add a periodic check (on each new block) for entries that have reached maturity. For mature entries, automatically construct a claim transaction: create a spending input that references the payout commitment UTXO (the coinbase output), build the claim witness (Merkle branch, leaf index, leaf data, signature), set the output to send the leaf amount to a fresh wallet address, sign the signature using the payout script's key, and broadcast the transaction. Track the claim transaction in the wallet like any other outgoing transaction.

In `src/wallet/rpc/coins.cpp`, extend `getbalances` to include a `pooled` object:

    "pooled": {
      "pending": <amount>,
      "claimable": <amount>
    }

`pending` is the sum of all reward leaves addressed to the wallet that are in immature blocks. `claimable` is the sum of all reward leaves that have reached maturity but whose claim transactions have not yet confirmed. Once a claim transaction confirms, the amount moves from `pooled.claimable` to `mine.trusted`.


## Implementation Units

### Unit 1: Internal Miner Share Production

Goal: After activation, the internal miner produces shares continuously alongside its block search.

Requirements advanced: R3, R5, R10.

Dependencies: Plan 005 (ShareRecord type, sharechain store), Plan 004 (DEPLOYMENT_SHAREPOOL activation, SharePoolParams).

Files to modify:

- `src/node/internal_miner.cpp`
- `src/node/internal_miner.h`

Tests to add:

- `test/functional/feature_sharepool_mining.py` (new)

Approach: In the worker thread's nonce-grinding loop, after the existing block-target comparison, add a share-target comparison. The share target is computed from the consensus parameters (`SharePoolParams`) using the same difficulty-to-target conversion that `GetNextWorkRequired()` uses for block difficulty, but with a lower difficulty that produces solutions roughly every few seconds per thread (the exact rate is determined by the `target_share_spacing` parameter locked in Plan 002's simulator). When a hash meets the share target, the worker constructs a `ShareRecord` with the current block template's context (previous block hash, coinbase script, timestamp, nonce, etc.) and pushes it onto a per-worker share queue. The coordinator thread drains these queues, submits shares to the sharechain store, and relays them to peers.

Add `m_shares_found` as an `std::atomic<uint64_t>` to the `InternalMiner` class. Increment it each time the coordinator successfully submits a share. Report it alongside `m_blocks_found` in log output.

When a hash meets both the share target and the block target, the coordinator submits the block (existing path) and also records it as a share (new path). The share is submitted before the block to ensure the share window includes the block finder's work.

Test scenarios:

- On activated regtest, a single-threaded miner produces at least one share within the first 10 block intervals. The share is visible in the sharechain store.
- Two miners with 1 and 4 threads respectively both produce shares. The 4-thread miner produces approximately 4x as many shares as the 1-thread miner over a 50-block window.
- On non-activated regtest (default), no shares are produced. The miner searches only for full blocks.
- When a share also meets the block target, both a block and a share are recorded. The block's payout commitment includes the share.

### Unit 2: Mining RPC Extensions

Goal: Surface sharepool status and pending rewards through mining RPCs.

Requirements advanced: R3, R11.

Dependencies: Unit 1, Plan 005 (sharechain store queries).

Files to modify:

- `src/rpc/mining.cpp`

Tests to add:

- `test/functional/feature_sharepool_mining.py` (extend from Unit 1)

Approach: In the `getmininginfo` RPC handler (line 435 of `src/rpc/mining.cpp`), after assembling the existing result object, check whether `DEPLOYMENT_SHAREPOOL` is active at the current chain tip. If active, add four fields to the result: `sharepool_active` (boolean true), `share_tip` (the hex-encoded hash of the best share from the sharechain store), `pending_pooled_reward` (computed by scanning the last N immature blocks for payout commitment leaves matching the node's `-mineaddress` script, summing their amounts, and converting to RNG with 8 decimal places), and `accepted_shares` (the internal miner's `m_shares_found` counter). When sharepool is not active, omit these fields entirely rather than returning null or zero values.

Test scenarios:

- On activated regtest with mining enabled, `getmininginfo` returns all four new fields. `sharepool_active` is true, `accepted_shares` is greater than zero after mining a few blocks, and `pending_pooled_reward` is a positive number.
- On non-activated regtest, `getmininginfo` does not include the new fields (backward-compatible).
- `pending_pooled_reward` reflects the correct proportional amount. With two miners of equal hashrate, each sees approximately half the total pending reward.

### Unit 3: Wallet Payout Recognition and Tracking

Goal: The wallet recognizes payout commitment leaves addressed to local scripts and tracks pending vs claimable rewards.

Requirements advanced: R3, R11.

Dependencies: Plan 007 (ComputeRewardCommitment, payout commitment output format), Plan 005 (sharechain store for leaf reconstruction).

Files to modify:

- `src/wallet/wallet.cpp`
- `src/wallet/wallet.h`

Tests to add:

- `test/functional/feature_sharepool_wallet.py` (new)
- `src/wallet/test/wallet_tests.cpp` (extend)

Approach: Add a new data structure `PooledRewardEntry` to the wallet:

    struct PooledRewardEntry {
        uint256 block_hash;
        int block_height;
        uint32_t leaf_index;
        CAmount amount;
        CScript payout_script;
        int maturity_height;  // block_height + COINBASE_MATURITY
        bool claimed;         // true once claim tx is broadcast
        uint256 claim_txid;   // txid of the claim transaction, if any
    };

In the wallet's `blockConnected()` callback, after processing regular transactions, check whether the block is post-activation. If so, examine the coinbase for a witness v2 output (the payout commitment). If found, call `ComputeRewardCommitment()` to reconstruct the leaves. For each leaf, check `IsMine(leaf.payout_script)`. If the leaf belongs to the wallet, create a `PooledRewardEntry` and persist it. In `blockDisconnected()`, remove any `PooledRewardEntry` records for the disconnected block.

Add accessor methods: `GetPendingPooledReward()` returns the sum of amounts for entries where `block_height + COINBASE_MATURITY > current_tip_height` and `claimed == false`. `GetClaimablePooledReward()` returns the sum of amounts for entries where `block_height + COINBASE_MATURITY <= current_tip_height` and `claimed == false`.

Test scenarios:

- After mining 5 activated blocks with one miner, the wallet shows a non-zero pending pooled reward.
- The pending amount is proportional to the miner's share of total work.
- After a block reorganization, removed entries are deleted and re-added if the reorganized chain includes equivalent blocks.
- With two miners and unequal hashrate, each wallet shows a different pending amount corresponding to its share contribution.

### Unit 4: Automatic Claim Transaction Construction

Goal: The wallet automatically builds and broadcasts claim transactions once payout commitments mature.

Requirements advanced: R7, R11.

Dependencies: Unit 3, Plan 007 (claim witness format, VerifyMerkleBranch).

Files to modify:

- `src/wallet/wallet.cpp`
- `src/wallet/wallet.h`

Tests to add:

- `test/functional/feature_sharepool_wallet.py` (extend from Unit 3)

Approach: Add a method `BuildClaimTransaction()` that takes a `PooledRewardEntry` and produces a signed `CTransaction`. The method constructs a transaction input referencing the payout commitment UTXO (the coinbase output at the index where the witness v2 output lives). It builds the claim witness: compute the Merkle branch via `ComputeMerkleBranch()`, serialize the leaf data, sign the sighash with the key corresponding to the payout script, and assemble the witness stack as `[merkle_branch, leaf_index, leaf_data, signature]`. The transaction output sends the full leaf amount (minus a minimal fee) to a fresh wallet address.

Add a periodic claim check in the wallet's block processing. After connecting each new block, iterate `PooledRewardEntry` records. For any entry that has reached maturity (`block_height + COINBASE_MATURITY <= current_tip_height`) and is not yet claimed, call `BuildClaimTransaction()`, broadcast it, and mark the entry as claimed with the resulting txid.

Test scenarios:

- After mining enough blocks for the first payout commitment to mature (block height > 100 on activated regtest), the wallet automatically broadcasts a claim transaction.
- The claim transaction is accepted by the mempool and confirms in the next block.
- After the claim confirms, `getbalances` shows the claimed amount in `mine.trusted`, and it disappears from `pooled.claimable`.
- If the claim transaction is rejected (for any reason), the wallet logs the error and retries on the next block.
- Pre-activation blocks do not trigger any claim construction.

### Unit 5: Balance RPC Extensions and Documentation

Goal: Surface pooled reward status in wallet balance RPCs and update documentation.

Requirements advanced: R11.

Dependencies: Units 3 and 4.

Files to modify:

- `src/wallet/rpc/coins.cpp`
- `src/wallet/rpc/transactions.cpp`
- `README.md`

Tests to add:

- `test/functional/feature_sharepool_wallet.py` (extend from Units 3-4)

Approach: In `src/wallet/rpc/coins.cpp`, extend the `getbalances` RPC to include a `pooled` object in the response when sharepool is active. The object has two fields: `pending` (sum of immature pooled rewards in RNG) and `claimable` (sum of mature unclaimed pooled rewards in RNG). When sharepool is not active, the `pooled` object is omitted.

In `src/wallet/rpc/transactions.cpp`, annotate claim transactions in `listtransactions` and `gettransaction` with a `category` value of `"claim"` so they are distinguishable from regular sends and receives.

Update `README.md` to document the post-activation mining experience: shares are produced automatically, pending rewards accrue, claims are built automatically. Update the `-mineaddress` documentation to note its dual role as share payout destination after activation.

Test scenarios:

- `getbalances` on an activated network with pending rewards includes the `pooled` object with a positive `pending` value.
- After maturity, the `pending` value decreases and `claimable` increases by the same amount.
- After a claim confirms, the `claimable` value decreases and `mine.trusted` increases.
- On a non-activated network, `getbalances` does not include the `pooled` object.
- `listtransactions` shows claim transactions with `category: "claim"`.


## Concrete Steps

All commands run from the repository root.

1. After implementing all units, build the project:

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli test_bitcoin

   Expected: clean build with no new warnings.

2. Run the mining functional test:

       test/functional/test_runner.py feature_sharepool_mining.py

   Expected: the test starts two regtest miners with different thread counts on an activated network, verifies both produce shares, checks `getmininginfo` returns the new fields, and verifies pending rewards are proportional.

3. Run the wallet functional test:

       test/functional/test_runner.py feature_sharepool_wallet.py

   Expected: the test starts miners, waits for payout commitments to mature, verifies automatic claim transactions are broadcast and confirmed, and checks balance transitions from pending to claimable to trusted.

4. Run the wallet unit tests:

       build/src/test/test_bitcoin --run_test=wallet_tests

   Expected: existing wallet tests pass. New tests for pooled reward tracking pass.

5. Verify pre-activation behavior is preserved:

       test/functional/test_runner.py feature_sharepool_activation.py

   Expected: the activation test from Plan 004 still passes. Default regtest with no sharepool activation shows no share production, no pooled fields in RPCs, and no pooled balance.

6. Manual end-to-end verification on regtest:

       # Terminal 1: Miner A (4 threads)
       build/src/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0 \
         -mine -mineaddress=<addr-A> -minethreads=4 -datadir=/tmp/miner-a

       # Terminal 2: Miner B (1 thread)
       build/src/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0 \
         -mine -mineaddress=<addr-B> -minethreads=1 -datadir=/tmp/miner-b \
         -addnode=127.0.0.1:<port-A>

       # After ~20 blocks, check mining info on both:
       build/src/rng-cli -regtest -datadir=/tmp/miner-a getmininginfo
       build/src/rng-cli -regtest -datadir=/tmp/miner-b getmininginfo

   Expected: both show `sharepool_active: true`. Miner A shows ~4x the `accepted_shares` of Miner B. Both show non-zero `pending_pooled_reward`.

       # After 100+ blocks, check balances:
       build/src/rng-cli -regtest -datadir=/tmp/miner-a getbalances
       build/src/rng-cli -regtest -datadir=/tmp/miner-b getbalances

   Expected: `pooled.claimable` appears on both. Claim transactions have been broadcast. After confirmation, `mine.trusted` increases.

       # Check claim transactions:
       build/src/rng-cli -regtest -datadir=/tmp/miner-a listtransactions

   Expected: at least one transaction with `category: "claim"` appears.


## Validation and Acceptance

The implementation is accepted when all of the following are true.

Two miners with different thread counts (1 and 4 threads) both accumulate pending pooled rewards on an activated regtest network. The 4-thread miner's pending reward is approximately 4 times the 1-thread miner's pending reward over a 50-block window.

The wallet shows pending rewards before maturity (the `pooled.pending` field in `getbalances` is positive). After maturity, the pending amount transitions to claimable (the `pooled.claimable` field becomes positive).

The wallet automatically builds claim transactions that land successfully in the next block. After the claim confirms, the claimed amount appears in `mine.trusted` and disappears from `pooled.claimable`.

A low-resource miner (1 thread) participating at a much lower share rate than the dominant miner still receives a non-zero pending balance rather than nothing. This is the core product improvement over classical mining.

`getmininginfo` on an activated network returns `sharepool_active: true`, a non-zero `accepted_shares`, a valid `share_tip` hash, and a positive `pending_pooled_reward`.

On non-activated regtest (default), none of the new behavior is observable. Mining RPCs return their existing format. The wallet does not track pooled rewards.


## Idempotence and Recovery

All steps can be repeated from a clean build and fresh regtest datadirs. The functional tests create their own temporary datadirs. If the wallet database schema for `PooledRewardEntry` changes during development, delete the regtest datadir and rerun from scratch.

The automatic claim construction is idempotent: if the wallet crashes between building a claim and recording it as claimed, the next startup will detect unclaimed mature entries and rebuild the claim transactions. Duplicate claim attempts are safe because only one can spend each commitment UTXO; the second attempt will be rejected as a double-spend and the wallet will notice the first claim already confirmed.

If the internal miner produces shares that are rejected by the sharechain store (for example, due to a timestamp drift), the miner logs the rejection and continues. No crash or corrupt state results from rejected shares.


## Artifacts and Notes

The internal miner's modified worker loop pseudocode:

    while (!stop_signal) {
        hash = randomx_hash(header_with_nonce)
        if (hash <= share_target) {
            share = build_share_record(template, nonce, hash)
            push_to_coordinator(share)
            shares_found++
            if (hash <= block_target) {
                push_block_to_coordinator(template, nonce)
                blocks_found++
            }
        }
        nonce += stride
    }

The `getmininginfo` response after activation:

    {
      "blocks": 150,
      "difficulty": 1.23456,
      "networkhashps": 5000,
      "chain": "regtest",
      "sharepool_active": true,
      "share_tip": "abcdef1234...",
      "pending_pooled_reward": 12.50000000,
      "accepted_shares": 47
    }

The `getbalances` response after activation with pending rewards:

    {
      "mine": {
        "trusted": 100.00000000,
        "untrusted_pending": 0.00000000,
        "immature": 0.00000000
      },
      "pooled": {
        "pending": 25.00000000,
        "claimable": 12.50000000
      }
    }


## Interfaces and Dependencies

This plan depends on the following interfaces from earlier plans.

From Plan 004 (`src/consensus/params.h`):

    enum DeploymentPos { ..., DEPLOYMENT_SHAREPOOL, ... };

    struct SharePoolParams {
        uint32_t target_share_spacing;
        uint32_t reward_window_work;
        uint8_t claim_witness_version;
        uint16_t max_orphan_shares;
    };

From Plan 005 (`src/sharechain/share.h`):

    namespace sharechain {
    struct ShareRecord { ... };
    uint256 GetShareId(const ShareRecord&);
    } // namespace sharechain

From Plan 005, the sharechain store interface for submitting and querying shares:

    namespace sharechain {
    class ShareChainStore {
    public:
        bool AcceptShare(const ShareRecord& share);
        uint256 GetBestShareTip() const;
        std::vector<ShareRecord> GetRewardWindow(
            const CBlockIndex& prev_block) const;
    };
    } // namespace sharechain

From Plan 007 (`src/sharechain/payout.h`):

    namespace sharechain {
    struct RewardLeaf { ... };
    struct RewardCommitment { ... };
    RewardCommitment ComputeRewardCommitment(
        const std::vector<ShareRecord>& window,
        CAmount total_reward);
    std::vector<uint256> ComputeMerkleBranch(
        const std::vector<uint256>& leaf_hashes,
        size_t index);
    } // namespace sharechain

This plan introduces the following new interfaces.

In `src/node/internal_miner.h`, extend the `InternalMiner` class:

    class InternalMiner {
    public:
        // ... existing methods ...
        uint64_t GetSharesFound() const {
            return m_shares_found.load(std::memory_order_relaxed);
        }
    private:
        std::atomic<uint64_t> m_shares_found{0};
    };

In `src/wallet/wallet.h`, add pooled reward tracking:

    struct PooledRewardEntry {
        uint256 block_hash;
        int block_height;
        uint32_t leaf_index;
        CAmount amount;
        CScript payout_script;
        int maturity_height;
        bool claimed;
        uint256 claim_txid;
    };

    class CWallet {
    public:
        // ... existing methods ...
        CAmount GetPendingPooledReward() const;
        CAmount GetClaimablePooledReward() const;
    private:
        std::vector<PooledRewardEntry> m_pooled_rewards;
        RecursiveMutex cs_pooled_rewards;
        bool BuildClaimTransaction(const PooledRewardEntry& entry,
                                    CTransactionRef& tx_out);
    };

In `src/rpc/mining.cpp`, extend `getmininginfo`:

    getmininginfo

    Result (post-activation, additional fields):
    {
      ...
      "sharepool_active": true,
      "share_tip": "hex",
      "pending_pooled_reward": n.nnnnnnnn,
      "accepted_shares": n
    }

In `src/wallet/rpc/coins.cpp`, extend `getbalances`:

    getbalances

    Result (post-activation, additional object):
    {
      "mine": { ... },
      "pooled": {
        "pending": n.nnnnnnnn,
        "claimable": n.nnnnnnnn
      }
    }

This plan does not specify any QSB interface because those source files were not present in the inspected checkout. All planned changes here are additive to the verified internal miner, mining RPCs, wallet, and balance RPC surfaces.
