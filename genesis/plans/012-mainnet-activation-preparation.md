# Mainnet Activation Preparation

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root. It is plan 012 in the `genesis/plans/` corpus indexed by `genesis/PLANS.md`.


## Purpose / Big Picture

This plan prepares everything needed to activate protocol-native pooled mining on the RNG mainnet. It does not activate the feature. The actual activation requires operator approval of the parameters chosen here, followed by a fleet deployment and version-bits signaling period. This plan produces the activation parameters, updated documentation, an operator guide, a fleet rollout plan, and a release candidate binary.

After this plan completes, an operator will have: a build of RNG that includes sharepool support with real mainnet activation parameters, documentation that accurately describes the new mining behavior, a guide for running a sharepool-enabled node, and a step-by-step rollout plan for the first operator fleet wave. The operator then decides when and whether to deploy.

This plan is deliberately research-shaped. The activation parameters (start time, timeout, minimum activation height) are recommendations based on devnet experience from Plan 011, not locked values. The operator must review and approve them. The plan includes an explicit decision gate: no mainnet deployment begins until the operator signs off on the parameters in the Decision Log.

For users, the visible change after mainnet activation would be the shift from "block finder takes all" to "proportional pooled rewards." But that visible change is not part of this plan. This plan only ensures that the code, documentation, and operational procedures are ready for the operator to make that decision.


## Requirements Trace

`R8`. Before activation, existing RNG behavior remains unchanged. Existing blocks, mining RPCs, wallet flows, and tests must continue to work until the new deployment is explicitly activated via version-bits signaling. This plan must verify that the mainnet parameters leave pre-activation behavior intact.

`R9`. Activation must be staged through RNG's existing version-bits infrastructure first on regtest, then on devnet, then on mainnet. This plan is the mainnet preparation stage. It follows successful devnet validation in Plan 011.


## Scope Boundaries

This plan does not activate sharepool on mainnet. It sets the activation parameters and prepares the deployment artifacts. The actual activation happens through version-bits signaling after the prepared binary is deployed and miners begin signaling.

This plan does not deploy binaries to any operator fleet. It produces the rollout plan and the release candidate. The actual deployment is an operational step that follows operator approval.

This plan does not change consensus rules beyond setting the activation parameters. The sharepool consensus logic was implemented in Plans 005, 007, and 008. This plan only sets the mainnet-specific timing and threshold values.

This plan does not reduce coinbase maturity. The existing 100-confirmation maturity rule remains unchanged. Pending pooled reward is visible before maturity, but claim spends still require maturity. Any change to maturity is a separate future consensus proposal.

This plan does not promise that the activation will succeed. If miners do not signal support during the signaling period, the deployment times out and sharepool remains inactive. That outcome is acceptable and does not indicate a plan failure.


## Progress

- [ ] Set DEPLOYMENT_SHAREPOOL parameters for mainnet in `src/kernel/chainparams.cpp`.
- [ ] Update `specs/consensus.md` with sharepool activation parameters.
- [ ] Update `specs/activation.md` with DEPLOYMENT_SHAREPOOL entry.
- [ ] Update `specs/agent-integration.md` to reflect protocol-native pool mining.
- [ ] Update `README.md` mining documentation.
- [ ] Write `docs/sharepool-operator-guide.md`.
- [ ] Prepare fleet rollout plan for the first operator validator wave.
- [ ] Build release candidate binary with sharepool support.
- [ ] Record operator approval decision in Decision Log.


## Surprises & Discoveries

No discoveries yet. This section will be populated during parameter selection and documentation work.


## Decision Log

No decisions yet. The critical decision this plan produces is the operator's approval (or rejection) of the mainnet activation parameters. That decision and its rationale will be recorded here.


## Outcomes & Retrospective

Not yet applicable. This section will summarize the preparation outcome, any parameter adjustments made during review, and the final operator decision.


## Context and Orientation

This plan sits at the end of the genesis corpus dependency chain. It depends on every prior plan: the spec and simulator (Plan 002), the deployment skeleton (Plan 004), the sharechain implementation (Plan 005), the payout commitment and claim program (Plan 007), the miner and wallet integration (Plan 008), the regtest proof (Plan 009), the regtest review gate (Plan 010), and the devnet validation (Plan 011). If any of those plans produced findings that required design revisions, those revisions must have been completed and re-validated before this plan proceeds.

The key files this plan modifies are:

`src/kernel/chainparams.cpp` defines the network parameters for mainnet, testnet, signet, and regtest. This is where the `DEPLOYMENT_SHAREPOOL` version-bits parameters (start time, timeout, minimum activation height) and the sharepool consensus parameters (target share spacing, reward window work, claim witness version, maximum orphan shares) are set for each network. Currently, sharepool parameters exist only for regtest (added by Plan 004). This plan adds the mainnet values.

`specs/consensus.md` is the repository's canonical description of RNG's consensus rules. It must be updated to describe the sharepool payout commitment, the claim program, and the activation parameters.

`specs/activation.md` lists all version-bits deployments. It currently describes DEPLOYMENT_TESTDUMMY and DEPLOYMENT_TAPROOT. This plan adds DEPLOYMENT_SHAREPOOL with the chosen mainnet parameters.

`specs/agent-integration.md` describes how AI agents interact with RNG. It currently includes a "pool mining" mode that imagines an external pool URL and an `rng-cli pool-mine --pool ...` command. After sharepool activation, pool mining is protocol-native and does not require an external pool. This plan updates the spec to reflect the actual protocol-native design, replacing the imagined external pool surface with the real sharechain participation model.

`README.md` contains user-facing mining documentation. It must be updated to explain that after sharepool activation, mining produces shares by default and rewards are pooled proportionally.

The working tree includes a local root `EXECPLAN.md` that documents a Contabo/QSB rollout and a canary-first deployment pattern. That is useful local operational context, but it was not independently replayed as part of this document review and the corresponding QSB source files were not present in the inspected checkout. Treat that rollout pattern as optional precedent to validate before reuse, not as settled repository truth.

Several terms used in the activation parameters need definition.

"Start time" is the earliest Unix timestamp at which miners can begin signaling support for the sharepool deployment. Before this time, the deployment is in the DEFINED state and has no effect.

"Timeout" is the Unix timestamp after which the deployment gives up if it has not locked in. After timeout, the deployment enters the FAILED state and sharepool remains inactive unless a new deployment is defined.

"Minimum activation height" is the earliest block height at which the deployment can transition from LOCKED_IN to ACTIVE. This provides a grace period between lock-in and activation so operators can upgrade their nodes.

"Threshold" is the fraction of blocks in a signaling period that must signal support. The standard BIP9 threshold is 95% of blocks in each 2016-block period.

"Period" is the number of blocks in each signaling window. Bitcoin uses 2016 blocks (approximately 2 weeks at 10-minute blocks). RNG with 120-second blocks and a 2016-block period has a signaling window of approximately 2.8 days.


## Plan of Work

The work proceeds in three phases: parameter selection, documentation updates, and release preparation.

Begin with parameter selection. Based on devnet experience from Plan 011, choose the mainnet activation parameters. The start time should be far enough in the future to allow all operators to upgrade. A reasonable default is 4 weeks after the release candidate is tagged. The timeout should allow enough time for the signaling process to complete. A reasonable default is 6 months after the start time. The minimum activation height should be at least 2016 blocks (one signaling period) after the expected lock-in, giving operators time to verify their nodes are ready. Set the signaling threshold to 95% (the standard BIP9 value) and the period to 2016 blocks.

Write the parameter values into `src/kernel/chainparams.cpp` in the `CMainParams` constructor. The sharepool consensus parameters (target share spacing, reward window work, claim witness version, maximum orphan shares) should match the values validated on devnet. If devnet testing in Plan 011 revealed that any constant needed adjustment, use the adjusted value and document the change in the Decision Log.

Next, update the specification documents. In `specs/consensus.md`, add a section describing the sharepool payout commitment format, the claim program, and the consensus rules that activate with DEPLOYMENT_SHAREPOOL. In `specs/activation.md`, add a DEPLOYMENT_SHAREPOOL entry with the chosen parameters, following the format used for DEPLOYMENT_TAPROOT. In `specs/agent-integration.md`, replace the imagined external pool mining flow with the real protocol-native sharechain participation model. The updated spec should describe: how an agent starts mining and begins submitting shares, how it queries pending pooled reward via `getmininginfo`, how it builds claim transactions after maturity via wallet RPCs, and how it verifies its reward independently via `getrewardcommitment`. Remove or clearly mark as superseded any references to `pool-mine --pool ...` or external pool URLs.

Update `README.md` to reflect the new mining model. The existing mining documentation explains how to start the internal miner with `-mine -mineaddress=<addr>`. After sharepool activation, this same command causes the miner to produce shares instead of only searching for full blocks. The README should explain this behavior change, describe pending pooled reward, and point to the operator guide for details.

Write the operator guide at `docs/sharepool-operator-guide.md`. This guide should cover: what sharepool activation means for node operators, how to verify that a node is sharepool-ready, how to monitor share production and reward accrual, how to build and submit claim transactions, how the version-bits signaling process works, what happens if the deployment times out, and how to read the trust surface report from Plan 011.

Prepare the fleet rollout plan. A canary-first deployment shape is still the right default: deploy the release candidate to one validator first, verify that it starts, syncs, and reports the correct sharepool deployment parameters in `getdeploymentinfo`, then roll to the remaining validators one at a time. If the local root `EXECPLAN.md` canary procedure is still current when this plan is executed, reuse its backup and rollback pattern explicitly; otherwise write a fresh canary procedure here.

Finally, build the release candidate. Tag the commit, build stripped binaries, and record their SHA256 hashes. The release candidate is ready when it passes the full test suite, includes the mainnet activation parameters, and matches the documentation.


## Implementation Units

### Unit 1: Mainnet Activation Parameters

Goal: set the DEPLOYMENT_SHAREPOOL version-bits parameters and sharepool consensus parameters for mainnet.

Requirements advanced: `R8`, `R9`.

Dependencies: Plan 011 completed successfully.

Files to create or modify:

- `src/kernel/chainparams.cpp`

Tests to add or modify:

- `test/functional/feature_sharepool_activation.py` (extend with a mainnet-params check if not already present)

Approach: In the `CMainParams` constructor in `src/kernel/chainparams.cpp`, set the `DEPLOYMENT_SHAREPOOL` parameters: start time (Unix timestamp), timeout (Unix timestamp), minimum activation height, BIP9 bit position, threshold (95%), and period (2016). Set the sharepool consensus parameters to the values validated on devnet. The values are recommendations until the operator approves them.

Specific test scenarios:

On default mainnet startup (no `-vbparams` override), `getdeploymentinfo` reports the sharepool deployment in DEFINED state before the start time. On regtest with `-vbparams=sharepool:0:9999999999:0`, sharepool activates immediately and existing regtest tests continue to pass. The full test suite passes with the new mainnet parameters present in the binary.

### Unit 2: Specification Updates

Goal: bring `specs/consensus.md`, `specs/activation.md`, and `specs/agent-integration.md` up to date with the sharepool protocol.

Requirements advanced: `R8`, `R9`.

Dependencies: Unit 1 (parameters must be finalized first).

Files to create or modify:

- `specs/consensus.md`
- `specs/activation.md`
- `specs/agent-integration.md`

Tests to add or modify: Test expectation: none -- this unit produces documentation.

Approach: In `specs/consensus.md`, add a "Sharepool" section describing the payout commitment, reward window, and claim rules. In `specs/activation.md`, add a DEPLOYMENT_SHAREPOOL row with the exact parameters from Unit 1. In `specs/agent-integration.md`, replace the external-pool mining flow with the protocol-native sharechain flow. Be explicit about what changed: the old `pool-mine --pool ...` concept is superseded by native share submission through the standard mining path.

Specific test scenarios:

A reader who follows the agent integration spec's mining instructions can start mining shares on regtest without referencing any other document. The spec does not describe features that do not exist (no phantom pool URLs, no unimplemented RPCs).

### Unit 3: User Documentation Updates

Goal: update README.md and write the operator guide.

Requirements advanced: `R9`, `R11`.

Dependencies: Units 1 and 2.

Files to create or modify:

- `README.md`
- `docs/sharepool-operator-guide.md` (new)

Tests to add or modify: Test expectation: none -- this unit produces documentation.

Approach: In `README.md`, update the mining section to explain that after sharepool activation, the internal miner submits shares by default and rewards are pooled proportionally. Keep the existing mining commands (`-mine`, `-mineaddress`, `-minethreads`) unchanged since they still work. Add a brief note about pending pooled reward and point to the operator guide. Write `docs/sharepool-operator-guide.md` covering activation monitoring, share production, reward tracking, claim building, signaling, timeout behavior, and the trust surface.

Specific test scenarios:

The README accurately describes post-activation mining behavior without making claims about features that are not implemented. The operator guide can be followed by someone who has never operated a sharepool node.

### Unit 4: Fleet Rollout Plan and Release Candidate

Goal: produce a deployable release candidate and a step-by-step rollout plan for the first operator fleet wave.

Requirements advanced: `R9`.

Dependencies: Units 1, 2, and 3.

Files to create or modify:

- `docs/sharepool-fleet-rollout-plan.md` (new, or incorporated into the operator guide)

Tests to add or modify: Test expectation: none -- this unit produces a rollout plan and build artifacts.

Approach: Tag the release candidate commit. Build stripped binaries and record SHA256 hashes. Write the rollout plan following a canary pattern: deploy to one validator, verify health (genesis hash, deployment info, peer connections, sync state), then roll to the others one at a time. If the local root `EXECPLAN.md` backup commands are still current when this plan is executed, adopt them explicitly; otherwise define fresh backup and rollback commands here.

Specific test scenarios:

The release candidate passes the full test suite. The stripped binaries report the correct version and include DEPLOYMENT_SHAREPOOL in `getdeploymentinfo`. The rollout plan includes explicit verification commands for each deployment step and explicit rollback commands if any step fails.


## Concrete Steps

All commands assume the working directory is the repository root.

1. Set activation parameters in chainparams.

   Edit `src/kernel/chainparams.cpp` and add in the `CMainParams` constructor, after the existing deployments:

       consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].bit = <bit>;
       consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = <unix-timestamp>;
       consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = <unix-timestamp>;
       consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].min_activation_height = <height>;

   The exact values depend on devnet results and operator discussion. Placeholder reasoning: start time 4 weeks after RC tag, timeout 6 months later, minimum activation height at current tip plus 4032 (two signaling periods).

2. Build and test.

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli test_bitcoin
       build/src/test/test_bitcoin
       test/functional/test_runner.py

   Expected outcome: all tests pass, including the sharepool functional tests.

3. Verify mainnet parameters.

       build/src/rngd -datadir=/tmp/rng-mainnet-param-check -daemon
       build/src/rng-cli -datadir=/tmp/rng-mainnet-param-check getdeploymentinfo

   Expected outcome: output shows `sharepool` deployment in DEFINED state with the correct start time and timeout.

       build/src/rng-cli -datadir=/tmp/rng-mainnet-param-check stop

4. Update specs and documentation. Edit the files listed in Units 2 and 3. After editing, verify that no spec references unimplemented features.

5. Tag the release candidate.

       git tag -a v3.1.0-rc1 -m "Release candidate with sharepool activation parameters"

6. Build stripped release artifacts.

       strip -o /tmp/rng-sharepool-rc/rngd build/src/rngd
       strip -o /tmp/rng-sharepool-rc/rng-cli build/src/rng-cli
       sha256sum /tmp/rng-sharepool-rc/rngd /tmp/rng-sharepool-rc/rng-cli

   Record the hashes in the rollout plan.

7. Write the fleet rollout plan. If the local root `EXECPLAN.md` canary procedure is still current, adapt it explicitly. Otherwise, author a fresh canary sequence appropriate to the actual operator fleet:

       # Canary on the first chosen validator
       ssh <validator-host> 'cp /path/to/rngd /path/to/rngd.pre-sharepool.$(date -u +%Y%m%dT%H%M%SZ)'
       ssh <validator-host> 'cp /path/to/rng-cli /path/to/rng-cli.pre-sharepool.$(date -u +%Y%m%dT%H%M%SZ)'
       scp /tmp/rng-sharepool-rc/rngd <validator-host>:/tmp/rngd.new
       scp /tmp/rng-sharepool-rc/rng-cli <validator-host>:/tmp/rng-cli.new
       ssh <validator-host> 'install -m 0755 /tmp/rngd.new /path/to/rngd && install -m 0755 /tmp/rng-cli.new /path/to/rng-cli'
       ssh <validator-host> 'systemctl restart rngd'
       ssh <validator-host> '<path-to-rng-cli> getdeploymentinfo'

   Expected outcome: `getdeploymentinfo` shows sharepool in DEFINED state with the correct parameters.

   Repeat for each remaining validator only after verifying the previous one.


## Validation and Acceptance

This plan is accepted when all of the following are true.

The `src/kernel/chainparams.cpp` file contains DEPLOYMENT_SHAREPOOL parameters for mainnet that have been reviewed and approved by the operator (approval recorded in Decision Log).

`specs/consensus.md` accurately describes the sharepool consensus rules, including the payout commitment format and claim program.

`specs/activation.md` includes a DEPLOYMENT_SHAREPOOL entry with the correct mainnet parameters.

`specs/agent-integration.md` describes protocol-native share-based mining and does not reference phantom external pool features.

`README.md` accurately describes post-activation mining behavior.

`docs/sharepool-operator-guide.md` exists and can be followed by a novice operator.

A fleet rollout plan exists with explicit deployment, verification, and rollback steps for each validator in the first operator wave.

A release candidate binary passes the full test suite and reports the correct DEPLOYMENT_SHAREPOOL parameters on mainnet startup.

The Decision Log contains the operator's explicit approval (or rejection with rationale) of the activation parameters.


## Idempotence and Recovery

Parameter changes in `src/kernel/chainparams.cpp` are safe to re-apply. The file is compiled into the binary, so rebuilding always produces a binary with the latest parameters. There is no migration or state change.

Documentation updates are idempotent. Re-editing the same files to correct wording or parameters does not create drift as long as the final content is consistent across all documents.

The fleet rollout should use an explicit backup-and-restore pattern. Each validator's binaries are backed up with a timestamp before replacement. If the local root `EXECPLAN.md` procedure is still current when this plan runs, reuse it; otherwise the rollback command sequence must be written into this plan before deployment begins. A representative rollback shape is:

    ssh <validator-host> 'mv /path/to/rngd.pre-sharepool.<timestamp> /path/to/rngd'
    ssh <validator-host> 'mv /path/to/rng-cli.pre-sharepool.<timestamp> /path/to/rng-cli'
    ssh <validator-host> 'systemctl restart rngd'

If the deployment times out (miners do not signal 95% support within the timeout window), sharepool simply does not activate. The binary continues running with pre-activation behavior. No rollback is needed; the deployment enters FAILED state and has no consensus effect.

If the operator rejects the activation parameters after review, revise the parameters based on feedback and re-run Units 1 through 4. There is no partial deployment state to clean up because no mainnet deployment happens until the operator approves.


## Artifacts and Notes

The activation parameter recommendation template:

    DEPLOYMENT_SHAREPOOL mainnet parameters (RECOMMENDATION -- requires operator approval):

    bit:                     <TBD, next available bit>
    nStartTime:              <TBD, approximately 4 weeks after RC tag>
    nTimeout:                <TBD, approximately 6 months after start>
    min_activation_height:   <TBD, current tip + 4032>
    threshold:               95% (standard BIP9)
    period:                  2016 blocks (~2.8 days at 120s target)

    SharePoolParams:
    target_share_spacing:    <TBD, from devnet experience>
    reward_window_work:      <TBD, from devnet experience>
    claim_witness_version:   <TBD, from Plan 007>
    max_orphan_shares:       <TBD, from devnet experience>

The fleet rollout plan should record the actual validator inventory and readiness state at execution time. Do not inherit the host list or sync status from older local planning notes without re-verifying them.

The agent-integration spec update is the most substantive documentation change. The current spec imagines this flow:

    rng-cli pool-mine --pool <pool-url> --threads 1 --address <addr>

After this plan, the spec should describe this flow instead:

    rng-cli -mine -mineaddress=<addr> -minethreads=1

    (After sharepool activation, this command submits shares
     to the peer-to-peer sharechain. No pool URL needed.
     Query pending rewards with: rng-cli getmininginfo)

The operator guide should include a "what changed" section that clearly states: before activation, the full block reward goes to the block finder; after activation, the block reward is split proportionally across the reward window based on accepted share work. Solo mining remains possible as a special case where one miner contributes all the work in the window.


## Interfaces and Dependencies

This plan modifies the following existing interfaces.

In `src/kernel/chainparams.cpp`, the `CMainParams` constructor gains DEPLOYMENT_SHAREPOOL version-bits parameters and the `SharePoolParams` values in `consensus.sharepool`. The interface shape was defined by Plan 004 and uses the types from `src/consensus/params.h`:

    enum DeploymentPos {
        DEPLOYMENT_TESTDUMMY,
        DEPLOYMENT_TAPROOT,
        DEPLOYMENT_SHAREPOOL,
        MAX_VERSION_BITS_DEPLOYMENTS
    };

    struct SharePoolParams {
        uint32_t target_share_spacing;
        uint32_t reward_window_work;
        uint8_t claim_witness_version;
        uint16_t max_orphan_shares;
    };

This plan fills in the mainnet values for these parameters. The parameter types and struct layout are unchanged.

In `specs/activation.md`, the deployment table gains a new row. The existing table has entries for DEPLOYMENT_TESTDUMMY and DEPLOYMENT_TAPROOT. This plan adds DEPLOYMENT_SHAREPOOL with the same column structure.

In `specs/agent-integration.md`, the mining integration section is revised. The existing spec describes `pool-mine` as a future command. This plan replaces that description with the actual protocol-native behavior: standard mining commands produce shares after activation, `getmininginfo` reports sharepool state, and claim transactions are built by the wallet. The spec must not describe `pool-mine --pool ...` as a supported or future feature after this update.

In `README.md`, the mining section is extended. The existing commands (`rng-start-miner`, `-mine`, `-mineaddress`) continue to work. The documentation adds a note about post-activation behavior and links to the operator guide.

This plan creates one new document: `docs/sharepool-operator-guide.md`. This guide has no code interface but must be consistent with the RPC surfaces documented in `doc/JSON-RPC-interface.md` and the consensus rules in `specs/consensus.md`.

This plan depends on the full chain of prior plans (002 through 011) being completed. It may borrow deployment ideas from the local root `EXECPLAN.md`, but it should not depend on unverified QSB rollout claims as if they were part of the inspected source baseline.

Change note: Created plan 012 on 2026-04-12 as the mainnet activation preparation plan. Reason: the genesis corpus requires a final preparation step that sets real activation parameters and updates all documentation before any mainnet deployment can be considered. The plan is deliberately conservative: it prepares but does not deploy, and it requires explicit operator approval before any mainnet action.
