# Devnet Deployment and Adversarial Testing

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.

## Purpose / Big Picture

This plan deploys the sharepool-enabled RNG build to a multi-node devnet and runs adversarial scenarios that cannot be tested on a single-machine regtest network. After this plan, the sharepool protocol has survived sustained operation (48+ hours) under hostile conditions: share withholding, eclipse attacks, relay spam, reorg behavior, and claim abuse.

The devnet is not mainnet. It uses a separate genesis and can be destroyed and recreated without consequence. Its purpose is to surface bugs and attack vectors before mainnet activation.

## Requirements Trace

`R1`. Devnet runs 4+ nodes across at least 2 independent hosts.

`R2`. Devnet runs continuously for 48+ hours with sharepool active.

`R3`. Adversarial scenarios are executed and documented: share withholding, eclipse attack, relay spam, orphan flooding, claim front-running, settlement draining attempt.

`R4`. No consensus-breaking bugs or exploitable attack vectors found, or fixes applied before proceeding.

`R5`. Stability report committed with measurement data.

## Scope Boundaries

This plan covers devnet deployment and adversarial testing. It does not change mainnet activation parameters (Plan 012). It does not change consensus rules unless bugs are found that require fixes. It does not test mainnet-specific conditions like real hashrate distribution.

This plan has unresolved feasibility in its adversarial testing scope. The exact adversarial scenarios depend on the attack surface revealed by the regtest proof. The scenarios listed here are the expected minimum set, but additional scenarios may be added based on Plan 010 review findings. This plan should remain research-shaped until the regtest proof establishes the concrete attack surface.

## Progress

- [ ] Provision devnet infrastructure
- [ ] Deploy sharepool-enabled build
- [ ] Run stability baseline (24h nominal operation)
- [ ] Execute adversarial scenarios
- [ ] Document results
- [ ] Commit stability report

## Surprises & Discoveries

None yet.

## Decision Log

None yet -- blocked on Plan 010 (regtest proof review).

## Outcomes & Retrospective

Not yet started.

## Context and Orientation

Devnet infrastructure does not yet exist. The existing Contabo fleet runs mainnet. A separate set of nodes (or temporary reuse of non-mining slots) is needed for devnet. The devnet should use a fresh genesis and a short activation window so sharepool activates quickly.

Adversarial testing may use modified node binaries or test scripts that simulate malicious behavior. The existing functional test framework can be adapted for some scenarios, but multi-host coordination may require additional scripting.

Lessons from Zend reference repo: The rBTC pool in Zend encountered practical failure modes including tarfile extraction safety issues, type-checking failures in pool tools, operator documentation drift, and inactivity timeout enforcement gaps. While these are application-layer concerns rather than consensus issues, they highlight the importance of testing operational paths alongside protocol correctness.

## Plan of Work

### Phase 1: Infrastructure and deployment

Provision 4+ nodes across at least 2 hosts. Configure devnet genesis with a short BIP9 activation window. Deploy the sharepool-enabled binary. Verify basic operation: nodes sync, mine blocks, produce shares, relay shares, commit settlements.

### Phase 2: Stability baseline

Run the devnet for 24 hours under nominal conditions (all nodes honest, mining continuously). Monitor: block production rate, share production rate, relay latency, orphan rate, settlement commitment correctness, memory and storage growth.

### Phase 3: Adversarial scenarios

Each scenario should be documented with: hypothesis, test setup, expected behavior, actual behavior, and verdict.

1. **Share withholding**: A miner with 25% hashrate withholds some shares. Measure whether the withholding advantage exceeds the 5% threshold validated by the simulator.

2. **Eclipse attack**: Isolate one node from honest peers. Feed it shares from a controlled attacker. Verify that the isolated node's reward window does not diverge permanently from the honest network after reconnection.

3. **Relay spam**: Flood one node with invalid shares (wrong RandomX proof, wrong target, garbage data). Verify that misbehavior scoring correctly disconnects the attacker and that the victim node's performance does not degrade.

4. **Orphan flooding**: Send shares with nonexistent parents to fill the 64-entry orphan buffer. Verify FIFO eviction works and does not cause memory growth or performance degradation.

5. **Claim front-running**: Two parties race to claim the same settlement leaf. Verify that exactly one claim succeeds and the other is rejected (UTXO single-spend prevents double claims).

6. **Settlement draining**: Attempt to construct a claim that extracts more value than the committed leaf amount. Verify consensus rejection.

7. **Reorg with shares**: Mine a short fork (2-3 blocks) while shares are being produced on the main chain. Verify that the reward window correctly adjusts after the reorg and that shares anchored to the abandoned fork are excluded.

### Phase 4: Results and report

Document all findings. If any scenario reveals a bug or exploitable vector, fix it, re-run the affected scenario, and update the report.

## Implementation Units

### Unit 1: Devnet infrastructure
- Goal: 4+ node devnet with sharepool active
- Requirements advanced: R1
- Dependencies: Plan 010 (regtest proof review GO)
- Files to create or modify: Devnet configuration files, deployment scripts
- Tests to add or modify: Test expectation: none -- infrastructure, no code changes
- Approach: Provision nodes, deploy binary, configure devnet genesis
- Specific test scenarios: Nodes sync and produce blocks with sharepool active

### Unit 2: Stability baseline
- Goal: 24h nominal operation
- Requirements advanced: R2
- Dependencies: Unit 1
- Files to create or modify: Monitoring scripts, baseline report
- Tests to add or modify: Test expectation: none -- measurement, no code changes
- Approach: Run devnet for 24h, collect metrics
- Specific test scenarios: Block production, share production, relay latency, storage growth all within expected bounds

### Unit 3: Adversarial scenarios
- Goal: Execute and document adversarial tests
- Requirements advanced: R3, R4
- Dependencies: Unit 2
- Files to create or modify: Adversarial test scripts, scenario reports
- Tests to add or modify: Scenario-specific assertions
- Approach: Modified binaries or scripts simulate each attack
- Specific test scenarios: Seven scenarios listed in Phase 3

### Unit 4: Stability report
- Goal: Committed report with all findings
- Requirements advanced: R5
- Dependencies: Units 2 and 3
- Files to create or modify: Stability report in `genesis/plans/` or `contrib/sharepool/reports/`
- Tests to add or modify: Test expectation: none -- report, no code changes
- Approach: Compile results, record verdicts
- Specific test scenarios: Not applicable

## Concrete Steps

These will be defined concretely after devnet infrastructure decisions are made. Placeholder:

    # Deploy to devnet
    # (specific commands depend on infrastructure choice)

    # Run stability baseline
    # Monitor for 24 hours

    # Execute adversarial scenarios
    # Document results

    # Commit stability report

## Validation and Acceptance

1. Devnet ran for 48+ hours total (24h baseline + adversarial phase).
2. All adversarial scenarios executed and documented.
3. No unresolved consensus bugs or exploitable attack vectors.
4. Stability report committed.

## Idempotence and Recovery

The devnet can be destroyed and recreated at any time. It has no mainnet consequences. Failed scenarios can be re-run after fixes. The stability report is cumulative.

## Artifacts and Notes

To be filled with devnet addresses, binary hashes, monitoring data, and scenario results.

## Interfaces and Dependencies

- Plan 010 (regtest proof review must be GO)
- All Plans 002-009 (full sharepool implementation)
- Devnet infrastructure (to be provisioned)
- Monitoring tools (to be determined)
