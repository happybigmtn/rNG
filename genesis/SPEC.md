# RNG Product Specification

## Current Verified Product Contract

RNG is a CPU-mineable cryptocurrency for AI agents built on a Bitcoin Core-derived node. The checked-in repo docs still describe the base as Bitcoin Core `v29.0`; separate local planning documents discuss later port work, but this specification avoids presenting that as settled current behavior without corresponding in-tree source evidence.

`README.md` says the current mainnet restarted from genesis on February 26, 2026. The node preserves Bitcoin-style transactions, UTXOs, wallets, RPCs, and script surfaces while replacing proof of work with RandomX and adopting RNG-specific network and economic parameters.

## Current Concrete Behaviors

### Mining

- `rngd` can run the built-in internal miner with `-mine`, `-mineaddress`, `-minethreads`, and `-minerandomx`.
- `src/node/internal_miner.cpp` implements a multithreaded RandomX miner.
- Mining is still classical block mining: one winning block pays the block finder under the existing coinbase model.

### Consensus and block production

- Target spacing is 120 seconds.
- Subsidy starts at 50 RNG, halves every 2,100,000 blocks, and floors at 0.6 RNG tail emission.
- Coinbase maturity remains 100 confirmations.
- `src/kernel/chainparams.cpp` currently enables only the documented Bitcoin-derived soft-fork deployments already present in code (`testdummy`, `taproot`).

### Transactions, wallet, and script

- RNG preserves Bitcoin-style transaction format and script execution.
- SegWit and Taproot are active from genesis per current code and specs.
- Wallet behavior remains Bitcoin-derived; mining rewards accrue through the existing wallet surfaces.

### Network and tooling

- P2P port: 8433
- RPC port: 8432
- Bech32 prefix: `rng1`
- The repo includes bootstrap assets and operator helper scripts such as `rng-start-miner`, `rng-doctor`, and public-node install/apply helpers.

## Proposed Near-Term Direction

The near-term product direction documented in the root pooled-mining plan and this `genesis/` corpus is protocol-native pooled mining:

- public shares rather than external pool admission
- deterministic reward windows
- a compact payout commitment in each block
- delayed but trustless post-maturity claims

This is a proposal, not current behavior. No sharepool/sharechain implementation was verified in the inspected target repo.

## Important Non-Current Surfaces

These are documented somewhere in the repo or local planning docs, but are not verified current behavior in the inspected target code:

- Protocol-native pooled mining
- Sharechain / share relay
- `submitshare`, `getsharechaininfo`, `getrewardcommitment`
- `createagentwallet`
- MCP server support
- `pool-mine --pool ...`
- P2P atomic swaps
- Agent identity / autonomy / webhook surfaces

## QSB Status

The local root `EXECPLAN.md` describes QSB rollout work, but the corresponding QSB source files named there were not present in the inspected checkout. This specification therefore does not treat QSB as a current product contract. If that work lands in-tree later, the pooled-mining plans should be rechecked for interaction points at that time.

## System Properties

| Property | Current verified value |
|---|---|
| Block target | 120 seconds |
| Initial reward | 50 RNG |
| Halving interval | 2,100,000 blocks |
| Tail emission floor | 0.6 RNG |
| Coinbase maturity | 100 blocks |
| Proof of work | RandomX |
| P2P port | 8433 |
| RPC port | 8432 |
| Address HRP | `rng1` |
| Documented genesis restart | February 26, 2026 |
| In-code named deployments | `testdummy`, `taproot` |

## Product Truthfulness Rules

- Present pooled mining as planned work until sharepool code exists in-tree.
- Present agent-wallet, MCP, swap, and external-pool examples as aspirational or future-only until implemented.
- Treat local planning files as planning inputs, not as substitutes for inspected source behavior.
