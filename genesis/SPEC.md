# RNG System Specification

## Problem Statement

How might we let any CPU miner on a Bitcoin-derived RandomX chain earn proportional block rewards without trusting a pool operator, so that small miners can start accruing deterministic reward entitlement immediately and claim it trustlessly after maturity?

## System Identity

RNG is a Bitcoin Core v30.2-derived full node for the RNG mainnet, a live proof-of-work chain using RandomX for CPU-friendly block mining. The chain has been live since January 30, 2025. Committed root docs record the chain in the low-32,000 block range on 2026-04-13, with validator-02/04/05 healthy at height 32122 and validator-01 crash-looping on a zero-byte `settings.json`; this corpus review did not perform a fresh live-network probe.

The intended near-term direction, not current behavior, is protocol-native trustless pooled mining: after BIP9 activation of `DEPLOYMENT_SHAREPOOL`, every miner automatically participates in a deterministic reward-sharing protocol. The protocol smooths block-finder lumpiness by distributing each block's reward proportionally to recent public share work, and lets miners claim their committed share after coinbase maturity without any operator involvement.

## Current Behavior (Pre-Activation)

### Mining
A miner starts with `rngd -mine -mineaddress=<addr> -minethreads=N`. The internal miner spawns one coordinator thread and N worker threads. Workers grind RandomX nonces in lock-free stride-based loops. When a worker finds a hash meeting the block difficulty target, the coordinator submits the block. The block finder receives the entire block reward (subsidy + fees) in the coinbase output. No reward sharing occurs.

### Block production
Blocks target 120-second spacing on mainnet. Difficulty adjusts per-block using LWMA with a 720-block window and 60-timestamp outlier cut. RandomX uses a fixed genesis seed `"RNG Genesis Seed"` with Argon salt `"RNGCHAIN01"`. The subsidy starts at 50 RNG and halves every 2,100,000 blocks.

### Wallet
The wallet is the standard Bitcoin Core v30.2 descriptor-based SQLite wallet. It supports P2WPKH, P2WSH, and P2TR addresses with Bech32 HRP `rng`. The wallet tracks confirmed, unconfirmed, and immature balances. There is no concept of pooled reward.

### GUI
The repository includes inherited Qt wallet/node GUI code and an optional `rng-qt` target, but no sharepool-specific GUI is implemented or planned in the near-term critical path. Planned pooled-balance behavior is specified for RPC/wallet internals first; GUI parity is a future support question if Qt remains a first-class distribution surface.

### P2P
Peers communicate using Bitcoin-style P2P with RNG network magic `0xB07C010E` on port 8433. BIP324 encrypted transport is available from genesis. Four hardcoded seed IPs provide initial peer discovery.

### QSB (operator-only)
The QSB path lets operators create and mine quantum-safe transactions via local-only RPCs (`submitqsbtransaction`, `listqsbtransactions`, `removeqsbtransaction`). Enabled with `-enableqsboperator`. Not exposed to public relay. Proven on mainnet with canary funding/spend at heights 29946-29947.

## Planned Behavior (Post-Activation)

### Shares
The internal miner checks each RandomX hash against two targets: the block difficulty target and a lower share difficulty target (block target * 120 for 1-second shares on 120-second mainnet blocks). When a hash meets the share target but not the block target, the miner constructs a `ShareRecord` containing the candidate header, share nBits, parent share link, and payout script, then relays it via `shareinv`/`getshare`/`share` P2P messages.

### Reward window
For each block, the assembler walks backward through accepted shares from the best share tip, accumulating share work until reaching 7200 target-spacing shares of work. Shares in the window are aggregated by payout script. Each script's share of the total reward is `floor(total_reward * script_work / window_work)`, with deterministic remainder distribution.

### Payout commitment
The block assembler builds sorted payout leaves (by `Hash(payout_script)`) into a binary Merkle tree. The payout root, together with an initial all-unclaimed claim-status root, forms the settlement state hash. The coinbase contains a witness-v2 settlement output: `OP_2 <state_hash>` with `nValue = total_reward`.

### Claims
After 100-block coinbase maturity, any party can construct a claim transaction that:
1. Spends the current settlement output (input 0)
2. Pays exactly one committed leaf's amount to that leaf's payout script (output 0)
3. Creates a successor settlement output with updated claim-status root (output 1, unless final claim)
4. Funds fees from non-settlement inputs

The claim witness stack is: settlement descriptor, leaf index, leaf data, payout branch, status branch. No inner signature is needed because the payout destination is consensus-locked.

### Wallet integration (planned)
`getbalances` will add `pooled.pending` (reward window entitlement before maturity) and `pooled.claimable` (matured, ready to claim). The wallet will auto-construct claim transactions when settlements mature.

### RPCs (planned)
- `submitshare <hex>`: Validate, store, and relay a share
- `getsharechaininfo`: Best share tip, height, orphan count, reward window size
- `getrewardcommitment <blockhash>`: Commitment root, leaves, amounts for an activated block

### Solo mining under sharepool
Solo mining is the degenerate case. If only one miner fills the reward window, that miner gets a single-leaf settlement output for the full block reward. The claim path works identically. The transition from "one miner" to "many miners" is seamless and requires no configuration change.

## Architecture Boundaries

### What RNG consensus owns
- Share validity rules (RandomX proof, target bounds, parent chain, payout script)
- Sharechain tip selection (cumulative work, deterministic tie break)
- Reward window construction and payout leaf computation
- Settlement output creation in coinbase
- Witness-v2 settlement program verification
- Claim transaction shape enforcement and value conservation
- Claim-status state transitions

### What RNG node owns
- SharechainStore (LevelDB persistence, orphan buffering)
- P2P share relay (activation-gated `shareinv`/`getshare`/`share`)
- Block template construction with settlement commitment
- Internal miner dual-target production
- Sharepool RPCs

### What the wallet owns
- Pending/claimable pooled reward tracking
- Auto-claim transaction construction
- Fee management for claims

### What stays outside RNG (e.g., in Zend or other tooling)
- Operator fleet management and deployment
- Mobile control planes and UX
- HTTP-based pool protocols for external miners
- Explorer and block visualization
- Third-party miner management dashboards

## Activation

`DEPLOYMENT_SHAREPOOL` uses BIP9 versionbits activation with bit 3, period 2016, and threshold 1916/2016 (95%). Mainnet remains `NEVER_ACTIVE` until regtest proof, devnet adversarial testing, and mainnet activation preparation are complete. Regtest can activate immediately with `-vbparams=sharepool:0:9999999999:0`.

## Constraints

- All changes layer on Bitcoin Core v30.2. No changes to transaction format, script evaluation (except witness v2), or P2P framing.
- Coinbase maturity remains 100 blocks. Claims cannot spend settlement outputs before maturity.
- Share relay bandwidth must stay under ~10 KB/s per node at the 1-second cadence.
- v1 settlement supports one claim per transaction. Batched claims are deferred.
- Witness version 2 with 32-byte programs is reserved exclusively for sharepool settlement in v1.
