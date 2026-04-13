# Mainnet Activation Preparation

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan changes `DEPLOYMENT_SHAREPOOL` from `NEVER_ACTIVE` to a live mainnet activation window, updates all operator documentation, and cuts a release tag. After this plan, mainnet miners running the new release will begin signaling for sharepool activation, and the protocol will activate when the 95% threshold (1916/2016 blocks) is met.

This is the final plan in the sharepool sequence. Everything before this point has been regtest or devnet. This plan makes the protocol change live on the real network.

## Requirements Trace

`R1`. `DEPLOYMENT_SHAREPOOL` in `src/kernel/chainparams.cpp` must be changed from `NEVER_ACTIVE` to a concrete `nStartTime` and `nTimeout`.

`R2`. The activation window must give operators reasonable time to upgrade (recommendation: at least 4 weeks from release to `nStartTime`).

`R3`. All operator documentation must be updated: README, specs, operator onboarding guides.

`R4`. `scripts/doctor.sh` must check sharepool deployment status.

`R5`. A release tag must be cut with the activation parameters.

`R6`. Bootstrap assets should be refreshed to a recent height.

`R7`. The release includes the full sharepool implementation (Plans 007-008) and the proven e2e test (Plan 009).

## Scope Boundaries

This plan does not implement new features. It changes activation parameters, updates documentation, and cuts a release. If the devnet proof (Plan 011) revealed issues that require code changes, those changes must be landed before this plan begins.

This plan has significant unresolved details that depend on the devnet findings. The exact `nStartTime` and `nTimeout` depend on when the devnet stability report is complete and how long the operator upgrade window should be. These values are treated as open questions until Plan 011 produces its stability report.

## Progress

- [ ] Choose activation parameters (`nStartTime`, `nTimeout`)
- [ ] Update `src/kernel/chainparams.cpp`
- [ ] Update operator documentation
- [ ] Refresh bootstrap assets
- [ ] Extend `scripts/doctor.sh` with sharepool checks
- [ ] Cut release tag
- [ ] Communicate to operators

## Surprises & Discoveries

None yet.

## Decision Log

None yet -- blocked on Plan 011 (devnet stability report).

## Outcomes & Retrospective

Not yet started.

## Context and Orientation

`src/kernel/chainparams.cpp` sets per-network deployment parameters for `DEPLOYMENT_SHAREPOOL`. Currently all networks use `NEVER_ACTIVE`. Regtest uses activatable parameters with `-vbparams`. Mainnet parameters must be set to a future timestamp with appropriate signaling window.

Operator documentation is spread across: `README.md`, `specs/*.md`, `doc/qsb-operations.md`, `scripts/doctor.sh`, `specs/120426-operator-onboarding.md`. All must be updated to describe sharepool behavior and the activation timeline.

The release process is defined by `scripts/build-release.sh` and `.github/workflows/release.yml`. A tag triggers the release workflow which builds cross-platform tarballs.

## Plan of Work

1. Choose `nStartTime` (earliest activation date) and `nTimeout` (deadline for activation).
2. Set the parameters in `src/kernel/chainparams.cpp` for mainnet, testnet, testnet4, and signet.
3. Update `README.md` with sharepool activation status.
4. Update `specs/sharepool.md` with activation parameters.
5. Update `specs/120426-operator-onboarding.md` with upgrade instructions.
6. Extend `scripts/doctor.sh` to report sharepool deployment state.
7. Refresh bootstrap assets to a recent height.
8. Run the full test suite one final time.
9. Tag the release and push.
10. Communicate to operators via existing channels.

## Implementation Units

### Unit 1: Activation parameters
- Goal: Set mainnet activation window
- Requirements advanced: R1, R2
- Dependencies: Plan 011 (devnet stability report)
- Files to create or modify: `src/kernel/chainparams.cpp`
- Tests to add or modify: `build/bin/test_bitcoin --run_test=versionbits_tests`
- Approach: Set nStartTime to ~4 weeks after release, nTimeout to ~6 months after start
- Specific test scenarios: versionbits_tests pass with new parameters; getdeploymentinfo shows sharepool with start time

### Unit 2: Documentation update
- Goal: All docs reflect sharepool activation
- Requirements advanced: R3, R4
- Dependencies: Unit 1
- Files to create or modify: `README.md`, `specs/sharepool.md`, `specs/120426-operator-onboarding.md`, `scripts/doctor.sh`
- Tests to add or modify: Test expectation: none -- documentation
- Approach: Update each document to reflect activation timeline and sharepool behavior
- Specific test scenarios: doctor.sh reports sharepool deployment state

### Unit 3: Release
- Goal: Tagged release with activation parameters
- Requirements advanced: R5, R6, R7
- Dependencies: Units 1 and 2, full test suite pass
- Files to create or modify: Git tag, release artifacts
- Tests to add or modify: Full test suite
- Approach: Refresh bootstrap, run tests, tag, push
- Specific test scenarios: Release workflow produces tarballs; verify-release.sh passes

## Concrete Steps

    # Update chainparams
    # (edit src/kernel/chainparams.cpp with chosen timestamps)

    cmake --build build -j$(nproc)
    build/bin/test_bitcoin --run_test=versionbits_tests

    # Update docs
    # (edit README.md, specs, scripts)

    # Refresh bootstrap
    scripts/load-bootstrap.sh --help

    # Final test suite
    build/bin/test_bitcoin
    python3 test/functional/test_runner.py --configfile=build/test/config.ini

    # Tag and release
    git tag -a v4.0.0 -m "Sharepool activation release"
    git push origin v4.0.0

## Validation and Acceptance

1. `rngd --version` reports the new version with sharepool activation parameters.
2. `rng-cli getdeploymentinfo` shows `sharepool` with the configured start time and timeout.
3. `scripts/doctor.sh` reports sharepool deployment status.
4. All tests pass.
5. Release tarballs are produced by the CI workflow.

## Idempotence and Recovery

The activation parameters are a configuration change. If the wrong timestamps are chosen, a new release can be cut with corrected parameters before `nStartTime`. After `nStartTime`, the BIP9 mechanism ensures that activation only happens if 95% of miners signal support. If activation stalls or is rejected, the timeout mechanism reverts to `NEVER_ACTIVE` equivalent behavior.

## Artifacts and Notes

To be filled with chosen timestamps, release tag hash, and operator communication evidence.

## Interfaces and Dependencies

- Plan 011 (devnet stability report must be complete and GO)
- `src/kernel/chainparams.cpp` (activation parameters)
- `scripts/build-release.sh` (release pipeline)
- `.github/workflows/release.yml` (CI release workflow)
- All operator documentation files
