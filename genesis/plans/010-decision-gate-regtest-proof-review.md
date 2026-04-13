# Decision Gate: Regtest Proof Review

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This is a decision gate. Before deploying to a multi-day devnet or preparing mainnet activation, the regtest proof must demonstrate that the sharepool lifecycle is correct and complete. This gate reviews the Plan 009 results and decides whether to proceed with devnet deployment.

After this gate passes, the sharepool implementation is considered regtest-proven and ready for adversarial testing on a longer-running network.

## Requirements Trace

`R1`. The regtest end-to-end test (Plan 009) must pass reproducibly.

`R2`. All sharepool unit tests must pass.

`R3`. All pre-existing tests (Bitcoin Core inherited + RNG-specific) must pass without regression.

`R4`. The reward distribution must be within ±5% of expected proportions.

`R5`. No consensus-level bugs discovered during the regtest proof that require protocol changes.

## Scope Boundaries

This plan reviews results and records a decision. It does not write new code. If the decision is NO-GO, it triggers fixes that loop back to Plans 007-009 before proceeding.

## Progress

- [ ] Run full test suite including e2e proof
- [ ] Review results against decision criteria
- [ ] Record GO/NO-GO decision

## Surprises & Discoveries

None yet.

## Decision Log

None yet -- awaiting regtest proof results.

## Outcomes & Retrospective

Not yet started.

## Context and Orientation

This gate sits between Plan 009 (regtest proof) and Plan 011 (devnet deployment). It ensures that the regtest proof is complete, reproducible, and clean before committing to a longer devnet run.

## Plan of Work

1. Run the full test suite:
   - All C++ unit tests
   - All Python functional tests including `feature_sharepool_e2e.py`
   - Settlement model self-test
   - Simulator tests
2. Review test results for regressions.
3. Review reward distribution measurements against ±5% tolerance.
4. Record decision.

## Implementation Units

### Unit 1: Test suite execution and review
- Goal: Comprehensive test validation and decision
- Requirements advanced: R1, R2, R3, R4, R5
- Dependencies: Plan 009 (e2e test must exist)
- Files to create or modify: Decision report to be created at this checkpoint
- Tests to add or modify: Test expectation: none -- review checkpoint, no code changes
- Approach: Execute tests, evaluate results, record decision
- Specific test scenarios: Not applicable

## Concrete Steps

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin
    python3 test/functional/test_runner.py --configfile=build/test/config.ini
    python3 test/functional/feature_sharepool_e2e.py --configfile=build/test/config.ini
    python3 contrib/sharepool/settlement_model.py --self-test
    python3 -m pytest contrib/sharepool/test_simulate.py

## Validation and Acceptance

All tests pass. The e2e proof demonstrates the full lifecycle. No regressions in pre-existing tests. Decision recorded as GO or NO-GO with evidence.

## Idempotence and Recovery

Tests are non-destructive and can be rerun. The decision can be re-evaluated from test output.

## Artifacts and Notes

Decision report to be created during this checkpoint with test output evidence.

## Interfaces and Dependencies

- Plan 009 (e2e test)
- All Plans 002-008 (full sharepool implementation)
- CMake build system
- Python 3 functional test framework
