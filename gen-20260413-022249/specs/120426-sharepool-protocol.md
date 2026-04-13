# Specification: Sharepool Protocol (Planned)

## Objective

Define the protocol-native pooled mining system planned for RNG: public sharechain, deterministic reward windows, compact payout commitments in coinbase, and trustless post-maturity claim transactions. This spec describes the **intended future design** as laid out in the planning corpus (plans 001–012). No sharepool code exists in the current codebase.

## Evidence Status

### Verified Facts (grounded in live codebase)

- **No sharepool code exists**: No `ShareRecord`, sharechain store, share relay messages, payout commitment, or claim program implementation in the inspected checkout
- **No `DEPLOYMENT_SHAREPOOL`**: Only `DEPLOYMENT_TESTDUMMY` and `DEPLOYMENT_TAPROOT` defined in `src/consensus/params.h`
- **No share-related RPCs**: `submitshare`, `getsharechaininfo`, `getrewardcommitment` do not exist in `src/rpc/`
- **BIP9 machinery exists**: Version-bits deployment infrastructure is present and proven (used for Taproot activation from genesis)
- **Witness versions**: SegWit and Taproot (witness v0 and v1) are active from genesis; witness v2–v16 are reserved for future soft forks
- **Coinbase maturity**: 100 blocks (consensus rule, unchanged) — relevant because claims would depend on this
- **Internal miner**: Exists in `src/node/internal_miner.cpp` — the system that would be extended to produce shares
- **Block template assembly**: Standard Bitcoin-derived `CreateNewBlock()` in `src/node/miner.cpp` — would need to include payout commitments

### Recommendations (from planning corpus — plans 001–012)

**Share Object (`ShareRecord`)**:
- Fields: `parent_share`, `prev_block_hash`, `candidate_header_hash`, `nTime`, `nBits`, `nNonce`, `payout_script`
- Identified by SHA256 hash (`share_id`)
- Proves RandomX work below block target but above a lower share target
- Serializes with Bitcoin-standard methods

**Sharechain**:
- Ordered chain of accepted shares linked by parent references
- Separate from blockchain but anchored to it (shares reference the most recent known block)
- Tip selection: highest cumulative work
- Bounded orphan buffer: max 64 shares for shares whose parent hasn't arrived
- Persistent storage: LevelDB (plan 005)

**Share Target**:
- Proposed: `block_target / 12` (yields ~10-second share spacing at 120-second block time)
- This means ~12 shares per block on average

**Reward Window**:
- Trailing range of accepted shares whose cumulative work reaches a threshold
- Proposed: ~720 shares (~1 hour at 10-second share spacing)
- Boundaries reset with each block
- All shares in window contribute to that block's payout split

**Payout Commitment**:
- Merkle tree built from reward leaves (one per unique payout script in window)
- Each leaf: `payout_script`, `amount_in_roshi`, `first_share_id`, `last_share_id`
- Leaves sorted by `SHA256(payout_script)` for determinism
- Root encoded in coinbase as witness v2 OP_RETURN output: `OP_RETURN <4-byte-tag> <32-byte-root>`
- Consensus rule: blocks must include correct commitment root when sharepool is active

**Claim Spend**:
- New witness program version 2 (32-byte program = commitment root)
- Witness stack: `[merkle_branch] [leaf_index] [leaf_data] [signature]`
- Verifier reconstructs root from leaf + branch, compares to program, checks signature against payout script
- No new opcodes — soft-fork compatible
- Spendable only after 100-block coinbase maturity

**Activation**:
- Version-bits (BIP9) deployment `DEPLOYMENT_SHAREPOOL`
- Proposed BIP9 period: 2016 blocks, threshold: 1815 (95%)
- Mainnet: dormant (`NEVER_ACTIVE`) until explicitly activated
- Regtest: immediately activatable via `-vbparams=sharepool:0:9999999999:0`

**P2P Relay**:
- Three new message types: `shareinv`, `getshare`, `share`
- Peer-to-peer relay with no central gatekeeper
- Share admission: valid PoW proof required

**Internal Miner Extension (plan 008)**:
- Workers compute two targets per hash: share target and block target
- Share-meeting hash → construct `ShareRecord`, store, relay
- Block-meeting hash → submit block (also counts as share)
- New counter: `m_shares_found`

**Wallet Extension (plan 008)**:
- Scan coinbase for witness v2 commitment outputs
- For each matching payout script: record `PooledRewardEntry` in wallet DB
- `GetPendingPooledReward()`: sum immature entries
- `GetClaimablePooledReward()`: sum mature entries
- Auto-build claim transactions on maturity
- `getbalances` extended: `{ "pooled": { "pending": ..., "claimable": ... } }`

**New RPCs (plan 008)**:
- `submitshare <hex>`: accept share, validate, store, relay
- `getsharechaininfo`: tip, height, orphan count, reward window size
- `getrewardcommitment <blockhash>`: commitment root, leaves, amounts
- `getmininginfo`: extended with `sharepool_active`, `share_tip`, `pending_pooled_reward`, `accepted_shares`

### Hypotheses / Unresolved Questions

- **Exact constants**: Share spacing (10s), window size (720 shares), share target ratio (block/12) are all proposed starting values pending simulator validation (plan 002/003 decision gate)
- **Withholding resistance**: Plan 002 simulator must verify withholding advantage < 5%; if breached, options include finder bonus, publication incentive, or protocol redesign
- **Reward variance**: Target is CV < 10% for a 10% hashrate miner over 100 blocks — unvalidated until simulator runs
- **Commitment size**: Must fit under 100 bytes per the plan 003 threshold
- **Eclipse attack resilience**: Untested until devnet adversarial testing (plan 011)
- **Minimum viable peer count**: Unknown until relay viability measurement (plan 006)
- **QSB compatibility**: If QSB is merged before sharepool, interaction between QSB spending rules and claim transactions must be analyzed

## Acceptance Criteria

These criteria apply to the **planned implementation**, not the current codebase. They will become testable as code lands.

- `DEPLOYMENT_SHAREPOOL` is defined in `src/consensus/params.h` with appropriate BIP9 parameters
- Sharepool activation is controllable on regtest via `-vbparams`
- Share records contain valid RandomX proofs below share target
- The sharechain maintains an ordered chain of shares linked by parent references
- Orphan shares are buffered (max 64) until their parent arrives
- The reward window computes proportional payouts: miner with X% of work receives ~X% of reward
- Payout commitment roots are deterministic — any node reconstructing the same reward window produces the same root
- Coinbase includes the correct commitment root in a witness v2 OP_RETURN output
- Blocks without valid commitment root are rejected when sharepool is active
- Claim transactions using witness v2 programs are valid only after 100-block coinbase maturity
- Claim witness verification reconstructs the Merkle root and checks signature against payout script
- `submitshare` validates PoW and rejects shares that don't meet share target
- `getsharechaininfo` returns current sharechain state
- `getbalances` includes `pooled.pending` and `pooled.claimable` fields when sharepool is active
- Solo mining (one miner) produces single-leaf commitment with full reward — no special-casing required
- Pre-activation nodes treat sharepool commitment and claim outputs as valid (soft-fork compatibility)

## Verification

Verification commands will become applicable when sharepool code lands. The planned verification sequence from the corpus:

```bash
# Plan 009: Regtest end-to-end proof

# Start regtest with sharepool activated
rngd -regtest -vbparams=sharepool:0:9999999999:0

# Two miners: 1 thread (miner A) and 4 threads (miner B)
# Mine 50+ activated blocks
# Verify: reward ratio within 70-95% stochastic tolerance for 4:1 thread ratio
# Verify: observer independently computes same commitment roots
# Verify: claims succeed after maturity
# Verify: solo mining produces valid single-leaf commitment

# Plan 002: Simulator validation (prerequisite)
cd contrib/sharepool
python3 simulate.py --scenario baseline
# Verify: withholding advantage < 5%
# Verify: reward variance CV < 10%
# Verify: commitment size < 100 bytes
```

## Open Questions

1. **Simulator-first gate**: Plans 002/003 must validate economics before any consensus code is written. Has the simulator been started?
2. **Finder bonus**: Should the block finder receive a bonus above proportional share? If so, what percentage? The plan flags this as a possible future enhancement.
3. **Share relay bandwidth**: On a 4-node network with 10-second share spacing, is the bandwidth overhead acceptable (plan 006 target: < 10 KB/s per node)?
4. **Relay latency**: Plan 006 targets < 5s p50, < 10s p99 for share propagation — is this achievable on a geographically distributed small network?
5. **Orphan rate**: Plan 006 targets < 20% orphan rate — what is the expected rate given RNG's small peer count?
6. **Witness version 2 allocation**: Is v2 confirmed for sharepool claims, or should a different version be considered?
7. **Hard fork boundary**: The plan asserts no hard fork is needed. Is this confirmed for all edge cases (e.g., what happens to pre-activation nodes when they encounter commitment outputs)?
8. **QSB interaction**: If QSB operator support lands first, do QSB spending rules affect claim transaction validation?
9. **Scope boundary**: Agent integration features (MCP, `createagentwallet`, swarm autonomy) documented in `specs/agent-integration.md` are explicitly out of scope until pooled mining is proven. Is this boundary respected in all planning documents?
