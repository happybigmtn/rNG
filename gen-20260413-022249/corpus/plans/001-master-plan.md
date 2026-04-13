# Protocol-Native Pooled Mining For RNG -- Master Plan

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This is the master plan for the generated pooled-mining corpus under `genesis/plans/`. Its job is to explain, in one place, what RNG can already do, what protocol-native pooled mining would add, and how the numbered sub-plans fit together.

The user-visible goal is clear even though the feature does not exist yet: after this work, a low-hashrate miner should be able to contribute useful work continuously, see pending proportional reward accrue without joining an external pool, and later claim that reward through a consensus-defined on-chain path. Today, the inspected RNG checkout still uses classical winner-takes-all block rewards.

This master plan is intentionally conservative about repository reality. It does not assume that untracked local plans or out-of-tree experiments are already present in source. It treats `genesis/` as a supplemental decomposition of existing root planning documents, not as a second active planning surface.

## Requirements Trace

`R1`. The corpus must describe protocol-native pooled mining as proposed work, not as current behavior in the inspected checkout.

`R2`. After activation, pooled rewards must be consensus-enforced and replayable from public data, not dependent on an operator ledger or external pool runtime.

`R3`. Before activation, existing RNG behavior must remain unchanged.

`R4`. The first implementation phase must be a spec-plus-simulator pass that can reject bad economics before consensus code lands.

`R5`. The plan set must preserve explicit decision gates after the simulator phase, the relay phase, and the regtest proof phase.

`R6`. The generated corpus must remain reconciled with the repo-root planning surfaces rather than pretending that `genesis/` already owns the control plane.

`R7`. The plan set must stay grounded in verified repo files and commands. Where local planning documents describe parallel work that is not present in the inspected source tree, that uncertainty must be stated plainly.

`R8`. The future pooled-mining contract must distinguish pending accrual from claimable reward under the existing 100-block coinbase maturity rule unless a separate maturity change is explicitly planned.

`R9`. The future design should prefer extending existing mining, wallet, RPC, and activation surfaces over inventing disconnected replacements.

`R10`. The final implementation path should remain staged: simulator, decision gate, activation boundary, sharechain, payout/claim path, miner/wallet integration, regtest proof, decision gate, devnet, mainnet preparation.

`R11`. The numbered plans must stay internally consistent with one another: names, numbering, dependencies, and expected artifacts must match the actual files under `genesis/plans/`.

`R12`. The plan must not assume unmerged QSB source files are present. If parallel QSB work lands later, compatibility should be reassessed from the merged code, not from local planning prose.

## Scope Boundaries

This plan does not claim that pooled mining already exists in RNG.

This plan does not make `genesis/` the active planning surface for the repository. It is a generated planning corpus that must be reconciled back into one chosen root plan before coding.

This plan does not treat Zend as a dependency. Zend remains reference material for tooling, proofs, and onboarding patterns.

This plan does not reduce coinbase maturity.

This plan does not promise agent wallets, MCP, swaps, or identity features. Those surfaces are currently aspirational in the target repo.

This plan does not assume that the root `EXECPLAN.md` QSB rollout is merged into the inspected source tree.

## Progress

- [x] (2026-04-13 03:00Z) Reconciled this master plan with the actual `genesis/plans/` inventory and the repo-root planning documents.
- [x] (2026-04-13 03:00Z) Removed assumptions that QSB source files already exist in the inspected checkout.
- [ ] (2026-04-13 03:00Z) Complete Plan 002 and produce a simulator-backed protocol spec.
- [ ] (2026-04-13 03:00Z) Pass Plan 003 with a documented go/no-go on constants and soft-fork shape.
- [ ] (2026-04-13 03:00Z) Implement the activation boundary and sharechain core from Plans 004 and 005.
- [ ] (2026-04-13 03:00Z) Pass Plan 006 after relay behavior is measured on RNG’s small-network assumptions.
- [ ] (2026-04-13 03:00Z) Complete payout commitment, claim path, and miner/wallet integration from Plans 007 and 008.
- [ ] (2026-04-13 03:00Z) Produce a convincing regtest proof in Plan 009 and review it in Plan 010.
- [ ] (2026-04-13 03:00Z) Run devnet validation in Plan 011 before any mainnet preparation in Plan 012.

## Surprises & Discoveries

- Observation: The generated corpus originally treated `genesis/` as the active planning surface, but the inspected working tree already contains root planning artifacts that predate it.
  Evidence: `PLANS.md`, `docs/rng-protocol-native-pooled-mining-execplan.md`, `EXECPLAN.md`, and `IMPLEMENTATION_PLAN.md` all exist at repo root.

- Observation: The pooled-mining direction is real as a plan, but not as code.
  Evidence: no sharepool/sharechain implementation was found in the inspected target-repo source files.

- Observation: The root local `EXECPLAN.md` describes QSB work that the inspected checkout does not currently contain as source files.
  Evidence: file lookups for `src/script/qsb.h`, `src/rpc/qsb.cpp`, `src/node/qsb_pool.h`, and `src/node/qsb_validation.h` in the working tree failed.

- Observation: Repo documentation truthfulness is a first-order planning concern, not cleanup trivia.
  Evidence: `specs/INDEX.md`, `specs/agent-integration.md`, and `specs/randomx.md` all describe outdated or unimplemented surfaces.

## Decision Log

- Decision: Keep the pooled-mining direction, but downgrade all “already implemented” claims to verified reality unless source inspection proves otherwise.
  Rationale: planning quality depends on source-truth first; otherwise later plans inherit false constraints.
  Date/Author: 2026-04-13 / Codex

- Decision: Treat `genesis/` as a generated supplemental corpus instead of the repo-designated control plane.
  Rationale: the inspected repo-root docs do not authorize `genesis/` to replace existing planning ownership.
  Date/Author: 2026-04-13 / Codex

- Decision: Preserve the simulator-first and decision-gated execution order.
  Rationale: the biggest uncertainty is still economics and protocol shape, not coding volume.
  Date/Author: 2026-04-13 / Codex

- Decision: Remove hard dependencies on QSB file paths that are absent from the inspected checkout.
  Rationale: compatibility with parallel local work can only be specified against merged code, not against planning prose.
  Date/Author: 2026-04-13 / Codex

## Outcomes & Retrospective

At this stage the outcome is a corrected master plan, not implementation work. The most important improvement is not architectural novelty. It is trustworthiness: the plan now matches the actual `genesis/plans/` inventory, matches the repo-root planning context, and no longer asks an implementer to rely on source files that are not in the checkout.

The strategic conclusion did not change. Protocol-native pooled mining is still the strongest near-term product direction surfaced by the inspected plans. What changed is the bar for truth: the work starts from a spec-plus-simulator phase and from one chosen root ExecPlan, not from an invented assumption that the protocol or QSB seams already exist in source.

## Context and Orientation

RNG is a live RandomX-based chain with a Bitcoin-derived node architecture. The checked-in docs still describe it as a fork of Bitcoin Core `v29.0`. The current code relevant to this plan lives in a familiar set of seams:

`src/pow.cpp` validates RandomX proof of work and encodes the current difficulty logic.

`src/node/miner.cpp` and `src/node/internal_miner.cpp` still model mining as classical block production under the current coinbase contract.

`src/consensus/params.h`, `src/deploymentinfo.cpp`, and `src/kernel/chainparams.cpp` define the current activation and consensus parameters. Only `testdummy` and `taproot` are present in code today.

`src/rpc/mining.cpp` is the existing mining-RPC surface that future sharepool work would most naturally extend.

`docs/rng-protocol-native-pooled-mining-execplan.md` is the existing repo-root pooled-mining plan. This corpus should be read as a decomposition and review of that direction, not as a separate authority that supersedes it.

Three planning-surface terms matter here:

An “active planning surface” is the document a contributor would treat as the source of day-to-day implementation truth. This master plan explicitly says `genesis/` is not that by default.

A “decision gate” is a deliberate stop point where later implementation is blocked until a document records either a go decision or a redesign requirement.

A “supplemental corpus” is a generated set of review and decomposition artifacts that may feed the active plan but does not silently replace it.

## Plan of Work

The work sequence is straightforward once grounded in current repo state.

First, Plan 002 writes the protocol down in a truthful way and builds a deterministic simulator. That phase must settle constants and expose whether the proposed reward-window and claim design are even worth implementing.

Plan 003 then evaluates the simulator output and records an explicit go/no-go. If the economics or soft-fork assumptions fail there, the right action is to revise the design, not to continue coding optimistically.

If the gate passes, Plan 004 adds only the activation boundary. Plan 005 adds the sharechain data model, storage, and relay. Plan 006 then checks whether that relay model is viable on RNG’s small-network assumptions.

Only after those checkpoints should the corpus proceed to payout commitment and claim design in Plan 007, then miner, wallet, and RPC integration in Plan 008.

Plan 009 proves the end-to-end behavior on regtest. Plan 010 reviews that proof before any devnet rollout. Plan 011 handles devnet validation and adversarial testing. Plan 012 prepares mainnet activation but does not itself activate anything.

## Implementation Units

### Unit 1: `002-sharepool-spec-and-simulator.md`

Goal: define the protocol contract and build a simulator that can reject bad economics early.

Requirements advanced: `R1`, `R2`, `R4`, `R8`, `R10`.

Dependencies: none.

Files advanced: future spec files under `specs/` plus a future simulator under `contrib/sharepool/`.

Specific proof target: given the same accepted-share trace twice, the simulator emits the same commitment root twice.

### Unit 2: `003-decision-gate-simulator-results.md`

Goal: stop and decide whether the simulator output justifies implementation.

Requirements advanced: `R4`, `R5`, `R10`.

Dependencies: Unit 1.

Specific proof target: a written go/no-go decision with concrete reasons, not implied optimism.

### Unit 3: `004-sharepool-deployment-skeleton.md`

Goal: add a clean activation boundary without changing pre-activation behavior.

Requirements advanced: `R3`, `R9`.

Dependencies: Unit 2.

Specific proof target: a new deployment can be activated on regtest while default behavior remains unchanged elsewhere.

### Unit 4: `005-sharechain-data-model-storage-relay.md`

Goal: add share records, persistence, relay, and basic mining-facing queries.

Requirements advanced: `R2`, `R7`, `R9`.

Dependencies: Unit 3.

Specific proof target: two activated regtest nodes accept and agree on the same best share tip.

### Unit 5: `006-decision-gate-share-relay-viability.md`

Goal: verify that the share-relay model is acceptable on RNG’s target network shape.

Requirements advanced: `R5`, `R10`.

Dependencies: Unit 4.

Specific proof target: a written decision based on measured relay behavior, orphan handling, and replay cost.

### Unit 6: `007-payout-commitment-and-claim-program.md`

Goal: define and implement the compact payout commitment plus claim path.

Requirements advanced: `R2`, `R8`, `R9`.

Dependencies: Unit 5.

Specific proof target: a node can derive one deterministic commitment from one accepted share window and validate claim proofs against it.

### Unit 7: `008-miner-wallet-rpc-integration.md`

Goal: make the future pooled-mining contract visible and usable through the existing miner, wallet, and RPC surfaces.

Requirements advanced: `R8`, `R9`.

Dependencies: Unit 6.

Specific proof target: pending and claimable pooled-reward states are visible without inventing a parallel user surface.

### Unit 8: `009-regtest-end-to-end-proof.md`

Goal: prove the whole flow in a deterministic local network.

Requirements advanced: `R2`, `R3`, `R8`.

Dependencies: Unit 7.

Specific proof target: unequal miners both accrue proportional rewards and an observer independently reproduces the same commitment roots.

### Unit 9: `010-decision-gate-regtest-proof-review.md`

Goal: explicitly review the regtest proof before allowing devnet work.

Requirements advanced: `R5`, `R10`.

Dependencies: Unit 8.

Specific proof target: a written go/no-go decision for devnet.

### Unit 10: `011-devnet-deployment-and-adversarial-testing.md`

Goal: move from local proof to controlled multi-node proof.

Requirements advanced: `R5`, `R9`, `R10`.

Dependencies: Unit 9.

Specific proof target: multiple nodes converge on the same share and payout state under realistic conditions.

### Unit 11: `012-mainnet-activation-preparation.md`

Goal: prepare mainnet parameters, operator docs, and rollout steps without actually activating the feature.

Requirements advanced: `R3`, `R6`, `R10`.

Dependencies: Unit 10.

Specific proof target: a release candidate and operator playbook exist, and the final activation choice remains explicit.

## Concrete Steps

All commands below assume the working directory is the repository root.

1. Re-read the root planning context before implementation:

       sed -n '1,220p' PLANS.md
       sed -n '1,220p' docs/rng-protocol-native-pooled-mining-execplan.md
       sed -n '1,220p' genesis/PLANS.md

2. Start with the simulator/spec documents, not source edits:

       sed -n '1,260p' genesis/plans/002-sharepool-spec-and-simulator.md
       sed -n '1,260p' genesis/plans/003-decision-gate-simulator-results.md

3. Only after Plan 003 records a go decision should implementation work proceed to:

       sed -n '1,260p' genesis/plans/004-sharepool-deployment-skeleton.md
       sed -n '1,260p' genesis/plans/005-sharechain-data-model-storage-relay.md
       sed -n '1,260p' genesis/plans/006-decision-gate-share-relay-viability.md

4. Treat Plans 007 through 012 as blocked until the earlier gates pass.

5. Before coding, fold the adopted plan material back into the chosen root ExecPlan so the repository is not running dual planning surfaces.

## Validation and Acceptance

This master plan is acceptable when all of the following remain true:

- Its numbering and dependencies match the actual files under `genesis/plans/`.
- It does not present pooled mining as implemented current behavior.
- It does not rely on unmerged QSB source files.
- It preserves the simulator-first and decision-gated execution order.
- It makes the relationship between `genesis/` and repo-root planning docs explicit.

For the future implementation it describes, acceptance still depends on the later proof points: simulator determinism, regtest end-to-end proof, devnet validation, and explicit mainnet preparation only after those pass.

## Idempotence and Recovery

This document is safe to revise repeatedly as discoveries are made. If a later implementation or local branch changes repo reality, update this master plan from the new inspected source tree rather than layering more assumptions on top of outdated text.

If later review chooses a different root plan as the active surface, this file can remain as historical decomposition so long as it clearly points contributors back to that chosen root plan.

## Artifacts and Notes

### Current repo facts that anchor this plan

    README.md says RNG is a fork of Bitcoin Core v29.0.
    src/consensus/params.h only defines DEPLOYMENT_TESTDUMMY and DEPLOYMENT_TAPROOT.
    src/deploymentinfo.cpp only exposes testdummy and taproot.
    No sharepool/sharechain implementation was found in the inspected target-repo source.

### Actual numbered plan mapping

    001 master plan
    002 spec + simulator
    003 simulator decision gate
    004 deployment skeleton
    005 sharechain data model / storage / relay
    006 relay decision gate
    007 payout commitment + claim path
    008 miner / wallet / RPC integration
    009 regtest proof
    010 regtest decision gate
    011 devnet validation
    012 mainnet preparation

## Interfaces and Dependencies

The future implementation described by this corpus is expected to evolve existing RNG surfaces rather than create disconnected replacements.

The most important future dependencies are:

- activation surfaces in `src/consensus/params.h`, `src/deploymentinfo.cpp`, and `src/kernel/chainparams.cpp`
- mining surfaces in `src/node/miner.cpp`, `src/node/internal_miner.cpp`, and `src/rpc/mining.cpp`
- validation surfaces in `src/validation.cpp`
- future sharechain modules under a new subtree such as `src/sharechain/`
- wallet surfaces once pending and claimable pooled reward become real

This plan intentionally avoids naming concrete QSB interfaces because those source files were not present in the inspected checkout. If that work lands later, compatibility should be specified from the merged interfaces at that time.

Change note: Rewritten on 2026-04-13 during the corpus review pass. Reason: the generated master plan had incorrect sub-plan numbering, assumed QSB source files that were not present in the inspected checkout, and described `genesis/` as the active planning surface without repo evidence for that claim.
