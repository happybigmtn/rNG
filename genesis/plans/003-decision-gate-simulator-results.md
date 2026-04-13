# Decision Gate: Simulator Results

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This is a decision gate. Before any consensus code is written, the simulator results must prove that the chosen sharepool constants produce acceptable variance for small miners and acceptable withholding resistance. If the gate fails, the constants must be revised (back to Plan 002) before proceeding to Plan 004.

After this gate passes, an operator or contributor can be confident that the constants have been validated against the specified economic criteria and are suitable for consensus implementation.

## Requirements Trace

`R1`. 10% miner coefficient of variation over 100 blocks must be below 10% for seed 42 and all seeds 1-20.

`R2`. Withholding advantage must be below 5%.

`R3`. The decision (GO or NO-GO) must be committed with evidence.

## Scope Boundaries

This plan does not write code. It evaluates simulator evidence and records a decision. If the decision is NO-GO, it triggers a revision loop back to Plan 002.

## Progress

- [x] (2026-04-13) POOL-03: Ran baseline -- NO-GO (25.10% CV for 10-second candidate)
- [x] (2026-04-13) POOL-01R: Revised spec with 1-second and 2-second candidates
- [x] (2026-04-13) POOL-02R: Ran revised sweep -- 1-second candidate passes (max CV 8.06%)
- [x] (2026-04-13) POOL-03R: Decision: GO on 1-second candidate

## Surprises & Discoveries

- Observation: The original 10-second / 720-share candidate failed badly (CV 25.10%). This was not expected from the initial protocol sketch.
  Evidence: This numbered gate records the NO-GO decision; the current generated tree does not contain a separate `003-decision-report.md` artifact.

- Observation: The 2-second candidate passes seed 42 but fails one of the stress seeds (max CV 10.33%).
  Evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`

- Observation: The 1-second candidate passes all 21 seeds with max CV 8.06%.
  Evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`

## Decision Log

- Decision: NO-GO on 10-second / 720-share baseline.
  Rationale: CV 25.10% exceeds 10% threshold.
  Date/Author: 2026-04-13 / POOL-03

- Decision: GO on 1-second / 7200-share candidate. Constants promoted to `[CONFIRMED]` in `specs/sharepool.md`.
  Rationale: Max CV 8.06% across 20 stress seeds. Withholding advantage 0.00%. Only candidate that passes all required gates.
  Date/Author: 2026-04-13 / POOL-03R

## Outcomes & Retrospective

Complete. The revision loop worked as designed: the initial candidate failed, revised candidates were tested, and the surviving candidate was promoted. The explicit decision gate prevented premature implementation of bad constants.

## Context and Orientation

This numbered ExecPlan is the consolidated decision record for the original NO-GO and revised GO outcomes. The current generated tree does not contain separate `003-decision-report.md` or `003r-decision-report.md` artifacts. Evidence is in `contrib/sharepool/reports/pool-02r-revised-sweep.json`; confirmed constants are in `specs/sharepool.md`.

## Plan of Work

1. Run the simulator with the proposed constants.
2. Compare results against the decision thresholds.
3. If any threshold is exceeded, return to Plan 002 for revision.
4. Commit the decision report with evidence.

## Implementation Units

### Unit 1: Decision evaluation
- Goal: Evaluate simulator evidence against thresholds
- Requirements advanced: R1, R2, R3
- Dependencies: Plan 002 evidence
- Files to create or modify: `genesis/plans/003-decision-gate-simulator-results.md`, `contrib/sharepool/reports/pool-02r-revised-sweep.json`
- Tests to add or modify: Test expectation: none -- decision gate, no code changes
- Approach: Read evidence, compare against thresholds, record decision
- Specific test scenarios: Not applicable

## Concrete Steps

    cd contrib/sharepool && python3 simulate.py --sweep revised-candidates
    grep "CONFIRMED" specs/sharepool.md | wc -l

Expected: Sweep output matches committed evidence. At least 4 confirmed constants.

## Validation and Acceptance

The decision report is committed. The constants in `specs/sharepool.md` are either confirmed or explicitly rejected with evidence.

## Idempotence and Recovery

The simulator is deterministic. The decision can be re-evaluated from the committed evidence at any time.

## Artifacts and Notes

- Consolidated decision record: `genesis/plans/003-decision-gate-simulator-results.md`
- Evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`

## Interfaces and Dependencies

- Depends on Plan 002 simulator and evidence
- No code dependencies
