# Genesis Plan Set Index

This index catalogs the genesis planning corpus for the RNG sharepool implementation. All numbered plans (001-012) follow the root `PLANS.md` ExecPlan standard at the repository root. The root standard governs format, self-containment, living-document sections, and implementation unit structure. This index explains sequencing, dependency order, and why this plan slice was chosen.

Corpus type: subordinate reconciliation overlay. The active repo planning/control surface remains the root `IMPLEMENTATION_PLAN.md`, with operational context in `EXECPLAN.md`, `WORKLIST.md`, and `LEARNINGS.md`. No repo-root `plans/` directory exists in the target repo. This `genesis/` corpus provides detailed ExecPlans and assessment material that reconcile to the root tracker; it does not replace root `PLANS.md` as the standard and does not become a second active backlog by filename alone.

## Governing Standard

The root `PLANS.md` at the repository root is the authoritative ExecPlan standard. Every numbered plan in this directory is a standalone ExecPlan that can be read from top to bottom by a novice implementer with no prior context. Each plan contains all mandatory sections: Purpose/Big Picture, Requirements Trace, Scope Boundaries, Progress, Surprises & Discoveries, Decision Log, Outcomes & Retrospective, Context and Orientation, Plan of Work, Implementation Units, Concrete Steps, Validation and Acceptance, Idempotence and Recovery, Artifacts and Notes, and Interfaces and Dependencies.

The root `PLANS.md` is only a format contract. It does not, by itself, designate this generated corpus as the active planning root. The active work-state source remains `IMPLEMENTATION_PLAN.md` unless the repository's root instructions are changed.

## Why This Plan Slice

The sharepool dependency chain has a natural order dictated by what must exist before the next thing can be built. Specifications must precede implementations. Implementations must precede integration. Integration must precede proof. Proof must precede deployment. Each numbered plan produces a verifiable artifact that the next plan depends on.

The obvious alternative would be to combine plans 007 and 008 into a single "build everything" plan, or to skip the decision gates (003, 006, 010) in favor of a shorter sequence. Both alternatives were rejected. Combining 007 and 008 would produce a plan too large for a single implementer to hold in context; 007 alone touches the script interpreter, validation, and block assembly, which are three of the most sensitive surfaces in a Bitcoin Core fork. Skipping decision gates would remove the checkpoints where bad results, like the 10-second spacing rejection at plan 003, can redirect the sequence without wasting implementation effort.

The twelve-plan sequence also separates regtest proof (009-010) from devnet adversarial testing (011) and mainnet activation (012) because each environment raises qualitatively different failure modes. Regtest catches logic errors. Devnet catches network-scale failure modes (withholding, eclipse, relay spam). Mainnet activation is an irreversible deployment decision that must follow both.

## Dependency Graph

Plans are numbered in topological dependency order. Each plan's dependencies are strictly on earlier-numbered plans.

    002 (spec + simulator)
     |
    003 (decision gate: simulator results)
     |
    004 (BIP9 deployment skeleton)
     |
    005 (sharechain data model + relay)
     |
    006 (decision gate: relay viability)
     |
    007 (payout commitment + claim program)
     |
    008 (miner + wallet + RPC integration)
     |
    009 (regtest end-to-end proof)
     |
    010 (decision gate: regtest proof review)
     |
    011 (devnet adversarial testing)
     |
    012 (mainnet activation)

Decision gates at 003, 006, and 010 are the only plans that can produce a NO-GO result. A NO-GO at any gate loops back to the most recent implementation plan for fixes before proceeding.

## Assessment and Design Corpus

These files provide context, analysis, and design review that informed the numbered plans. They are not ExecPlans.

| File | Purpose |
|------|---------|
| `FOCUS.md` | Focus brief: normalized themes, likely code surfaces, priority reordering |
| `ASSESSMENT.md` | Full codebase assessment: what works, what's broken, what's half-built, tech debt, security, test gaps, DX |
| `SPEC.md` | System specification: current (pre-activation) and planned (post-activation) behavior |
| `DESIGN.md` | Design review: CLI, RPC, and operator surfaces |

## Numbered Plans

### Completed plans (artifacts committed, decision gates passed)

| Plan | Title | Status | Key Artifact |
|------|-------|--------|--------------|
| 001 | Master plan: sequencing index | Complete | This dependency graph |
| 002 | Sharepool spec and simulator | Complete | `specs/sharepool.md`, `contrib/sharepool/simulate.py` |
| 003 | Decision gate: simulator results | Complete (consolidated in numbered gate) | 1-second spacing confirmed, `pool-02r-revised-sweep.json` |
| 004 | BIP9 deployment skeleton | Complete | `DEPLOYMENT_SHAREPOOL` in `src/consensus/params.h` |
| 005 | Sharechain data model, storage, relay | Complete | `src/node/sharechain.{h,cpp}`, relay handlers |
| 006 | Decision gate: relay viability | Complete | `pool-06-relay-viability.json` |

### Partially completed plans

| Plan | Title | Status | What Remains |
|------|-------|--------|--------------|
| 007 | Payout commitment and claim program | 07A-07E done; 07 core not started | Witness-v2 verification, ConnectBlock enforcement, multi-leaf commitment |

### Not started plans

| Plan | Title | Blocked On |
|------|-------|------------|
| 008 | Miner, wallet, RPC integration | Plan 007 completion |
| 009 | Regtest end-to-end proof | Plan 008 completion |
| 010 | Decision gate: regtest proof review | Plan 009 completion |
| 011 | Devnet adversarial testing | Plan 010 GO decision |
| 012 | Mainnet activation preparation | Plan 011 stability report |

## Critical Path

The critical path runs through one plan: **007**. Everything downstream (008-012) is blocked on it. Plan 007 requires three implementation units:

1. Witness-v2 settlement verification in `src/script/interpreter.cpp`
2. ConnectBlock sharepool commitment enforcement in `src/validation.cpp`
3. Multi-leaf payout commitment in `src/node/miner.cpp`

These are the three most consensus-critical code changes in the entire sequence. They touch the script interpreter, validation, and block assembly -- the surfaces where a bug could allow settlement draining or invalid claims on the live network.

## Decision Gates and Historical Artifacts

The current generated tree contains only numbered plan files under `genesis/plans/`. Decision gates are represented by numbered plans 003, 006, and 010. Older standalone checkpoint/report artifact names (`chkpt-02-pre-consensus-review.md`, `chkpt-03a-settlement-design-review.md`, `003-decision-report.md`, `003r-decision-report.md`, and `rel-01-reproducible-release-report.md`) are absent from the current tree and should not be cited as current evidence. Where root docs still reference one of those historical files, treat that reference as a stale artifact pointer and reconcile against source, specs, committed reports, and `IMPLEMENTATION_PLAN.md`.

## Relationship to IMPLEMENTATION_PLAN.md

The root `IMPLEMENTATION_PLAN.md` is the active task tracker that maps POOL-XX identifiers to code deliverables. This genesis plan set provides detailed ExecPlans that support and explain those work items; it does not supersede the root tracker. The mapping is:

| IMPLEMENTATION_PLAN.md ID | Genesis Plan |
|---------------------------|--------------|
| POOL-01, POOL-02 | 002 |
| POOL-03, POOL-03R | 003 |
| POOL-04 | 004 |
| POOL-05, POOL-06-GATE | 005, 006 |
| POOL-07A through POOL-07E, POOL-07 | 007 |
| POOL-08 | 008 |
| CHKPT-03 | 009, 010 |
| FUTURE-01 | 011 |
| FUTURE-02 | 012 |
