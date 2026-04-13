# Genesis Report

## Corpus Summary

This genesis corpus was produced on 2026-04-13 by a deep-repo review of this RNG repository (main branch, live mainnet in the low-32,000 block range per committed docs) and a separate Zend reference repository. The review was steered by an operator focus on protocol-native trustless pooled mining as the default mining mode.

The corpus contains 4 assessment files (`FOCUS.md`, `ASSESSMENT.md`, `SPEC.md`, `DESIGN.md`), 12 numbered execution plans (`001` through `012`), and this report. All numbered plans follow the root `PLANS.md` ExecPlan standard. The current generated tree does not contain separate checkpoint/report markdown files; decision gates are consolidated into numbered plans 003, 006, and 010. This corpus is a subordinate planning overlay that reconciles to root `IMPLEMENTATION_PLAN.md`; it is not a replacement for the root `PLANS.md` standard or a second active backlog.

## Major Findings

### What the codebase has accomplished

RNG is a genuine Bitcoin Core v30.2 fork with working RandomX PoW, running a live mainnet. The sharepool specification is locked with simulator-validated constants (1-second share spacing, 7200-share reward window, 120x target ratio). The settlement state machine is fully specified, reference-modeled, and has C++ consensus helpers with parity tests. The sharechain data model and P2P relay are built and tested. The solo-settlement coinbase is wired into block assembly. BIP9 activation gating is in place. Two decision gates have passed (simulator results, relay viability). The project is approximately 60% through its sharepool dependency chain by implementation plan item count.

### What remains unbuilt

The three most consensus-critical code surfaces are not yet implemented:

1. **Witness-v2 claim verification** (`src/script/interpreter.cpp`): The script interpreter does not dispatch on witness version 2. Settlement program verification is specified but has no C++ implementation.

2. **ConnectBlock enforcement** (`src/validation.cpp`): No code validates that blocks contain correct settlement commitments or that claim transactions conserve settlement value.

3. **Multi-leaf payout commitment** (`src/node/miner.cpp`): Block assembly emits the solo case (one miner fills the reward window). The multi-miner case, where the reward window contains shares from multiple miners and must build a multi-leaf payout tree, is not implemented.

Beyond consensus, the user-facing integration layer is entirely unbuilt: dual-target share production in the internal miner, wallet pooled-reward tracking and auto-claim, and the sharepool RPCs (`submitshare`, `getsharechaininfo`, `getrewardcommitment`).

### Operational risks independent of sharepool

- `contabo-validator-01` is crash-looping on a zero-byte `settings.json`. Three of four validators are healthy. This is a thin margin.
- All four mainnet seed peers are Contabo-hosted on a single ASN. A vendor policy change or ASN-level failure could partition the network.
- DNS seeds referenced in old documentation are absent from `src/kernel/chainparams.cpp`. Network discovery relies entirely on hardcoded IPs.

## Recommended Direction

Complete the sharepool critical path: **Plan 007 (consensus enforcement) -> Plan 008 (miner/wallet/RPC integration) -> Plan 009 (regtest proof) -> Plan 010 (decision gate)**. This is the sequence already encoded in `IMPLEMENTATION_PLAN.md` and the focus confirms it as the single highest-priority work stream.

Plan 007 is the bottleneck. Everything downstream is blocked on it. Within 007, the three implementation units (witness-v2 verification, ConnectBlock enforcement, multi-leaf commitment) should be done in that order because ConnectBlock enforcement depends on the verifier, and multi-leaf commitment depends on both.

After the regtest proof passes the Plan 010 gate, proceed to devnet adversarial testing (011) and mainnet activation preparation (012).

## Top Priorities

1. **Plan 007**: Witness-v2 verification, ConnectBlock enforcement, multi-leaf commitment. This is the critical path and the only plan that touches consensus-level security surfaces.

2. **Plan 008**: Dual-target miner, wallet auto-claim, sharepool RPCs. This is where the "default mining mode" and "small miner accrual" goals become tangible.

3. **Plan 009 + 010**: Regtest end-to-end proof and decision gate. The proof validates that plans 002-008 actually work together.

4. **Validator-01 repair**: Independent of sharepool, but reduces operational risk. Three healthy validators is a thin margin for a four-node mainnet.

5. **1-second relay bandwidth measurement**: The relay viability gate (Plan 006) was measured at 10-second intervals. The confirmed 1-second cadence needs its own measurement. This can be done in parallel with Plan 007.

## Not Doing

These items were considered and explicitly excluded from the current planning scope:

| Item | Reason |
|------|--------|
| Port Zend's HTTP pool protocol | Goal is protocol-native consensus-enforced pooling, not an HTTP/control-plane overlay. Zend has a trustless-track handoff model when direct-only, MinerBuilt, replay-verified, and peer-mirrored conditions are satisfied, but its architecture is still not RNG-native consensus settlement. |
| Agent wallet / MCP server (FUTURE-04) | Depends on proven sharepool. Building agent wrappers before the core lifecycle works is premature. |
| Atomic swap protocol (FUTURE-06) | Blocked until sharepool is stable on mainnet. Different risk profile entirely. |
| Batched multi-leaf claims | Deferred to v2. Single claims are simpler to verify and sufficient for launch. |
| Non-sharepool witness-v2 uses | v1 reserves witness version 2 exclusively for settlement. Relaxing this is a future protocol version decision. |
| Coinbase maturity reduction | The 100-block rule is inherited and well-understood. Changing it introduces new economic dynamics outside sharepool scope. |
| Cross-platform reproducible release verification | Same-machine linux-x86_64 is proven. Cross-machine/cross-platform is valuable but does not block sharepool work. |
| DNS seed infrastructure | Important for network resilience but independent of the sharepool critical path. |

## Focus Impact

### How the focus changed priority ordering

The existing `IMPLEMENTATION_PLAN.md` already centered on sharepool as the primary work stream. The operator focus did not reorder the plan sequence but sharpened emphasis in three ways:

1. **POOL-07 is the critical path.** The focus on trustless consensus enforcement made it clear that witness-v2 verification and ConnectBlock enforcement are the single highest-priority code changes. Everything else is downstream.

2. **POOL-08 is "next" not "eventually."** The focus on default mining mode and small-miner accessibility elevated the miner/wallet integration from "blocked future work" to "the work immediately after consensus enforcement."

3. **Decision gates are non-negotiable.** The trustlessness claim requires proof. The regtest proof (009/010) and devnet adversarial testing (011) exist precisely to validate that the protocol is actually trustless under hostile conditions.

### Higher-priority issues outside the requested focus

Two issues outrank sharepool work in urgency but are independent of it:

1. **Validator-01 crash-loop**: Reduces mainnet redundancy from 4 to 3 validators. Repair is a 10-minute operator task (restore `settings.json`, restart `rngd.service`) that does not require code changes.

2. **Single-ASN seed infrastructure**: All mainnet seeds are Contabo-hosted. This is a network resilience risk regardless of sharepool status. Mitigation requires adding seed peers on diverse ASNs.

## Decision Audit Trail

### Mechanical decisions

These decisions follow directly from the codebase state and have no reasonable alternative:

| Decision | Basis |
|----------|-------|
| Plan 007 is the critical path | Everything downstream (008-012) depends on it; no alternative ordering exists |
| Plans follow root `PLANS.md` standard | Explicit user instruction; standard already exists in repo |
| 12-plan sequence matches `IMPLEMENTATION_PLAN.md` IDs | Natural mapping; the implementation plan already defined the work items |
| Genesis plan index is subordinate to root control docs | No root `plans/` directory exists, root `PLANS.md` is only a standard, and root `IMPLEMENTATION_PLAN.md` is the active task tracker |
| Deleted checkpoint/report files are not cited as current evidence | The current generated tree lacks those files; several historical versions are stale against current source |
| Design review acknowledges inherited Qt GUI but skips visual UI rewrites | `src/qt` and `rng-qt` exist, but no sharepool-specific GUI is in scope |
| Solo settlement coinbase is credited as complete | `miner_tests` pass, code review confirms correct OP_2 output construction |
| Decision gates at 003, 006, 010 | Already in `IMPLEMENTATION_PLAN.md`; gates provide checkpoints where bad results redirect work |

### Taste decisions

These decisions reflect judgment where multiple valid approaches exist:

| Decision | Chosen | Alternative | Why |
|----------|--------|-------------|-----|
| 12 separate plans vs fewer larger plans | 12 plans | 6-8 combined plans | Each plan stays under ~400 lines, fits in context for a single implementer, and isolates consensus-critical work (007) from integration work (008) |
| Devnet adversarial testing (011) as separate plan | Separate from regtest proof | Combined "test everything" plan | Devnet raises qualitatively different failure modes (multi-host coordination, sustained operation, adversarial scenarios) that regtest cannot surface |
| Zend as lesson source only | Extract operational lessons, do not port code | Port Zend's pool accounting layer into RNG | Zend's pool can report a trustless-track state under strict handoff/proof conditions, but it remains an HTTP/control-plane pool architecture. The RNG goal is protocol-native consensus settlement, so the architectures are incompatible at the design level. |
| Do not restore historical checkpoint files | Consolidate gates in numbered ExecPlans and call out stale root references | Recreate separate checkpoint/report markdown files | The old checkpoint contents are partially stale against current source. Updating the numbered plans avoids duplicate active-looking artifacts. |
| Plan 011 left research-shaped | Adversarial scenarios listed as expected minimum set | Fully specified test matrix | The exact attack surface depends on Plan 010 findings; over-specifying would create false precision |
| Plan 012 activation parameters left open | `nStartTime` and `nTimeout` as TBD pending Plan 011 results | Pick dates now | Choosing dates before the devnet stability report exists would commit to a timeline without evidence |

### User Challenges

None added by the Codex review. The review did not find a code-grounded reason to recommend changing the user's stated protocol-native sharepool direction.

### Focus-bound tradeoffs

These are scope implications of the user's stated focus, not challenges to it:

| Decision | What the focus dictated | What would have been different |
|----------|------------------------|-------------------------------|
| Trustless pooling as the design center | Protocol-native consensus enforcement, no operator ledger, no single control plane | A trust-minimized pool (like Zend's rBTC pool) would have been simpler to build and could have launched sooner |
| Default mining mode | After activation, every miner participates in sharepool automatically | Solo mining as default with opt-in pooling would have been a smaller consensus change |
| Small-miner accessibility as success criterion | Pending reward visible immediately, auto-claim after maturity | Could have required miners to run separate claim tools or submit manual transactions |
| Agent wallet deprioritized | Sharepool lifecycle must work before agent wrappers | The `specs/agent-integration.md` feature set could have been prioritized as a parallel track |

## Zend Lessons Incorporated

The Zend reference repo was reviewed for operational lessons, not code to port. Codex specifically inspected Zend's `README.md`, `docs/rbtc-pool-third-party-onboarding.md`, `services/home-miner-daemon/rbtc_tools.py`, and `LEARNINGS.md`. Key lessons incorporated into the plan set:

1. **Tarfile extraction safety**: Zend encountered practical failures with archive handling in pool tools. The genesis plans do not use archive-based share exchange -- shares are individual P2P messages.

2. **Type-checking failures in pool tools**: Zend's Python pool handlers had type-safety gaps. The RNG settlement model and simulator both include self-test suites with deterministic vectors.

3. **Operator documentation drift**: Zend's docs diverged from runtime behavior. The genesis plans include explicit documentation update requirements in Plans 008 and 012.

4. **Inactivity timeout enforcement gaps**: Zend's pool had edge cases in timeout handling. The RNG sharepool uses a fixed-size reward window (7200 shares) rather than time-based timeouts, avoiding this class of issue.

5. **Trust model transitions**: Zend's third-party onboarding path can report a trustless state only after direct-only, MinerBuilt, replay-verified, and peer-mirrored conditions are satisfied. RNG starts from a different design center: the settlement state machine is consensus-enforced from activation, with no operator-controlled payout ledger in the protocol path.

## What This Corpus Does Not Cover

This corpus does not produce implementation code. It produces plans that a novice implementer can follow to produce implementation code. The gap between "plan exists" and "code works" is the remaining 40% of the sharepool dependency chain.

This corpus does not predict timeline. The plans are sequenced by dependency, not calendar. The critical path through Plan 007 depends on the complexity of wiring witness-v2 verification into Bitcoin Core's script interpreter, which is a non-trivial consensus change on a live network's codebase.

This corpus does not validate the economic model beyond what the simulator proved. The simulator validated variance bounds for the confirmed constants. The claim fee market, claim throughput under many-miner conditions, and sharechain storage costs at 1-second cadence are flagged as "needs proof" in the assumption ledger and addressed by Plans 009-011.
