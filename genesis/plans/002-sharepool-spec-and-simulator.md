# Sharepool Specification and Economic Simulator

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

Before writing any consensus code, RNG needs a precise protocol specification and an economic simulator that validates the sharepool constants. After this plan, the sharepool protocol is written down, the simulator can test any candidate constant set, and the decision gate at Plan 003 has the evidence it needs to decide whether the chosen constants produce acceptable variance and withholding properties.

A user or operator gains: the ability to review and challenge the sharepool protocol design before any irreversible consensus code is written.

## Requirements Trace

`R1`. The specification must define share objects, sharechain rules, reward window construction, payout commitment encoding, claim program sketch, activation semantics, and P2P relay rules.

`R2`. The simulator must compute reward splits from share traces, verify deterministic replay (byte-identical commitment roots), measure withholding advantage, and measure 10%-miner variance across 100 blocks.

`R3`. All constants must be explicitly labeled as `[PROPOSED]` or `[CONFIRMED]` depending on whether the decision gate has approved them.

## Scope Boundaries

This plan produces specifications and simulation tooling. It does not change any C++ source code, add any consensus rules, or modify any P2P behavior. The existing codebase is unchanged by this plan.

## Progress

- [x] (2026-04-13) POOL-01: Wrote `specs/sharepool.md` with proposed constants
- [x] (2026-04-13) POOL-02: Built `contrib/sharepool/simulate.py` with reward-window, payout-leaf, commitment-root, withholding, and variance metrics
- [x] (2026-04-13) POOL-02: Added `contrib/sharepool/test_simulate.py` with deterministic test coverage
- [x] (2026-04-13) POOL-01R: Revised spec after POOL-03 no-go, added 1-second and 2-second candidates
- [x] (2026-04-13) POOL-02R: Added revised-constants sweep, committed evidence in `contrib/sharepool/reports/pool-02r-revised-sweep.json`

## Surprises & Discoveries

- Observation: The initial share-target ratio formula in older planning text (`block_target / 12`) was reversed for Bitcoin-style target arithmetic. RNG accepts hashes `<= target`, so easier shares need a larger target number. The correct formula is `share_target = min(powLimit, block_target * ratio)`.
  Evidence: `specs/sharepool.md`, "Share target ratio" confirmed constant

- Observation: The remainder distribution after integer division of rewards can produce off-by-one disagreements between implementations if not specified precisely. The spec now uses deterministic distribution by ascending `Hash(payout_script)`.
  Evidence: `specs/sharepool.md`, "Reward calculation" section

## Decision Log

- Decision: Produce the specification before the simulator, so the simulator tests the actual protocol rather than an informal sketch.
  Rationale: Ensures the simulator metrics are meaningful.
  Date/Author: 2026-04-13 / POOL-01

- Decision: Label all constants as `[PROPOSED]` until the decision gate passes.
  Rationale: Prevents premature implementation of unvalidated constants.
  Date/Author: 2026-04-13 / POOL-01

## Outcomes & Retrospective

Complete. The spec exists at `specs/sharepool.md` and the simulator at `contrib/sharepool/simulate.py`. The initial 10-second candidate failed the decision gate, but the revision loop (POOL-01R/02R/03R) confirmed the 1-second candidate. All constants are now `[CONFIRMED]`.

Lesson: Starting with a no-go result and revising was better than choosing the wrong constants and discovering the problem in consensus code.

## Context and Orientation

`specs/sharepool.md` is the top-level sharepool protocol specification. `contrib/sharepool/simulate.py` is the economic simulator. `contrib/sharepool/test_simulate.py` provides pytest coverage. Example traces live in `contrib/sharepool/examples/`. The simulator can be run with `python3 contrib/sharepool/simulate.py --scenario baseline` or `--sweep revised-candidates`.

## Plan of Work

1. Write the sharepool spec covering all protocol surfaces (share object, sharechain, reward window, commitment, claim, activation, relay).
2. Build the simulator that accepts JSON/CSV share traces and computes all required metrics.
3. Run the baseline scenario and record evidence.
4. If the decision gate rejects the baseline, revise constants and rerun.

## Implementation Units

### Unit 1: Protocol specification
- Goal: Complete sharepool spec
- Requirements advanced: R1, R3
- Dependencies: None
- Files to create or modify: `specs/sharepool.md` (create)
- Tests to add or modify: `grep "PROPOSED" specs/sharepool.md | wc -l` (at least 4 constants)
- Approach: Write spec from first principles, define every term, mark constants as proposed
- Specific test scenarios: Verify spec file exists with the required constant labels

### Unit 2: Economic simulator
- Goal: Deterministic simulator with all required metrics
- Requirements advanced: R2
- Dependencies: Unit 1
- Files to create or modify: `contrib/sharepool/simulate.py` (create), `contrib/sharepool/test_simulate.py` (create)
- Tests to add or modify: `python3 -m pytest contrib/sharepool/test_simulate.py`
- Approach: Implement reward-window walk, payout-leaf construction, Merkle commitment, withholding measurement, variance measurement
- Specific test scenarios: Two-miner 90/10 split produces proportional rewards; deterministic replay produces identical roots; withholding metric stays below threshold

## Concrete Steps

    python3 -m pytest contrib/sharepool/test_simulate.py
    cd contrib/sharepool && python3 simulate.py --trace examples/two_miners_90_10.json --verbose
    cd contrib/sharepool && python3 simulate.py --sweep revised-candidates
    grep "CONFIRMED" specs/sharepool.md | wc -l

Expected: All tests pass. The sweep shows 1-second candidate with CV < 10% across all seeds. At least 4 confirmed constants in the spec.

## Validation and Acceptance

The spec file exists with confirmed constants. The simulator produces deterministic output. The evidence JSON is committed. This plan is validated by the downstream decision gate (Plan 003).

## Idempotence and Recovery

All artifacts are files. Rerunning the simulator produces identical output (deterministic seed). The spec can be revised without affecting any C++ code.

## Artifacts and Notes

- Spec: `specs/sharepool.md`
- Simulator: `contrib/sharepool/simulate.py`
- Evidence: `contrib/sharepool/reports/pool-02r-revised-sweep.json`
- Example trace: `contrib/sharepool/examples/two_miners_90_10.json`

## Interfaces and Dependencies

- Python 3 runtime for the simulator
- No C++ dependencies
- No external libraries beyond Python stdlib
