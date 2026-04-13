# Genesis Corpus Report

## Corpus Refresh Summary

This corpus was reviewed on 2026-04-13 against this repository on branch `feat/bitcoin-30-qsb`. The target repository and the supplied sibling reference repository `../zend` were both inspected directly. Repo-root planning documents were also inspected before treating any generated artifact as authoritative: `PLANS.md`, `docs/rng-protocol-native-pooled-mining-execplan.md`, the local root `EXECPLAN.md`, and `IMPLEMENTATION_PLAN.md`.

The result of this review is a stricter, more conservative corpus. The generated plans still pursue protocol-native pooled mining, but they now distinguish verified source facts from local planning claims, treat `genesis/` as a supplemental planning decomposition rather than a second active control plane, and stop presenting unmerged QSB work as if it already exists in the current checkout.

## What Was Reviewed

- Target repo code and docs: `README.md`, `CHANGES.md`, `PLANS.md`, `IMPLEMENTATION_PLAN.md`, `specs/INDEX.md`, `specs/consensus.md`, `specs/activation.md`, `specs/agent-integration.md`, `specs/randomx.md`, `docs/rng-protocol-native-pooled-mining-execplan.md`, `src/consensus/params.h`, `src/deploymentinfo.cpp`, `src/kernel/chainparams.cpp`, `src/pow.cpp`, `src/node/miner.cpp`, `src/node/internal_miner.cpp`, `src/rpc/mining.cpp`
- Generated corpus: all files under `genesis/`, including every numbered plan under `genesis/plans/`
- Reference repo: `../zend`, focused on sharechain, pool-proof, and operator-tooling patterns

## Major Corrections

### 1. Current checkout reality is narrower than the original corpus claimed

Verified target-repo facts:

- RNG is a live RandomX-based chain with an internal miner, Bitcoin-derived wallet and RPC surfaces, bootstrap tooling, and mainnet-specific chain parameters.
- `README.md` and `CHANGES.md` still describe RNG as a fork of Bitcoin Core `v29.0`.
- `src/consensus/params.h` and `src/deploymentinfo.cpp` expose only `DEPLOYMENT_TESTDUMMY` and `DEPLOYMENT_TAPROOT`.
- No sharepool/sharechain implementation exists in the inspected source tree.

The original corpus incorrectly upgraded those facts into stronger claims such as “working Bitcoin Core v30.2 fork” and “proven QSB operator support.” Those claims were removed or downgraded to planning context.

### 2. QSB is a local planning surface here, not verified in-tree source reality

The local root `EXECPLAN.md` describes a QSB rollout and names files like `src/node/qsb_pool.h` and `src/rpc/qsb.cpp`, but those files were not present in the inspected checkout. The revised corpus therefore treats QSB as parallel, unmerged, or otherwise unverified local planning work. Generated sharepool plans no longer depend on concrete QSB file paths that are absent from the target repo.

### 3. `genesis/` is not the repo-designated active planning surface

The repository already has root planning artifacts. `PLANS.md` is the ExecPlan standard. `docs/rng-protocol-native-pooled-mining-execplan.md` is an existing pooled-mining plan. The local root `EXECPLAN.md` is a separate operational plan. `IMPLEMENTATION_PLAN.md` is an older, stale planning artifact. No inspected repo-root instruction file designated `genesis/` as the sole active control plane. The revised corpus now states plainly that `genesis/` is a generated supplemental decomposition that must reconcile back to one chosen root plan before coding starts.

### 4. Design review is mostly information architecture, not UI polish

RNG has no meaningful GUI, web app, or explorer in this repo. The design pass therefore focused on CLI/RPC/operator experience: naming, state coverage, empty/error/loading states, and documentation truthfulness. Visual design rewrites were intentionally skipped.

### 5. Zend remains reference material, not consensus dependency

Zend contains useful patterns for portable proofs, replay tooling, and operator onboarding. It does not change the target-repo fact that RNG must implement pooled mining in its own consensus, networking, miner, and wallet layers if the goal is truly protocol-native pooled mining.

## Recommended Direction

Keep the pooled-mining direction, but keep it honest and staged:

1. Treat pooled mining as a proposed near-term protocol initiative, not existing behavior.
2. Start with the spec-plus-simulator phase and preserve the explicit decision gates after simulator work, relay work, and regtest proof.
3. Reconcile implementation ownership back to one root-owned ExecPlan before writing code.
4. Keep documentation cleanup and stale-spec labeling in scope, because they materially affect contributor and operator correctness.

## Not Doing

- Do not treat local root plans as proof that corresponding source files exist in the current checkout.
- Do not present `genesis/` as a second active control plane.
- Do not collapse the pooled-mining design into Zend or any external pool runtime.
- Do not promise agent-wallet, MCP, swap, or identity features that the current repo does not implement.

## Decision Audit Trail

### Mechanical

- Reframed `genesis/PLANS.md` as an index to a supplemental generated plan set rather than the active planning surface.
- Rewrote `ASSESSMENT.md` to separate verified facts, assumptions, stale docs, and reference-repo findings.
- Rewrote `SPEC.md` so current behavior excludes unverified QSB claims and unbuilt sharepool behavior.
- Replaced the master plan so its numbering matches the actual files in `genesis/plans/`.

### Taste

- Kept the twelve-file corpus shape rather than collapsing it into a much smaller set. The existing decision gates at 003, 006, and 010 are useful once the documents are grounded in real repo state.
- Preserved a pooled-mining-first plan ordering, but made it explicit that this is a planning recommendation layered on top of root docs, not a repo-designated change of control plane.

### User Challenges

- If the intended direction was “plan pooled mining against the already-implemented QSB code,” the current checkout does not support that premise. The corpus now challenges that assumption explicitly instead of silently building on it.
- If the intended direction was “treat `genesis/` as the new master control plane,” the repo evidence does not support that. The corpus now challenges that assumption explicitly.
- If the intended direction was “improve small-miner UX by shortening coinbase maturity,” the independent review still recommends keeping the 100-block maturity unchanged for the first pooled-mining design.
