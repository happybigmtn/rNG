# Devnet Deployment, Observability, and Adversarial Testing

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds.

This document must be maintained in accordance with `PLANS.md` at the repository root. It is plan 011 in the `genesis/plans/` corpus indexed by `genesis/PLANS.md`.


## Purpose / Big Picture

This plan proves the protocol-native pooled mining implementation on a live multi-node test network with real network latency, divergent hashrates, and adversarial scenarios. Before this plan, all sharepool testing has been on regtest with scripted inputs and controlled timing. After this plan, the implementation will have survived a real deployment where nodes must agree on share tips, payout commitments, and claim validity across an actual TCP network with no test harness orchestrating behavior.

The observable proof is a 3+ node devnet where at least two miners of different hashrates submit shares, every block carries a consensus-valid payout commitment that all nodes agree on, claim transactions succeed after maturity, and an independent observer node can export the share window and recompute the payout commitment from scratch. Beyond basic correctness, this plan also subjects the protocol to adversarial conditions: share withholding, sharechain eclipse attempts, and chain reorgs during active share windows.

For users, this plan answers the question "does the pooled mining protocol actually work when real nodes disagree, lag, or misbehave?" For operators, it produces a deployment guide and observability surfaces that will carry forward to mainnet. For the project, it is the last validation step before Plan 012 prepares mainnet activation parameters.

Three terms from Zend's prior rBTC work appear in this plan as design patterns, not as consensus dependencies. A "portable proof bundle" is a self-contained archive of share data and payout commitments that an external tool can replay without connecting to the network. A "standalone verifier" is a script or tool that reads a proof bundle and independently computes whether the payout commitment is correct. A "trust surface report" is a document that explicitly states which parts of the system are decentralized (share relay, reward computation, claim verification) and which parts still depend on operator coordination (devnet bootstrapping, seed peer configuration). Zend proved these patterns are useful for building operator and community confidence. RNG should adopt them as tooling around its consensus protocol, not as part of consensus itself.


## Requirements Trace

`R2`. The pooled reward contract must be derived from a public share history. Any fully validating RNG node must be able to replay the same accepted share window and derive the same per-block payout commitment. This plan tests that property across a real network, not just on a single regtest instance.

`R4`. Share admission and share propagation must be peer-to-peer. This plan verifies that shares propagate reliably between devnet nodes without a central relay.

`R9`. Activation must be staged through RNG's existing version-bits infrastructure first on regtest, then on devnet, then on mainnet. This plan is the devnet stage.

`R11`. The plan must preserve user truthfulness. Observability surfaces must accurately report share state, reward accrual, and claim readiness. Any discrepancy between what the node reports and what an independent observer can verify is a truthfulness bug.


## Scope Boundaries

This plan does not activate sharepool on mainnet. It operates exclusively on a dedicated devnet with its own genesis or activation parameters.

This plan does not attempt to break the protocol through novel cryptographic attacks or formal verification. Adversarial testing here means operational adversarial conditions: withholding shares, eclipsing a node's share view, and triggering chain reorgs while shares are in flight.

This plan does not require Zend to be running. Zend patterns inform the observability design, but no Zend service is required for any devnet test. The proof bundles, standalone verifier, and trust surface report are implemented as standalone RNG-side tools.

This plan does not benchmark mainnet-scale performance. The devnet is 3-5 nodes. Performance under hundreds of miners is a future concern.

This plan does not produce the final mainnet activation parameters. Those belong to Plan 012.


## Progress

- [ ] Write devnet deployment guide at `docs/devnet-sharepool-rollout.md`.
- [ ] Provision 3+ node devnet with at least 2 miners of different hashrates.
- [ ] Activate sharepool via version-bits on devnet.
- [ ] Verify basic operation: share convergence, payout commitment agreement, claim flow.
- [ ] Run adversarial test: share withholding by one miner.
- [ ] Run adversarial test: sharechain eclipse attempt.
- [ ] Run adversarial test: chain reorg during active share window.
- [ ] Implement node-native export surfaces for share window and payout commitment.
- [ ] Write `contrib/sharepool/export_proof.py` for portable proof bundle export.
- [ ] Write `test/functional/feature_sharepool_reorg.py` for reorg behavior.
- [ ] Write `test/functional/feature_sharepool_devnet_smoke.py` for devnet smoke test.
- [ ] Verify observability: node logs share tip, reward window, and payout root per block.
- [ ] Write trust surface report documenting what is decentralized versus what is not.
- [ ] Record all findings in Surprises & Discoveries.


## Surprises & Discoveries

No discoveries yet. This section will be populated during devnet operation.


## Decision Log

No decisions yet. Decisions about devnet topology, activation timing, and adversarial test parameters will be recorded here as they are made.


## Outcomes & Retrospective

Not yet applicable. This section will summarize devnet results, adversarial test outcomes, and readiness assessment for Plan 012.


## Context and Orientation

This plan follows Plan 010 (Decision Gate: Regtest Proof Review Before Devnet), which must pass before this work begins. The Plan 010 gate verified reward accuracy, determinism, claim reliability, pre-activation compatibility, security, and performance on regtest. This plan now tests those same properties under real network conditions.

The implementation being tested was built across Plans 002 through 009:

Plan 002 created the protocol spec at `specs/sharepool.md` and the economic simulator at `contrib/sharepool/simulate.py`. The simulator accepts share traces and emits payout commitment roots. It is the reference implementation for reward computation.

Plan 004 wired the version-bits deployment skeleton. The new `DEPLOYMENT_SHAREPOOL` flag lives in `src/consensus/params.h` and can be activated on any network through `-vbparams=sharepool:<start>:<timeout>:<min_activation_height>`.

Plan 005 implemented the sharechain data model in `src/sharechain/`, including share serialization (`share.h`, `share.cpp`), persistent storage (`store.h`, `store.cpp`), reward window reconstruction (`window.h`, `window.cpp`), and P2P relay via new messages in `src/protocol.h` and handling in `src/net_processing.cpp`.

Plan 007 implemented the payout commitment in `src/sharechain/payout.cpp` and the claim program using a new witness-program version in `src/script/interpreter.cpp`. Block assembly in `src/node/miner.cpp` now computes and embeds the commitment. Validation in `src/validation.cpp` rejects blocks with incorrect commitments.

Plan 008 integrated the internal miner to produce shares continuously, added wallet claim tracking, and extended the mining RPCs with sharepool fields.

The devnet builds on the same codebase. A "devnet" in this context means a small private network of RNG nodes running with custom activation parameters so sharepool activates immediately or after a short signaling period. It is not a public testnet. The operator (the person running this plan) controls all nodes.

The local root `EXECPLAN.md` provides some Contabo-fleet context for deployment planning, but the devnet should not depend on that live fleet state. The devnet should run on separate infrastructure or on local machines to avoid interfering with mainnet operators. If any fleet-specific assumptions from the local root plan are still relevant when this plan is executed, re-verify them at that time rather than inheriting them blindly here.

Five terms need definition for a novice reader:

A "share" is a proof of work that meets the share difficulty target (lower than the block target) and counts toward the reward window. Shares are relayed between peers and stored in the sharechain.

A "sharechain tip" is the best known latest share, analogous to the best block tip in the main chain. All nodes should converge on the same share tip.

A "payout commitment" is a Merkle root in each block's coinbase that commits to the reward split across all miners in the current reward window.

A "claim transaction" is a post-maturity spend that proves a miner's payout leaf exists under the committed root and transfers funds to the miner's address.

A "proof bundle" is a self-contained archive of share data and payout commitments that can be verified offline by a standalone tool without connecting to any node.


## Plan of Work

The work splits into four phases: deployment, basic verification, adversarial testing, and observability tooling.

Start by writing the devnet deployment guide at `docs/devnet-sharepool-rollout.md`. This guide must be detailed enough that a novice operator can set up the devnet from scratch. It should specify the exact daemon arguments, activation parameters, and peer configuration for each node. Use `-vbparams=sharepool:0:9999999999:0` to activate sharepool immediately on the devnet. Each node should use a separate datadir. At least two nodes should mine with different thread counts (for example, 1 thread and 4 threads) to create unequal hashrate.

Provision at least 3 nodes. Two are miners with different hashrates. The third is an observer node that does not mine but validates shares, blocks, and payout commitments. The observer exists specifically to test R2: independent verification without being a miner. If convenient, add a fourth node to test share propagation through multi-hop relay (miner A connects only to observer, observer connects to miner B, so shares from A must traverse the observer to reach B).

After the devnet is running and sharepool is active, verify basic operation. All nodes should converge on the same share tip within a few share intervals. Every block should carry a payout commitment that all nodes agree on. The two miners should see pending pooled rewards accumulating after each block. After coinbase maturity (100 confirmations), claim transactions should succeed.

Then run three adversarial scenarios.

For share withholding: configure one miner to withhold a fraction of its shares (do not relay them). Measure the advantage gained. If the withholding miner earns more than 5% above its proportional hashrate, the protocol has a significant withholding vulnerability that must be documented and potentially addressed before mainnet.

For sharechain eclipse: temporarily partition one miner from the share relay network (disconnect its peers or firewall share messages). Reconnect it and observe whether it converges back to the correct share tip. If the eclipsed node diverges permanently or produces invalid payout commitments after reconnection, the sharechain convergence logic has a bug.

For reorg behavior: mine a short chain segment on two partitioned nodes, then reconnect them to trigger a reorg. Verify that the share window is rebuilt deterministically after the reorg and that the new payout commitments match on all nodes. The functional test `test/functional/feature_sharepool_reorg.py` should automate this scenario.

After adversarial testing, build the observability tooling. Add node-native export surfaces so each node logs the share tip hash, reward window size, and payout commitment root for every block it processes. Write `contrib/sharepool/export_proof.py` to export a portable proof bundle: the accepted share window, payout commitment leaves, and enough data for an external tool to recompute the commitment root. Verify that the exported proof data is sufficient by feeding it to the simulator and confirming the roots match.

Finally, write a trust surface report. This report must explicitly state: share relay is peer-to-peer and decentralized, reward computation is deterministic and verifiable by any node, claim verification is consensus-enforced, but devnet bootstrapping and seed peer configuration are operator-controlled. This distinction matters for honest communication about what the protocol achieves.


## Implementation Units

### Unit 1: Devnet Deployment Guide

Goal: produce a self-contained guide for setting up and operating the sharepool devnet.

Requirements advanced: `R9`.

Dependencies: Plan 010 passed.

Files to create or modify:

- `docs/devnet-sharepool-rollout.md` (new)

Tests to add or modify: Test expectation: none -- this unit produces documentation, not code.

Approach: Write a step-by-step guide covering node provisioning, datadir setup, activation parameters, peer configuration, mining configuration, and health verification. Include exact command lines and expected output for each step. The guide must be usable by someone who has never operated an RNG node before.

Specific test scenarios:

A novice operator following the guide from step one can have a 3-node devnet running with sharepool active within 30 minutes. The guide's verification commands produce the expected output.

### Unit 2: Devnet Activation and Basic Verification

Goal: prove that sharepool works on a live multi-node network under normal conditions.

Requirements advanced: `R2`, `R4`, `R9`.

Dependencies: Unit 1.

Files to create or modify: none (operational work, not code changes).

Tests to add or modify:

- `test/functional/feature_sharepool_devnet_smoke.py` (new)

Approach: Stand up the devnet per the Unit 1 guide. Mine at least 150 blocks (enough for coinbase maturity plus a reasonable reward window). Verify share convergence, payout agreement, and claim success.

Specific test scenarios:

All 3+ devnet nodes report the same share tip within 10 seconds of a new share being produced. All nodes report the same payout commitment root for every block. The observer node, which does not mine, independently derives the same commitment. After 100 confirmations, both miners successfully submit claim transactions. The devnet smoke test automates these checks.

### Unit 3: Adversarial Testing -- Share Withholding

Goal: quantify the advantage a miner gains by withholding shares.

Requirements advanced: `R4`, `R11`.

Dependencies: Unit 2.

Files to create or modify: none (operational testing).

Tests to add or modify: Test expectation: none -- this unit produces measurements, not automated tests.

Approach: Configure one devnet miner to withhold 50% of its shares (relay only shares that contribute to blocks it finds). Run for at least 50 blocks and compare the withholding miner's reward share against its actual hashrate contribution. The difference is the withholding advantage.

Specific test scenarios:

A miner contributing approximately 50% of total hashrate but withholding 50% of its shares should earn less than 55% of total rewards (withholding advantage less than 5% above proportional). If the advantage exceeds 5%, record the exact measurements and flag this as a design concern for mainnet. If the advantage exceeds 15%, the reward window formula needs revision.

### Unit 4: Adversarial Testing -- Sharechain Eclipse

Goal: verify that a temporarily eclipsed node recovers correctly.

Requirements advanced: `R4`.

Dependencies: Unit 2.

Files to create or modify: none (operational testing).

Tests to add or modify: Test expectation: none -- this unit produces observations, not automated tests.

Approach: Disconnect one miner from the share network by removing its peer connections or firewalling share relay messages. Let the remaining nodes mine 10-20 blocks. Reconnect the eclipsed node and observe whether it catches up to the correct share tip and produces valid payout commitments.

Specific test scenarios:

After reconnection, the eclipsed node must converge to the same share tip as the other nodes within 30 seconds. Payout commitments for blocks mined after reconnection must match across all nodes. If the eclipsed node produces divergent commitments, the sharechain sync logic in `src/net_processing.cpp` or `src/sharechain/store.cpp` has a bug.

### Unit 5: Adversarial Testing -- Reorg Behavior

Goal: verify that share windows rebuild deterministically after a chain reorg.

Requirements advanced: `R2`.

Dependencies: Unit 2.

Files to create or modify:

- `test/functional/feature_sharepool_reorg.py` (new)

Tests to add or modify:

- `test/functional/feature_sharepool_reorg.py` (new)

Approach: Partition the devnet into two halves. Mine a few blocks on each half so they diverge. Reconnect and let the reorg resolve. Verify that the winning chain's payout commitments are identical on all nodes and that the share window was rebuilt correctly from the surviving chain.

Specific test scenarios:

After a 3-block reorg, all nodes agree on the same share tip and the same payout commitment for the new chain tip. The orphaned chain's payout commitments are discarded. No claim transaction that was valid before the reorg becomes invalid after it (unless the claim's payout leaf was in a reorged block, in which case the claim should fail with a clear error). The functional test `feature_sharepool_reorg.py` automates this on regtest.

### Unit 6: Observability and Proof Export

Goal: add observability surfaces and portable proof export for independent verification.

Requirements advanced: `R2`, `R11`.

Dependencies: Unit 2.

Files to create or modify:

- `src/rpc/mining.cpp` (extend existing surfaces)
- `src/rpc/blockchain.cpp` (extend existing surfaces)
- `contrib/sharepool/export_proof.py` (new)

Tests to add or modify: Test expectation: none -- verification is done by running the export tool and the simulator.

Approach: Ensure that node logs include the share tip hash, reward window share count, and payout commitment root for every block processed. Add or extend RPC methods so external tools can query the full share window and individual payout leaves. Write `export_proof.py` to package this data into a portable proof bundle that the simulator can replay.

Specific test scenarios:

The export tool produces a proof bundle from the observer node. Feeding that bundle to `contrib/sharepool/simulate.py` produces the same commitment roots as the node reported. The bundle is self-contained: a reviewer who has never connected to the devnet can verify it using only the simulator and the bundle file. The exported data includes the share records, their ordering, the reward window boundaries, and the leaf amounts.

### Unit 7: Trust Surface Report

Goal: document what the protocol decentralizes and what it does not.

Requirements advanced: `R11`.

Dependencies: Units 2 through 6.

Files to create or modify: none (the trust surface report is an artifact of this plan, recorded in the Artifacts and Notes section or as a separate document if warranted).

Tests to add or modify: Test expectation: none -- this unit produces a document, not code.

Approach: After devnet testing, write a clear statement of which system properties are trustless and which are operator-dependent. This follows Zend's pattern of explicit trust surface reporting.

Specific test scenarios:

The report must state at minimum: (a) share relay is peer-to-peer and does not depend on a central server, (b) reward computation is deterministic and independently verifiable, (c) claim verification is consensus-enforced, (d) devnet seed peer configuration is operator-controlled, (e) the current share target and reward window parameters are set by consensus code, not by operator configuration. Any property that is less decentralized than claimed must be flagged.


## Concrete Steps

All commands assume the working directory is the repository root unless a remote host is specified.

1. Build the sharepool-enabled binaries.

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli test_bitcoin

   Expected outcome: build completes without errors.

2. Set up the devnet nodes. Create three separate datadirs and start each node.

       mkdir -p /tmp/devnet-miner1 /tmp/devnet-miner2 /tmp/devnet-observer

       build/src/rngd -regtest -datadir=/tmp/devnet-miner1 -port=18501 -rpcport=18601 \
         -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<addr1> -minethreads=4 \
         -listen -addnode=127.0.0.1:18502 -addnode=127.0.0.1:18503 -daemon

       build/src/rngd -regtest -datadir=/tmp/devnet-miner2 -port=18502 -rpcport=18602 \
         -vbparams=sharepool:0:9999999999:0 -mine -mineaddress=<addr2> -minethreads=1 \
         -listen -addnode=127.0.0.1:18501 -addnode=127.0.0.1:18503 -daemon

       build/src/rngd -regtest -datadir=/tmp/devnet-observer -port=18503 -rpcport=18603 \
         -vbparams=sharepool:0:9999999999:0 \
         -listen -addnode=127.0.0.1:18501 -addnode=127.0.0.1:18502 -daemon

   Expected outcome: all three nodes start and connect to each other. `getpeerinfo` on each shows the other two.

3. Verify sharepool activation.

       build/src/rng-cli -regtest -rpcport=18601 getdeploymentinfo

   Expected outcome: output includes `sharepool` deployment with status `active`.

4. Mine at least 150 blocks and verify convergence.

       build/src/rng-cli -regtest -rpcport=18601 getblockcount
       build/src/rng-cli -regtest -rpcport=18601 getsharechaininfo
       build/src/rng-cli -regtest -rpcport=18602 getsharechaininfo
       build/src/rng-cli -regtest -rpcport=18603 getsharechaininfo

   Expected outcome: all three nodes report the same share tip and the same block height. The observer node's share tip matches the miners.

5. Verify payout commitment agreement.

       build/src/rng-cli -regtest -rpcport=18601 getrewardcommitment <recent-height>
       build/src/rng-cli -regtest -rpcport=18603 getrewardcommitment <recent-height>

   Expected outcome: both return the same commitment root.

6. Export proof data and verify with the simulator.

       python3 contrib/sharepool/export_proof.py \
         --rpcport=18603 \
         --start-height=<activation-height> \
         --end-height=<current-height> \
         --output=/tmp/devnet-proof-bundle.json

       python3 contrib/sharepool/simulate.py --replay /tmp/devnet-proof-bundle.json

   Expected outcome: the simulator confirms all commitment roots match.

7. Run the automated functional tests.

       test/functional/test_runner.py feature_sharepool_reorg.py feature_sharepool_devnet_smoke.py

   Expected outcome: both tests pass.


## Validation and Acceptance

The implementation is accepted only when all of the following are demonstrably true.

A 3+ node devnet with at least 2 miners of different hashrates converges on the same share tip and payout commitment for every block over at least 50 blocks.

An observer node that does not mine independently derives the same payout commitment as the mining nodes for every block.

The observer node exports a proof bundle that the simulator can replay, producing matching commitment roots for every block in the sample.

A short reorg (at least 2 blocks deep) rebuilds the share window deterministically, and all nodes agree on the new payout commitments after the reorg resolves.

The share withholding advantage is measured and documented. If it exceeds 5%, the finding is flagged for mainnet consideration. If it exceeds 15%, the reward formula needs revision before mainnet activation.

The sharechain eclipse test shows that a temporarily disconnected node reconverges to the correct share tip after reconnection.

Claim transactions work reliably on the devnet after coinbase maturity.

Node logs include share tip, reward window size, and payout root for every processed block.

The trust surface report exists and honestly states what is decentralized and what is not.


## Idempotence and Recovery

The devnet deployment is designed to be disposable. Each node uses a dedicated datadir that can be deleted and recreated. If a test fails or the devnet state becomes corrupted, wipe all datadirs and restart from the deployment guide. The `-vbparams=sharepool:0:9999999999:0` parameter ensures immediate activation, so there is no accumulated state that must be preserved across restarts.

Adversarial tests that partition the network can be reversed by restoring peer connections. If a node becomes permanently diverged after an adversarial test, that is a finding, not a recovery problem. Document the divergence and fix the underlying code before re-running the test.

The proof export tool is idempotent. Running it multiple times against the same block range produces the same output.

If the devnet needs to be reset mid-test (for example, after finding and fixing a bug), delete all datadirs, rebuild if needed, and start over. No state from a failed devnet run should carry forward to a new one.


## Artifacts and Notes

The devnet deployment guide should specify a topology like:

    Miner 1 (4 threads) <----> Observer <----> Miner 2 (1 thread)
         \                                        /
          \---------- direct connection ----------/

This ensures shares propagate through multiple paths and the observer can verify independently.

The proof bundle format exported by `export_proof.py` should include at minimum:

    {
      "network": "regtest",
      "activation_height": <int>,
      "blocks": [
        {
          "height": <int>,
          "block_hash": "<hex>",
          "payout_commitment_root": "<hex>",
          "reward_window": {
            "start_share": "<hex>",
            "end_share": "<hex>",
            "share_count": <int>,
            "total_work": "<hex>"
          },
          "leaves": [
            {
              "payout_script": "<hex>",
              "amount_roshi": <int>,
              "first_share": "<hex>",
              "last_share": "<hex>"
            }
          ]
        }
      ],
      "shares": [
        {
          "share_id": "<hex>",
          "parent_share": "<hex>",
          "prev_block_hash": "<hex>",
          "payout_script": "<hex>",
          "nBits": <int>,
          "nTime": <int>,
          "nNonce": <int>
        }
      ]
    }

The trust surface report should follow this template:

    Trust Surface Report: RNG Protocol-Native Pooled Mining
    Date: <date>
    Network: devnet

    Decentralized properties:
    - Share relay: peer-to-peer, no central relay
    - Reward computation: deterministic, any node can verify
    - Claim verification: consensus-enforced, no operator approval
    - Share admission: based on valid PoW, no gatekeeper

    Operator-dependent properties:
    - Devnet seed peer configuration: chosen by operator
    - Activation parameters: set in chainparams, chosen by developers
    - Network bootstrapping: first node must exist before others can join

    Open questions for mainnet:
    - Share withholding advantage measured at <X>%
    - Minimum viable peer count for reliable share propagation: <N>


## Interfaces and Dependencies

This plan depends on all code produced by Plans 002 through 009 passing the Plan 010 gate.

The new files this plan creates are:

`docs/devnet-sharepool-rollout.md` is a deployment guide. It has no code dependencies but must reference the correct binary build commands and daemon arguments.

`contrib/sharepool/export_proof.py` is a Python script that uses the RNG RPC interface to export share window and payout data. It depends on the RPC surfaces in `src/rpc/mining.cpp`, specifically `getsharechaininfo`, `getrewardcommitment`, and any share-window export RPC added by Plan 008. It also depends on `contrib/sharepool/simulate.py` being able to consume the exported format.

`test/functional/feature_sharepool_reorg.py` is a Python functional test that extends RNG's existing test framework at `test/functional/test_framework/`. It depends on the regtest mining and sharechain infrastructure from Plans 005, 007, and 008. The test should create a network partition using the test framework's `disconnect_nodes` and `connect_nodes` helpers, mine on both sides, reconnect, and verify payout commitment agreement.

`test/functional/feature_sharepool_devnet_smoke.py` is a Python functional test that starts 3+ nodes with sharepool activated, mines blocks, and verifies share convergence and payout agreement. It uses the same test framework.

This plan extends the following existing surfaces in `src/rpc/mining.cpp` and `src/rpc/blockchain.cpp`:

Node logging should include a line per processed block with the share tip hash, reward window share count, and payout commitment root. This is a logging enhancement, not an RPC change. The log format should be structured enough for log parsing tools but does not need to be machine-parseable JSON.

The `getsharechaininfo` RPC may need to be extended with fields for the full share window (list of share IDs in the current reward window) to support proof export. If the share window is too large for a single RPC response, add pagination or a dedicated `getsharelist` RPC with height range parameters.

This plan does not modify consensus rules, P2P protocol messages, or the activation mechanism. It tests and documents what already exists.

Change note: Created plan 011 on 2026-04-12 as the devnet deployment and adversarial testing plan. Reason: the genesis corpus dependency graph requires live multi-node validation with adversarial scenarios before mainnet activation can be prepared. The plan incorporates Zend-inspired observability patterns (portable proof bundles, standalone verifiers, trust surface reporting) as tooling around RNG's consensus protocol.
