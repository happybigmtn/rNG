# Design Review

## Applicability

RNG has inherited Qt GUI sources from Bitcoin Core (`src/qt`) and an optional `rng-qt` build target (`BUILD_GUI`), but the current sharepool planning scope does not add a bespoke GUI, web app, explorer, or visual sharepool interface. The user-facing surfaces for the planned sharepool work are:

1. **CLI binaries**: `rngd`, `rng-cli`, `rng-tx`, `rng-util`, `rng-wallet`
2. **RPC API**: JSON-RPC over HTTP, bound to localhost by default
3. **P2P protocol**: Peer-to-peer networking on port 8433
4. **Operator scripts**: Bash scripts for deployment, mining, health checks
5. **Configuration**: `rng.conf` file and command-line flags

This design review focuses on information architecture, state coverage, naming, error clarity, accessibility of terminal/API surfaces, and operator/developer experience for these surfaces. Visual design for a new sharepool UI is not applicable. If the project intends `rng-qt` to remain supported after wallet pooled-balance work lands, a later GUI-specific pass should verify that pooled pending/claimable balances are represented in Qt wallet views rather than only in `getbalances`.

## Information Architecture

### Current state (pre-activation)

The RPC surface is inherited from Bitcoin Core v30.2 and is well-organized by category: blockchain, mining, wallet, network, raw transactions, utility. RNG adds `getinternalmininginfo` (mining category) and the QSB operator RPCs (`submitqsbtransaction`, `listqsbtransactions`, `removeqsbtransaction`).

The naming is consistent: verbs for mutations (`sendtoaddress`, `submitblock`), `get*` for reads (`getblockchaininfo`, `getbalances`). The QSB RPCs follow this pattern cleanly.

### Planned sharepool surface

The planned RPCs (`submitshare`, `getsharechaininfo`, `getrewardcommitment`) follow the same naming convention. The `get*` prefix for reads and bare verbs for mutations is preserved.

**Concern**: `getbalances` currently returns `{ mine: { trusted, untrusted_pending, immature }, watchonly: { ... } }`. Adding `pooled: { pending, claimable }` extends this cleanly. However, the wallet must clearly distinguish immature coinbase outputs (which the miner earned by finding a block) from pooled claimable amounts (which the miner earned by contributing shares). After activation, both exist simultaneously for the block finder.

### Mining state machine

The mining flow has four states:

| State | Condition | User sees |
|-------|-----------|-----------|
| Not mining | `-mine` not set | `getinternalmininginfo` returns `{ running: false }` |
| Mining, pre-activation | `-mine` set, sharepool not active | Classical mining; block finder gets full reward |
| Mining, post-activation | `-mine` set, sharepool active | Dual-target mining; shares accrue, blocks commit settlement |
| Backoff | Not enough peers or repeated failures | Exponential backoff, logged prominently |

After activation, `getinternalmininginfo` should add: `sharepool_active`, `shares_found`, `share_tip`, `pending_pooled_reward`. The transition from pre-activation to post-activation mining should be invisible to the operator (no config change needed) and logged clearly.

## State Coverage

### Empty states
- **No shares in reward window**: The solo case. One miner fills the window. Settlement is a single leaf. This is handled correctly in the current miner_tests.
- **No miners at all**: Blocks cannot be produced. This is the existing Bitcoin Core behavior when no miner is running.
- **Sharepool active but no shares relayed**: The block assembler must still produce a valid settlement commitment using whatever shares it has (possibly just its own).

### Error states
- **Invalid share received**: P2P relay scores misbehavior, rejects share, does not relay.
- **Orphan share received**: Buffered (max 64), parent requested. If buffer full, oldest orphan evicted.
- **Claim for already-claimed leaf**: Consensus rejects (status flag already `1`). Mempool should also reject.
- **Claim with wrong amount**: Consensus rejects (value conservation check fails).
- **Settlement output spent before maturity**: Standard coinbase maturity rule applies.
- **No fee input for claim**: Transaction is invalid (fees must come from non-settlement inputs).

### Loading states
- **IBD (initial block download)**: Mining does not start until IBD completes and at least one peer is connected. Share relay should also be suppressed during IBD.
- **Sharechain syncing**: After activation, a newly joining node must catch up on recent shares. This is analogous to mempool sync -- the node requests missing shares from peers as they arrive.

## User Journeys

### Small CPU miner (primary persona)

1. Install RNG from release tarball or Docker
2. Load bootstrap (`scripts/load-bootstrap.sh`)
3. Create wallet: `rng-cli createwallet miner`
4. Get address: `rng-cli -rpcwallet=miner getnewaddress`
5. Start mining: add `-mine -mineaddress=<addr> -minethreads=4` to `rng.conf`
6. Restart daemon
7. Check status: `rng-cli getinternalmininginfo`
8. After activation: shares begin accruing. `rng-cli getbalances` shows `pooled.pending`.
9. After 100-block maturity: `pooled.claimable` appears. Wallet auto-claims.

**Key UX requirement**: Steps 1-7 should work identically before and after activation. The miner does not need to know about sharepool to start earning. The transition is protocol-level, not operator-level.

### Operator (fleet manager)

1. Deploy `rngd` binaries to fleet (via scripts or Docker)
2. Configure `rng.conf` per host
3. Monitor: `rng-cli getblockchaininfo`, `rng-cli getinternalmininginfo`, `rng-cli getpeerinfo`
4. After activation: additionally monitor `rng-cli getsharechaininfo`
5. Health check: `scripts/doctor.sh` should add sharepool health checks after activation

### Developer (contributor)

1. Clone repo, build: `cmake -B build && cmake --build build -j$(nproc)`
2. Run regtest with activation: `rngd -regtest -vbparams=sharepool:0:9999999999:0`
3. Mine blocks to activate
4. Run sharepool tests: `python3 test/functional/feature_sharepool_relay.py --configfile=build/test/config.ini`
5. Inspect settlement: `rng-cli -regtest getrewardcommitment <blockhash>`

## Naming and Terminology

### Good naming in current codebase
- `ShareRecord`, `SharechainStore`, `SettlementLeaf`, `SettlementDescriptor` -- clear, descriptive
- `shareinv`/`getshare`/`share` P2P messages -- follows Bitcoin's `inv`/`getdata`/`tx` pattern
- `payout_script`, `amount_roshi`, `claim_status_root` -- self-documenting

### Naming concerns
- **"roshi"**: The codebase uses `amount_roshi` as the smallest unit (like satoshi for Bitcoin). This term appears in specs and code but is never defined in a user-facing document. A brief note in README or help text would help.
- **"settlement"**: Technically accurate but potentially confusing for miners used to pool "payouts." The spec correctly uses "claim" for the user action and "settlement" for the on-chain state machine.
- **`RNGS` commitment tag**: Optional OP_RETURN marker. If kept, its discoverability vs the actual settlement output should be clearly documented to avoid confusion.

## Accessibility

There is no new visual UI to audit for color contrast, keyboard focus, or responsive layout. For the inherited Qt GUI, accessibility should be handled in a future Qt-specific pass if pooled balances are surfaced there. For this planning scope, accessibility means CLI/RPC clarity: terminal output should be plain text, parseable where appropriate, and not rely on color alone. The operator scripts use standard terminal conventions. Error messages in `scripts/doctor.sh` are clear and actionable.

## Responsive Behavior

Not applicable for the planned CLI/RPC sharepool work. The RPC API is stateless and works identically across clients. If `rng-qt` wallet support is later updated, responsive/resize behavior belongs in that follow-up GUI pass.

## AI-Slop Risk

The specs and documentation in this repo are notably free of AI-generated filler. The PLANS.md standard explicitly requires concrete evidence, exact commands, and observable outcomes. The EXECPLAN.md contains 63.8 KB of detailed decision logs with real transaction IDs, binary hashes, and fleet hostnames. The risk of AI-slop in the documentation is low.

However, the nine `specs/120426-*.md` files carry a dated prefix and some contain stale claims (e.g., DNS seeds, `fPowAllowMinDifficultyBlocks` interpretation). These should be refreshed or clearly labeled as historical.

## Design Recommendations

1. **Add `--sharepool` status to `scripts/doctor.sh`**: After activation, the health check script should report sharepool activation state, share tip, orphan count, and pending pooled reward alongside existing chain and miner health.

2. **Define "roshi" in user-facing help**: Add a brief note to `rng-cli help getbalances` or similar that 1 RNG = 100,000,000 roshi.

3. **Log sharepool activation clearly**: When the BIP9 deployment transitions from `started` to `locked_in` to `active`, the daemon should log a prominent message so operators know the transition happened.

4. **Make pending reward visible early**: A miner contributing shares should see `pooled.pending` in `getbalances` within the first few minutes of mining. This is the "meaningful success moment" for the small-miner persona.

5. **Document the solo-mining degenerate case explicitly in help text**: So a miner running alone does not wonder why their "pooled" reward equals the full block reward.
