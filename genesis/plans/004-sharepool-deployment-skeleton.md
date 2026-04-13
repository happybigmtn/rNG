# Sharepool BIP9 Deployment Skeleton

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

Add the BIP9 versionbits deployment boundary for sharepool so that all subsequent consensus code can be gated behind `DeploymentActiveAt(DEPLOYMENT_SHAREPOOL)`. After this plan, a regtest node can activate the sharepool deployment, and future plans can add activation-gated behavior incrementally.

## Requirements Trace

`R1`. `DEPLOYMENT_SHAREPOOL` must exist in `src/consensus/params.h` with bit 3.

`R2`. Mainnet must be `NEVER_ACTIVE`. Regtest must be activatable via `-vbparams=sharepool:0:9999999999:0`.

`R3`. Threshold must be 1916/2016 (95%), period 2016.

`R4`. Existing deployments (testdummy, taproot) must be unchanged.

## Scope Boundaries

This plan adds only the deployment enum and chainparams wiring. No consensus rules, P2P messages, or mining behavior changes.

## Progress

- [x] (2026-04-13) POOL-04: Added `DEPLOYMENT_SHAREPOOL` to `src/consensus/params.h`
- [x] (2026-04-13) POOL-04: Wired deployment into all network chainparams with confirmed parameters
- [x] (2026-04-13) POOL-04: `versionbits_tests` pass

## Surprises & Discoveries

- Observation: Older planning text used 1815/2016 as "95%" which is actually ~90%. The correct 95% threshold is 1916/2016.
  Evidence: `specs/sharepool.md` confirmed constant table

## Decision Log

- Decision: Use `NEVER_ACTIVE` on all non-regtest networks until later plans explicitly change activation parameters.
  Rationale: Prevents accidental activation before the protocol is proven.
  Date/Author: 2026-04-13 / POOL-04

## Outcomes & Retrospective

Complete. The deployment boundary exists and regtest can activate it. All versionbits tests pass.

## Context and Orientation

`src/consensus/params.h` defines the `DeploymentPos` enum. `src/kernel/chainparams.cpp` sets per-network deployment parameters. `src/deploymentinfo.cpp` names deployments. A deployment is active when signaled blocks meet the threshold within a period.

## Plan of Work

1. Add `DEPLOYMENT_SHAREPOOL` to the enum in `src/consensus/params.h`.
2. Wire deployment parameters in `src/kernel/chainparams.cpp` for all networks.
3. Add deployment name in `src/deploymentinfo.cpp`.
4. Verify with `versionbits_tests`.

## Implementation Units

### Unit 1: Deployment boundary
- Goal: BIP9 deployment exists and is activatable on regtest
- Requirements advanced: R1, R2, R3, R4
- Dependencies: None
- Files to create or modify: `src/consensus/params.h`, `src/kernel/chainparams.cpp`, `src/deploymentinfo.cpp`
- Tests to add or modify: `build/bin/test_bitcoin --run_test=versionbits_tests`
- Approach: Add enum value, wire params, add name string
- Specific test scenarios: Existing versionbits tests still pass; regtest can activate sharepool

## Concrete Steps

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=versionbits_tests
    grep "DEPLOYMENT_SHAREPOOL" src/consensus/params.h

## Validation and Acceptance

`versionbits_tests` pass. `DEPLOYMENT_SHAREPOOL` exists in `params.h`. `getdeploymentinfo` on regtest shows `sharepool` as a known deployment.

## Idempotence and Recovery

Adding an enum value is additive. Reverting the three files restores the previous state.

## Artifacts and Notes

None beyond the committed code.

## Interfaces and Dependencies

- `src/consensus/params.h`: New enum value
- `src/kernel/chainparams.cpp`: Per-network deployment parameters
- `src/deploymentinfo.cpp`: Human-readable name
