# Regtest End-to-End Proof

This ExecPlan is a living document. The sections `Progress`, `Surprises & Discoveries`, `Decision Log`, and `Outcomes & Retrospective` must be kept up to date as work proceeds. This document must be maintained in accordance with `PLANS.md` at the repository root, which defines the ExecPlan standard for all plans in this corpus.


## Purpose / Big Picture

After this change, the complete pooled mining flow is proven on regtest in a single automated test before any multi-node deployment. Before this change, each component has its own tests (share relay in Plan 005, payout commitment in Plan 007, miner and wallet integration in Plan 008), but no test exercises all components together end-to-end. This plan is the first complete proof: two miners with unequal hashrate mine on the same activated regtest network, rewards are split proportionally, an independent observer verifies the split, and the wallet claim flow works from share submission through claim confirmation.

This is a VALIDATION plan. It writes no new consensus code, no new data structures, and no new RPCs. It exercises Plans 004 through 008 together in one comprehensive functional test. If this test passes, the protocol is ready for the multi-node devnet deployment in Plan 011. If it fails, the failure identifies which earlier plan needs revision.

The observable result: running the test produces clear output showing the 90/10 hashrate split between two miners, the corresponding approximately 90/10 reward split across 50 activated blocks, the independent observer's verification that it computed the same payout roots from share replay alone, and the successful claim transactions on both miners' wallets. A developer who has never seen the sharepool code can run one command and see the entire flow work.

This plan depends on Plan 004 (version-bits deployment skeleton), Plan 005 (sharechain data model, storage, and P2P relay), Plan 007 (payout commitment and claim program), and Plan 008 (internal miner, wallet, and RPC integration). It is the entry point for Plan 010 (the decision gate that reviews regtest results before devnet deployment).

Terminology used throughout this plan:

"Regtest" is RNG's regression test mode, an isolated blockchain where blocks can be mined instantly or on demand. Regtest is configured in `src/kernel/chainparams.cpp` with its own network parameters and is activated via the `-regtest` flag. It is the standard environment for automated functional tests.

A "functional test" is a Python test script in the `test/functional/` directory that starts one or more `rngd` nodes, exercises behavior through RPCs, and asserts expected outcomes. Functional tests inherit from `BitcoinTestFramework` in `test/functional/test_framework/test_framework.py`. They create temporary datadirs, start daemons, run assertions, and clean up automatically.

An "observer node" is a fully validating node that does not mine. It connects to the mining nodes as a peer, receives shares and blocks, and independently computes payout commitments from its local copy of the sharechain. If the observer's computed commitment roots match the commitments in the blocks it received, the deterministic replay property (R2) is proven.

"Hashrate split" in this test is simulated by giving Miner A four worker threads and Miner B one worker thread. Over a sufficient number of blocks (50 in this test), the share counts approximate a 4:1 ratio, which produces an approximately 80/20 reward split. The exact split depends on RandomX hash distribution, but the test asserts the ratio is within a configurable tolerance (for example, the dominant miner receives between 70% and 95% of total rewards).

"Share withholding" means a miner finds valid shares but does not relay them to the network. In this test, one scenario verifies that if Miner B stops relaying shares (simulated by disconnecting it from the network mid-test), Miner A continues accumulating rewards normally and Miner B's unreported work does not corrupt the reward split for other participants.


## Requirements Trace

This plan validates all requirements because it exercises the complete system.

`R1` (consensus-enforced reward). The test verifies that every activated block carries a payout commitment output and that the observer node accepts the block only when its locally computed commitment matches.

`R2` (deterministic from share history). The observer node computes commitment roots from its own share history copy and compares them to the block commitments. A mismatch would cause the test to fail.

`R3` (proportional accrual). The test verifies that each miner's pending pooled reward is proportional to its thread count (and therefore its share production rate) over a 50-block window.

`R4` (peer-to-peer share admission). Shares propagate between the two miners and the observer without any central coordinator. The test verifies all three nodes have the same share tip after each block.

`R5` (miner-built templates). Both miners build their own block templates using `CreateNewBlock()`. No external service constructs templates.

`R6` (compact commitment). The test verifies that each activated block's coinbase has exactly one payout commitment output regardless of the number of participating miners.

`R7` (trustless claims). The test verifies that wallet-built claim transactions are accepted and confirmed, spending the correct amount from the payout commitment.

`R8` (pre-activation preservation). The test includes a regression phase that mines pre-activation blocks and verifies they work exactly as classical Bitcoin-style blocks: single coinbase output, full reward to finder, no payout commitment.

`R9` (staged deployment). This regtest proof is itself a stage in the deployment pipeline. It must pass before devnet deployment (Plan 011) begins.

`R10` (solo as special case). The test includes a phase where only one miner is active (Miner B is stopped), verifying that the solo miner receives 100% of the reward through the sharepool mechanism.

`R11` (truthfulness). The test verifies that `getbalances` correctly reports pending vs claimable amounts and that claimed funds appear as trusted balance.


## Scope Boundaries

This plan writes one new functional test file. It does not modify any source code in `src/`. It does not modify consensus rules, P2P relay, wallet logic, mining RPCs, or any other runtime code. If the test reveals a bug, the fix belongs in the plan that owns the buggy component (Plans 004-008), not here.

This plan does not deploy to devnet, signet, or mainnet. It runs entirely on regtest with temporary datadirs that are cleaned up automatically.

This plan does not test adversarial scenarios beyond basic share withholding (disconnecting one miner). Advanced adversarial testing (selfish mining, eclipse attacks, timestamp manipulation) is Plan 011's scope.

This plan does not test external miners or `getblocktemplate` extensions. It uses only the internal miner.

This plan does not test network performance, bandwidth, or latency. It runs all nodes on the same machine with loopback connections.


## Progress

- [ ] Write `test/functional/feature_sharepool_e2e.py` test framework setup.
- [ ] Implement Phase 1: pre-activation regression.
- [ ] Implement Phase 2: activation and share production.
- [ ] Implement Phase 3: proportional reward verification.
- [ ] Implement Phase 4: observer independent verification.
- [ ] Implement Phase 5: wallet claim flow.
- [ ] Implement Phase 6: share withholding resilience.
- [ ] Implement Phase 7: solo mining special case.
- [ ] Run the test and verify all assertions pass.
- [ ] Record test output as artifacts.


## Surprises & Discoveries

(No entries yet. This section will be updated as implementation proceeds.)

- Observation: ...
  Evidence: ...


## Decision Log

- Decision: Use a 50-block activated window as the primary test span, not 10 or 200.
  Rationale: 10 blocks provides insufficient statistical confidence in the reward split ratio. 200 blocks would make the test slow (each block requires share production and relay). 50 blocks balances confidence with test runtime. At 50 blocks, the law of large numbers gives reasonable convergence: with a 4:1 thread ratio, the expected share ratio is 80/20, and 50 blocks of shares provide enough samples that a 70-95% confidence band for the dominant miner is realistic.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Use a tolerance band for reward ratio assertions rather than exact percentages.
  Rationale: RandomX hashing is stochastic. Even with a 4:1 thread count ratio, the actual share production ratio varies from block to block. Asserting an exact 80/20 split would cause flaky tests. Instead, the test asserts that the dominant miner's cumulative reward is between 70% and 95% of the total reward over 50 blocks. This range accommodates statistical variance while still detecting a fundamentally broken reward split.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Simulate share withholding by disconnecting the miner rather than patching the relay code.
  Rationale: Disconnecting a peer is a realistic simulation of share withholding (the miner produces shares but they do not reach the network). It requires no source code modification and can be done via the `disconnectnode` RPC. The test verifies that the remaining miner's rewards are unaffected.
  Date/Author: 2026-04-12 / Plan authored

- Decision: Run the observer node as a third `rngd` instance with `-mine=0`.
  Rationale: The observer must be a full node that validates blocks and receives shares but does not produce its own shares. Setting `-mine=0` (or simply omitting `-mine`) achieves this. The observer connects to both miners as a peer and receives shares and blocks through normal P2P relay.
  Date/Author: 2026-04-12 / Plan authored


## Outcomes & Retrospective

(No entries yet. This section will be updated after the test passes and is reviewed at the Plan 010 decision gate.)


## Context and Orientation

The reader needs to understand two things to implement this plan: the functional test framework and the RPC interfaces being tested.

The functional test framework lives in `test/functional/test_framework/`. Every functional test is a Python class that extends `BitcoinTestFramework` (defined in `test/functional/test_framework/test_framework.py`). The key methods are `set_test_params()` (configure the number of nodes and their startup arguments), `setup_network()` (start nodes and connect them), and `run_test()` (the test body). Nodes are accessed via `self.nodes[i]`, which provides an RPC proxy: `self.nodes[0].getmininginfo()` calls the `getmininginfo` RPC on node 0. The framework handles starting daemons, creating temporary datadirs, connecting nodes, and cleanup.

Tests are run from the repository root with:

    test/functional/test_runner.py feature_sharepool_e2e.py

Or directly with:

    python3 test/functional/feature_sharepool_e2e.py

The RPC interfaces this test exercises are:

`getdeploymentinfo` returns activation status for all deployments. After activation, the `sharepool` entry shows `status: "active"`.

`getmininginfo` returns mining status. After activation, it includes `sharepool_active`, `share_tip`, `pending_pooled_reward`, and `accepted_shares` (added by Plan 008).

`getbalances` returns wallet balance categories. After activation with pending rewards, it includes a `pooled` object with `pending` and `claimable` fields (added by Plan 008).

`getrewardcommitment <blockhash>` returns the payout commitment for a block, including the root hash and all reward leaves with amounts (added by Plans 007 and 008).

`getbestblockhash` returns the tip block hash. `getblock <hash>` returns block details including the coinbase transaction. `getsharechaininfo` returns the current sharechain state including the best share tip (added by Plan 005).

`listtransactions` returns recent wallet transactions, including claim transactions with `category: "claim"` (added by Plan 008).

`disconnectnode <address>` disconnects a peer, used to simulate share withholding.

The test starts three `rngd` instances on regtest with `-vbparams=sharepool:0:9999999999:0` to activate sharepool from block 0. Node 0 is Miner A (4 threads), Node 1 is Miner B (1 thread), and Node 2 is the observer (no mining). All three are connected in a triangle topology.


## Plan of Work

The work is a single new file: `test/functional/feature_sharepool_e2e.py`. It contains one test class with seven phases executed sequentially. Each phase has its own assertions and produces diagnostic output that is captured in the test log for review at the Plan 010 decision gate.

Phase 1 (pre-activation regression) starts the three nodes without the sharepool vbparams override and mines 10 blocks using `self.generatetoaddress()`. It verifies that `getdeploymentinfo` does not show sharepool as active, that blocks have a single coinbase output paying the full reward to the miner, and that `getmininginfo` does not include sharepool fields. This phase proves that existing behavior is preserved. After verification, it stops the nodes.

Phase 2 (activation and share production) restarts all three nodes with `-vbparams=sharepool:0:9999999999:0` and mining enabled on nodes 0 and 1. It verifies that `getdeploymentinfo` shows sharepool as active on all nodes. It then lets the internal miners run for approximately 20 blocks (waiting for `self.nodes[0].getblockcount()` to reach the target). It verifies that both miners show `accepted_shares > 0` in `getmininginfo` and that the observer's sharechain tip matches the miners' sharechain tips.

Phase 3 (proportional reward verification) lets mining continue for an additional 50 blocks beyond Phase 2. It then queries `getmininginfo` on both miners to get their `accepted_shares` counts and `pending_pooled_reward` values. It computes the share ratio (Miner A shares / total shares) and the reward ratio (Miner A pending / total pending). It asserts that both ratios are between 0.70 and 0.95 (the expected range for a 4:1 thread ratio over 50 blocks). It also queries `getrewardcommitment` for several blocks and verifies that the leaf amounts sum to the total block reward (subsidy plus fees) for each block.

Phase 4 (observer independent verification) queries the observer node (node 2, no mining) for its view of the sharechain and payout commitments. For each of the last 10 blocks, it calls `getrewardcommitment` on the observer and compares the root to the root in the block's coinbase (obtained via `getblock`). All roots must match. This proves R2: the observer independently computed the same commitment from its own copy of the share history.

Phase 5 (wallet claim flow) waits for enough blocks that the earliest activated block's coinbase has matured (100 confirmations after the first activated block). It then checks `getbalances` on both miners and verifies that `pooled.claimable` is positive on at least one miner. It waits up to 10 more blocks for claim transactions to appear (the wallet auto-claims from Plan 008). It then checks `listtransactions` on both miners for transactions with `category: "claim"` and verifies at least one claim has confirmed. It verifies that the claimed amounts match the expected leaf amounts from `getrewardcommitment`. After claims confirm, it verifies that `mine.trusted` increased and `pooled.claimable` decreased.

Phase 6 (share withholding resilience) disconnects Miner B (node 1) from both Miner A and the observer using `disconnectnode`. With Miner B isolated, Miner A continues mining 20 more blocks. The test verifies that Miner A's `accepted_shares` continues to increase, that the observer sees the same share tip as Miner A, and that payout commitments on these blocks allocate rewards only to Miner A (since Miner B's shares are not visible). It then reconnects Miner B and verifies that the network converges (all three nodes agree on the best block and share tip).

Phase 7 (solo mining special case) stops Miner B entirely and lets Miner A mine 10 more blocks alone. It verifies that payout commitments on these blocks have exactly one leaf, that the single leaf's payout script matches Miner A's `-mineaddress`, and that the leaf amount equals the full block reward. This proves R10: solo mining is a degenerate case of pooled mining.

At the end of all phases, the test prints a summary table showing: total blocks mined, Miner A shares, Miner B shares, share ratio, reward ratio, observer verification results, claims attempted, claims confirmed, and any anomalies. This summary is the primary artifact for the Plan 010 decision gate.


## Implementation Units

### Unit 1: Test Framework and Phase 1 (Pre-Activation Regression)

Goal: Establish the test file, configure three nodes, and verify pre-activation behavior is unchanged.

Requirements advanced: R8.

Dependencies: Plan 004 (DEPLOYMENT_SHAREPOOL must exist but be inactive by default).

Files to create:

- `test/functional/feature_sharepool_e2e.py` (new)

Approach: Create a new Python test class `SharepoolEndToEndTest` extending `BitcoinTestFramework`. In `set_test_params()`, set `self.num_nodes = 3`. Phase 1 starts nodes without sharepool activation, mines 10 blocks, and asserts pre-activation conditions. The test uses `self.generatetoaddress()` for deterministic block production in Phase 1 only; subsequent phases use the internal miner for realistic share production.

Test scenarios:

- `getdeploymentinfo` shows sharepool not active on default regtest.
- 10 mined blocks each have a single coinbase output with the full block reward.
- `getmininginfo` does not contain `sharepool_active` or `accepted_shares` fields.

### Unit 2: Phases 2-3 (Activation, Share Production, Proportional Rewards)

Goal: Verify shares are produced, propagated, and rewards are split proportionally.

Requirements advanced: R1, R3, R4, R5, R6.

Dependencies: Plans 004, 005, 007, 008.

Files to modify:

- `test/functional/feature_sharepool_e2e.py` (extend)

Approach: Restart nodes with sharepool activated. Nodes 0 and 1 mine with 4 and 1 threads respectively. Node 2 is the observer. Let mining run until 50 activated blocks accumulate. Assert share counts and reward proportions.

Test scenarios:

- Both miners show `accepted_shares > 0` after 20 blocks.
- The share ratio (Miner A / total) is between 0.70 and 0.95 over 50 blocks.
- The pending reward ratio matches the share ratio within tolerance.
- Every activated block's coinbase has exactly one witness v2 payout commitment output.
- Leaf amounts in `getrewardcommitment` sum to the block reward for each block.

### Unit 3: Phase 4 (Observer Independent Verification)

Goal: Prove that an independent non-mining node computes the same payout commitments.

Requirements advanced: R2.

Dependencies: Plans 005, 007.

Files to modify:

- `test/functional/feature_sharepool_e2e.py` (extend)

Approach: For the last 10 blocks, call `getrewardcommitment` on the observer (node 2) and extract the root. Also call `getblock` to get the block's coinbase and extract the witness v2 output's program (the commitment root from the chain). Assert they are byte-identical.

Test scenarios:

- For each of 10 sampled blocks, the observer's computed root matches the block's on-chain commitment root.
- The observer's sharechain tip matches both miners' sharechain tips (queried via `getsharechaininfo`).

### Unit 4: Phase 5 (Wallet Claim Flow)

Goal: Prove that wallet claim transactions are built, broadcast, and confirmed end-to-end.

Requirements advanced: R7, R11.

Dependencies: Plan 008 (automatic claim construction, getbalances pooled fields).

Files to modify:

- `test/functional/feature_sharepool_e2e.py` (extend)

Approach: Wait for the first activated block's coinbase to mature (mine additional blocks until block count exceeds 100 + first activated block height). Check `getbalances` for `pooled.claimable > 0`. Wait up to 10 more blocks for claim transactions to appear in `listtransactions` with `category: "claim"`. Verify claimed amounts and balance transitions.

Test scenarios:

- `getbalances` shows `pooled.pending > 0` before maturity.
- `getbalances` shows `pooled.claimable > 0` after maturity.
- At least one claim transaction appears in `listtransactions` with `category: "claim"`.
- After claim confirmation, `mine.trusted` increases and `pooled.claimable` decreases.

### Unit 5: Phases 6-7 (Withholding Resilience and Solo Mining)

Goal: Verify that one miner's disconnection does not break the other's rewards, and that solo mining works as a special case.

Requirements advanced: R4, R10.

Dependencies: Plans 005, 007, 008.

Files to modify:

- `test/functional/feature_sharepool_e2e.py` (extend)

Approach: Phase 6 disconnects Miner B, mines 20 blocks with Miner A only, verifies that payout commitments allocate only to Miner A, then reconnects. Phase 7 stops Miner B entirely, mines 10 blocks, and verifies single-leaf payout commitments matching Miner A's address with full block reward.

Test scenarios:

- After disconnecting Miner B, blocks mined by Miner A have payout commitments with leaves only for Miner A.
- The observer sees the same blocks and commits as Miner A during the disconnection.
- After reconnection, all three nodes converge on the same block tip and share tip.
- With only Miner A active, payout commitments have exactly one leaf with the full block reward.
- The solo miner's `pending_pooled_reward` in `getmininginfo` accounts for the full block reward.


## Concrete Steps

All commands run from the repository root.

1. Ensure the project builds with all dependencies from Plans 004-008:

       cmake -S . -B build
       cmake --build build -j"$(nproc)" --target rngd rng-cli

   Expected: clean build, no new warnings.

2. Run the end-to-end test:

       test/functional/test_runner.py feature_sharepool_e2e.py

   Expected: the test runs for several minutes (regtest mining with internal miner takes real time for share production). Output ends with "Tests successful" and a summary table.

   Alternative, for more verbose output:

       python3 test/functional/feature_sharepool_e2e.py --loglevel=DEBUG

   Expected: detailed phase-by-phase output including share counts, reward ratios, observer verification results, and claim transaction details.

3. If the test fails, examine the log file:

       cat /tmp/test_runner_*/feature_sharepool_e2e/test_framework.log

   The log contains per-phase diagnostics, RPC responses, and the assertion that failed. Use this to identify which earlier plan's component is broken.

4. Run the prerequisite tests to confirm the components work individually:

       test/functional/test_runner.py \
         feature_sharepool_activation.py \
         feature_sharepool_relay.py \
         feature_sharepool_claims.py \
         feature_sharepool_wallet.py \
         feature_sharepool_mining.py

   Expected: all pass. If any fail, fix the component before running the end-to-end test.

5. Run the full existing test suite to check for regressions:

       test/functional/test_runner.py

   Expected: all existing tests pass. The sharepool changes do not regress non-sharepool behavior.


## Validation and Acceptance

The implementation is accepted when all of the following are true.

The end-to-end test passes completely when run via `test/functional/test_runner.py feature_sharepool_e2e.py`.

Over 50 activated blocks with a 4:1 thread ratio, the dominant miner's share of total rewards is between 70% and 95%. This proves proportional reward distribution under stochastic hash production.

The observer node computes the same payout commitment root as the block's on-chain commitment for every sampled block. This proves deterministic replay from public share history.

At least one wallet claim transaction succeeds after maturity on each miner. This proves the complete claim flow works end-to-end.

After Miner B is disconnected, Miner A's rewards continue accumulating normally and payout commitments on those blocks do not include Miner B. After reconnection, all nodes converge. This proves share withholding does not corrupt rewards for honest miners.

Pre-activation blocks (Phase 1) validate unchanged: classical coinbase, no payout commitment, no sharepool RPC fields.

The existing functional test suite (non-sharepool tests) passes without regression.


## Idempotence and Recovery

The test creates temporary datadirs and cleans up automatically. It can be run repeatedly without any manual cleanup. If the test hangs (for example, waiting for blocks that are never mined due to a bug), it will time out after the framework's default timeout (typically 60 seconds per wait call). The timeout produces a clear error indicating which wait call failed.

If the test fails partway through, the most useful diagnostic is the test log. The Concrete Steps section above shows how to find and read it.

No persistent state is modified by this test. No regtest datadir survives after the test completes (whether it passes or fails). Running the test again starts from scratch.


## Artifacts and Notes

The test summary table printed at the end of a successful run:

    === Sharepool End-to-End Proof Summary ===
    Total blocks mined:          80+
    Activated blocks:            50+ (Phase 2-3)
    Miner A shares:              ~200
    Miner B shares:              ~50
    Share ratio (A / total):     ~0.80
    Reward ratio (A / total):    ~0.80
    Observer verified blocks:    10/10
    Claims attempted (A):       >=1
    Claims confirmed (A):       >=1
    Claims attempted (B):       >=1
    Claims confirmed (B):       >=1
    Solo mining leaf count:     1
    Pre-activation regression:  PASS
    ==========================================

The exact numbers will vary due to RandomX hash stochasticity. The assertions use tolerance bands, not exact values. The summary is printed to stdout and also written to the test log.

The test file structure:

    test/functional/feature_sharepool_e2e.py
    |
    |-- class SharepoolEndToEndTest(BitcoinTestFramework)
    |   |-- set_test_params()         # 3 nodes, regtest
    |   |-- run_test()                # orchestrates all phases
    |   |-- phase_1_preactivation()   # regression test
    |   |-- phase_2_activation()      # start mining with shares
    |   |-- phase_3_proportional()    # verify reward split
    |   |-- phase_4_observer()        # independent verification
    |   |-- phase_5_claims()          # wallet claim flow
    |   |-- phase_6_withholding()     # disconnect and verify
    |   |-- phase_7_solo()            # single miner special case
    |   |-- print_summary()           # summary table

Each phase method is self-contained and prints its own result. If a phase fails, subsequent phases are skipped and the failure is reported clearly.


## Interfaces and Dependencies

This plan consumes interfaces from Plans 004-008 but introduces no new runtime interfaces. The only artifact is the test file itself.

From Plan 004, the test depends on the `-vbparams=sharepool:0:9999999999:0` activation mechanism working on regtest.

From Plan 005, the test depends on:

- `getsharechaininfo` RPC returning the current share tip.
- Share relay working between connected peers (shares propagate from miners to observer).

From Plan 007, the test depends on:

- `getrewardcommitment <blockhash>` RPC returning the commitment root and leaves.
- Payout commitment outputs appearing in coinbase transactions of activated blocks.
- Claim-spend validation accepting valid claim transactions.

From Plan 008, the test depends on:

- Internal miner producing shares continuously after activation.
- `getmininginfo` returning `sharepool_active`, `share_tip`, `pending_pooled_reward`, and `accepted_shares`.
- `getbalances` returning `pooled.pending` and `pooled.claimable`.
- Automatic wallet claim construction after maturity.
- `listtransactions` showing claim transactions with `category: "claim"`.

The test uses standard `BitcoinTestFramework` facilities:

- `self.nodes[i]` for RPC access to each node.
- `self.connect_nodes(i, j)` to establish P2P connections.
- `self.disconnect_nodes(i, j)` to simulate network partitions.
- `self.generatetoaddress(n, addr)` for deterministic block production in Phase 1.
- `self.wait_until(lambda: condition, timeout=60)` for waiting on asynchronous events.
- `self.sync_blocks()` to wait for all nodes to reach the same block height.

No external dependencies. No network access. No operator infrastructure. The test runs entirely on the local machine using loopback connections between regtest nodes.
