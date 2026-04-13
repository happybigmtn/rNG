# RNG Repository Assessment

## Review Scope

This assessment is grounded in direct inspection of this repository and the supplied sibling reference repository at `../zend`.

### Target repo files inspected directly

- Repo-root control docs: `PLANS.md`, `EXECPLAN.md`, `IMPLEMENTATION_PLAN.md`
- Product and operator docs: `README.md`, `CHANGES.md`, `docs/rng-protocol-native-pooled-mining-execplan.md`, `docs/devnet-mining-summary-2026-02-27.md`
- Specs: `specs/INDEX.md`, `specs/consensus.md`, `specs/activation.md`, `specs/agent-integration.md`, `specs/randomx.md`
- Code: `src/consensus/params.h`, `src/deploymentinfo.cpp`, `src/kernel/chainparams.cpp`, `src/pow.cpp`, `src/node/miner.cpp`, `src/node/internal_miner.cpp`, `src/rpc/mining.cpp`

### Reference repo inspected directly

- `../zend` sharechain/pool tooling and related plan material

## How Might We

How might RNG move from classical block-finder rewards toward protocol-native pooled mining for small CPU miners and agents without confusing contributors about what already exists, what is merely planned, and which planning document is actually steering the work?

## Verified Facts About The Target Repo

### Current product and chain facts

- `README.md` describes RNG as a “CPU-mineable cryptocurrency for AI agents.”
- `README.md` and `CHANGES.md` still identify RNG as a fork of Bitcoin Core `v29.0`.
- `README.md` says mainnet restarted from genesis on February 26, 2026.
- `src/pow.cpp` and `src/crypto/randomx_hash.*` implement RandomX-based proof of work.
- `src/node/internal_miner.cpp` contains a built-in multithreaded miner.
- `src/kernel/chainparams.cpp` sets a 120-second target spacing, 2.1M-block halving interval, 0.6 RNG tail emission floor, and the mainnet genesis message “Life is a random number generator”.
- `src/kernel/chainparams.cpp` sets `fPowAllowMinDifficultyBlocks = true` on mainnet.

### Activation and deployment facts

- `src/consensus/params.h` defines only `DEPLOYMENT_TESTDUMMY` and `DEPLOYMENT_TAPROOT`.
- `src/deploymentinfo.cpp` only exposes `testdummy` and `taproot`.
- No `DEPLOYMENT_SHAREPOOL` or comparable pooled-mining deployment exists in the inspected code.

### What does not exist in the inspected source tree

- No sharechain or sharepool code
- No `submitshare`, `getsharechaininfo`, or `getrewardcommitment` RPCs
- No pooled payout commitment logic
- No claim-program or share-relay implementation

## Current Behavior vs. Planned Direction

| Surface | Verified current state in target repo | Planning direction in root/genesis docs |
|---|---|---|
| Mining | Classical block mining with RandomX and an internal miner | Protocol-native pooled mining via public shares |
| Activation | `testdummy` and `taproot` only | Add a future `DEPLOYMENT_SHAREPOOL` |
| Wallet / agent UX | Standard Bitcoin-derived wallet surfaces | Pending vs. claimable pooled reward tracking |
| Agent integration | Aspirational docs only | Formalize after pooled-mining contract is real |
| Operator rollout | Scripts and local plans exist | Future staged rollout after regtest/devnet proof |

## Unverified Or Contradicted Claims

### Contradicted by direct inspection

- “RNG is now a clean working Bitcoin Core v30.2 fork”  
  Repo docs still say `v29.0`, and the inspected source files alone do not establish a completed 30.2 port.

- “QSB source files are present in this checkout”  
  The inspected checkout did not contain `src/script/qsb.h`, `src/rpc/qsb.cpp`, `src/node/qsb_pool.h`, or `src/node/qsb_validation.h`.

- “QSB operator support is proven current behavior”  
  The local root `EXECPLAN.md` makes that claim, but it is a local planning document, not source truth. The inspected code reviewed for this corpus did not support that conclusion.

### Unverified but plausible planning claims

- The pooled-mining design in `docs/rng-protocol-native-pooled-mining-execplan.md` may be implementable as a version-bits-activated soft fork, but that has not yet been proven by prototype or test code.
- The fixed RandomX seed policy may complicate share-difficulty or share-validation design, but the exact impact is still a design-time question.

## Stale Documentation Claims

### Verified stale or misleading docs

- `specs/INDEX.md`
  - marks `agent-integration.md` as “✅ Complete”
  - still says Bitcoin Core `27.x` is the base
  - still says RandomX seed rotation happens every 2048 blocks with lag
  - still contains a mostly pre-launch implementation checklist

- `specs/agent-integration.md`
  - documents `createagentwallet`
  - documents an MCP server
  - documents `pool-mine --pool ...`
  None of those are implemented in the inspected target repo.

- `specs/randomx.md`
  - describes rotating RandomX seeds
  The inspected code uses a fixed genesis-seed model.

- `IMPLEMENTATION_PLAN.md`
  - describes older Botcoin-era assumptions and outdated parameters
  It should not be treated as a current control document.

## Reference Repo Findings

These findings come from `../zend`, not from RNG itself:

- Zend already has substantial sharechain/pool-tooling and proof-export work.
- Zend demonstrates useful patterns for append-only histories, proof bundles, and operator onboarding.
- Zend does not reduce the need for RNG to implement pooled mining in its own consensus and P2P layers if the end goal is truly protocol-native pooled mining.

## DX Findings

RNG is operator-facing and developer-facing enough that DX matters here.

- The repo lacks a clear contributor-facing architecture overview for RNG-specific seams.
- Root planning documents are fragmented: `PLANS.md` is the standard, `docs/rng-protocol-native-pooled-mining-execplan.md` is an existing pooled-mining plan, `EXECPLAN.md` is a separate local ops plan, and `IMPLEMENTATION_PLAN.md` is stale.
- The stale specs materially increase onboarding error because they present future features as if they already exist.

## Assessment Summary

The codebase is a real, working RandomX-based RNG node with a live chain and useful operator tooling, but protocol-native pooled mining is still a design proposal, not a partial implementation. The generated corpus was directionally right to start with a simulator and explicit checkpoints, but it overclaimed the current repo state and invented a stronger planning authority for `genesis/` than the repo evidence supports.

The strongest grounded next step remains the same: keep pooled mining as a proposed near-term initiative, but make the first move a truthful spec-and-simulator pass that reconciles to the existing root pooled-mining plan rather than pretending the protocol work or QSB integration already landed.
