# Codex Corpus Review

## Summary

I independently reviewed the generated corpus under `genesis/` against the target repo at `/home/r/Coding/RNG` and the supplied reference repo at `/home/r/Coding/zend`. The main problems were source-truth drift and planning-surface drift: the corpus overstated current repo reality, treated local root planning notes as if they proved in-tree implementation, and presented `genesis/` as the active planning surface without repo evidence for that claim.

I amended the corpus in place. The revised documents now distinguish verified target-repo facts from local planning claims, treat pooled mining as proposed work rather than current behavior, frame `genesis/` as a supplemental generated corpus rather than a second control plane, and remove hard dependencies on QSB source files that were not present in the inspected checkout.

## Files Reviewed

### Target repo

- `README.md`
- `CHANGES.md`
- `PLANS.md`
- `EXECPLAN.md`
- `IMPLEMENTATION_PLAN.md`
- `docs/rng-protocol-native-pooled-mining-execplan.md`
- `docs/devnet-mining-summary-2026-02-27.md`
- `specs/INDEX.md`
- `specs/consensus.md`
- `specs/activation.md`
- `specs/agent-integration.md`
- `specs/randomx.md`
- `src/consensus/params.h`
- `src/deploymentinfo.cpp`
- `src/kernel/chainparams.cpp`
- `src/pow.cpp`
- `src/node/miner.cpp`
- `src/node/internal_miner.cpp`
- `src/rpc/mining.cpp`

### Reference repo

- `/home/r/Coding/zend` sharechain/pool tooling and related plan material

### Generated corpus

- `genesis/GENESIS-REPORT.md`
- `genesis/ASSESSMENT.md`
- `genesis/SPEC.md`
- `genesis/PLANS.md`
- `genesis/FOCUS.md`
- `genesis/DESIGN.md`
- all numbered plans under `genesis/plans/`

## Changes Made

- Rewrote `genesis/GENESIS-REPORT.md` to reflect verified repo state, explicit control-plane status, and explicit user challenges.
- Rewrote `genesis/ASSESSMENT.md` to separate inspected facts, contradicted claims, stale docs, reference-repo findings, and DX findings.
- Rewrote `genesis/SPEC.md` so current behavior excludes unverified QSB claims and unbuilt sharepool behavior.
- Rewrote `genesis/PLANS.md` so it is an index to a supplemental generated plan set rather than a claim that `genesis/` is the active planning surface.
- Rewrote `genesis/FOCUS.md` around repo-grounded code seams and removed overclaims about current QSB and 30.2 state.
- Rewrote `genesis/DESIGN.md` as an operator/CLI information-architecture pass instead of a UI-design pass.
- Replaced `genesis/plans/001-master-plan.md` with a new master plan that:
  - matches the actual `genesis/plans/` inventory
  - explicitly treats `genesis/` as supplemental
  - removes assumptions about absent QSB source files
  - preserves the simulator-first, checkpointed pooled-mining sequence
- Patched plans `002`, `003`, `004`, `005`, `007`, `008`, `010`, `011`, and `012` to remove or downgrade unsupported claims about QSB source presence, 30.2 completion, and inherited fleet facts.

## Decision Audit Trail

### Mechanical

- `genesis/` was downgraded from “active planning surface” to “supplemental generated corpus” because the inspected repo-root docs do not designate it as the sole control plane.
- QSB was treated as unverified local planning context rather than current source behavior because the cited source files were absent from the inspected checkout.
- The master plan was replaced because its sub-plan numbering and file mapping did not match the actual generated plan set.
- Current-state references to Bitcoin Core `v30.2` were downgraded unless supported by inspected source truth; repo docs still say `v29.0`.

### Taste

- I kept the 12-plan corpus structure instead of collapsing it. Once grounded in real repo state, the checkpoints at 003, 006, and 010 are useful and worth preserving.
- I kept pooled mining as the recommended near-term direction rather than demoting it behind all documentation cleanup, because the existing root pooled-mining plan and the repo’s product direction still support it. The key change was honesty about present state, not a strategy reversal.

### User Challenge

- If the intended direction was to plan pooled mining against already-merged QSB code, the current checkout does not support that premise. I preserved that challenge explicitly instead of silently accepting it.
- If the intended direction was to let `genesis/` replace repo-root planning ownership, the inspected repo evidence does not support that. I preserved that challenge explicitly.
- If the intended direction was to improve small-miner UX by reducing coinbase maturity in the first pooled-mining version, I preserved the contrary recommendation: keep 100-block maturity unchanged unless a separate consensus proposal is justified.

## User Challenges

- The repo currently contains multiple planning surfaces at root. Before implementation starts, one root-owned ExecPlan should be chosen as the active surface and updated with any corpus material worth keeping.
- The local root `EXECPLAN.md` may still be operationally valuable, but it is not a substitute for inspected source truth. If QSB work is important to pooled-mining compatibility, that code should land or be checked out explicitly before final integration planning.
- The stale specs are not harmless. They materially affect operator and contributor understanding and should be treated as part of the pooled-mining preparation work, not as unrelated cleanup.

## Taste Decisions

- I rewrote top-level corpus docs rather than patching sentence by sentence because the underlying problems were structural and repeated.
- I generalized some operator-fleet language in later plans. Where the original corpus inherited very specific host names and rollout claims from local root plans, the revised plans now require re-verification at execution time.

## Validation

- Verified required section coverage across all numbered plans: each file under `genesis/plans/` now contains all 15 required sections.
- Verified the old phantom master-plan file mapping strings are gone from `genesis/`.
- Verified the rewritten corpus no longer contains claims that `genesis/` is the active planning surface for pooled-mining work.
- Verified the remaining explicit QSB references are now either:
  - evidence that certain files were absent from the inspected checkout, or
  - cautious references to local root planning context rather than claims of merged implementation

## Remaining Risks

- I did not edit root docs or source code outside the allowed boundary, so stale or contradictory repository materials outside `genesis/` still exist. In particular, `specs/INDEX.md`, `specs/agent-integration.md`, `specs/randomx.md`, and `IMPLEMENTATION_PLAN.md` remain misleading until separately cleaned up.
- I did not run integration tests or build artifacts for this document-review pass. Validation here was limited to source inspection and corpus-shape checks.
- Several numbered plans remain ambitious. They now have truthful assumptions and correct structure, but they still represent substantial future implementation work, not validated code paths.
