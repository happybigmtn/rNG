# Sharechain Data Model, Storage, and P2P Relay

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.


## Purpose / Big Picture

After this work lands, RNG nodes will be able to create, validate, persist, relay, and query lower-difficulty mining proofs called "shares." A share is a RandomX proof of work that meets a target easier than the full block target but hard enough to prove that the miner contributed meaningful work. Shares form their own chain (the "sharechain"), separate from the block chain but anchored to it through block-hash references. Two connected regtest nodes will relay shares to each other, agree on the same best share tip, store accepted shares persistently across restarts, and expose the sharechain state through new and extended RPCs.

Before this plan, RNG nodes have no concept of sub-block-difficulty proofs, no share data type, no share storage, and no share relay. After this plan, the sharechain infrastructure exists as a complete, testable subsystem gated behind the `DEPLOYMENT_SHAREPOOL` activation flag from Plan 004. No payout commitment, claim program, or coinbase modification happens in this plan. Those are Plan 007's scope.

A developer can verify the work by starting two activated regtest nodes, mining a share on one, and observing that it propagates to the other. Calling `getsharechaininfo` on both nodes should return the same best share tip. Calling `submitshare` with an invalid proof should return a rejection. Calling `getblocktemplate` on an activated node should include a `sharepool` section with the current share tip and share target.


## Requirements Trace

`R2`. The pooled reward contract must be derived from a public share history. Any fully validating RNG node must be able to replay the same accepted share window and derive the same per-block payout commitment without a privileged coordinator. This plan advances R2 by making shares public, persistent, and independently verifiable objects.

`R4`. Share admission and share propagation must be peer-to-peer. The protocol must not rely on one operator-managed pool server. This plan advances R4 by adding P2P share relay messages and handling them in `src/net_processing.cpp`.

`R5`. Block construction must remain miner-built. The protocol may extend `getblocktemplate` with sharepool context. This plan advances R5 by extending `getblocktemplate` with a `sharepool` section and adding `submitshare` so miners submit shares through existing RPC infrastructure.

`R10`. Solo mining must remain possible as a special case of pooled mining. This plan advances R10 because a solo miner's shares form a valid sharechain even when no other miners contribute.

`R8`. Before activation, existing RNG behavior remains unchanged. This plan preserves R8 by gating all share handling behind `IsActiveAfter(DEPLOYMENT_SHAREPOOL)`.

These requirements come from the master requirement set in `docs/rng-protocol-native-pooled-mining-execplan.md`.


## Scope Boundaries

This plan does not implement payout commitment computation, claim spends, or any coinbase modification. Blocks mined after this plan still pay the full reward to the block finder. The payout machinery is Plan 007's scope.

This plan does not implement wallet integration for pending reward tracking. The wallet is unaware of shares.

This plan does not modify the internal miner to produce shares automatically. The internal miner continues searching only for full blocks. Plan 008 will teach it to submit shares.

This plan does not define the final economic constants (share target spacing, reward window work). It uses placeholder values from Plan 004's `SharePoolParams` struct and accepts whatever the simulator (Plan 002-003) eventually decides. The share validation logic uses the declared `nBits` in the share header and checks that the RandomX hash meets that target, regardless of the specific constant.

This plan does not depend on any parallel QSB work described in the local root `EXECPLAN.md`. The inspected checkout did not contain the QSB source files named there, so this plan should be implemented against verified sharepool seams only. If QSB code lands later, re-evaluate the block-assembly interaction then.

This plan does not add share compact-block relay, share header-first sync, or any optimization beyond basic inventory-based relay. Those can be added later if Plan 006 (the relay viability decision gate) identifies bandwidth or latency problems.


## Progress

- [ ] Create `src/sharechain/share.h` and `src/sharechain/share.cpp` with `ShareRecord` struct, `GetShareId()`, `GetShareWork()`, and serialization.
- [ ] Create `src/sharechain/store.h` and `src/sharechain/store.cpp` with persistent share storage.
- [ ] Create `src/sharechain/window.h` and `src/sharechain/window.cpp` with reward window reconstruction.
- [ ] Add new P2P message types to `src/protocol.h`.
- [ ] Add share relay handling to `src/net_processing.cpp`.
- [ ] Add `submitshare` and `getsharechaininfo` RPCs to `src/rpc/mining.cpp`.
- [ ] Extend `getblocktemplate` with `sharepool` section in `src/rpc/mining.cpp`.
- [ ] Wire sharechain initialization in `src/init.cpp`.
- [ ] Update `src/CMakeLists.txt` with new source files.
- [ ] Add unit tests in `src/test/sharechain_tests.cpp`.
- [ ] Add functional test `test/functional/feature_sharepool_relay.py`.
- [ ] Verify all existing tests still pass.


## Surprises & Discoveries

No entries yet. This section will be updated as implementation proceeds.


## Decision Log

- Decision: Store shares in a dedicated LevelDB database under the node's datadir, separate from the block index and chainstate.
  Rationale: Shares are not transactions and do not belong in the mempool. They are not blocks and do not belong in the block index. A separate store keeps the sharechain lifecycle independent: it can be wiped without affecting block validation, and it can be compacted or pruned on a different schedule. LevelDB is already a dependency of Bitcoin Core (used for chainstate and block index) so no new library is needed.
  Date/Author: 2026-04-12 / Plan author

- Decision: Use three new P2P message types (`shareinv`, `getshare`, `share`) rather than overloading the existing `inv`/`getdata` messages with a new inventory type.
  Rationale: The existing `inv` and `getdata` messages use the `GetDataMsg` enum with well-defined type codes. Adding a share type would require updating every switch statement and every helper (`IsGenTxMsg`, `IsGenBlkMsg`, `GetMessageType`) throughout the codebase. Dedicated messages are cleaner, easier to gate behind the activation check, and easier to remove if the protocol is revised. The trade-off is that shares do not benefit from existing inv batching and relay scheduling, but for a 4-10 node network this is acceptable.
  Date/Author: 2026-04-12 / Plan author

- Decision: The share target for validation purposes is the `nBits` declared in the share header, not a global constant from `SharePoolParams`.
  Rationale: The share difficulty should adapt over time as the network grows. The `SharePoolParams::target_share_spacing` constant controls how often shares should appear on average, and the actual per-share target will eventually be derived from recent share history (analogous to how block difficulty is derived from recent block history). For this plan, we validate that the declared work meets the declared target. The difficulty adjustment algorithm for shares will be added in a follow-up once the economic simulator (Plans 002-003) settles the formula.
  Date/Author: 2026-04-12 / Plan author

- Decision: The share's `candidate_header_hash` field references the block header the miner was working on when it found the share, but the share itself is not a block header.
  Rationale: A share must prove that the miner was doing useful work toward block production. The candidate header hash links the share to a specific block template context. If the share also meets the full block difficulty, the miner submits a block as well. The share and the block are separate objects even when the same RandomX nonce satisfies both targets.
  Date/Author: 2026-04-12 / Plan author

- Decision: Orphan shares (shares whose parent is unknown) are held in a bounded in-memory buffer and discarded if the parent does not arrive within `max_orphan_shares` entries.
  Rationale: This mirrors Bitcoin Core's orphan transaction handling. Without a bound, a malicious peer could flood orphan shares to exhaust memory. The `max_orphan_shares` limit from `SharePoolParams` provides a consensus-defined bound.
  Date/Author: 2026-04-12 / Plan author


## Outcomes & Retrospective

No entries yet. This section will be updated at milestones and at completion.


## Context and Orientation

This plan builds on Plan 004 (Sharepool Version-Bits Deployment Skeleton), which adds `DEPLOYMENT_SHAREPOOL` to the version-bits machinery and introduces the `SharePoolParams` struct in `Consensus::Params`. Plan 004 must be completed before this plan begins.

This section orients a novice to the six code areas this plan touches, plus the new `src/sharechain/` subtree it creates.

`src/protocol.h` defines the P2P message types that RNG nodes exchange. Each message type is a short string constant in the `NetMsgType` namespace (such as `"inv"`, `"tx"`, `"block"`). The `ALL_NET_MESSAGE_TYPES` array lists every known message for protocol-level validation. The `GetDataMsg` enum defines inventory object types (MSG_TX, MSG_BLOCK, etc.) used in `inv` and `getdata` messages. This plan adds three new message-type constants (`SHAREINV`, `GETSHARE`, `SHARE`) to the namespace and the array.

`src/net_processing.cpp` handles incoming P2P messages. It contains a large `ProcessMessage` function (or equivalent dispatcher) that switches on the message type string and executes the appropriate handler. This plan adds handlers for the three new share messages. The handler for `shareinv` checks whether the node already has the announced share and, if not, sends a `getshare` request. The handler for `share` validates the received share and either accepts it into the sharechain store or rejects it. The handler for `getshare` looks up the requested share and sends it back.

`src/rpc/mining.cpp` exposes mining-related RPCs such as `getblocktemplate`, `submitblock`, and `getmininginfo`. This plan adds two new RPCs (`submitshare` and `getsharechaininfo`) and extends `getblocktemplate` with a `sharepool` section when the deployment is active.

`src/init.cpp` handles node startup and shutdown. It creates and destroys global subsystems like the mempool, block index, and peer manager. This plan adds sharechain store initialization during startup and cleanup during shutdown.

`src/pow.cpp` contains the RandomX proof-of-work validation functions. The function `CheckProofOfWorkImpl` verifies that a hash meets a target specified by `nBits`. This plan reuses that function to validate share work proofs. The function `GetBlockPoWHash` computes the RandomX hash of a block header given a seed hash. Shares need an analogous function that computes the RandomX hash of the share's candidate context.

`src/CMakeLists.txt` lists all source files for the build. New `.cpp` files in `src/sharechain/` must be added here.

The new `src/sharechain/` subtree created by this plan contains three modules:

`share.h` / `share.cpp` defines the `ShareRecord` struct and its core operations. A `ShareRecord` has seven fields: `parent_share` (the share ID of the previous share in the sharechain, or a null hash for the genesis share), `prev_block_hash` (the hash of the most recent block the miner knew about when creating this share), `candidate_header_hash` (the hash of the block header the miner was working on), `nTime` (the share's timestamp as a Unix epoch second), `nBits` (the compact difficulty target the share claims to meet), `nNonce` (the RandomX nonce), and `payout_script` (the `CScript` that identifies the miner's payout destination). `GetShareId()` computes a deterministic identifier by hashing the serialized share. `GetShareWork()` converts the share's `nBits` into an `arith_uint256` work value for cumulative-work scoring.

`store.h` / `store.cpp` manages persistent share storage. The store wraps a LevelDB database and provides operations to add a validated share, look up a share by ID, enumerate shares in a range, and track the current best share tip (the share with the highest cumulative work). The store also maintains a bounded orphan buffer for shares whose parent has not yet arrived.

`window.h` / `window.cpp` reconstructs the reward window from the accepted sharechain. The reward window is a contiguous range of shares ending at the current share tip, bounded by a cumulative-work threshold defined in `SharePoolParams::reward_window_work`. This module provides a view of the window that later plans (Plan 007) will use to compute payout commitments. For this plan, the window module is tested in isolation but not yet consumed by block assembly.

Five terms specific to this plan:

A "share" is a proof of work that meets a lower difficulty than the full block target. It proves the miner performed real RandomX computation toward block production. Shares are relayed between peers and stored persistently.

A "sharechain" is a chain of shares linked by parent-share references. Each share points to one parent (or null for the first share). The best sharechain tip is the tip with the highest cumulative work, analogous to the best block chain tip.

A "share ID" is the SHA256d hash of a serialized `ShareRecord`. It uniquely identifies a share and is used for inventory announcements and lookups.

The "reward window" is the set of recent shares whose cumulative work falls within the `reward_window_work` threshold. It determines which miners are eligible for proportional reward in a given block.

An "orphan share" is a received share whose parent share is not yet known to the node. Orphan shares are buffered temporarily in case the parent arrives soon. If the parent does not arrive before the buffer fills, the orphan is discarded.


## Plan of Work

The work proceeds in five sequential phases.

Phase 1 creates the `src/sharechain/` subtree with the share data model. Create `src/sharechain/share.h` defining the `ShareRecord` struct with the seven fields described in Context and Orientation. Add `SERIALIZE_METHODS` for binary serialization following Bitcoin Core's serialization macros. Implement `GetShareId()` as `Hash(serialized_share)` using the existing `Hash()` function from `src/hash.h`. Implement `GetShareWork()` by converting `nBits` to a target via `arith_uint256::SetCompact()` and computing `powLimit / target` (the same formula used in `src/pow.cpp` for per-block difficulty). Create `src/sharechain/share.cpp` with the implementations.

Phase 2 creates the persistent store. Create `src/sharechain/store.h` and `src/sharechain/store.cpp`. The store wraps a `CDBWrapper` (the LevelDB wrapper used throughout Bitcoin Core, defined in `src/dbwrapper.h`). The database path is `<datadir>/sharechain/`. Key-value layout: share ID maps to serialized `ShareRecord`, a separate key tracks the best tip share ID, and cumulative work per share ID is stored for tip selection. The store provides `AddShare(ShareRecord)`, `GetShare(uint256 id)`, `GetBestTip()`, `HasShare(uint256 id)`, and an orphan buffer (`AddOrphan`, `GetOrphan`, `RemoveOrphan`, `OrphanCount`). The orphan buffer is in-memory only (a `std::map<uint256, ShareRecord>` bounded by `max_orphan_shares`).

Phase 3 creates the reward window module. Create `src/sharechain/window.h` and `src/sharechain/window.cpp`. The `RewardWindow` class takes a reference to the share store and a `SharePoolParams`. `GetWindow(uint256 tip_share_id)` walks backward from the tip, accumulating shares until the cumulative work in the window reaches `reward_window_work` or the chain is exhausted. It returns a `std::vector<ShareRecord>` ordered oldest to newest. This module is the foundation for payout computation in Plan 007 but is independently testable now.

Phase 4 adds P2P relay. In `src/protocol.h`, add three new message-type constants in the `NetMsgType` namespace: `SHAREINV` (announces one or more share IDs), `GETSHARE` (requests shares by ID), and `SHARE` (delivers a serialized `ShareRecord`). Add them to the `ALL_NET_MESSAGE_TYPES` array. In `src/net_processing.cpp`, add handlers gated on `IsActiveAfter(DEPLOYMENT_SHAREPOOL)`:

The `SHAREINV` handler receives a vector of share IDs. For each ID not already in the store or orphan buffer, the node sends a `GETSHARE` request to the announcing peer. The `GETSHARE` handler receives a vector of share IDs. For each ID present in the store, the node sends a `SHARE` message back. The `SHARE` handler receives a serialized `ShareRecord`. It validates the share: the declared `nBits` must produce a valid target, the RandomX hash of the share's candidate context must be below that target, the timestamp must be within a reasonable range (no more than two hours in the future, no earlier than the median time of the referenced block), and if the parent share is known, the share's cumulative work must exceed the parent's. If the parent is unknown, the share goes into the orphan buffer. On successful validation, the share is added to the store and a `SHAREINV` is broadcast to other connected peers.

Phase 5 adds RPCs and `getblocktemplate` extension. In `src/rpc/mining.cpp`, add `submitshare` which accepts a hex-encoded serialized share, validates it through the same logic as the P2P handler, and returns the share ID on success or a descriptive error on failure. Add `getsharechaininfo` which returns the best share tip ID, tip height (number of shares in the chain from genesis), and the current reward window size. Extend `getblocktemplate` so that when the sharepool deployment is active, the response includes a `sharepool` object with `share_tip` (best tip ID), `share_target` (current share difficulty target as a hex string), and `reward_window_shares` (number of shares in the current window).

Throughout all phases, update `src/CMakeLists.txt` to include the new `.cpp` files and wire the sharechain store initialization into `src/init.cpp`.


## Implementation Units

### Unit A: Share Data Model

Goal: Define the `ShareRecord` type with serialization, ID computation, and work scoring.

Requirements advanced: R2.

Dependencies on earlier units: Plan 004 (for `SharePoolParams` in `Consensus::Params`).

Files to create or modify:
- `src/sharechain/share.h` (create)
- `src/sharechain/share.cpp` (create)
- `src/CMakeLists.txt` (modify)

Tests to add or modify:
- `src/test/sharechain_tests.cpp` (create, partial)

Approach: Define the struct, implement serialization, implement `GetShareId()` and `GetShareWork()`. Add a unit test that constructs a `ShareRecord` with known values, serializes and deserializes it, and verifies the share ID is deterministic.

Specific test scenarios:
- Construct a `ShareRecord`, serialize it, deserialize the bytes into a second `ShareRecord`, and assert all fields match.
- Compute `GetShareId()` on the same share twice and assert the results are identical.
- Compute `GetShareWork()` for two shares with different `nBits` and assert the share with the harder target (lower `nBits` compact value) has higher work.

### Unit B: Persistent Store

Goal: Store and retrieve shares in a LevelDB database with best-tip tracking and a bounded orphan buffer.

Requirements advanced: R2.

Dependencies on earlier units: Unit A.

Files to create or modify:
- `src/sharechain/store.h` (create)
- `src/sharechain/store.cpp` (create)
- `src/CMakeLists.txt` (modify)

Tests to add or modify:
- `src/test/sharechain_tests.cpp` (extend)

Approach: Wrap `CDBWrapper` for the `<datadir>/sharechain/` path. Implement `AddShare`, `GetShare`, `HasShare`, `GetBestTip`, and the orphan buffer operations. Best-tip selection: when a new share is added, compute its cumulative work (parent cumulative work plus this share's work) and compare against the current best tip's cumulative work. If higher, update the tip.

Specific test scenarios:
- Add a share, retrieve it by ID, and assert the fields match.
- Add two shares forming a chain (share B references share A as parent). Assert the best tip is share B.
- Add an orphan share (parent unknown). Assert `HasShare` returns false but `OrphanCount` returns 1. Then add the parent. Assert the orphan is promoted to the store and `OrphanCount` returns 0.
- Fill the orphan buffer to `max_orphan_shares` and add one more orphan. Assert the oldest orphan is evicted and the buffer size remains at the limit.
- Restart the store (close and reopen the LevelDB). Assert that previously added shares are still retrievable and the best tip is preserved.

### Unit C: Reward Window

Goal: Reconstruct the reward window from the share store for later use by payout computation.

Requirements advanced: R2, R10.

Dependencies on earlier units: Unit B.

Files to create or modify:
- `src/sharechain/window.h` (create)
- `src/sharechain/window.cpp` (create)
- `src/CMakeLists.txt` (modify)

Tests to add or modify:
- `src/test/sharechain_tests.cpp` (extend)

Approach: Walk backward from the tip share, accumulating shares into the window until cumulative work reaches `reward_window_work` or the chain start is reached. Return the window as a vector ordered oldest first.

Specific test scenarios:
- Build a sharechain of 10 shares. Set `reward_window_work` high enough to include all 10. Call `GetWindow` and assert the returned vector has 10 entries ordered oldest to newest.
- Build a sharechain of 100 shares. Set `reward_window_work` to include only the last 20 shares' worth of work. Call `GetWindow` and assert the returned vector has approximately 20 entries (exact count depends on per-share work values).
- With only one share in the chain (genesis share), call `GetWindow` and assert the returned vector has 1 entry.

### Unit D: P2P Share Relay

Goal: Relay shares between peers using dedicated messages, with validation and orphan handling.

Requirements advanced: R4.

Dependencies on earlier units: Unit B.

Files to create or modify:
- `src/protocol.h` (modify)
- `src/net_processing.cpp` (modify)

Tests to add or modify:
- `test/functional/feature_sharepool_relay.py` (create)

Approach: Add the three message types to `src/protocol.h`. Add handlers in `src/net_processing.cpp` gated on the sharepool deployment being active. Share validation checks: valid `nBits`, RandomX hash below target, timestamp within range, parent exists or share goes to orphan buffer. On acceptance, broadcast `SHAREINV` to other peers.

Specific test scenarios:
- Two regtest nodes with sharepool active. Submit a valid share to node A via `submitshare`. After a short delay, call `getsharechaininfo` on node B and assert the best tip matches node A's best tip.
- Submit a share with `nBits` set to a target that the RandomX hash does not meet. Assert the share is rejected with a descriptive error mentioning insufficient work.
- Submit a share whose parent share ID does not exist on the receiving node. Assert the share is held as an orphan. Then submit the parent. Assert the orphan is promoted and the best tip updates.
- Submit shares exceeding the `max_orphan_shares` limit. Assert that old orphans are evicted and memory usage remains bounded (check `getsharechaininfo` orphan count).

### Unit E: Mining RPCs and Template Extension

Goal: Let miners submit shares and query sharechain state through RPCs.

Requirements advanced: R5, R10.

Dependencies on earlier units: Units B and D.

Files to create or modify:
- `src/rpc/mining.cpp` (modify)
- `src/init.cpp` (modify)

Tests to add or modify:
- `test/functional/feature_sharepool_relay.py` (extend)

Approach: Add `submitshare` RPC that accepts hex-encoded serialized share data, runs the same validation as the P2P handler, and returns a JSON object with the share ID on success or a validation error. Add `getsharechaininfo` RPC that returns best tip share ID, chain height, orphan count, and reward window size. Extend `getblocktemplate` to include a `sharepool` section when the deployment is active. In `src/init.cpp`, initialize the share store on startup (create the `<datadir>/sharechain/` directory and open the LevelDB) and shut it down cleanly on exit.

Specific test scenarios:
- On an activated regtest node, call `submitshare` with valid share hex and assert the response includes a `share_id` field.
- Call `submitshare` with malformed hex and assert the response is an RPC error with a descriptive message.
- Call `getsharechaininfo` after submitting three shares forming a chain. Assert `height` is 3 and `best_tip` matches the last share's ID.
- Call `getblocktemplate` on an activated node and assert the response includes a `sharepool` key with `share_tip`, `share_target`, and `reward_window_shares` fields.
- Call `getblocktemplate` on a non-activated node and assert the response does not include a `sharepool` key.


## Concrete Steps

All commands assume the working directory is the repository root. Plan 004 must be completed first.

Create the sharechain source directory:

    mkdir -p src/sharechain

After making all source edits described in the Plan of Work, build:

    cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON -DWITH_ZMQ=OFF -DENABLE_IPC=OFF
    cmake --build build --target rngd rng-cli test_bitcoin -j"$(nproc)"

Expected outcome: clean build with no new warnings.

Run existing tests to verify nothing regressed:

    ./build/bin/test_bitcoin --run_test=versionbits_tests
    python3 test/functional/feature_sharepool_activation.py --configfile=build/test/config.ini

Expected outcome: all Plan 004 tests still pass.

Run the new sharechain unit tests:

    ./build/bin/test_bitcoin --run_test=sharechain_tests

Expected outcome:

    Running N test cases...
    *** No errors detected

Run the new relay functional test:

    python3 test/functional/feature_sharepool_relay.py --configfile=build/test/config.ini

Expected outcome:

    feature_sharepool_relay.py passed

Manually exercise the RPCs on activated regtest:

    ./build/bin/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0
    ./build/bin/rng-cli -regtest getsharechaininfo

Expected outcome: JSON output with `best_tip`, `height`, `orphan_count`, and `reward_window_shares` fields. Initially `height` is 0 and `best_tip` is null.

Submit a share (after constructing one through test tooling or the simulator):

    ./build/bin/rng-cli -regtest submitshare <hex-encoded-share>

Expected outcome: JSON response with `share_id` field.

Check `getblocktemplate` includes sharepool section:

    ./build/bin/rng-cli -regtest getblocktemplate '{"rules":["segwit"]}'

Expected outcome: the response JSON includes a `sharepool` key.

Stop the test daemon:

    ./build/bin/rng-cli -regtest stop


## Validation and Acceptance

The implementation is accepted when all of the following are true:

Two activated regtest nodes relay a valid share and agree on the same best share tip. This is the core proof that the sharechain data model, storage, validation, and P2P relay work end to end.

A share with insufficient RandomX work is rejected by both the `submitshare` RPC and the P2P handler, with a descriptive error message.

An orphan share (whose parent is unknown) is stored temporarily and promoted to the accepted sharechain when the parent arrives. If the parent never arrives and the orphan buffer fills, old orphans are evicted.

`getblocktemplate` on an activated node includes a `sharepool` section with the current share tip, share target, and reward window parameters. On a non-activated node, `getblocktemplate` output is unchanged.

`submitshare` returns the share ID for a valid share and a descriptive rejection error for an invalid share.

All existing tests (unit and functional) pass without modification. The sharepool deployment skeleton from Plan 004 continues to work as before.

The sharechain store persists across node restart. Shares added before shutdown are retrievable after restart.


## Idempotence and Recovery

The sharechain LevelDB can be safely deleted and recreated from scratch by re-syncing shares from peers. This is analogous to deleting the chainstate and re-indexing. No irreplaceable data lives only in the sharechain store because shares are relayed between peers.

All source edits are additive. Rebuilding after making the same edit twice produces the same binary. New files in `src/sharechain/` do not conflict with any existing files.

If the share store becomes corrupted, the recovery path is to stop the node, delete the `<datadir>/sharechain/` directory, and restart. The node will re-request shares from peers via the normal relay protocol.

If a P2P message handler crashes during share validation, the node should disconnect the offending peer (standard Bitcoin Core behavior for malformed messages) without corrupting local state. The share store uses LevelDB's atomic write batches for all mutations.

Test datadirs are ephemeral. Each functional test run creates a fresh regtest datadir, so tests do not depend on prior state.


## Artifacts and Notes

Expected `ShareRecord` serialization layout (field order matches struct definition):

    [32 bytes: parent_share]
    [32 bytes: prev_block_hash]
    [32 bytes: candidate_header_hash]
    [4 bytes: nTime, little-endian uint32]
    [4 bytes: nBits, little-endian uint32]
    [4 bytes: nNonce, little-endian uint32]
    [varint + N bytes: payout_script, using CScript serialization]

Expected new entries in `src/protocol.h`:

    inline constexpr const char* SHAREINV{"shareinv"};
    inline constexpr const char* GETSHARE{"getshare"};
    inline constexpr const char* SHARE{"share"};

Expected `getsharechaininfo` output shape:

    {
      "best_tip": "abc123...",
      "height": 42,
      "orphan_count": 0,
      "reward_window_shares": 20,
      "deployment_active": true
    }

Expected `getblocktemplate` sharepool section shape:

    "sharepool": {
      "share_tip": "abc123...",
      "share_target": "00000fff...",
      "reward_window_shares": 20
    }

Expected `submitshare` success response:

    {
      "share_id": "def456...",
      "accepted": true
    }

Expected `submitshare` failure response:

    {
      "accepted": false,
      "reject_reason": "share-work-insufficient"
    }

LevelDB key layout for the sharechain store:

    Key prefix 's' + [32 bytes share_id] -> serialized ShareRecord
    Key prefix 'c' + [32 bytes share_id] -> [32 bytes cumulative_work as arith_uint256]
    Key 'T' (single byte) -> [32 bytes best_tip_share_id]


## Interfaces and Dependencies

This plan depends on Plan 004 for the `DEPLOYMENT_SHAREPOOL` enum value and the `SharePoolParams` struct in `Consensus::Params`.

This plan depends on the existing Bitcoin Core infrastructure: `CDBWrapper` from `src/dbwrapper.h` for LevelDB access, `SERIALIZE_METHODS` from `src/serialize.h` for binary serialization, `arith_uint256` from `src/arith_uint256.h` for cumulative work arithmetic, `Hash()` from `src/hash.h` for share ID computation, and `CheckProofOfWorkImpl()` from `src/pow.h` for validating that a share's RandomX hash meets its declared target.

This plan produces the following new interfaces that later plans will consume.

In `src/sharechain/share.h`, the `ShareRecord` struct is the canonical share data type used throughout the codebase. Plan 007 (payout commitment) reads `ShareRecord` objects from the reward window. Plan 008 (miner integration) constructs `ShareRecord` objects when the internal miner produces shares.

    namespace sharechain {

    struct ShareRecord {
        uint256 parent_share;
        uint256 prev_block_hash;
        uint256 candidate_header_hash;
        uint32_t nTime;
        uint32_t nBits;
        uint32_t nNonce;
        CScript payout_script;

        SERIALIZE_METHODS(ShareRecord, obj) {
            READWRITE(obj.parent_share, obj.prev_block_hash,
                      obj.candidate_header_hash, obj.nTime,
                      obj.nBits, obj.nNonce, obj.payout_script);
        }
    };

    uint256 GetShareId(const ShareRecord& share);
    arith_uint256 GetShareWork(const ShareRecord& share);

    } // namespace sharechain

In `src/sharechain/store.h`, the `ShareStore` class is the persistent backend. Plan 007 queries it to build the reward window. Plan 008 writes to it when the internal miner produces shares locally.

    namespace sharechain {

    class ShareStore {
    public:
        explicit ShareStore(fs::path path, size_t cache_bytes);
        ~ShareStore();

        bool AddShare(const ShareRecord& share);
        std::optional<ShareRecord> GetShare(const uint256& id) const;
        bool HasShare(const uint256& id) const;
        uint256 GetBestTip() const;
        arith_uint256 GetCumulativeWork(const uint256& id) const;

        void AddOrphan(const ShareRecord& share);
        std::optional<ShareRecord> GetOrphan(const uint256& id) const;
        void RemoveOrphan(const uint256& id);
        size_t OrphanCount() const;
    };

    } // namespace sharechain

In `src/sharechain/window.h`, the `RewardWindow` provides the ordered share list that Plan 007 uses to compute payout leaves.

    namespace sharechain {

    class RewardWindow {
    public:
        RewardWindow(const ShareStore& store,
                     const Consensus::SharePoolParams& params);

        std::vector<ShareRecord> GetWindow(const uint256& tip_id) const;
    };

    } // namespace sharechain

In `src/protocol.h`, the three new message types (`SHAREINV`, `GETSHARE`, `SHARE`) define the P2P surface for share propagation. Plan 006 (the relay viability decision gate) will measure the performance of these messages on a small test network.

In `src/rpc/mining.cpp`, the `submitshare` and `getsharechaininfo` RPCs and the `getblocktemplate` sharepool extension define the external mining surface. Plan 008 (miner integration) will use `submitshare` internally to feed shares from the internal miner into the store.

This plan does not modify any existing interface. The existing `inv`, `getdata`, `block`, and `tx` message handling remains unchanged. The existing `getblocktemplate` output is extended (new key added) but not altered (no existing keys removed or changed). All existing RPC calls continue to return the same results.
