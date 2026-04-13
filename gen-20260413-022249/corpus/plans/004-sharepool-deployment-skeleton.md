# Sharepool Version-Bits Deployment Skeleton

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root.


## Purpose / Big Picture

After this work lands, the RNG node will carry a new version-bits deployment called `sharepool` through every layer of the activation machinery, from the consensus parameter struct through the chain parameter files, deployment info strings, the `getdeploymentinfo` RPC, and both unit and functional test coverage. The deployment will be dormant on mainnet (set to `NEVER_ACTIVE`) and will only activate on regtest when an operator passes an explicit startup flag. No mining behavior, reward accounting, share relay, or coinbase construction changes in this plan. The sole purpose is to establish the activation boundary so that later plans can add sharepool-specific consensus rules behind a clean `IsActiveAfter(DEPLOYMENT_SHAREPOOL)` guard.

A developer or operator can verify the work by starting a regtest node with `-vbparams=sharepool:0:9999999999:0`, calling `getdeploymentinfo`, and observing that the `sharepool` deployment is reported as `active`. On the same node, classical mining must still work exactly as before. On a default regtest node without the override, or on mainnet, the sharepool deployment must show as `failed` or `defined`, and nothing else changes.


## Requirements Trace

`R8`. Before activation, existing RNG behavior remains unchanged. Existing blocks, mining RPCs, wallet flows, and tests must continue to work until the new deployment is explicitly activated. This plan satisfies R8 by keeping the deployment `NEVER_ACTIVE` on mainnet and testnet, and by verifying that all existing functional and unit tests still pass after the code lands.

`R9`. Activation must be staged through RNG's existing version-bits infrastructure first on regtest, then on a dedicated devnet or signet-style test deployment, and only then on mainnet. This plan satisfies R9 by wiring the deployment into the existing BIP9 version-bits machinery and making regtest the first activation target through `-vbparams`.

These two requirements come from the master requirement set defined in `docs/rng-protocol-native-pooled-mining-execplan.md`. The remaining requirements (R1-R7, R10, R11) are not advanced by this plan because this plan changes no mining, reward, relay, or wallet behavior.


## Scope Boundaries

This plan does not add any sharechain code, share relay, share storage, share validation, payout commitment logic, claim program, wallet integration, or mining behavior change. It adds only the deployment flag, the consensus parameter struct, and the activation reporting and test surfaces.

This plan does not activate sharepool on mainnet, testnet, testnet4, or signet. Those networks remain at `NEVER_ACTIVE`. Only regtest supports explicit activation through `-vbparams`.

This plan does not modify `src/pow.cpp`, `src/node/miner.cpp`, `src/node/internal_miner.cpp`, `src/validation.cpp`, `src/net_processing.cpp`, `src/protocol.h`, or any wallet code. If you find yourself editing those files to complete this plan, you have left scope.

This plan does not change any parallel QSB rollout work described in the local root `EXECPLAN.md`, and it does not assume an in-tree QSB candidate pool already exists in the inspected checkout. It only adds a new pooled-mining deployment boundary alongside the existing in-code deployments (`testdummy`, `taproot`).


## Progress

- [ ] Add `DEPLOYMENT_SHAREPOOL` to `DeploymentPos` enum in `src/consensus/params.h`.
- [ ] Add `SharePoolParams` struct to `Consensus::Params` in `src/consensus/params.h`.
- [ ] Wire deployment info in `src/deploymentinfo.cpp` and `src/deploymentinfo.h`.
- [ ] Set deployment parameters per network in `src/kernel/chainparams.cpp`.
- [ ] Extend `getdeploymentinfo` reporting in `src/rpc/blockchain.cpp` (if needed beyond automatic version-bits enumeration).
- [ ] Add unit test coverage in `src/test/versionbits_tests.cpp`.
- [ ] Add functional test `test/functional/feature_sharepool_activation.py`.
- [ ] Verify all existing tests still pass.


## Surprises & Discoveries

No entries yet. This section will be updated as implementation proceeds.


## Decision Log

- Decision: Place `DEPLOYMENT_SHAREPOOL` immediately before `MAX_VERSION_BITS_DEPLOYMENTS` in the `DeploymentPos` enum.
  Rationale: The existing comment in `src/consensus/params.h` says "Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp." The enum is order-dependent because `MAX_VERSION_BITS_DEPLOYMENTS` is used as an array size. The new entry must go after `DEPLOYMENT_TAPROOT` and before `MAX_VERSION_BITS_DEPLOYMENTS`.
  Date/Author: 2026-04-12 / Plan author

- Decision: Use bit position 3 for the sharepool deployment signal.
  Rationale: Bit 28 is used by `DEPLOYMENT_TESTDUMMY` and bit 2 is used by `DEPLOYMENT_TAPROOT`. Bit 3 is unused and low enough to be practical. The exact bit does not matter for correctness but must not collide with any existing deployment on the same network.
  Date/Author: 2026-04-12 / Plan author

- Decision: Set `NEVER_ACTIVE` for mainnet, testnet, testnet4, and signet. Set start time 0 with `NO_TIMEOUT` for regtest so `-vbparams` can override it.
  Rationale: Regtest already supports `-vbparams` overrides through the `version_bits_parameters` loop in `CRegTestParams`. Setting start time 0 and no timeout makes the deployment configurable on regtest without changing the override infrastructure. Other networks stay dormant until a future plan explicitly activates them.
  Date/Author: 2026-04-12 / Plan author

- Decision: Define `SharePoolParams` with default zero values in the struct definition so that networks that do not set sharepool parameters explicitly still compile and behave correctly.
  Rationale: Most networks will not touch sharepool parameters until later plans land. Zero-initialized defaults prevent uninitialized-member warnings and make the struct safe to read even when the deployment is inactive.
  Date/Author: 2026-04-12 / Plan author


## Outcomes & Retrospective

No entries yet. This section will be updated at milestones and at completion.


## Context and Orientation

RNG is a Bitcoin Core-derived chain with RandomX proof of work, live on mainnet since February 2026. The checked-in docs still identify the base as Bitcoin Core `v29.0`, and this plan relies only on verified current activation seams rather than on unmerged port claims. It preserves Bitcoin's version-bits activation machinery for consensus upgrades. This section explains the five files you will edit, what they do, and how they relate.

`src/consensus/params.h` defines the `Consensus::Params` struct, which every network (mainnet, testnet, regtest, etc.) populates with its own values. Inside this file, two enums matter. `BuriedDeployment` lists old consensus changes whose activation heights are hardcoded. `DeploymentPos` lists BIP9 version-bits deployments that can be activated by miner signaling. The current `DeploymentPos` entries are `DEPLOYMENT_TESTDUMMY` (a placeholder for testing) and `DEPLOYMENT_TAPROOT` (Schnorr/Taproot, active from genesis on RNG). The sentinel `MAX_VERSION_BITS_DEPLOYMENTS` must always be the last entry because it is used to size the `vDeployments` array in `Consensus::Params`. Each deployment entry in that array is a `BIP9Deployment` struct with fields for the signaling bit, start time, timeout, minimum activation height, period, and threshold.

`src/deploymentinfo.cpp` and `src/deploymentinfo.h` map each `DeploymentPos` entry to a human-readable name and a flag indicating whether getblocktemplate clients can safely ignore the rule. The `VersionBitsDeploymentInfo` array must have exactly `MAX_VERSION_BITS_DEPLOYMENTS` entries, in the same order as the enum. If you add a new enum value but forget to add the matching info entry, the build will fail because the array initializer count will not match the expected size.

`src/kernel/chainparams.cpp` creates network-specific parameter objects. Each network class (such as `CMainParams` for mainnet, `CRegTestParams` for regtest) sets `consensus.vDeployments[Consensus::DEPLOYMENT_*]` fields. The regtest class has a loop that applies `-vbparams` overrides from the command line, stored in `opts.version_bits_parameters`. This is the mechanism that lets regtest activate arbitrary deployments at test time.

`src/rpc/blockchain.cpp` contains the `getdeploymentinfo` RPC, which iterates over all version-bits deployments and reports their current state (defined, started, locked_in, active, or failed). Adding a new `DeploymentPos` entry automatically makes it appear in `getdeploymentinfo` output because the RPC iterates from 0 to `MAX_VERSION_BITS_DEPLOYMENTS`. No manual RPC editing is needed unless you want to add extra sharepool-specific fields to the output.

`src/versionbits.cpp` implements the BIP9 state machine. It does not need modification for this plan because the state machine is generic over all deployments. The version-bits cache, condition checker, and GBT status logic all iterate the deployment array automatically.

Three terms used in this plan:

A "version-bits deployment" is a mechanism from BIP9 for activating consensus changes. Miners signal readiness by setting a specific bit in the block header's version field. After enough miners signal within a defined period, the deployment transitions through states (defined, started, locked_in, active) and eventually becomes permanently active.

"NEVER_ACTIVE" is a special start-time value (`-2`) in the `BIP9Deployment` struct that causes the version-bits state machine to immediately report the deployment as `failed`. This is how RNG keeps unready deployments dormant on production networks.

"SharePoolParams" is the new struct this plan introduces to hold consensus constants for the pooled mining protocol. These constants (share spacing, reward window work, claim witness version, orphan share limit) will be populated by later plans after the economic simulator settles the values. For now they default to zero.


## Plan of Work

The work proceeds in four sequential edits plus two test additions.

First, open `src/consensus/params.h` and add `DEPLOYMENT_SHAREPOOL` to the `DeploymentPos` enum between `DEPLOYMENT_TAPROOT` and `MAX_VERSION_BITS_DEPLOYMENTS`. Add a comment matching the style of the existing entries. Then add a `SharePoolParams` struct with four fields: `target_share_spacing` (uint32_t, default 0), `reward_window_work` (uint32_t, default 0), `claim_witness_version` (uint8_t, default 0), and `max_orphan_shares` (uint16_t, default 0). Add a `SharePoolParams sharepool;` member to the `Params` struct.

Second, open `src/deploymentinfo.cpp` and append a third entry to the `VersionBitsDeploymentInfo` array initializer for the new deployment. The name is `"sharepool"` and `gbt_optional_rule` is `false` (after activation, blocks must include the sharepool commitment, so GBT clients cannot ignore it).

Third, open `src/kernel/chainparams.cpp` and add `DEPLOYMENT_SHAREPOOL` configuration to every network class. For mainnet, testnet, testnet4, and signet, set `nStartTime = NEVER_ACTIVE`, `nTimeout = NO_TIMEOUT`, and `bit = 3`. For regtest, set `nStartTime = 0`, `nTimeout = NO_TIMEOUT`, `bit = 3`, `threshold = 108`, `period = 144`, and `min_activation_height = 0`. This makes regtest sharepool activatable via `-vbparams=sharepool:0:9999999999:0`.

Fourth, verify that `src/rpc/blockchain.cpp` does not need editing. The `getdeploymentinfo` RPC already iterates all deployments by index. Adding the enum entry and the info array entry is sufficient.

Fifth, add unit test coverage in `src/test/versionbits_tests.cpp`. Add a test case that verifies the `DEPLOYMENT_SHAREPOOL` entry exists, its info name is `"sharepool"`, and on default regtest parameters the deployment starts in state `DEFINED` (not `ACTIVE`, because regtest sets `nStartTime = 0` which means it will start signaling at time 0 but needs miner votes to lock in, unless overridden).

Sixth, add a functional test `test/functional/feature_sharepool_activation.py` that starts a regtest node with `-vbparams=sharepool:0:9999999999:0`, calls `getdeploymentinfo`, and asserts the sharepool deployment is `active`. Then starts a second regtest node without the override, mines a block, and asserts classical mining works and sharepool is not active.


## Implementation Units

### Unit A: Consensus Parameter and Deployment Info

Goal: Add the `DEPLOYMENT_SHAREPOOL` enum value, `SharePoolParams` struct, and deployment info entry so the codebase compiles with the new deployment.

Requirements advanced: R8, R9.

Dependencies on earlier units: None.

Files to create or modify:
- `src/consensus/params.h` (modify)
- `src/deploymentinfo.cpp` (modify)

Tests to add or modify: None in this unit (tested in Unit C).

Approach: Insert the new enum value and struct as described in the Plan of Work. The key constraint is that the `VersionBitsDeploymentInfo` array in `deploymentinfo.cpp` must gain exactly one entry in the position matching the new enum value.

Specific test scenarios:
- The codebase compiles cleanly after the changes.
- `static_assert(Consensus::ValidDeployment(Consensus::DEPLOYMENT_SHAREPOOL))` passes.

### Unit B: Per-Network Deployment Parameters

Goal: Set `DEPLOYMENT_SHAREPOOL` parameters in every network class so the deployment is dormant on production networks and configurable on regtest.

Requirements advanced: R8, R9.

Dependencies on earlier units: Unit A.

Files to create or modify:
- `src/kernel/chainparams.cpp` (modify)

Tests to add or modify: None in this unit (tested in Unit C).

Approach: Add a block of `consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL]` assignments in each network class constructor, following the exact pattern used for `DEPLOYMENT_TAPROOT`. For mainnet, testnet, testnet4, and signet, use `NEVER_ACTIVE`. For regtest, use start time 0 with `NO_TIMEOUT` so the `-vbparams` loop can override it.

Specific test scenarios:
- A mainnet node reports sharepool deployment as `failed` in `getdeploymentinfo` (because `NEVER_ACTIVE` maps to the `FAILED` terminal state).
- A default regtest node reports sharepool deployment as `started` or `defined` depending on median time (start time 0 means signaling begins immediately, but without miner votes the deployment stays in `STARTED`).
- A regtest node with `-vbparams=sharepool:0:9999999999:0` reports sharepool as `active`.

### Unit C: Tests

Goal: Prove the deployment skeleton works end to end without changing pre-activation behavior.

Requirements advanced: R8, R9.

Dependencies on earlier units: Units A and B.

Files to create or modify:
- `src/test/versionbits_tests.cpp` (modify)
- `test/functional/feature_sharepool_activation.py` (create)

Tests to add or modify:
- `src/test/versionbits_tests.cpp`: Add a test that verifies the deployment exists, has the expected name, and the `ValidDeployment` check passes.
- `test/functional/feature_sharepool_activation.py`: Add a functional test with three scenarios.

Approach: The unit test uses the existing test framework to instantiate a `Consensus::Params` object and verify the `DEPLOYMENT_SHAREPOOL` index is valid and the info array has the correct name. The functional test starts two regtest nodes: one with `-vbparams=sharepool:0:9999999999:0` (to force immediate activation) and one without. On the activated node, call `getdeploymentinfo` and assert the sharepool entry shows `active`. On the default node, mine a block with `generatetoaddress`, verify the block is accepted, and assert classical coinbase reward is paid to the miner address. Then verify that `getdeploymentinfo` does not show sharepool as `active`.

Specific test scenarios:

1. Default regtest: Start a node with no sharepool overrides. Mine one block with `generatetoaddress`. Verify the block is accepted and the coinbase pays the expected 50 RNG to the miner address. Call `getdeploymentinfo` and verify the sharepool entry exists but is not in state `active`.

2. Regtest with forced activation: Start a node with `-vbparams=sharepool:0:9999999999:0`. Call `getdeploymentinfo` and verify the sharepool entry shows `status_next: "active"` or equivalent active state. Mine one block with `generatetoaddress`. Verify the block is accepted. Verify that classical mining still works because no sharepool consensus rules are enforced yet (this plan adds no consensus rules, only the deployment flag).

3. Cross-node compatibility: Start two nodes, one activated and one default. Connect them. Mine a block on the activated node. Verify the default node accepts the block. This proves the deployment flag alone does not change block validity.


## Concrete Steps

All commands assume the working directory is the repository root.

After making the source edits described in the Plan of Work, build:

    cmake -B build -DENABLE_WALLET=ON -DBUILD_TESTING=ON -DWITH_ZMQ=OFF -DENABLE_IPC=OFF
    cmake --build build --target rngd rng-cli test_bitcoin -j"$(nproc)"

Expected outcome: clean build, no new warnings.

Run existing tests to verify nothing regressed:

    ./build/bin/test_bitcoin --run_test=versionbits_tests
    python3 test/functional/feature_versionbits_warning.py --configfile=build/test/config.ini

Expected outcome: all existing version-bits tests pass.

Run the new unit test:

    ./build/bin/test_bitcoin --run_test=versionbits_tests/sharepool_deployment_exists

Expected outcome: one new test case passes.

Run the new functional test:

    python3 test/functional/feature_sharepool_activation.py --configfile=build/test/config.ini

Expected outcome:

    feature_sharepool_activation.py passed

Manually verify on regtest with forced activation:

    ./build/bin/rngd -regtest -daemon -vbparams=sharepool:0:9999999999:0
    ./build/bin/rng-cli -regtest getdeploymentinfo

Expected outcome: the JSON output includes a `sharepool` key with `"active": true` or `"status_next": "active"` (depending on the exact RPC output format, which uses the `BIP9Info` structure).

Stop the test daemon:

    ./build/bin/rng-cli -regtest stop


## Validation and Acceptance

The implementation is accepted when all of the following are true:

A regtest node started with `-vbparams=sharepool:0:9999999999:0` reports the sharepool deployment as active in `getdeploymentinfo`.

A default regtest node without the override reports the sharepool deployment as not active, and classical mining with `generatetoaddress` still works and produces blocks with the expected coinbase reward.

A block mined by an activated regtest node is accepted by a non-activated regtest peer. This proves that the deployment flag alone does not change block validity.

All existing unit tests in `versionbits_tests` pass without modification.

All existing functional tests that exercise version-bits or deployment reporting pass without modification.

The build produces no new compiler warnings related to the changes.


## Idempotence and Recovery

All steps in this plan are idempotent. The source edits are additive (inserting new enum values and array entries) and do not modify existing entries. Rebuilding after making the same edit twice produces the same binary.

If the build breaks because the `VersionBitsDeploymentInfo` array size does not match `MAX_VERSION_BITS_DEPLOYMENTS`, the fix is to verify the array has exactly one entry per `DeploymentPos` value, in order.

If a test fails because the deployment state is unexpected, verify the `-vbparams` flag is passed correctly. The format is `-vbparams=sharepool:<start_time>:<timeout>:<min_activation_height>`. Using `0:9999999999:0` means "start signaling immediately, never time out, no minimum activation height."

The regtest datadir can be safely deleted and recreated at any time. No persistent state from this plan is required to survive across test runs.


## Artifacts and Notes

Expected shape of the new `DeploymentPos` enum after this plan:

    enum DeploymentPos : uint16_t {
        DEPLOYMENT_TESTDUMMY,
        DEPLOYMENT_TAPROOT,
        DEPLOYMENT_SHAREPOOL,
        // NOTE: Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp
        MAX_VERSION_BITS_DEPLOYMENTS
    };

Expected shape of the new `SharePoolParams` struct:

    struct SharePoolParams {
        uint32_t target_share_spacing{0};
        uint32_t reward_window_work{0};
        uint8_t claim_witness_version{0};
        uint16_t max_orphan_shares{0};
    };

Expected third entry in the `VersionBitsDeploymentInfo` array:

    VBDeploymentInfo{
        .name = "sharepool",
        .gbt_optional_rule = false,
    },

Expected regtest deployment configuration:

    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].bit = 3;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].min_activation_height = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].threshold = 108;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].period = 144;

Expected mainnet deployment configuration:

    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].bit = 3;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].min_activation_height = 0;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].threshold = 1815;
    consensus.vDeployments[Consensus::DEPLOYMENT_SHAREPOOL].period = 2016;

Expected `getdeploymentinfo` output snippet on an activated regtest node:

    "sharepool": {
      "type": "bip9",
      "bip9": {
        "bit": 3,
        "start_time": 0,
        "timeout": 9223372036854775807,
        "min_activation_height": 0,
        "status": "active",
        ...
      },
      "active": true
    }


## Interfaces and Dependencies

This plan depends on the existing version-bits deployment machinery in `src/consensus/params.h`, `src/deploymentinfo.cpp`, `src/deploymentinfo.h`, `src/versionbits.cpp`, and `src/kernel/chainparams.cpp`. It does not introduce new library dependencies.

This plan produces the following new interfaces that later plans will consume:

In `src/consensus/params.h`, the `DEPLOYMENT_SHAREPOOL` enum value is the stable identifier that all later sharepool code will pass to `IsActiveAfter()`, `VersionBitsCache::Info()`, and similar deployment-query functions.

In `src/consensus/params.h`, the `SharePoolParams` struct and the `sharepool` member on `Consensus::Params` are the stable location where later plans will read consensus constants such as share target spacing and reward window work. Plans 002-003 (the simulator and decision gate) will propose concrete values for these fields. Until those values are set, the defaults are zero, and no code should branch on them.

In `src/deploymentinfo.cpp`, the `"sharepool"` name string is the human-readable identifier that appears in `getdeploymentinfo` output and in the `-vbparams` command-line flag. The `-vbparams` parser in `src/common/args.cpp` matches deployment names by iterating the `VersionBitsDeploymentInfo` array, so the name string must exactly match what operators pass on the command line.

This plan does not modify any existing interface. The `DeploymentPos` enum grows by one value, which changes `MAX_VERSION_BITS_DEPLOYMENTS` from 2 to 3. Code that iterates deployments by index (such as `VersionBitsCache::GBTStatus` and `ComputeBlockVersion` in `src/versionbits.cpp`) automatically picks up the new entry. Code that switches on `DeploymentPos` does not need a new case because the version-bits machinery handles deployments generically.

Later plans that depend on this one: Plan 005 (Sharechain Data Model, Storage, and P2P Relay) will gate share relay and storage behind `IsActiveAfter(DEPLOYMENT_SHAREPOOL)`. Plan 007 (Compact Payout Commitment and Claim Program) will gate payout-commitment consensus validation behind the same check. Plan 008 (Internal Miner, Template, and Wallet Integration) will gate the mining behavior change behind the same check. All of these depend on the deployment skeleton being present and testable first.
